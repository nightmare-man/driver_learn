#include <ntddk.h>
#include "common.h"
#include "tool.h"
#include "VMM.h"
extern VOID save_and_virtualize(ULONG cpu_idx);

NTSTATUS driver_unload(PDRIVER_OBJECT driver) {
	UNREFERENCED_PARAMETER(driver);
	leave_vmx();
	return STATUS_SUCCESS;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING reg) {
	UNREFERENCED_PARAMETER(reg);
	driver->DriverUnload = driver_unload;
	init_vmx();
	//ULONG64 eptp = init_eptp();
	ULONG cpu_count= KeQueryActiveProcessorCount(0);
	for (ULONG i = 0; i < cpu_count; i++) {
		allocate_vmm_stack(i);
		allocate_msr_bitmaps(i);
		run_on_cpu(i, save_and_virtualize);
	}
	return STATUS_SUCCESS;
}