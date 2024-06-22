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
extern ULONG64 get_flags();
extern USHORT get_gdt_limit();
extern USHORT get_idt_limit();
extern VOID vmx_exit_handler();
extern VOID restore_state();


ULONG64 g_guest_rsp = 0;
ULONG64 g_guest_rip = 0;

VOID leave_vmx() {
	ULONG cpu_count = KeQueryActiveProcessorCount(0);
	for (ULONG i = 0; i < cpu_count; i++) {

		KeSetSystemAffinityThread((KAFFINITY)(1 << i));
		__vmx_off();
		KeRevertToUserAffinityThread();
	}
}
void write_and_log(ULONG64 index, ULONG64 value) {
	__vmx_vmwrite(index, value);
	DbgPrint("[%p,%p]\n", index, value);
}

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
	KdPrint(("[prefix] vmxon va:%p\n", (ULONG64)buffer));
	KdPrint(("[prefix] vmxon aligned va:%p\n", (ULONG64)aligned_buffer));
	KdPrint(("[prefix] vmxon aligned pa:%p\n", (ULONG64)aligned_pa));
	union VMX_BASE_MSR base_msr;
	base_msr.all = __readmsr(IA32_VMX_BASIC_MSR);
	KdPrint(("[prefix] base msr revision: %p\n", base_msr.fields.revision));
	KdPrint(("[prefix] base msr size require is: %p\n", base_msr.fields.RegionSize));
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
	KdPrint(("[prefix] vmcs va:%p\n", buffer));
	KdPrint(("[prefix] vmcs aligned va:%p\n", aligned_buffer));
	KdPrint(("[prefix] vmcs aligned pa:%p\n", aligned_pa));
	union VMX_BASE_MSR base_msr;
	base_msr.all = __readmsr(IA32_VMX_BASIC_MSR);
	KdPrint(("[prefix] base msr revision: %p\n", base_msr.fields.revision));
	*((ULONG32*)aligned_buffer) = base_msr.fields.revision;
	vmm_state->vmcs_pa = aligned_pa;
	vmm_state->vmcs_va_unaligned = buffer;
	KdPrint(("[prefix]vmptrld success\n"));
	return TRUE;
}
BOOLEAN enable_vmx() {
	ULONG64 cr4_origin = __readcr4();
	cr4_origin |= 1ULL << 13;
	__writecr4(cr4_origin );
	/*
	union FEATURE_CONTROL_MSR feature_control = { .all = 0 };
	feature_control.all=__readmsr(IA32_FEATURE_CONTROL_MSR);
	feature_control.fields.lock = 1;
	feature_control.fields.enable_vmx_in_smx = 1;
	feature_control.fields.enable_vmx_out_smx = 1;
	__writemsr(IA32_FEATURE_CONTROL_MSR, feature_control.all);
		这些是bios固定的，不能再改了，会报错的
	*/


	/*
	ULONG64 cr0_fixed0_control = __readmsr(IA32_VMX_CR0_FIXED0);
	ULONG64 cr0_fixed1_control = __readmsr(IA32_VMX_CR0_FIXED1);
	ULONG64 cr4_fixed0_control = __readmsr(IA32_VMX_CR4_FIXED0);
	ULONG64 cr4_fixed1_control = __readmsr(IA32_VMX_CR4_FIXED1);
	ULONG64 cr0_origin = __readcr0();
	cr4_origin = __readcr4();
	
	这里理解有偏差了，以为这几个fixed msr是要根据它手动设置，实际上
	它的作用是显示哪些位已经被固定死了，不需要也不能设置了
	*/
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
		ka = (KAFFINITY)(1 << i);
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
	
	return TRUE;
}




