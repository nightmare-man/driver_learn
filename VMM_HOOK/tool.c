#include "tool.h"
#include <ntddk.h>
BOOLEAN test_cpuid_bit(int idx, int register_id, int bit) {
	int cpu_info[4] = { 0 };
	__cpuidex(cpu_info, idx, 0);
	unsigned int result = (unsigned int)(cpu_info[register_id]);
	result = result & (1UL << bit);
	return result > 0;
}
ULONG64 virtual_to_physic(ULONG64 va) {
	return MmGetPhysicalAddress((PVOID)va).QuadPart;
}
ULONG64 physic_to_virtual(ULONG64 pa) {
	PHYSICAL_ADDRESS addr = { .QuadPart = pa };

	return (ULONG64)MmGetVirtualForPhysical(addr);
}