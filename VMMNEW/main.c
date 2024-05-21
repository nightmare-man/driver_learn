#include <ntddk.h>
#include "common.h"
#include "tool.h"
#include "VMM.h"
PDEVICE_OBJECT dev = NULL;

NTSTATUS
device_create(
	_In_ struct _DEVICE_OBJECT* DeviceObject,
	_Inout_ struct _IRP* Irp
) {
	UNREFERENCED_PARAMETER(DeviceObject);
	if (!init_vmx()) {
		return STATUS_UNSUCCESSFUL;
	}
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}
NTSTATUS
device_close(
	_In_ struct _DEVICE_OBJECT* DeviceObject,
	_Inout_ struct _IRP* Irp
) {
	UNREFERENCED_PARAMETER(DeviceObject);
	leave_vmx();
	
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}


NTSTATUS driver_unload(PDRIVER_OBJECT driver) {
	UNREFERENCED_PARAMETER(driver);
	IoDeleteDevice(dev);
	UNICODE_STRING link_name = RTL_CONSTANT_STRING(L"\\??\\VMMNEW");
	IoDeleteSymbolicLink(&link_name);
	//leave_vmx(g_vmm_state_ptr);
	return STATUS_SUCCESS;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING reg) {
	UNREFERENCED_PARAMETER(reg);
	driver->DriverUnload = driver_unload;
	NTSTATUS ret = STATUS_SUCCESS;
	UNICODE_STRING dev_name = RTL_CONSTANT_STRING(L"\\Device\\VMMNEW");
	UNICODE_STRING link_name = RTL_CONSTANT_STRING(L"\\??\\VMMNEW");
	driver->MajorFunction[IRP_MJ_CREATE] = device_create;
	driver->MajorFunction[IRP_MJ_CLOSE] = device_close;
	//driver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = device_control;
	ret = IoCreateDevice(driver, 0, &dev_name, FILE_DEVICE_UNKNOWN, 0, FALSE, &dev);
	if (!NT_SUCCESS(ret)) {
		KdPrint(("[prefix] create device fail\n"));
		return ret;
	}
	KdPrint(("[prefix] create device success\n"));
	ret = IoCreateSymbolicLink(&link_name, &dev_name);
	if (!NT_SUCCESS(ret)) {
		KdPrint(("[prefix] create link fail\n"));
		IoDeleteDevice(dev);
		return ret;
	}
	
	return STATUS_SUCCESS;
}