#include "ntddk.h"
#include "ProcessProtect.h"
#include "common.h"


struct Globals global;

BOOLEAN check_in_protect_list(ULONG pid) {
	BOOLEAN ret = FALSE;
	ExAcquireFastMutex(&global.lock);
	for (int i = 0; i < global.pid_count; i++) {
		if (global.pids[i] == pid) {
			ret = TRUE;
			break;
		}
	}
	ExReleaseFastMutex(&global.lock);
	return ret;
}
VOID add_protect_list(ULONG pid) {
	ExAcquireFastMutex(&global.lock);
	BOOLEAN existed = FALSE;
	for (int i = 0; i < global.pid_count; i++) {
		if (global.pids[i] == pid) {
			existed = TRUE;
			break;
		}
	}
	if (!existed) {
		global.pids[global.pid_count] = pid;
		global.pid_count++;
	}
	ExReleaseFastMutex(&global.lock);
}
VOID remove_protect_list(ULONG pid) {
	ExAcquireFastMutex(&global.lock);
	int idx = -1;
	for (int i = 0; i < global.pid_count; i++) {
		if (global.pids[i] == pid) {
			idx = i;
			break;
		}
	}
	if (idx != -1) {
		for (int i = idx; i < global.pid_count - 1; i++) {
			global.pids[i] = global.pids[i + 1];
		}
		global.pid_count--;
	}
	ExReleaseFastMutex(&global.lock);
}
VOID clear_protect_list() {
	ExAcquireFastMutex(&global.lock);
	global.pid_count = 0;
	ExReleaseFastMutex(&global.lock);
}


OB_PREOP_CALLBACK_STATUS on_pre_open(
	_In_ PVOID RegistrationContext,
	_Inout_ POB_PRE_OPERATION_INFORMATION OperationInformation
) {
	UNREFERENCED_PARAMETER(RegistrationContext);
	//内核访问 直接放行
	
	PVOID process = OperationInformation->Object;
	ULONG pid =  HandleToULong(PsGetProcessId(process));
	if (check_in_protect_list(pid)) {
		OperationInformation->Parameters->CreateHandleInformation.DesiredAccess &= ~PROCESS_TERMINATE;
		//去掉其关闭权限
	}
	return OB_PREOP_SUCCESS;
	
};

NTSTATUS complete_irp(PIRP irp, NTSTATUS status, ULONG information) {
	irp->IoStatus.Status = status;
	irp->IoStatus.Information = information;
	IoCompleteRequest(irp, IO_NO_INCREMENT);
	return status;
}

NTSTATUS driver_create_close(_In_ PDEVICE_OBJECT dev, _Inout_ PIRP irp) {
	UNREFERENCED_PARAMETER(dev);
	return complete_irp(irp, STATUS_SUCCESS, 0);
}

NTSTATUS driver_control(_In_ PDEVICE_OBJECT dev, _Inout_ PIRP irp) {
	
	UNREFERENCED_PARAMETER(dev);
	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(irp);
	NTSTATUS ret = STATUS_SUCCESS;
	ULONG len = stack->Parameters.DeviceIoControl.InputBufferLength;
	KdPrint(("[ProcessProtect] recevice control(0x%08X)", stack->Parameters.DeviceIoControl.IoControlCode));
	switch (stack->Parameters.DeviceIoControl.IoControlCode) {
	case IOCTL_PROCESS_PROTECT_BY_PID: {
		if (len < sizeof(ULONG)) {
			ret = STATUS_INVALID_BUFFER_SIZE;
			break;
		}
		//正让我碰见了用户内存访问异常了
		//我在这儿用了buffered方式，但是用错了地址
		//应该使用irp->AssociatedIrp.SystemBuffer
		//提供缓冲io和直接io的作用是，防止内核访问用户内存违例，很容易早就被释放了
		ULONG pid = *((ULONG*)irp->AssociatedIrp.SystemBuffer);
		//ULONG pid = *((ULONG*)(stack->Parameters.DeviceIoControl.Type3InputBuffer));
		add_protect_list(pid);
		KdPrint(("[ProcessProtect] pid(0x%08X)", pid));
		break;
	}
		
	case IOCTL_PROCESS_UNPROTECT_BY_PID: {
		if (len < sizeof(ULONG)) {
			ret = STATUS_INVALID_BUFFER_SIZE;
			break;
		}
		ULONG pid = *((ULONG*)irp->AssociatedIrp.SystemBuffer);
		remove_protect_list(pid);
		KdPrint(("[ProcessProtect] pid(0x%08X)", pid));
		break;
	}
		
	case IOCTL_PROCESS_PROTECT_CLEAR:
		clear_protect_list();
		break;
	default:
		ret = STATUS_DEVICE_INSUFFICIENT_RESOURCES;
		break;
	}
	return complete_irp(irp, ret, 0);
}