struct SEGMENT_DESCRIPTOR read_normal_segment_descriptor(USHORT selector) {
	ULONG64 gdt_base = get_gdt_base();
	struct SEGMENT_DESCRIPTOR desc = { 0 };
	RtlZeroMemory(&desc, sizeof(desc));
	//这儿有问题，没有读取到desc
	ULONG64 gdt_desc = *((ULONG64*)(gdt_base + (selector >> 3) * 8));
	desc.SEL = selector;
	desc.LIMIT = (gdt_desc & 0xffff) + (((gdt_desc>>48)&0xf)<<16);
	desc.BASE = ((gdt_desc >> 16)&0xffff) + (((gdt_desc >> 32)&0xff) << 16) + (((gdt_desc >> 56)&0xff) << 24);
	desc.ATTRIBUTES.UCHARs = (gdt_desc >> 40) & 0xffff;
	if (desc.ATTRIBUTES.Fields.S == 0) {
		ULONG64 tmp = *((ULONG64*)(gdt_base + (selector >> 3) * 8+8) );
		desc.BASE = (desc.BASE & 0xffffffff) | (tmp << 32);
	}
	if (desc.ATTRIBUTES.Fields.G) {
		desc.LIMIT = (desc.LIMIT << 12) + 0xfff;
	}
	return desc;
}

