#pragma once
#include <ntddk.h>
#include <intrin.h>
#include "VMM.h"
#include "tool.h"
#include "msr.h"

extern USHORT get_cs();
extern USHORT get_ss();
extern USHORT get_ds();
extern USHORT get_es();
extern USHORT get_fs();
extern USHORT get_gs();
extern USHORT get_ldtr();//这里获取的也是16为选择子
extern USHORT get_tr();
extern ULONG64 get_gdt_base();
extern ULONG64 get_idt_base();
extern ULONG32 get_gdt_limit();
extern ULONG32 get_idt_limit();
extern VOID vmx_exit_handler();
extern VOID save_host_state();
extern VOID restore_host_state();
extern ULONG64 g_guest_memory;
static BOOLEAN need_off = FALSE;
ULONG64 g_host_save_rsp = 0;
ULONG64 g_host_save_rbp = 0;

BOOLEAN allocate_vmxon(struct VMM_STATE* vmm_state) {
	//降低IRQL
	if (KeGetCurrentIrql() > DISPATCH_LEVEL) {
		KeRaiseIrqlToDpcLevel();
	}
	int vmxon_region_size = 2 * VMXON_SIZE;
	PHYSICAL_ADDRESS allowed_max = { .QuadPart = MAXULONG64 };
	PVOID buffer = MmAllocateContiguousMemory((SIZE_T)vmxon_region_size, allowed_max);
	if (buffer == NULL) {
		KdPrint(("[prefix]can't allocate vmxon region\n"));
		return FALSE;
	}
	RtlSecureZeroMemory(buffer, vmxon_region_size);
	ULONG64 aligned_buffer = ((ULONG64)buffer + ALIGNED_SIZE) & (~(ALIGNED_SIZE - 1));
	ULONG64 aligned_pa = vitual_to_physical(aligned_buffer);
	KdPrint(("[prefix] vmxon va:%lu\n", (ULONG64)buffer));
	KdPrint(("[prefix] vmxon aligned va:%lu\n", (ULONG64)aligned_buffer));
	KdPrint(("[prefix] vmxon aligned pa:%lu\n", (ULONG64)aligned_pa));
	union VMX_BASE_MSR base_msr;
	base_msr.all = __readmsr(IA32_VMX_BASIC_MSR);
	KdPrint(("[prefix] base msr revision: %u\n", base_msr.fields.revision));
	KdPrint(("[prefix] base msr size require is: %u\n", base_msr.fields.RegionSize));
	*((ULONG32*)aligned_buffer) = base_msr.fields.revision;
	unsigned char ret=__vmx_on(&aligned_pa);
	if (ret) {
		KdPrint(("[prefix]vmx on execute error\n"));
		return FALSE;
	}
	vmm_state->vmxon_region_pa = aligned_pa;
	vmm_state->vmxon_region_va_unaligned = buffer;
	KdPrint(("[prefix]vmx on success\n"));
	return TRUE;
}
BOOLEAN allocate_vmcs(struct VMM_STATE* vmm_state) {
	if (KeGetCurrentIrql() > DISPATCH_LEVEL) {
		KeRaiseIrqlToDpcLevel();
	}
	int vmxon_region_size = 2 * VMXON_SIZE;
	PHYSICAL_ADDRESS allowed_max = { .QuadPart = MAXULONG64 };
	PVOID buffer = MmAllocateContiguousMemory((SIZE_T)vmxon_region_size, allowed_max);
	if (buffer == NULL) {
		KdPrint(("[prefix]can't allocate vmcs region\n"));
		return FALSE;
	}
	RtlSecureZeroMemory(buffer, vmxon_region_size);
	ULONG64 aligned_buffer = ((ULONG64)buffer + ALIGNED_SIZE) & (~(ALIGNED_SIZE - 1));
	ULONG64 aligned_pa = vitual_to_physical(aligned_buffer);
	KdPrint(("[prefix] vmcs va:%lu\n", buffer));
	KdPrint(("[prefix] vmcs aligned va:%lu\n", aligned_buffer));
	KdPrint(("[prefix] vmcs aligned pa:%lu\n", aligned_pa));
	union VMX_BASE_MSR base_msr;
	base_msr.all = __readmsr(IA32_VMX_BASIC_MSR);
	KdPrint(("[prefix] base msr revision: %u\n", base_msr.fields.revision));
	*((ULONG32*)aligned_buffer) = base_msr.fields.revision;
	vmm_state->vmcs_pa = aligned_pa;
	vmm_state->vmcs_va_unaligned = buffer;
	KdPrint(("[prefix]vmptrld success\n"));
	return TRUE;
}
BOOLEAN enable_vmx() {
	ULONG64 cr4_origin = __readcr4();
	__writecr4(cr4_origin | (0b1 << 13));
	union FEATURE_CONTROL_MSR feature_control = { .all = 0 };
	feature_control.all=__readmsr(IA32_FEATURE_CONTROL_MSR);
	feature_control.fields.lock = 1;
	feature_control.fields.enable_vmx_in_smx = 1;
	feature_control.fields.enable_vmx_out_smx = 1;
	__writemsr(IA32_FEATURE_CONTROL_MSR, feature_control.all);
	return TRUE;
}

