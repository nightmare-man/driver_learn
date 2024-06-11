#include <ntddk.h>
#include "EPT.h"
#include "tool.h"
ULONG64 g_guest_memory = 0;

ULONG64 init_eptp() {
	PAGED_CODE();//只在《=2 irql上运行
	union EXTENDED_PAGE_TABLE_POINTER* ret = (union EXTENDED_PAGE_TABLE_POINTER*)ExAllocatePoolWithTag(
		NonPagedPool,
		PAGE_SIZE,
		DRIVER_TAG
	);
	if (!ret) {
		return NULL;
	}
	RtlZeroMemory((PVOID)ret, PAGE_SIZE);

	union PAGE_MAP_LEVEL_4_ENTRY* pml4_table = (union PAGE_MAP_LEVEL_4_ENTRY*)ExAllocatePoolWithTag(
		NonPagedPool,
		PAGE_SIZE,
		DRIVER_TAG
	);
	if (!pml4_table) {
		ExFreePoolWithTag(ret, DRIVER_TAG);
		return NULL;
	}
	RtlZeroMemory((PVOID)pml4_table, PAGE_SIZE);

	union PAGE_MAP_LEVEL_3_ENTRY_NORMAL* pml3_table = (union PAGE_MAP_LEVEL_3_ENTRY_NORMAL*)ExAllocatePoolWithTag(
		NonPagedPool,
		PAGE_SIZE,
		DRIVER_TAG
	);
	if (!pml3_table) {
		ExFreePoolWithTag(ret, DRIVER_TAG);
		ExFreePoolWithTag(pml4_table, DRIVER_TAG);
		return NULL;
	}
	RtlZeroMemory((PVOID)pml3_table, PAGE_SIZE);


	union PAGE_MAP_LEVEL_2_ENTRY_NORMAL* pml2_table = (union PAGE_MAP_LEVEL_2_ENTRY_NORMAL*)ExAllocatePoolWithTag(
		NonPagedPool,
		PAGE_SIZE,
		DRIVER_TAG
	);
	if (!pml2_table) {
		ExFreePoolWithTag(ret, DRIVER_TAG);
		ExFreePoolWithTag(pml4_table, DRIVER_TAG);
		ExFreePoolWithTag(pml3_table, DRIVER_TAG);
		return NULL;
	}
	RtlZeroMemory((PVOID)pml2_table, PAGE_SIZE);

	union PAGE_MAP_LEVEL_1_ENTRY* pml1_table = (union PAGE_MAP_LEVEL_1_ENTRY*)ExAllocatePoolWithTag(
		NonPagedPool,
		PAGE_SIZE,
		DRIVER_TAG
	);
	if (!pml1_table) {
		ExFreePoolWithTag(ret, DRIVER_TAG);
		ExFreePoolWithTag(pml4_table, DRIVER_TAG);
		ExFreePoolWithTag(pml3_table, DRIVER_TAG);
		ExFreePoolWithTag(pml2_table, DRIVER_TAG);
		return NULL;
	}
	RtlZeroMemory((PVOID)pml1_table, PAGE_SIZE);

	const ULONG64 pages_to_allocate = 100;
	g_guest_memory = ExAllocatePoolWithTag(NonPagedPool, pages_to_allocate * PAGE_SIZE, DRIVER_TAG);
	if (!g_guest_memory) {
		return NULL;
	}
	RtlFillMemory((PVOID)g_guest_memory, 100 * PAGE_SIZE, 0xf4);
	for (int i = 0; i < pages_to_allocate; i++) {
		pml1_table[i].fields.read_access = 1;
		pml1_table[i].fields.write_access = 1;
		pml1_table[i].fields.execute_access = 1;
		pml1_table[i].fields.memory_cache_type = 6;
		pml1_table[i].fields.ignore_pat_cache_type = 0;
		pml1_table[i].fields.dirty_flag = 0;
		pml1_table[i].fields.write_dirty_flag = 0;
		pml1_table[i].fields.user_mode_execute_access = 0;
		ULONG64 pa = vitual_to_physical(g_guest_memory + i * PAGE_SIZE);
		pa = pa >> 12;
		pml1_table[i].fields.referenced_pa = pa;
	}
	pml2_table->fields.read_access = 1;
	pml2_table->fields.write_access = 1;
	pml2_table->fields.execute_access = 1;
	pml2_table->fields.dirty_flag = 0;
	pml2_table->fields.user_mode_execute_access = 1;
	pml2_table->fields.referenced_pa = (vitual_to_physical(pml1_table) >> 12);

	pml3_table->fields.read_access = 1;
	pml3_table->fields.write_access = 1;
	pml3_table->fields.execute_access = 1;
	pml3_table->fields.dirty_flag = 0;
	pml3_table->fields.user_mode_execute_access = 1;
	pml3_table->fields.referenced_pa = (vitual_to_physical(pml2_table) >> 12);

	pml4_table->fields.read_access = 1;
	pml4_table->fields.write_access = 1;
	pml4_table->fields.execute_access = 1;
	pml4_table->fields.dirty_flag = 0;
	pml4_table->fields.user_mode_execute_access = 1;
	pml4_table->fields.referenced_pa = (vitual_to_physical(pml2_table) >> 12);

	ret->fields.pml4_pa = (vitual_to_physical(pml4_table) >> 12);
	ret->fields.ept_dirty_flag_enable = 1;
	ret->fields.ept_page_walk_length = 3;
	ret->fields.ept_paging_struct_cache_type = 6;

	
	return ret;
}