ULONG32 trans_desc_attr_to_access_right(struct SEGMENT_DESCRIPTOR sd) {
	union SEGMENT_ATTRIBUTES attr = sd.ATTRIBUTES;
	USHORT sel = sd.SEL;
	if (sel == 0) {
		return 0x10000;//if sel is null bit 16 must be 1
	}//这个地方非常重要，16位必须置1当 sel为0 典型是ldt sel
	ULONG32 ret = attr.Fields.TYPE + (attr.Fields.S << 4) +( attr.Fields.DPL << 5)\
		+ (attr.Fields.P << 7) + (attr.Fields.AVL<<12)+(attr.Fields.L<<13) + (attr.Fields.DB << 14) + ((attr.Fields.G) << 15);
	return ret;
}
/// <summary>
/// 以下这一段有问题
/// </summary>
/// <param name="vmm_state_ptr"></param>
/// <param name="eptp"></param>
/// <param name="guest_memory"></param>
/// <returns></returns>
BOOLEAN setup_vmcs(ULONG cpu_idx, ULONG64 rsp) {
	//guest context
	write_and_log(GUEST_CR0, __readcr0());
	write_and_log(GUEST_CR3, __readcr3());
	write_and_log(GUEST_CR4, __readcr4());
	write_and_log(GUEST_DR7, 0x400ULL);//硬件调试寄存器
	write_and_log(GUEST_RSP, rsp);
	write_and_log(GUEST_RIP, (ULONG64)restore_state);
	write_and_log(GUEST_RFLAGS, get_flags());

	struct SEGMENT_DESCRIPTOR es = read_normal_segment_descriptor(get_es());
	write_and_log(GUEST_ES_SELECTOR, es.SEL);
	write_and_log(GUEST_ES_BASE, es.BASE);
	write_and_log(GUEST_ES_LIMIT, es.LIMIT);
	write_and_log(GUEST_ES_AR_BYTES, trans_desc_attr_to_access_right(es));

	struct SEGMENT_DESCRIPTOR cs = read_normal_segment_descriptor(get_cs());
	write_and_log(GUEST_CS_SELECTOR, cs.SEL);
	write_and_log(GUEST_CS_BASE, cs.BASE);
	write_and_log(GUEST_CS_LIMIT, cs.LIMIT);
	write_and_log(GUEST_CS_AR_BYTES, trans_desc_attr_to_access_right(cs));

	//continue code with order es cs ss ds fs gs 
	struct SEGMENT_DESCRIPTOR ss = read_normal_segment_descriptor(get_ss());
	write_and_log(GUEST_SS_SELECTOR, ss.SEL);
	write_and_log(GUEST_SS_BASE, ss.BASE);
	write_and_log(GUEST_SS_LIMIT, ss.LIMIT);
	write_and_log(GUEST_SS_AR_BYTES, trans_desc_attr_to_access_right(ss));

	struct SEGMENT_DESCRIPTOR ds = read_normal_segment_descriptor(get_ds());
	write_and_log(GUEST_DS_SELECTOR, ds.SEL);
	write_and_log(GUEST_DS_BASE, ds.BASE);
	write_and_log(GUEST_DS_LIMIT, ds.LIMIT);
	write_and_log(GUEST_DS_AR_BYTES, trans_desc_attr_to_access_right(ds));

	struct SEGMENT_DESCRIPTOR fs = read_normal_segment_descriptor(get_fs());
	write_and_log(GUEST_FS_SELECTOR, fs.SEL);
	write_and_log(GUEST_FS_BASE, __readmsr(MSR_FS_BASE));
	write_and_log(GUEST_FS_LIMIT, fs.LIMIT);
	write_and_log(GUEST_FS_AR_BYTES, trans_desc_attr_to_access_right(fs));

	struct SEGMENT_DESCRIPTOR gs = read_normal_segment_descriptor(get_gs());
	write_and_log(GUEST_GS_SELECTOR, gs.SEL);
	write_and_log(GUEST_GS_BASE, __readmsr(MSR_GS_BASE));
	write_and_log(GUEST_GS_LIMIT, gs.LIMIT);
	write_and_log(GUEST_GS_AR_BYTES, trans_desc_attr_to_access_right(gs));

	struct SEGMENT_DESCRIPTOR ldtr = read_normal_segment_descriptor(get_ldtr());
	write_and_log(GUEST_LDTR_SELECTOR, ldtr.SEL);
	write_and_log(GUEST_LDTR_BASE, ldtr.BASE);
	write_and_log(GUEST_LDTR_LIMIT, ldtr.LIMIT);
	write_and_log(GUEST_LDTR_AR_BYTES, trans_desc_attr_to_access_right(ldtr));

	struct SEGMENT_DESCRIPTOR tr = read_normal_segment_descriptor(get_tr());
	write_and_log(GUEST_TR_SELECTOR, tr.SEL);
	write_and_log(GUEST_TR_BASE, tr.BASE);
	write_and_log(GUEST_TR_LIMIT, tr.LIMIT);
	write_and_log(GUEST_TR_AR_BYTES, trans_desc_attr_to_access_right(tr));

	write_and_log(GUEST_GDTR_BASE, get_gdt_base());
	write_and_log(GUEST_GDTR_LIMIT, 0xffff);
	write_and_log(GUEST_IDTR_BASE, get_idt_base());
	write_and_log(GUEST_IDTR_LIMIT, 0xffff);

	write_and_log(GUEST_IA32_DEBUGCTL, __readmsr(IA32_DEBUGCTL)&0xffffffff);
	write_and_log(GUEST_IA32_DEBUGCTL_HIGH, __readmsr(IA32_DEBUGCTL)>>32);

	write_and_log(TSC_OFFSET, 0);
	write_and_log(TSC_OFFSET_HIGH, 0);

	write_and_log(PAGE_FAULT_ERROR_CODE_MASK, 0);
	write_and_log(PAGE_FAULT_ERROR_CODE_MATCH, 0);

	write_and_log(VM_EXIT_MSR_STORE_COUNT, 0);
	write_and_log(VM_EXIT_MSR_LOAD_COUNT, 0);

	write_and_log(VM_ENTRY_MSR_LOAD_COUNT, 0);
	write_and_log(VM_ENTRY_INTR_INFO_FIELD, 0);

	write_and_log(GUEST_SYSENTER_EIP, __readmsr(IA32_SYSENTER_EIP));
	write_and_log(GUEST_SYSENTER_ESP, __readmsr(IA32_SYSENTER_ESP));
	write_and_log(GUEST_SYSENTER_CS, __readmsr(IA32_SYSENTER_CS));

	write_and_log(GUEST_ACTIVITY_STATE, 0);
	write_and_log(GUEST_INTERRUPTIBILITY_INFO, 0);
	write_and_log(VMCS_LINK_POINTER, ~0ULL);


	//host context
	write_and_log(HOST_CR0, __readcr0());
	write_and_log(HOST_CR3, __readcr3());
	write_and_log(HOST_CR4, __readcr4());
	write_and_log(HOST_RSP, (ULONG64)g_vmm_state_ptr[cpu_idx].vmm_stack + VMM_STACK_SIZE- 1);
	write_and_log(HOST_RIP, (ULONG64)vmx_exit_handler);
	write_and_log(HOST_CS_SELECTOR, get_cs() & 0xf8);
	write_and_log(HOST_SS_SELECTOR, get_ss() & 0xf8);
	write_and_log(HOST_DS_SELECTOR, get_ds() & 0xf8);
	write_and_log(HOST_ES_SELECTOR, get_es() & 0xf8);
	write_and_log(HOST_FS_SELECTOR, get_fs() & 0xf8);
	write_and_log(HOST_GS_SELECTOR, get_gs() & 0xf8);
	write_and_log(HOST_TR_SELECTOR, get_tr() & 0xf8);
	write_and_log(HOST_FS_BASE, __readmsr(MSR_FS_BASE));
	write_and_log(HOST_GS_BASE, __readmsr(MSR_GS_BASE));
	write_and_log(HOST_TR_BASE, tr.BASE);
	write_and_log(HOST_GDTR_BASE, get_gdt_base());
	write_and_log(HOST_IDTR_BASE, get_idt_base());
	write_and_log(HOST_IA32_SYSENTER_CS, __readmsr(IA32_SYSENTER_CS));
	write_and_log(HOST_IA32_SYSENTER_EIP, __readmsr(IA32_SYSENTER_EIP));
	write_and_log(HOST_IA32_SYSENTER_ESP, __readmsr(IA32_SYSENTER_ESP));

	ULONG64 IA32_VMX_BASIC_MSR_VALUE = __readmsr(IA32_VMX_BASIC_MSR);
	ULONG64 pin_based_vm_execution_reserve_control = 0;
	ULONG64 primary_process_based_vm_execution_reverse_control = 0;
	ULONG64 secondary_process_based_vm_execution_reverse_control = 0;
	ULONG64 vm_exit_control = 0;
	ULONG64 vm_entry_control = 0;


	BOOLEAN use_true_msr = ((IA32_VMX_BASIC_MSR_VALUE >> 55) & 0x01);
	
	ULONG64 pin_based_vm_execution_value = 0;
	ULONG64 primary_process_based_value = 0;
	ULONG64 secondary_process_based_value = 0;
	ULONG64 vm_exit_value = 0;
	ULONG64 vm_entry_value = 0;		

	primary_process_based_value |= (1ULL << 28);//use msrbitmaps;
	primary_process_based_value |= (1ULL << 31);//activate secondary process based control;
	secondary_process_based_value |= (1ULL << 3); // Enable RDTSCP
	secondary_process_based_value |= (1ULL << 12);//Enable INVPCID
	secondary_process_based_value |= (1ULL << 20);// Enable XSAVES / XRSTORS
	vm_exit_value |= (1ULL << 9); //enable 64bit in host when exit 
	vm_exit_value |= (1ULL << 15);
	vm_entry_value |= 1ULL << 9; //enable ia32e mode in guest when entry
	if (!use_true_msr) {
		pin_based_vm_execution_reserve_control = __readmsr(IA32_VMX_PINBASED_CTLS);
		primary_process_based_vm_execution_reverse_control = __readmsr(IA32_VMX_PROCBASED_CTLS);
		vm_exit_control = __readmsr(IA32_VMX_EXIT_CTLS);
		vm_entry_control = __readmsr(IA32_VMX_ENTRY_CTLS);
	}
	else {
		pin_based_vm_execution_reserve_control = __readmsr(IA32_VMX_TRUE_PINBASED_CTLS);
		primary_process_based_vm_execution_reverse_control = __readmsr(IA32_VMX_TRUE_PROCBASED_CTLS);
		vm_exit_control = __readmsr(IA32_VMX_TRUE_EXIT_CTLS);
		vm_entry_control = __readmsr(IA32_VMX_TRUE_ENTRY_CTLS);
	}
	secondary_process_based_vm_execution_reverse_control = __readmsr(IA32_VMX_PROCBASED_CTLS2);


	pin_based_vm_execution_value |= (pin_based_vm_execution_reserve_control & 0xffffffffULL);
	primary_process_based_value |= (primary_process_based_vm_execution_reverse_control & 0xffffffffULL);
	secondary_process_based_value |= (secondary_process_based_vm_execution_reverse_control & 0xffffffffULL);
	vm_exit_value |= (vm_exit_control & 0xffffffffULL);
	vm_entry_value |= (vm_entry_control & 0xffffffffULL);

	pin_based_vm_execution_value &= (pin_based_vm_execution_reserve_control >> 32) & 0xffffffffULL;
	primary_process_based_value &= (primary_process_based_vm_execution_reverse_control >> 32) & 0xffffffffULL;
	secondary_process_based_value &= (secondary_process_based_vm_execution_reverse_control >> 32) & 0xffffffffULL;
	vm_exit_value &= (vm_exit_control >> 32) & 0xffffffffULL;
	vm_entry_value &= (vm_entry_control >> 32) & 0xffffffffULL;
	/*
	if (!use_true_msr) {
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
		vm_entry_value |= 0b111111111ULL;
		vm_entry_value |= 1ULL << 12;
	}
	  这些不用手动设置，当！user_true_msr时， 对应的msr会在0-31中自动置1
	  上一步已经包括这些位了
	*/
	write_and_log(PIN_BASED_VM_EXEC_CONTROL, pin_based_vm_execution_value);
	write_and_log(CPU_BASED_VM_EXEC_CONTROL, primary_process_based_value);
	write_and_log(SECONDARY_VM_EXEC_CONTROL, secondary_process_based_value);
	write_and_log(EXCEPTION_BITMAP, 0ULL);
	write_and_log(MSR_BITMAP, g_vmm_state_ptr[cpu_idx].msr_bitmaps_pa);
	write_and_log(VM_EXIT_CONTROLS, vm_exit_value);
	write_and_log(VM_ENTRY_CONTROLS, vm_entry_value);
	return TRUE;
}