NTSTATUS driver_unload(_In_ PDRIVER_OBJECT driver_object) {
	UNICODE_STRING link_name = RTL_CONSTANT_STRING(L"\\??\\ProcessProtect");
	IoDeleteSymbolicLink(&link_name);
	IoDeleteDevice(driver_object->DeviceObject);
	ObUnRegisterCallbacks(global.reg_handle);
	return STATUS_SUCCESS;
}

//在DriverEntry没有返回之前， 设置的例程都不能被调用
NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT driver_object, _In_ PUNICODE_STRING register_path) {
	UNREFERENCED_PARAMETER(register_path);
	ExInitializeFastMutex(&global.lock);
	clear_protect_list();
	PDEVICE_OBJECT dev = NULL;
	UNICODE_STRING dev_name = RTL_CONSTANT_STRING(L"\\Device\\ProcessProtect");
	UNICODE_STRING link_name = RTL_CONSTANT_STRING(L"\\??\\ProcessProtect");
	NTSTATUS ret = STATUS_SUCCESS;
	BOOLEAN dev_created = FALSE;
	BOOLEAN link_created = FALSE;
	do {
		ret = IoCreateDevice(driver_object, 0, &dev_name, FILE_DEVICE_UNKNOWN, 0, FALSE, &dev);
		if (!NT_SUCCESS(ret)) {
			KdPrint(("[ProcessProtect] create device fail(0x%08X)\n", ret));
			break;
		}
		dev_created = TRUE;
		ret = IoCreateSymbolicLink(&link_name, &dev_name);
		if (!NT_SUCCESS(ret)) {
			KdPrint(("[ProcessProtect] create device symbol link fail(0x%08X)\n", ret));
			break;
		}
		link_created = TRUE;
		//需不需要设置用户IO访问模式呢？不需要 再IRP_MJ_DEVICE_CONTROL中由用户端设置

		//c语言也支持这种结构体初始化
		OB_OPERATION_REGISTRATION operations[] = {
			{
				.ObjectType = PsProcessType,
				.Operations = OB_OPERATION_HANDLE_CREATE | OB_OPERATION_HANDLE_DUPLICATE,
				.PreOperation = on_pre_open,
				.PostOperation = NULL,
			}
		};
		OB_CALLBACK_REGISTRATION reg = {
			.Altitude = RTL_CONSTANT_STRING(L"12345.678"),
			.Version = OB_FLT_REGISTRATION_VERSION,
			.OperationRegistrationCount = 1,
			.OperationRegistration = operations,
			.RegistrationContext = NULL
		};
		ret=ObRegisterCallbacks(&reg, &global.reg_handle);
		if (!NT_SUCCESS(ret)) {
			KdPrint(("[ProcessProtect] create object callback fail(0x%08X)\n", ret));
			break;
		}

	} while (FALSE);
	if (!NT_SUCCESS(ret)) {
		if (link_created) {
			IoDeleteSymbolicLink(&link_name);
		}
		if (dev_created) {
			IoDeleteDevice(dev);
		}
	}
	driver_object->DriverUnload = driver_unload;
	driver_object->MajorFunction[IRP_MJ_CREATE] = driver_object->MajorFunction[IRP_MJ_CLOSE]= driver_create_close;
	driver_object->MajorFunction[IRP_MJ_DEVICE_CONTROL] = driver_control;
	KdPrint(("[ProcessProtect] driver entry result(0x%08X)", ret));
	return ret;
}