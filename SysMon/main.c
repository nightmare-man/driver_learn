#include <ntddk.h>
#include "SysMon.h"
#define DRIVER_TAG 12345678U
struct Globals g_global;

//这是一个普通内核模式APC，在PASSIVE_LEVEL （irql 0）上运行，因此可以使用分页内存，
// 创建的通知回调是由创建的第一个线程调用 
// 退出的通知回调是由退出的最后一个线程调用。
//这里的handle不是真的 handle，就是一个DWORD pid

void PushItem(PLIST_ENTRY entry_to_insert) {
	ExAcquireFastMutex(&g_global.mutex);
	//理论上不应该使用1024，应该用读取注册表，可以配置
	if (g_global.item_count >= 1024) {
		PLIST_ENTRY entry1 = RemoveHeadList(&g_global.item_header);
		g_global.item_count--;
		//用entry反推整个结构的首地址
		struct FullProcessExitInfo* item = CONTAINING_RECORD(entry1, struct FullProcessExitInfo, entry);
		ExFreePool(item);//释放内存 防止泄露
	}
	InsertTailList(&g_global.item_header, entry_to_insert);
	g_global.item_count++;
	ExReleaseFastMutex(&g_global.mutex);
}

VOID on_process_create(_Inout_ PEPROCESS process, _In_ HANDLE process_id, _Inout_opt_ PPS_CREATE_NOTIFY_INFO create_info) {
	UNREFERENCED_PARAMETER(process);
	if (create_info) {
		//create process
		unsigned short alloc_size = sizeof(struct FullProcessCreateInfo);
		unsigned short cmd_line_size = 0;
		if (create_info->CommandLine) {
			cmd_line_size = create_info->CommandLine->Length;
			//alloc_size += cmd_line_size * sizeof(WCHAR);//这个地方原著写的没有*wchar的大小，我觉得有问题
			//后补充：原著是对的，因为UNICODE_STRING的Length是字节的大小
			alloc_size += cmd_line_size;
		}
		struct FullProcessCreateInfo* item = (struct FullProcessCreateInfo*)ExAllocatePoolWithTag(PagedPool, alloc_size, DRIVER_TAG);
		if (!item) {
			KdPrint(("[SysMon] fail to allocate mem"));
			return;
		}
		//这个size不是full的size，不包含链表结构的数据的实际大小
		item->info.header.size = sizeof(struct ProcessCreateInfo) + cmd_line_size;
		item->info.header.type = ProcessCreate;
		KeQuerySystemTimePrecise(&item->info.header.time);
		item->info.process_id = HandleToULong(process_id);
		item->info.cmd_line_offset = sizeof(struct ProcessCreateInfo);
		item->info.cmd_line_len = cmd_line_size;
		memcpy((unsigned char*)(&(item->info)) + item->info.cmd_line_offset, create_info->CommandLine->Buffer, cmd_line_size );
		PushItem(&item->entry);
	}
	else {
		//exit process
		struct FullProcessExitInfo* info = (struct FullProcessExitInfo*)ExAllocatePoolWithTag(PagedPool, sizeof(struct FullProcessExitInfo), DRIVER_TAG);
		if (!info) {
			KdPrint(("[SysMon] fail to allocate mem"));
			return;
		}
		info->info.header.size = sizeof(struct ProcessExitInfo);
		info->info.header.type = ProcessExit;
		KeQuerySystemTimePrecise(&(info->info.header.time));
		info->info.process_id = HandleToULong(process_id);
		PushItem(&(info->entry));
	}

}

NTSTATUS complete_irp(PIRP irp, NTSTATUS status, ULONG information) {
	irp->IoStatus.Status = status;
	irp->IoStatus.Information = information;
	IoCompleteRequest(irp, 0);
	return status;
}


NTSTATUS driver_unload(_In_ PDRIVER_OBJECT driver_object) {
	PsSetCreateProcessNotifyRoutineEx(on_process_create, TRUE);
	UNICODE_STRING dev_link_name = RTL_CONSTANT_STRING(L"\\??\\SysMon");
	IoDeleteSymbolicLink(&dev_link_name);
	IoDeleteDevice(driver_object->DeviceObject);
	while (!IsListEmpty(&g_global.item_header)) {
		PLIST_ENTRY entry1 = RemoveHeadList(&g_global.item_header);
		struct FullProcessExitInfo* item = CONTAINING_RECORD(entry1, struct FullProcessExitInfo, entry);
		ExFreePool(item);
	}
	return STATUS_SUCCESS;
}

