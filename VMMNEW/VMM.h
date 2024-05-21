#pragma once
#include <ntddk.h>
#include "EPT.h"
#define VMXON_SIZE 4096
#define VMCS_SIZE 4096
#define ALIGNED_SIZE 4096
#define VMM_STACK_SIZE 4096


struct VMM_STATE* g_vmm_state_ptr;
struct VMM_STATE {
	ULONG64 vmxon_region_pa;
	ULONG64 vmcs_pa;
	ULONG64 vmxon_region_va_unaligned;
	ULONG64 vmcs_va_unaligned;
	ULONG64 eptp;
	ULONG64 vmm_stack;
	ULONG64 msr_bitmaps_va;
	ULONG64 msr_bitmaps_pa;
};

BOOLEAN allocate_vmxon(struct VMM_STATE* vmm_state);
BOOLEAN allocate_vmcs(struct VMM_STATE* vmm_state);
BOOLEAN enable_vmx();
BOOLEAN init_vmx();
BOOLEAN leave_vmx();
ULONG64 vm_ptr_store();
BOOLEAN clear_vm_state(struct VMM_STATE* vmm_state_ptr);
BOOLEAN vm_ptr_load(struct VMM_STATE* vmm_state_ptr);
BOOLEAN launch_vmx(struct VMM_STATE* vmm_state_ptr,int process_id,union EXTENDED_PAGE_TABLE_POINTER* eptp);