BOOLEAN main_vmx_exit_handler(struct register_state* regs) {
	//KdPrint(("vm exit\n")); 在root模式下可能执行不了？
	ULONG64 exit_reason = 0;
	__vmx_vmread(VM_EXIT_REASON, &exit_reason);
	exit_reason &= 0xffff;//低16位记录原因
	BOOLEAN ret = FALSE;
	switch (exit_reason) {
	case EXIT_FOR_CPUID:
	{
		if (regs->rax == 0x87654321 && regs->rcx == 0x12345678) {
			ret = TRUE;
			//leave_vmx();
			break;
		}
		int cpu_info[4] = { 0,0,0,0 };
		//DbgPrint("execute cpuid rax %p, rcx %p\n", regs->rax, regs->rcx);
		__cpuidex(cpu_info, (regs->rax) & 0xffffffff, (regs->rcx) & 0xffffffff);
		if (regs->rax == 1)
		{
			//
			// Set the Hypervisor Present-bit in RCX, which Intel and AMD have both
			// reserved for this indication
			//
			cpu_info[2] |= 0x80000000;
		}

		else if (regs->rax == 0x40000001)
		{
			//
			// Return our interface identifier
			//
			cpu_info[0] = 'HVFS'; // [H]yper[V]isor [F]rom [S]cratch
		}
		regs->rax = cpu_info[0];
		regs->rbx = cpu_info[1];
		regs->rcx = cpu_info[2];
		regs->rdx = cpu_info[3];
		break;
	}
	
	case EXIT_FOR_VMCALL:
	case EXIT_FOR_VMCLEAR:
	case EXIT_FOR_VMLAUNCH:
	case EXIT_FOR_VMPTRLD:
	case EXIT_FOR_VMPTRST :
	case EXIT_FOR_VMREAD :
	case EXIT_FOR_VMRESUME:
	case EXIT_FOR_VMWRITE :
	case EXIT_FOR_VMXON:
	case EXIT_FOR_VMXOFF:
		{
		//根据文档，这些VM有关的指令，通过修改RFLAGS的某些位，来指示执行后是否
		//成功，而cf位置1表示vmfailvalid，参数合法，但执行失败
		
		ULONG64 rflags = 0;
		__vmx_vmread(GUEST_RFLAGS, &rflags);
		__vmx_vmwrite(GUEST_RFLAGS, rflags | 0x1);

		break;
		}	
	}
	ULONG64 ResumeRIP = NULL;
	ULONG64 CurrentRIP = NULL;
	ULONG   ExitInstructionLength = 0;
	
	__vmx_vmread(GUEST_RIP, &CurrentRIP);
	__vmx_vmread(VM_EXIT_INSTRUCTION_LEN, &ExitInstructionLength);
	ResumeRIP = CurrentRIP + ExitInstructionLength;
	__vmx_vmwrite(GUEST_RIP, ResumeRIP);//指向下一条指令
	g_guest_rip = ResumeRIP;
	__vmx_vmread(GUEST_RSP, &g_guest_rsp);
	return ret;
}

