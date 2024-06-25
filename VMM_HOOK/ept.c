#include "ept.h"
#include "msr.h"
#include "tool.h"
#include <intrin.h>
#define DRIVER_TAG 0x99887766

extern ULONG64 g_maximum_pa_size;
ULONG64 g_range_type_count=0;
ULONG64 g_range_type_real_count = 0;
ULONG64 g_variable_range_mtrr_count = 0;
BOOLEAN g_fixed_range_mtrr_support_and_enable = FALSE;
P_MEM_TYPE_RANGE g_range_type_map=NULL;
enum MEM_TYPE g_default_memtype = MEM_UC;

BOOLEAN check_support_and_enable_mtrr() {
	int cpu_info[4] = { 0 };
	__cpuidex(cpu_info, 1, 0);
	ULONG64 mtrr_support = (cpu_info[3] & (1ULL << 12));
	if (mtrr_support == 0) return FALSE;

	ULONG64 mtrr_def_set = __readmsr(IA32_MTRR_DEF_TYPE_MSR);
	ULONG64 mtrr_enable = (mtrr_def_set & (1ULL << 11));
	ULONG64 fixed_range_mtrr_enable = (mtrr_def_set & (1ULL << 10));
	if (mtrr_enable == 0) return FALSE;

	ULONG64 mtrr_cap_set = __readmsr(IA32_MTRRCAP_MSR);
	ULONG64 fixed_range_mtrr_support = (mtrr_cap_set & (1ULL << 8));

	g_default_memtype = (enum MEM_TYPE)(mtrr_def_set & 0x7);
	g_fixed_range_mtrr_support_and_enable = ((fixed_range_mtrr_support > 0) && (fixed_range_mtrr_enable > 0));
	g_variable_range_mtrr_count = (mtrr_cap_set & 0xff);
	return TRUE;
}

VOID read_fix_range_in_msr(ULONG64 msr_value, P_MEM_TYPE_RANGE* ptr, ULONG64 start_offset,ULONG64 sub_range_size) {
	
	for (ULONG i = 0; i < 8U; i++) {
		P_MEM_TYPE_RANGE list_ptr = (*ptr);
		list_ptr->start = start_offset + sub_range_size * i;
		list_ptr->end = list_ptr->start + sub_range_size;
		list_ptr->type = (enum MEM_TYPE)((msr_value >> (8 * i)) & 0xff);
		(*ptr) = list_ptr + 1;
	}
}
VOID read_fix_range_map(P_MEM_TYPE_RANGE ptr) {
	ULONG64 fix64k_value = __readmsr(IA32_MTRR_FIX64K_00000_MSR);
	read_fix_range_in_msr(fix64k_value, &ptr,0ULL, 64 * 1024ULL);
	for (ULONG i = 0; i < 2; i++) {
		ULONG64 fix16k_value = __readmsr(IA32_MTRR_FIX16K_80000_MSR + i);
		ULONG64 start_offset = 0x80000ULL + 16 * 1024 * 8 *i;
		read_fix_range_in_msr(fix16k_value, &ptr,start_offset, 16 * 1024ULL);
	}
	for (ULONG i = 0; i < 8; i++) {
		ULONG64 fix4k_value = __readmsr(IA32_MTRR_FIX4K_C0000_MSR);
		ULONG64 start_offset = 0xc0000ULL + 4 * 1024 * 8 * i;
		read_fix_range_in_msr(fix4k_value, &ptr,start_offset, 4 * 1024ULL);
	}
}
VOID read_variable_range_map(P_MEM_TYPE_RANGE ptr) {
	for (ULONG i = 0; i < g_variable_range_mtrr_count; i++) {
		ULONG64 base_value = __readmsr(IA32_MTRR_PHYSBASE0_MSR + i * 2);
		ULONG64 mask_value = __readmsr(IA32_MTRR_PHYSMASK0_MSR + i * 2);
		ULONG64 type_field_mask = 0xff;
		ULONG64 base_or_range_field_mask = ((1ULL << g_maximum_pa_size) - 1) & (~((1ULL << 12) - 1));
		ULONG64 valid_field_mask = (1ULL << 11);
		if (mask_value & valid_field_mask) {
			ptr->start = (base_value & base_or_range_field_mask);
			ULONG64 range = ((~(mask_value & base_or_range_field_mask)) & ((1ULL << g_maximum_pa_size) - 1)) + 1;
			ptr->end = ((ptr->start) +range);
			ptr->type = (enum MEM_TYPE)(base_value & type_field_mask);
			ptr++;
		}
		else {
			g_range_type_real_count--;
		}
	}
}
BOOLEAN read_mem_type_range_map_from_mtrr() {
	g_range_type_count = g_variable_range_mtrr_count;
	if (g_fixed_range_mtrr_support_and_enable) {
		g_range_type_count += 8;//8个64k的范围 IA32_MTRR_FIX64K_00000
		g_range_type_count += 16;//16个16k的范围 IA32_MTRR_FIX16K_80000
		g_range_type_count += 64;//64个4k的范围 IA32_MTRR_FIX4K_C0000
		//共1MB
	}
	g_range_type_real_count = g_range_type_count;
	g_range_type_map = ExAllocatePoolZero(NonPagedPool, sizeof(MEM_TYPE_RANGE) * g_range_type_count, DRIVER_TAG);
	if (!g_range_type_map) return FALSE;
	RtlZeroMemory(g_range_type_map, sizeof(MEM_TYPE_RANGE) * g_range_type_count);
	if (g_fixed_range_mtrr_support_and_enable) {
		read_fix_range_map(g_range_type_map);
	}
	read_variable_range_map( g_range_type_map+(8+16+64));
	return TRUE;
}
VOID print_map() {
	for (ULONG i = 0; i < g_range_type_real_count; i++) {
		Log("[0x%p,0x%p,%d]", g_range_type_map[i].start, g_range_type_map[i].end, g_range_type_map[i].type);
	}
}
BOOLEAN init_ept(ULONG64* eptp_ptr) {
	UNREFERENCED_PARAMETER(eptp_ptr);
	if (!check_support_and_enable_mtrr()) return FALSE;
	if (!read_mem_type_range_map_from_mtrr()) return FALSE;
	print_map();
	return TRUE;
	
}