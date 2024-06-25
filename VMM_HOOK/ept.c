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

BOOLEAN set_ept_table(ULONG64* eptp) {
	PHYSICAL_ADDRESS pa;
	pa.QuadPart = MAXULONG64;
	PML4E_PTR pml4_table = MmAllocateContiguousMemory(PAGE_SIZE, pa);
	//msdn上保证该分配是页对齐的
	if (!pml4_table) return FALSE;
	PML3E_PTR pml3_table = MmAllocateContiguousMemory(PAGE_SIZE, pa);
	if (!pml3_table) return FALSE;
	PML2E_PAGE_PTR pml2_page_table = MmAllocateContiguousMemory(PAGE_SIZE*512, pa);
	if (!pml2_page_table) return FALSE;
	RtlZeroMemory(pml4_table, PAGE_SIZE);
	RtlZeroMemory(pml3_table, PAGE_SIZE);
	RtlZeroMemory(pml2_page_table, PAGE_SIZE*512);


	for (ULONG64 pml3_table_idx = 0; pml3_table_idx < 512; pml3_table_idx++) {
		for (ULONG64 pml2_page_table_idx = 0; pml2_page_table_idx < 512; pml2_page_table_idx++) {
			PML2E_PAGE_PTR target= &(pml2_page_table[pml3_table_idx * 512 + pml2_page_table_idx]);
			target->field.read_access = 1;
			target->field.write_access = 1;
			target->field.execute_access = 1;
			if (pml3_table_idx == 0 && pml2_page_table_idx == 0) target->field.ept_memory_type = MEM_UC;
			else target->field.ept_memory_type = (ULONG64)MEM_WRITE_BACK;
			target->field.ignore_pat_memory_type = 0;
			target->field.must_be_one = 1;
			target->field.access_flag = 0;
			target->field.dirty_flag = 0;
			target->field.user_mode_execute_access = 1;
			target->field.page_pa = pml3_table_idx * 512 * 512 * 4096 + pml2_page_table_idx * 512 * 4096;
			target->field.verify_guest_paging = 0;
			target->field.paging_write_access = 0;
			target->field.supervisor_shadow_stack_access = 0;
			target->field.suppress_vm_exit = 0;
		}
	}

	for (ULONG64 pml3_table_idx = 0; pml3_table_idx < 512; pml3_table_idx++) {
		PML3E_PTR target = &(pml3_table[pml3_table_idx]);
		target->field.read_access = 1;
		target->field.write_access = 1;
		target->field.execute_access = 1;
		target->field.access_flag = 0;
		target->field.user_mode_execute_access = 1;
		target->field.next_level_table_pa = virtual_to_physic((ULONG64)(&(pml2_page_table[pml3_table_idx * 512]))) / PAGE_SIZE;
	}

	pml4_table[0].field.read_access = 1;
	pml4_table[0].field.write_access = 1;
	pml4_table[0].field.execute_access = 1;
	pml4_table[0].field.access_flag = 0;
	pml4_table[0].field.user_mode_execute_access = 1;
	pml4_table[0].field.next_level_table_pa = virtual_to_physic((ULONG64)pml3_table) / PAGE_SIZE;
	*eptp = virtual_to_physic((ULONG64)pml4_table);
	return TRUE;
}
BOOLEAN init_ept(ULONG64* eptp_ptr) {
	UNREFERENCED_PARAMETER(eptp_ptr);
	if (!check_support_and_enable_mtrr()) return FALSE;
	if (!read_mem_type_range_map_from_mtrr()) return FALSE;
	if (!set_ept_table(eptp_ptr)) return FALSE;
	return TRUE;
}