BOOLEAN init_vmx() {
	ULONG32 cpu_count = KeQueryActiveProcessorCount(NULL);
	if (!is_vmx_support()) {
		KdPrint(("[prefix] cpu not support vmx\n"));
		return FALSE;
	}
	g_vmm_state_ptr = (struct VMM_STATE*)ExAllocatePoolWithTag(NonPagedPool, sizeof(struct VMM_STATE) * cpu_count, DRIVER_TAG);
	if ((g_vmm_state_ptr) == NULL) {
		KdPrint(("[prefix] allocate g_vmm_state fail\n"));
		return FALSE;
	}
	KAFFINITY ka;
	for (ULONG32 i = 0; i < cpu_count; i++) {
		ka = (KAFFINITY)2 ^ i;//每个处理器的掩码
		KeSetSystemAffinityThread(ka);
		enable_vmx();
		KdPrint(("[prefix] enable vmx success on process %u\n", i));
		if (!allocate_vmxon(&(g_vmm_state_ptr[i]))) {
			continue;
		}
		if (!allocate_vmcs(&(g_vmm_state_ptr[i]))) {
			continue;
		}
	}
	need_off = TRUE;
	return TRUE;
}

BOOLEAN leave_vmx() {
	if (!need_off) {
		return TRUE;
	}
	ULONG32 cpu_count = KeQueryActiveProcessorCount(NULL);
	KAFFINITY ka;
	for (ULONG32 i = 0; i < cpu_count; i++) {
		ka = 2 ^ i;
		KeSetSystemAffinityThread(ka);
		__vmx_off();
		
		
		MmFreeContiguousMemory((PVOID)g_vmm_state_ptr[i].vmxon_region_va_unaligned);
		MmFreeContiguousMemory((PVOID)g_vmm_state_ptr[i].vmcs_va_unaligned);
	}
	ExFreePoolWithTag(g_vmm_state_ptr, DRIVER_TAG);
	return TRUE;
}

ULONG64 vm_ptr_store() {
	ULONG64 ret;
	__vmx_vmptrst(&ret);
	return ret;
}
BOOLEAN clear_vm_state(struct VMM_STATE* vmm_state_ptr) {
	unsigned char ret = __vmx_vmclear((ULONG64*)vmm_state_ptr);
	if (ret != 0) {
		return FALSE;
	}
	return TRUE;
}

BOOLEAN vm_ptr_load(struct VMM_STATE* vmm_state_ptr) {
	unsigned char ret = __vmx_vmptrld((ULONG64*)vmm_state_ptr);
	if (ret != 0) {
		return FALSE;
	}
	return TRUE;
}
struct SEGMENT_DESCRIPTOR read_normal_segment_descriptor(USHORT selector) {
	ULONG64 gdt_base = get_gdt_base();
	struct SEGMENT_DESCRIPTOR desc = { 0 };
	ULONG64 gdt_desc = *((ULONG64*)(gdt_base + (selector >> 3) * 8));
	desc.SEL = selector;
	desc.LIMIT = (gdt_desc & 0xffff) + ((gdt_base>>48)&0xf)<<16;
	desc.BASE = (gdt_desc >> 16)&0xffff + ((gdt_desc >> 32)&0xff) << 16 + ((gdt_desc >> 56)&0xff) << 24;
	desc.ATTRIBUTES.UCHARs = (gdt_desc >> 40) & 0xffff;
	if (desc.ATTRIBUTES.Fields.S == 0) {
		ULONG64 tmp = *((ULONG64*)(gdt_base + (selector >> 3) * 8) + 8);
		desc.BASE += tmp & 0xffffffff;
	}
	if (desc.ATTRIBUTES.Fields.G) {
		desc.LIMIT = (desc.LIMIT << 12) + 0xfff;
	}
	return desc;
}