NTSTATUS driver_create_close(_In_ PDEVICE_OBJECT dev, _In_ PIRP irp) {
	UNREFERENCED_PARAMETER(dev);
	return complete_irp(irp, STATUS_SUCCESS, 0);
}

NTSTATUS driver_read(_In_ PDEVICE_OBJECT dev, _In_ PIRP irp) {
	UNREFERENCED_PARAMETER(dev);
	PIO_STACK_LOCATION stack= IoGetCurrentIrpStackLocation(irp);
	ULONG len = stack->Parameters.Read.Length;
	ULONG count = 0;//储存最终写入的字节数
	NT_ASSERT(irp->MdlAddress);
	UCHAR* buffer = (UCHAR*)MmGetSystemAddressForMdlSafe(irp->MdlAddress, NormalPagePriority);
	if (!buffer) {
		return complete_irp(irp, STATUS_INVALID_BUFFER_SIZE, 0);
	}
	ExAcquireFastMutex(&g_global.mutex);
	while (TRUE) {
		if (IsListEmpty(&g_global.item_header)) {
			break;
		}
		//从队尾加 从队头拿
		LIST_ENTRY* entry1 = RemoveHeadList(&g_global.item_header);
		//这里说明，我们不知道下一个item到底是那种结构（FullProcessCreate/Exit），但是我们随便按照一个结构拿,
		//因为我们只需要读这个结构的大小就行了，都在相同的位置
		struct FullProcessExitInfo* item = CONTAINING_RECORD(entry1, struct FullProcessExitInfo, entry);
		USHORT size = item->info.header.size;
		if (len < size) {
			//用户态传过来的buffer小了，不复制的，插回去。
			InsertHeadList(&g_global.item_header, entry1);
			break;
		}
		g_global.item_count--;
		memcpy(buffer, &item->info, size);
		buffer += size;
		len -= size;
		count += size;
		ExFreePool(item);
	}
	ExReleaseFastMutex(&g_global.mutex);
	return complete_irp(irp, STATUS_SUCCESS, count);
}

NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT driver_object, _In_ PUNICODE_STRING register_path) {
	UNREFERENCED_PARAMETER(register_path);
	InitializeListHead(&(g_global.item_header));
	//其实这里没必要用这种形式显示分配非分页内存，windows驱动的全局变量默认就是非分页内存上的
	//g_global.mutex = ExAllocatePoolWithTag(NonPagedPool, sizeof(FAST_MUTEX), 123456);
	//if (!g_global.mutex) {
	//	return STATUS_INVALID_ADDRESS;
	//}
	ExInitializeFastMutex(&g_global.mutex);
	PDEVICE_OBJECT dev = NULL;
	UNICODE_STRING dev_name = RTL_CONSTANT_STRING(L"\\Device\\SysMon");
	UNICODE_STRING dev_link_name = RTL_CONSTANT_STRING(L"\\??\\SysMon");
	
	NTSTATUS ret = STATUS_SUCCESS;
	BOOLEAN dev_created = FALSE;
	BOOLEAN link_created = FALSE;
	do {
		ret = IoCreateDevice(driver_object, 0, &dev_name, FILE_DEVICE_UNKNOWN, 0, FALSE, &dev);
		if (!NT_SUCCESS(ret)) {
			KdPrint(("[SysMon] fail to create deivce(0x%08X)\n", ret));
			break;
		}
		dev_created = TRUE;

		dev->Flags |= DO_DIRECT_IO;
		ret = IoCreateSymbolicLink(&dev_link_name, &dev_name);
		if (!NT_SUCCESS(ret)) {
			KdPrint(("[SysMon] fail to create deivce symbol link(0x%08X)\n", ret));
			break;
		}
		link_created = TRUE;

		ret = PsSetCreateProcessNotifyRoutineEx(on_process_create, FALSE);
		if (!NT_SUCCESS(ret)) {
			KdPrint(("[SysMon] fail to set process create notify(0x%08X)\n", ret));
			break;
		}

	} while (FALSE);
	if (!NT_SUCCESS(ret)) {
		if (dev_created) {
			IoDeleteDevice(dev);
		}
		if (link_created) {
			IoDeleteSymbolicLink(&dev_link_name);
		}
	}
	driver_object->DriverUnload = driver_unload;
	driver_object->MajorFunction[IRP_MJ_CREATE] = driver_object->MajorFunction[IRP_MJ_CLOSE] = driver_create_close;
	driver_object->MajorFunction[IRP_MJ_READ] = driver_read;
	return ret;
}