#include <ntddk.h>
#include "common.h"
#include "tool.h"
#include "VMM.h"
extern VOID save_and_virtualize(ULONG cpu_idx);

NTSTATUS driver_unload(PDRIVER_OBJECT driver) {
	UNREFERENCED_PARAMETER(driver);
	return STATUS_SUCCESS;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING reg) {
	/*
		目前在虚拟机里多核会卡住，但是实体机上不会卡住，单核都没问题

	*/
	UNREFERENCED_PARAMETER(reg);
	driver->DriverUnload = driver_unload;
	//DbgBreakPoint();
	init_vmx();
	//ULONG64 eptp = init_eptp();
	ULONG cpu_count= KeQueryActiveProcessorCount(NULL);
	for (ULONG i = 0; i < cpu_count; i++) {
		allocate_vmm_stack(i);
		allocate_msr_bitmaps(i);
		run_on_cpu(i, save_and_virtualize);
	}
	return STATUS_SUCCESS;
}