ULONG32 trans_desc_attr_to_access_right(union SEGMENT_ATTRIBUTES attr) {
	ULONG32 ret = attr.Fields.TYPE + attr.Fields.S << 4 + attr.Fields.DPL << 5\
		+ attr.Fields.P << 7 + attr.Fields.AVL<<12+attr.Fields.DB<<14+attr.Fields.G<<15;
	return ret;
}
BOOLEAN setup_vmcs(struct VMM_STATE* vmm_state_ptr, union EXTENDED_PAGE_TABLE_POINTER* eptp, ULONG64 guest_memory) {
	
	//guest context
	__vmx_vmwrite(GUEST_CR0, __readcr0());
	__vmx_vmwrite(GUEST_CR3, __readcr3());
	__vmx_vmwrite(GUEST_CR4, __readcr4());
	__vmx_vmwrite(GUEST_DR7, 0x400ULL);//硬件调试寄存器
	__vmx_vmwrite(GUEST_RSP, guest_memory);
	__vmx_vmwrite(GUEST_RIP, guest_memory);
	__vmx_vmwrite(GUEST_RFLAGS, __readeflags());

	struct SEGMENT_DESCRIPTOR es = read_normal_segment_descriptor(get_es());
	__vmx_vmwrite(GUEST_ES_SELECTOR, es.SEL);
	__vmx_vmwrite(GUEST_ES_BASE, es.BASE);
	__vmx_vmwrite(GUEST_ES_LIMIT, es.LIMIT);
	__vmx_vmwrite(GUEST_ES_AR_BYTES, trans_desc_attr_to_access_right(es.ATTRIBUTES));

	struct SEGMENT_DESCRIPTOR cs = read_normal_segment_descriptor(get_cs());
	__vmx_vmwrite(GUEST_ES_SELECTOR, cs.SEL);
	__vmx_vmwrite(GUEST_ES_BASE, cs.BASE);
	__vmx_vmwrite(GUEST_ES_LIMIT, cs.LIMIT);
	__vmx_vmwrite(GUEST_ES_AR_BYTES, trans_desc_attr_to_access_right(cs.ATTRIBUTES));

	//continue code with order es cs ss ds fs gs 
	struct SEGMENT_DESCRIPTOR ss = read_normal_segment_descriptor(get_ss());
	__vmx_vmwrite(GUEST_SS_SELECTOR, ss.SEL);
	__vmx_vmwrite(GUEST_SS_BASE, ss.BASE);
	__vmx_vmwrite(GUEST_SS_LIMIT, ss.LIMIT);
	__vmx_vmwrite(GUEST_SS_AR_BYTES, trans_desc_attr_to_access_right(ss.ATTRIBUTES));

	struct SEGMENT_DESCRIPTOR ds = read_normal_segment_descriptor(get_ds());
	__vmx_vmwrite(GUEST_DS_SELECTOR, ds.SEL);
	__vmx_vmwrite(GUEST_DS_BASE, ds.BASE);
	__vmx_vmwrite(GUEST_DS_LIMIT, ds.LIMIT);
	__vmx_vmwrite(GUEST_DS_AR_BYTES, trans_desc_attr_to_access_right(ds.ATTRIBUTES));

	struct SEGMENT_DESCRIPTOR fs = read_normal_segment_descriptor(get_fs());
	__vmx_vmwrite(GUEST_FS_SELECTOR, fs.SEL);
	__vmx_vmwrite(GUEST_FS_BASE, fs.BASE);
	__vmx_vmwrite(GUEST_FS_LIMIT, fs.LIMIT);
	__vmx_vmwrite(GUEST_FS_AR_BYTES, trans_desc_attr_to_access_right(fs.ATTRIBUTES));

	struct SEGMENT_DESCRIPTOR gs = read_normal_segment_descriptor(get_gs());
	__vmx_vmwrite(GUEST_GS_SELECTOR, gs.SEL);
	__vmx_vmwrite(GUEST_GS_BASE, gs.BASE);
	__vmx_vmwrite(GUEST_GS_LIMIT, gs.LIMIT);
	__vmx_vmwrite(GUEST_GS_AR_BYTES, trans_desc_attr_to_access_right(gs.ATTRIBUTES));

	struct SEGMENT_DESCRIPTOR ldtr = read_normal_segment_descriptor(get_ldtr());
	__vmx_vmwrite(GUEST_GS_SELECTOR, ldtr.SEL);
	__vmx_vmwrite(GUEST_GS_BASE, ldtr.BASE);
	__vmx_vmwrite(GUEST_GS_LIMIT, ldtr.LIMIT);
	__vmx_vmwrite(GUEST_GS_AR_BYTES, trans_desc_attr_to_access_right(ldtr.ATTRIBUTES));

	struct SEGMENT_DESCRIPTOR tr = read_normal_segment_descriptor(get_tr());
	__vmx_vmwrite(GUEST_GS_SELECTOR, tr.SEL);
	__vmx_vmwrite(GUEST_GS_BASE, tr.BASE);
	__vmx_vmwrite(GUEST_GS_LIMIT, tr.LIMIT);
	__vmx_vmwrite(GUEST_GS_AR_BYTES, trans_desc_attr_to_access_right(tr.ATTRIBUTES));

	__vmx_vmwrite(GUEST_GDTR_BASE, get_gdt_base());
	__vmx_vmwrite(GUEST_GDTR_LIMIT, get_gdt_limit());
	__vmx_vmwrite(GUEST_IDTR_BASE, get_idt_base());
	__vmx_vmwrite(GUEST_IDTR_LIMIT, get_idt_limit());

	__vmx_vmwrite(GUEST_IA32_DEBUGCTL, __readmsr(IA32_DEBUGCTL));
	__vmx_vmwrite(GUEST_SYSENTER_EIP, __readmsr(IA32_SYSENTER_EIP));
	__vmx_vmwrite(GUEST_SYSENTER_ESP, __readmsr(IA32_SYSENTER_ESP));
	__vmx_vmwrite(GUEST_SYSENTER_CS, __readmsr(IA32_SYSENTER_CS));

	__vmx_vmwrite(GUEST_ACTIVITY_STATE, 0);
	__vmx_vmwrite(GUEST_INTERRUPTIBILITY_INFO, 0);
	__vmx_vmwrite(VMCS_LINK_POINTER, ~0ULL);


	//host context
	__vmx_vmwrite(HOST_CR0, __readcr0());
	__vmx_vmwrite(HOST_CR3, __readcr3());
	__vmx_vmwrite(HOST_CR4, __readcr4());
	__vmx_vmwrite(HOST_RSP, (ULONG64)vmm_state_ptr->vmm_stack + VMM_STACK_SIZE- 1);
	__vmx_vmwrite(HOST_RIP, (ULONG64)vmx_exit_handler);
	__vmx_vmwrite(HOST_CS_SELECTOR, get_cs() & 0xf8);
	__vmx_vmwrite(HOST_SS_SELECTOR, get_ss() & 0xf8);
	__vmx_vmwrite(HOST_DS_SELECTOR, get_ds() & 0xf8);
	__vmx_vmwrite(HOST_ES_SELECTOR, get_es() & 0xf8);
	__vmx_vmwrite(HOST_FS_SELECTOR, get_fs() & 0xf8);
	__vmx_vmwrite(HOST_GS_SELECTOR, get_gs() & 0xf8);
	__vmx_vmwrite(HOST_FS_BASE, fs.BASE);
	__vmx_vmwrite(HOST_GS_BASE, gs.BASE);
	__vmx_vmwrite(HOST_GDTR_BASE, get_gdt_base());
	__vmx_vmwrite(HOST_IDTR_BASE, get_idt_base());
	__vmx_vmwrite(HOST_IA32_SYSENTER_CS, __readmsr(IA32_SYSENTER_CS));
	__vmx_vmwrite(HOST_IA32_SYSENTER_EIP, __readmsr(IA32_SYSENTER_EIP));
	__vmx_vmwrite(HOST_IA32_SYSENTER_ESP, __readmsr(IA32_SYSENTER_ESP));

	ULONG64 IA32_VMX_BASIC_MSR_VALUE = __readmsr(IA32_VMX_BASIC_MSR);
	ULONG64 pin_based_vm_execution_reserve_control = 0;
	ULONG64 primary_process_based_vm_execution_reverse_control = 0;
	ULONG64 secondary_process_based_vm_execution_reverse_control = 0;
	ULONG64 vm_exit_control = 0;
	ULONG64 vm_entry_control = 0;


	BOOLEAN use_true_msr = (IA32_VMX_BASIC_MSR_VALUE >> 55) & 0x01;
	
	ULONG64 pin_based_vm_execution_value = 0;
	ULONG64 primary_process_based_value = 0;
	ULONG64 secondary_process_based_value = 0;
	ULONG64 vm_exit_value = 0;
	ULONG64 vm_entry_value = 0;		

	primary_process_based_value |= 1ULL << 7;//hlt exit enable;
	primary_process_based_value |= 1ULL << 31;//activate secondary process based control;
	secondary_process_based_value |= 1ULL << 3; // Enable RDTSCP
	vm_exit_value |= 1ULL << 9; //enable 64bit in host when exit 
	vm_entry_control |= 1ULL << 9; //enable ia32e mode in guest when entry
	if (use_true_msr) {
		pin_based_vm_execution_reserve_control = __readmsr(IA32_VMX_TRUE_PINBASED_CTLS);
		primary_process_based_vm_execution_reverse_control = __readmsr(IA32_VMX_TRUE_PROCBASED_CTLS);
		vm_exit_control = __readmsr(IA32_VMX_TRUE_EXIT_CTLS);
		vm_entry_control = __readmsr(IA32_VMX_TRUE_ENTRY_CTLS);
	}
	else {
		pin_based_vm_execution_reserve_control = __readmsr(IA32_VMX_PINBASED_CTLS);
		primary_process_based_vm_execution_reverse_control = __readmsr(IA32_VMX_PROCBASED_CTLS);
		vm_exit_control = __readmsr(IA32_VMX_EXIT_CTLS);
		vm_entry_control = __readmsr(IA32_VMX_ENTRY_CTLS);
	}
	secondary_process_based_vm_execution_reverse_control = __readmsr(IA32_VMX_PROCBASED_CTLS2);


	pin_based_vm_execution_value |= (pin_based_vm_execution_reserve_control & 0xffffffff);
	primary_process_based_value |= (primary_process_based_vm_execution_reverse_control & 0xffffffff);
	secondary_process_based_value |= (secondary_process_based_vm_execution_reverse_control & 0xffffffff);
	vm_exit_value |= (vm_exit_control & 0xffffffff);
	vm_entry_value |= (vm_entry_control & 0xffffffff);

	pin_based_vm_execution_value &= (pin_based_vm_execution_reserve_control >> 32) & 0xffffffff;
	primary_process_based_value &= (primary_process_based_vm_execution_reverse_control >> 32) & 0xffffffff;
	secondary_process_based_value &= (secondary_process_based_vm_execution_reverse_control >> 32) & 0xffffffff;
	vm_exit_value &= (vm_exit_control >> 32) & 0xffffffff;
	vm_exit_value &= (vm_entry_control >> 32) & 0xffffffff;

	if(!use_true_msr){
		pin_based_vm_execution_value |= 1ULL << 1;//这些位置需要置1
		pin_based_vm_execution_value |= 1ULL << 2;
		pin_based_vm_execution_value |= 1ULL << 4;
		
		primary_process_based_value |= 1ULL << 1;
		primary_process_based_value |= 0b111ULL << 4;
		primary_process_based_value |= 1ULL << 8;
		primary_process_based_value |= 0b1111 << 13;
		primary_process_based_value |= 1ULL << 26;
		
		vm_exit_value |= 0b111111111ULL;
		vm_exit_value |= 1ULL << 10;
		vm_exit_value |= 1ULL << 11;
		vm_exit_value |= 1ULL << 13;
		vm_exit_value |= 1ULL << 14;
		vm_exit_value |= 1ULL << 16;
		vm_exit_value |= 1ULL << 17;

		vm_entry_control |= 0b111111111ULL;
		vm_entry_control |= 1ULL << 12;

	}
	__vmx_vmwrite(PIN_BASED_VM_EXEC_CONTROL, pin_based_vm_execution_value);
	__vmx_vmwrite(CPU_BASED_VM_EXEC_CONTROL, primary_process_based_value);
	__vmx_vmwrite(EXCEPTION_BITMAP, 0ULL);
	__vmx_vmwrite(MSR_BITMAP, vmm_state_ptr->msr_bitmaps_pa);
	__vmx_vmwrite(VM_EXIT_CONTROLS, vm_exit_value);
	__vmx_vmwrite(VM_ENTRY_CONTROLS, vm_entry_value);
	return TRUE;
}

