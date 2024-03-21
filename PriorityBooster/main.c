#include <ntifs.h>
#include "ntddk.h"
#include "common.h"
void driver_unload(_In_ PDRIVER_OBJECT driver_object) {
	UNICODE_STRING symlink = RTL_CONSTANT_STRING(L"\\??\\PriorityBooster");
	IoDeleteSymbolicLink(&symlink);
	IoDeleteDevice(driver_object->DeviceObject);
}
_Use_decl_annotations_
NTSTATUS create_close(struct _DEVICE_OBJECT* driver_object, _In_ PIRP irp) {
	UNREFERENCED_PARAMETER(driver_object);
	irp->IoStatus.Status = STATUS_SUCCESS;
	irp->IoStatus.Information = 0;
	IoCompleteRequest(irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS device_control(struct _DEVICE_OBJECT* driver_object, _In_ PIRP irp) {
	UNREFERENCED_PARAMETER(driver_object);
	IO_STACK_LOCATION* stack = IoGetCurrentIrpStackLocation(irp);
	NTSTATUS ret = STATUS_SUCCESS;
	switch (stack->Parameters.DeviceIoControl.IoControlCode) {
	case IOCTL_PRIORITY_BOOSTER_SET_PRIORITY:
		if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(struct ThreadData)) {
			ret = STATUS_BUFFER_TOO_SMALL;
			
		}
		else {
			struct ThreadData* data = (struct ThreadData*)stack->Parameters.DeviceIoControl.Type3InputBuffer;
			if (data == NULL) {
				ret = STATUS_INVALID_PARAMETER;
			}
			else if (data->priority<1||data->priority>31) {
				ret = STATUS_INVALID_PARAMETER;
			}
			else {
				PETHREAD thread;
				ret = PsLookupThreadByThreadId(ULongToHandle(data->thread_id), &thread);//会增加引用计数，要手动减少
				if (!NT_SUCCESS(ret)) {
					break;
				}
				KeSetPriorityThread(thread, data->priority);
				ObDereferenceObject(thread);
			}
		}

		break;
	default:
		ret = STATUS_INVALID_DEVICE_REQUEST;
		break;
	}
	irp->IoStatus.Status = ret;
	irp->IoStatus.Information = 0;
	IoCompleteRequest(irp, IO_NO_INCREMENT);
	return ret;
}

NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT driver_object, _In_ PUNICODE_STRING register_path) {
	UNREFERENCED_PARAMETER(register_path);
	
	UNICODE_STRING dev_name = RTL_CONSTANT_STRING(L"\\Device\\PriorityBooster");
	PDEVICE_OBJECT dev_object;
	NTSTATUS status = IoCreateDevice(driver_object, 0, &dev_name, FILE_DEVICE_UNKNOWN, 0, FALSE, &dev_object);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Fail to create device obejct (0x%08X)\n", status));
		return status;
	}
	UNICODE_STRING symlink = RTL_CONSTANT_STRING(L"\\??\\PriorityBooster");
	status = IoCreateSymbolicLink(&symlink, &dev_name);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Fail to create device symlink (0x%08X)\n", status));
		IoDeleteDevice(dev_object);//失败了要回滚
		return status;
	}
	driver_object->DriverUnload = driver_unload;
	driver_object->MajorFunction[IRP_MJ_CREATE] = create_close;
	driver_object->MajorFunction[IRP_MJ_CLOSE] = create_close;
	driver_object->MajorFunction[IRP_MJ_DEVICE_CONTROL] = device_control;
	return STATUS_SUCCESS;
}