ULONG64 read_guest_rsp() {
	ULONG64 ret;
	__vmx_vmread(GUEST_RSP, &ret);
	return ret;
}

ULONG64 read_guest_rip() {
	ULONG64 ret;
	__vmx_vmread(GUEST_RIP, &ret);
	return ret;
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

VOID allocate_vmm_stack(ULONG cpu_idx) {
	ULONG64 stack_va = ExAllocatePoolWithTag(NonPagedPool, VMM_STACK_SIZE, DRIVER_TAG);
	if (!stack_va) {
		DbgPrint("allocate stack for vmm fail\n");
		return;
	}
	RtlZeroMemory(stack_va, VMM_STACK_SIZE);
	g_vmm_state_ptr[cpu_idx].vmm_stack = stack_va;
}

VOID allocate_msr_bitmaps(ULONG cpu_idx) {
	ULONG64 msr_bitmaps_va = ExAllocatePoolWithTag(NonPagedPool, PAGE_SIZE, DRIVER_TAG);
	if (!msr_bitmaps_va) {
		DbgPrint("allocate msr bitmaps fail\n");
		return;
	}
	RtlZeroMemory(msr_bitmaps_va, PAGE_SIZE);
	g_vmm_state_ptr[cpu_idx].msr_bitmaps_va = msr_bitmaps_va;
	g_vmm_state_ptr[cpu_idx].msr_bitmaps_pa = vitual_to_physical(msr_bitmaps_va);
}


VOID run_on_cpu(ULONG cpu_idx, VOID(*pfunc)(ULONG64)) {
	
	KeSetSystemAffinityThread((KAFFINITY)(1 << cpu_idx));
	KIRQL old_irql = KeRaiseIrqlToDpcLevel();
	pfunc(cpu_idx);
	KeLowerIrql(old_irql);
	KeRevertToUserAffinityThread();//恢复到正常cpu分配
}
VOID virtualize_cpu(ULONG cpu_idx, ULONG64 rsp) {
	__vmx_vmclear(&(g_vmm_state_ptr[cpu_idx].vmcs_pa));
	__vmx_vmptrld(&(g_vmm_state_ptr[cpu_idx].vmcs_pa));
	setup_vmcs(cpu_idx, rsp);
	__vmx_vmlaunch();
	DbgPrint("launch fail\n");
	DbgBreakPoint();
}