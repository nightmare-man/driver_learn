#include <intrin.h>
#include "tool.h"
#include "msr.h"

ULONG64 vitual_to_physical(ULONG64 va) {
	return (ULONG64)MmGetPhysicalAddress((PVOID)va).QuadPart;
}
ULONG64 physical_to_vitual(ULONG64 va) {
	PHYSICAL_ADDRESS pa = { .QuadPart = va };
	return (ULONG64)MmGetVirtualForPhysical(pa);
}
BOOLEAN test_cpuid_bit(int cpuid, int register_id, int bit_id) {
	int cpu_info[4] = { 0 };
	__cpuid(cpu_info, cpuid);
	unsigned int result = cpu_info[register_id];
	unsigned int test_bit = result * (0b1 << bit_id);
	return (BOOLEAN)test_bit;
}
BOOLEAN is_vmx_support() {
	if (!test_cpuid_bit(1, 2, 5)) {
		return FALSE;
	}
	return TRUE;
}