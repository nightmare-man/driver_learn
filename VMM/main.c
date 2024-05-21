#include <ntddk.h>
#include <intrin.h>
NTSTATUS DriverUnload(PDRIVER_OBJECT driver) {
	UNREFERENCED_PARAMETER(driver);
	return STATUS_SUCCESS;
}
NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING reg) {
	UNREFERENCED_PARAMETER(reg);
	driver->DriverUnload = DriverUnload;
	int cpuinfo[4] = { 0 };
	__cpuid(cpuinfo, 0x80000008);
	unsigned int eax = (unsigned int)cpuinfo[0];
	eax &= (0b11111111);
	KdPrint(("[prefix] addr length is %u\n", eax));
	return STATUS_SUCCESS;
}