VOID main_vmx_exit_handler() {
	ULONG64 exit_reason = 0;
	__vmx_vmread(VM_EXIT_REASON, &exit_reason);
	exit_reason &= 0xffff;//低16位是粗略原因
	switch (exit_reason) {
	case 12:
		KdPrint(("vm exit for hlt\n"));
		break;

	default:
		break;
	}
	return;
}

BOOLEAN launch_vmx(struct VMM_STATE* vmm_state_ptr, int process_id, union EXTENDED_PAGE_TABLE_POINTER* eptp) {
	KdPrint(("----launch vmx----\n"));
	KAFFINITY ka = 2 ^ process_id;
	KeSetSystemAffinityThread(ka);
	KdPrint(("current thread is executing in %d logical process\n", process_id));
	PAGED_CODE();
	
	//为VMM分配栈，理论上，开启VMXON就是在VMM了，用的栈是当前的ss rsp，
	//但是我们希望VMM使用新栈，因此重新分配一个，放在vmcs的host data中，当第一次vm exit后，
	//使用新分配的作为rsp
	//需要注意的是，这里需要的vmm_stack 是vmm里的虚拟地址
	//我们的vmm是windows环境下的驱动程序，因此使用的cr3是系统进程的cr3
	//因此这里的虚拟地址就是allocate分配下来的地址。
	//假设我们进入guest后，继续执行剩余的代码，这个时候的windows就是guest了。

	ULONG64 vmm_stack_va = ExAllocatePoolWithTag(NonPagedPool, VMM_STACK_SIZE, DRIVER_TAG);
	if (vmm_stack_va==NULL) {
		KdPrint(("allocate vmm stack fail\n"));
		return FALSE;
	}
	RtlZeroMemory(vmm_stack_va, VMM_STACK_SIZE);
	vmm_state_ptr->vmm_stack = vmm_stack_va;

	//分配msrbitmap,根据intel手册， vmcs里保存一个msrbitmap物理内存指针
	//指向一个4kb的区域
	ULONG64 msr_bitmap = ExAllocatePoolWithTag(NonPagedPool, PAGE_SIZE, DRIVER_TAG);
	if (msr_bitmap == NULL) {
		KdPrint(("allocate msr bitmaps fail\n"));
		return FALSE;
	}
	RtlZeroMemory(msr_bitmap, PAGE_SIZE);
	vmm_state_ptr->msr_bitmaps_va = msr_bitmap;
	vmm_state_ptr->msr_bitmaps_pa = vitual_to_physical(msr_bitmap);

	if (!clear_vm_state(vmm_state_ptr)) {
		return FALSE;
	}
	if (!vm_ptr_load(vmm_state_ptr)) {
		return FALSE;
	}

	
	if (!setup_vmcs(vmm_state_ptr, eptp, g_guest_memory)) {
		return FALSE;
	}
	save_host_state();
	__vmx_vmlaunch();//后面理论上不会被执行，执行就说明launch出问题里
	KdPrint(("not success\n"));
	return FALSE;
}

VOID vmx_resume_instruction() {
	//只有非hlt导致的exit会执行到这
	__vmx_vmresume();
	//后面理论上不会被执行，被执行说明出问题了
	ULONG64 ErrorCode = 0;
	__vmx_vmread(VM_INSTRUCTION_ERROR, &ErrorCode);
	__vmx_off();
	DbgPrint("[*] VMRESUME Error : 0x%llx\n", ErrorCode);

	//
	// It's such a bad error because we don't where to go!
	// prefer to break
	//
	DbgBreakPoint();
}