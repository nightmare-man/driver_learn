#include <ntddk.h>
#include "vmm.h"
#include "tool.h"
/// <summary>
/// 经过测试发现
/// </summary>
/// <param name="driver"></param>
/// <param name="register_path"></param>
/// <returns></returns>
NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING register_path) {
	UNREFERENCED_PARAMETER(driver);
	UNREFERENCED_PARAMETER(register_path);
	ExInitializeDriverRuntime(0);
	Log("init vmm start");
	if (init_vmm()) {
		Log("init vmm success");
	}
	else {
		Log("init vmm fail");
	}	
	PHYSICAL_ADDRESS pa = { .QuadPart = MAXULONG64 };
	ULONG64* target = MmAllocateContiguousMemory(PAGE_SIZE, pa);
	target[10] = 20;
	return STATUS_SUCCESS;
}