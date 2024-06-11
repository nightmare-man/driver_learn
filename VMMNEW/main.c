#include <ntddk.h>
#include "common.h"
#include "tool.h"
#include "VMM.h"


NTSTATUS driver_unload(PDRIVER_OBJECT driver) {
	UNREFERENCED_PARAMETER(driver);
	leave_vmx();
	return STATUS_SUCCESS;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING reg) {
	UNREFERENCED_PARAMETER(reg);
	driver->DriverUnload = driver_unload;
	init_vmx();
	ULONG64 eptp = init_eptp();
	launch_vmx(&(g_vmm_state_ptr[0]), 0, eptp);
	KdPrint(("return by launch\n"));
	return STATUS_SUCCESS;
}