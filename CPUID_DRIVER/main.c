#include <ntddk.h>

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING register_path) {
	UNREFERENCED_PARAMETER(driver);
	UNREFERENCED_PARAMETER(register_path);
	int cpu_info[4] = { 0 };
	__cpuidex(cpu_info, 0x87654321, 0x12345678);
	return STATUS_SUCCESS;
}