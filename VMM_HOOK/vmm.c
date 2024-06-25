#define POOL_ZERO_DOWN_LEVEL_SUPPORT
#include <ntddk.h>
#include <intrin.h>
#include "vmm.h"
#include "msr.h"
#include "tool.h"
#include "ept.h"
extern BOOLEAN save_state_and_virtualize(ULONG idx);
extern VOID restore_state();
extern VOID vmm_exit_handler();
extern USHORT get_es();
extern USHORT get_cs();
extern USHORT get_ss();
extern USHORT get_ds();
extern USHORT get_fs();
extern USHORT get_gs();
extern USHORT get_ldtr();
extern USHORT get_tr();
extern USHORT get_gdt_limit();
extern USHORT get_idt_limit();
extern ULONG64 get_gdt_base();
extern ULONG64 get_idt_base();
extern ULONG64 get_flags();

ULONG64 g_maximum_pa_size = 0;
ULONG g_cpu_count = 0;
P_VMM_STATE g_vmm_state_ptr = 0;
ULONG64 g_eptp = 0;
BOOLEAN support_vmx() {
	return test_cpuid_bit(1, 2, 5);
}

BOOLEAN enable_vmx() {
	int cpu_info[4] = { 0 };
	__cpuidex(cpu_info, 0x80000008, 0);
	g_maximum_pa_size = (cpu_info[0] & 0xff);
	Log("cpu maxium addr size is %lu", g_maximum_pa_size);
	g_cpu_count = KeQueryActiveProcessorCount(NULL);
	for (ULONG i = 0; i < g_cpu_count; i++) {
		KAFFINITY ka = (KAFFINITY)(1ULL << i);
		KeSetSystemAffinityThread(ka);
		ULONG64 origin_cr4 = __readcr4();
		origin_cr4 |= (1ULL << 13);
		__writecr4(origin_cr4);
		/*
		* 
		* 这些是bios锁定的
		ULONG64 cpu_feature = __readmsr(IA32_FEATURE_CONTROL_MSR);
		cpu_feature |= 1ULL;
		cpu_feature |= (1ULL << 1);
		cpu_feature |= (1ULL << 2);
		__writemsr(IA32_FEATURE_CONTROL_MSR, cpu_feature);
		*/
		KeRevertToUserAffinityThread();//恢复原cpu
	}
	return TRUE;
}
BOOLEAN allocate_vmx_region(ULONG idx) {
	if (KeGetCurrentIrql() > DISPATCH_LEVEL) {
		KeRaiseIrqlToDpcLevel();//MmAllocateContiguousMemory要求<=DISPATCH,所有内存分配都是这样
	}
	PHYSICAL_ADDRESS pa = { .QuadPart = MAXULONG64 };
	ULONG64 buffer = (ULONG64)MmAllocateContiguousMemory(2 * VMXON_REGION_SIZE, pa);
	if (!buffer) {
		return FALSE;
	}
	RtlZeroMemory((PVOID)buffer, 2 * VMXON_REGION_SIZE);
	ULONG64 aligned_va = ((buffer >> 12) << 12) + PAGE_SIZE;
	ULONG64 aligned_pa = virtual_to_physic(aligned_va);
	ULONG64 vmx_basic = __readmsr(IA32_VMX_BASIC_MSR);
	ULONG32 identify = (ULONG32)((vmx_basic & 0x7fffffff));
	*((ULONG32*)aligned_va) = identify;
	g_vmm_state_ptr[idx].vmx_on_region_va = aligned_va;
	g_vmm_state_ptr[idx].vmx_on_region_pa = aligned_pa;
	return TRUE;
}
BOOLEAN allocate_vmcs(ULONG idx) {
	if (KeGetCurrentIrql() > DISPATCH_LEVEL) {
		KeRaiseIrqlToDpcLevel();//MmAllocateContiguousMemory要求<=DISPATCH,所有内存分配都是这样
	}
	PHYSICAL_ADDRESS pa = { .QuadPart = MAXULONG64 };
	ULONG64 buffer = (ULONG64)MmAllocateContiguousMemory(2 * VMXON_REGION_SIZE, pa);
	if (!buffer) {
		return FALSE;
	}
	RtlZeroMemory((PVOID)buffer, 2 * VMXON_REGION_SIZE);
	ULONG64 aligned_va = ((buffer >> 12) << 12) + PAGE_SIZE;
	ULONG64 aligned_pa = virtual_to_physic(aligned_va);
	ULONG64 vmx_basic = __readmsr(IA32_VMX_BASIC_MSR);
	ULONG32 identify = (ULONG32)((vmx_basic & 0x7fffffff));
	*((ULONG32*)aligned_va) = identify;
	g_vmm_state_ptr[idx].vmcs_va = aligned_va;
	g_vmm_state_ptr[idx].vmcs_pa = aligned_pa;
	return TRUE;
}
BOOLEAN allocate_stack(ULONG idx) {
	ULONG64 buffer = (ULONG64)ExAllocatePoolZero(NonPagedPool, VMM_STACK_SIZE, DRIVER_TAG);
	if (!buffer) return FALSE;
	RtlZeroMemory((PVOID)buffer, VMM_STACK_SIZE);
	g_vmm_state_ptr[idx].vmm_stack_va = buffer;
	return TRUE;
}
BOOLEAN allocate_msrbitmap(ULONG idx) {
	ULONG64 buffer = (ULONG64)ExAllocatePoolZero(NonPagedPool, PAGE_SIZE, DRIVER_TAG);
	if (!buffer) return FALSE;
	RtlZeroMemory((PVOID)buffer, PAGE_SIZE);
	g_vmm_state_ptr[idx].msr_bitmap_pa = virtual_to_physic(buffer);
	return TRUE;
}
BOOLEAN allocate_resource() {
	PAGED_CODE();
	g_vmm_state_ptr = (P_VMM_STATE)ExAllocatePoolZero(NonPagedPool, sizeof(struct VMM_STATE) * g_cpu_count, DRIVER_TAG);
	if (!g_vmm_state_ptr) return FALSE;
	RtlZeroMemory(g_vmm_state_ptr, sizeof(struct VMM_STATE) * g_cpu_count);
	
	if (!init_ept(&g_eptp)) return FALSE;

	for (ULONG i = 0; i < g_cpu_count; i++) {
		KAFFINITY  ka = (KAFFINITY)(1ULL << i);
		KeSetSystemAffinityThread(ka);
		if (!allocate_vmx_region(i)) return FALSE;
		if (!allocate_vmcs(i)) return FALSE;
		if (!allocate_stack(i)) return FALSE;
		if (!allocate_msrbitmap(i)) return FALSE;
		unsigned char ret= __vmx_on(&(g_vmm_state_ptr[i].vmx_on_region_pa));
		KeRevertToUserAffinityThread();
		if (ret > 0)return FALSE;
	}
	return TRUE;
}

SEGMENT_DES read_normal_segment_descriptor(USHORT selector) {
	ULONG64 gdt_base = get_gdt_base();
	SEGMENT_DES desc = { 0 };
	RtlZeroMemory(&desc, sizeof(desc));
	ULONG64 gdt_desc = *((ULONG64*)(gdt_base + (selector >> 3) * 8));
	desc.SEL = selector;
	desc.LIMIT = (gdt_desc & 0xffff) + (((gdt_desc >> 48) & 0xf) << 16);
	desc.BASE = ((gdt_desc >> 16) & 0xffff) + (((gdt_desc >> 32) & 0xff) << 16) + (((gdt_desc >> 56) & 0xff) << 24);
	desc.ATTRIBUTES.UCHARs = (gdt_desc >> 40) & 0xffff;
	if (desc.ATTRIBUTES.Fields.S == 0) {
		ULONG64 tmp = *((ULONG64*)(gdt_base + (selector >> 3) * 8 + 8));
		desc.BASE = (desc.BASE & 0xffffffff) | (tmp << 32);
	}
	if (desc.ATTRIBUTES.Fields.G) {
		desc.LIMIT = (desc.LIMIT << 12) + 0xfff;
	}
	return desc;
}

ULONG32 trans_desc_attr_to_access_right(SEGMENT_DES sd) {
	SEGMENT_ATTR attr = sd.ATTRIBUTES;
	USHORT sel = sd.SEL;
	if (sel == 0) {
		return 0x10000;//if sel is null bit 16 must be 1
	}//这个地方非常重要，16位必须置1当 sel为0 典型是ldt sel
	ULONG32 ret = attr.Fields.TYPE + (attr.Fields.S << 4) + (attr.Fields.DPL << 5)\
		+ (attr.Fields.P << 7) + (attr.Fields.AVL << 12) + (attr.Fields.L << 13) + (attr.Fields.DB << 14) + ((attr.Fields.G) << 15);
	return ret;
}

VOID setup_vmcs(ULONG cpu_idx, ULONG64 rsp) {
	__vmx_vmwrite(GUEST_CR0, __readcr0());
	__vmx_vmwrite(GUEST_CR3, __readcr3());
	__vmx_vmwrite(GUEST_CR4, __readcr4());
	__vmx_vmwrite(GUEST_DR7, 0x400ULL);//硬件调试寄存器
	__vmx_vmwrite(GUEST_RSP, rsp);
	__vmx_vmwrite(GUEST_RIP, (ULONG64)restore_state);
	__vmx_vmwrite(GUEST_RFLAGS, get_flags());

	SEGMENT_DES es = read_normal_segment_descriptor(get_es());
	__vmx_vmwrite(GUEST_ES_SELECTOR, es.SEL);
	__vmx_vmwrite(GUEST_ES_BASE, es.BASE);
	__vmx_vmwrite(GUEST_ES_LIMIT, es.LIMIT);
	__vmx_vmwrite(GUEST_ES_AR_BYTES, trans_desc_attr_to_access_right(es));

	SEGMENT_DES cs = read_normal_segment_descriptor(get_cs());
	__vmx_vmwrite(GUEST_CS_SELECTOR, cs.SEL);
	__vmx_vmwrite(GUEST_CS_BASE, cs.BASE);
	__vmx_vmwrite(GUEST_CS_LIMIT, cs.LIMIT);
	__vmx_vmwrite(GUEST_CS_AR_BYTES, trans_desc_attr_to_access_right(cs));

	//continue code with order es cs ss ds fs gs 
	SEGMENT_DES ss = read_normal_segment_descriptor(get_ss());
	__vmx_vmwrite(GUEST_SS_SELECTOR, ss.SEL);
	__vmx_vmwrite(GUEST_SS_BASE, ss.BASE);
	__vmx_vmwrite(GUEST_SS_LIMIT, ss.LIMIT);
	__vmx_vmwrite(GUEST_SS_AR_BYTES, trans_desc_attr_to_access_right(ss));

	SEGMENT_DES ds = read_normal_segment_descriptor(get_ds());
	__vmx_vmwrite(GUEST_DS_SELECTOR, ds.SEL);
	__vmx_vmwrite(GUEST_DS_BASE, ds.BASE);
	__vmx_vmwrite(GUEST_DS_LIMIT, ds.LIMIT);
	__vmx_vmwrite(GUEST_DS_AR_BYTES, trans_desc_attr_to_access_right(ds));

	SEGMENT_DES fs = read_normal_segment_descriptor(get_fs());
	__vmx_vmwrite(GUEST_FS_SELECTOR, fs.SEL);
	__vmx_vmwrite(GUEST_FS_BASE, __readmsr(MSR_FS_BASE_MSR));
	__vmx_vmwrite(GUEST_FS_LIMIT, fs.LIMIT);
	__vmx_vmwrite(GUEST_FS_AR_BYTES, trans_desc_attr_to_access_right(fs));

	SEGMENT_DES gs = read_normal_segment_descriptor(get_gs());
	__vmx_vmwrite(GUEST_GS_SELECTOR, gs.SEL);
	__vmx_vmwrite(GUEST_GS_BASE, __readmsr(MSR_GS_BASE_MSR));
	__vmx_vmwrite(GUEST_GS_LIMIT, gs.LIMIT);
	__vmx_vmwrite(GUEST_GS_AR_BYTES, trans_desc_attr_to_access_right(gs));

	SEGMENT_DES ldtr = read_normal_segment_descriptor(get_ldtr());
	__vmx_vmwrite(GUEST_LDTR_SELECTOR, ldtr.SEL);
	__vmx_vmwrite(GUEST_LDTR_BASE, ldtr.BASE);
	__vmx_vmwrite(GUEST_LDTR_LIMIT, ldtr.LIMIT);
	__vmx_vmwrite(GUEST_LDTR_AR_BYTES, trans_desc_attr_to_access_right(ldtr));

	SEGMENT_DES tr = read_normal_segment_descriptor(get_tr());
	__vmx_vmwrite(GUEST_TR_SELECTOR, tr.SEL);
	__vmx_vmwrite(GUEST_TR_BASE, tr.BASE);
	__vmx_vmwrite(GUEST_TR_LIMIT, tr.LIMIT);
	__vmx_vmwrite(GUEST_TR_AR_BYTES, trans_desc_attr_to_access_right(tr));

	__vmx_vmwrite(GUEST_GDTR_BASE, get_gdt_base());
	__vmx_vmwrite(GUEST_GDTR_LIMIT, 0xffff);
	__vmx_vmwrite(GUEST_IDTR_BASE, get_idt_base());
	__vmx_vmwrite(GUEST_IDTR_LIMIT, 0xffff);

	__vmx_vmwrite(GUEST_IA32_DEBUGCTL, __readmsr(IA32_DEBUGCTL_MSR));
	

	__vmx_vmwrite(TSC_OFFSET, 0);
	__vmx_vmwrite(TSC_OFFSET_HIGH, 0);

	__vmx_vmwrite(PAGE_FAULT_ERROR_CODE_MASK, 0);
	__vmx_vmwrite(PAGE_FAULT_ERROR_CODE_MATCH, 0);

	__vmx_vmwrite(VM_EXIT_MSR_STORE_COUNT, 0);
	__vmx_vmwrite(VM_EXIT_MSR_LOAD_COUNT, 0);

	__vmx_vmwrite(VM_ENTRY_MSR_LOAD_COUNT, 0);
	__vmx_vmwrite(VM_ENTRY_INTR_INFO_FIELD, 0);

	__vmx_vmwrite(GUEST_SYSENTER_EIP, __readmsr(IA32_SYSENTER_EIP_MSR));
	__vmx_vmwrite(GUEST_SYSENTER_ESP, __readmsr(IA32_SYSENTER_ESP_MSR));
	__vmx_vmwrite(GUEST_SYSENTER_CS, __readmsr(IA32_SYSENTER_CS_MSR));

	__vmx_vmwrite(GUEST_ACTIVITY_STATE, 0);
	__vmx_vmwrite(GUEST_INTERRUPTIBILITY_INFO, 0);
	__vmx_vmwrite(VMCS_LINK_POINTER, ~0ULL);


	//host context
	__vmx_vmwrite(HOST_CR0, __readcr0());
	__vmx_vmwrite(HOST_CR3, __readcr3());
	__vmx_vmwrite(HOST_CR4, __readcr4());
	__vmx_vmwrite(HOST_RSP, (ULONG64)g_vmm_state_ptr[cpu_idx].vmm_stack_va + VMM_STACK_SIZE - 1);
	__vmx_vmwrite(HOST_RIP, (ULONG64)vmm_exit_handler);
	__vmx_vmwrite(HOST_CS_SELECTOR, get_cs() & 0xf8);
	__vmx_vmwrite(HOST_SS_SELECTOR, get_ss() & 0xf8);
	__vmx_vmwrite(HOST_DS_SELECTOR, get_ds() & 0xf8);
	__vmx_vmwrite(HOST_ES_SELECTOR, get_es() & 0xf8);
	__vmx_vmwrite(HOST_FS_SELECTOR, get_fs() & 0xf8);
	__vmx_vmwrite(HOST_GS_SELECTOR, get_gs() & 0xf8);
	__vmx_vmwrite(HOST_TR_SELECTOR, get_tr() & 0xf8);
	__vmx_vmwrite(HOST_FS_BASE, __readmsr(MSR_FS_BASE_MSR));
	__vmx_vmwrite(HOST_GS_BASE, __readmsr(MSR_GS_BASE_MSR));
	__vmx_vmwrite(HOST_TR_BASE, tr.BASE);
	__vmx_vmwrite(HOST_GDTR_BASE, get_gdt_base());
	__vmx_vmwrite(HOST_IDTR_BASE, get_idt_base());
	__vmx_vmwrite(HOST_IA32_SYSENTER_CS, __readmsr(IA32_SYSENTER_CS_MSR));
	__vmx_vmwrite(HOST_IA32_SYSENTER_EIP, __readmsr(IA32_SYSENTER_EIP_MSR));
	__vmx_vmwrite(HOST_IA32_SYSENTER_ESP, __readmsr(IA32_SYSENTER_ESP_MSR));

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
	//什么是PCID 和VPID?
	//都是用来实现多个上下文的同一线性地址的同时缓存的id
	//Processor Context Identify让本来切换cr3就清空缓存变成了，不一定清空，不同进程的同一线性地址，带上
	//PCID就可以在同一个TLB里共存，而不会冲突
	//Virtual Processor Identify也是类似：是多个Guest的Guest-pa物理地址到 Host pa的转换中，也能实现多个
	//guest不冲突

	//两者能够结合使用

	//SLAT和HLAT
	//Second Liner Address Translation 是虚拟化下的内存控制机制,一般是ept来实现（扩展页表）
	//Hypervisor Liner Address Translation是虚拟化下加速内存翻译的机制，能够实现guest va直接到host pa,
	//具体怎么实现的，手册说的比较模糊，只是说是一个不同寻常的线性地址转换机制

	primary_process_based_value |= (1ULL << 28);//use msrbitmaps;
	primary_process_based_value |= (1ULL << 31);//activate secondary process based control;
	secondary_process_based_value |= (1ULL << 3); // Enable RDTSCP
	secondary_process_based_value |= (1ULL << 12);//Enable INVPCID
	secondary_process_based_value |= (1ULL << 20);// Enable XSAVES / XRSTORS
	vm_exit_value |= (1ULL << 9); //enable 64bit in host when exit 
	vm_exit_value |= (1ULL << 15);
	vm_entry_value |= 1ULL << 9; //enable ia32e mode in guest when entry
	if (!use_true_msr) {
		pin_based_vm_execution_reserve_control = __readmsr(IA32_VMX_PINBASED_CTLS_MSR);
		primary_process_based_vm_execution_reverse_control = __readmsr(IA32_VMX_PROCBASED_CTLS_MSR);
		vm_exit_control = __readmsr(IA32_VMX_EXIT_CTLS_MSR);
		vm_entry_control = __readmsr(IA32_VMX_ENTRY_CTLS_MSR);
	}
	else {
		pin_based_vm_execution_reserve_control = __readmsr(IA32_VMX_TRUE_PINBASED_CTLS_MSR);
		primary_process_based_vm_execution_reverse_control = __readmsr(IA32_VMX_TRUE_PROCBASED_CTLS_MSR);
		vm_exit_control = __readmsr(IA32_VMX_TRUE_EXIT_CTLS_MSR);
		vm_entry_control = __readmsr(IA32_VMX_TRUE_ENTRY_CTLS_MSR);
	}
	secondary_process_based_vm_execution_reverse_control = __readmsr(IA32_VMX_PROCBASED_CTLS2_MSR);


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
	__vmx_vmwrite(PIN_BASED_VM_EXEC_CONTROL, pin_based_vm_execution_value);
	__vmx_vmwrite(CPU_BASED_VM_EXEC_CONTROL, primary_process_based_value);
	__vmx_vmwrite(SECONDARY_VM_EXEC_CONTROL, secondary_process_based_value);
	__vmx_vmwrite(EXCEPTION_BITMAP, 0ULL);
	__vmx_vmwrite(MSR_BITMAP, g_vmm_state_ptr[cpu_idx].msr_bitmap_pa);
	__vmx_vmwrite(VM_EXIT_CONTROLS, vm_exit_value);
	__vmx_vmwrite(VM_ENTRY_CONTROLS, vm_entry_value);
	
}

VOID virtualize_cpu(ULONG idx, ULONG64 rsp) {
	setup_vmcs(idx, rsp);
	__vmx_vmlaunch();
	DbgBreakPoint();
}

ULONG64 vmm_call_handler(ULONG64 call_number, ULONG64 option_p1, ULONG64 option_p2, ULONG64 option_p3) {
	UNREFERENCED_PARAMETER(option_p1);
	UNREFERENCED_PARAMETER(option_p2);
	UNREFERENCED_PARAMETER(option_p3);
	ULONG64 ret = (ULONG64)CALL_RET_SUCCESS;
	switch (call_number) {
	case CALL_TEST:
		Log("test call execute success");
		break;
	}

	return ret;
}
VOID main_exit_handler(PREG_STATE reg_ptr) {
	ULONG32 exit_reason = 0;
	__vmx_vmread(VM_EXIT_REASON, (size_t*)( & exit_reason));
	exit_reason &= 0xffff;
	switch (exit_reason) {
	case EXIT_FOR_VMCALL:
	{
		
		reg_ptr->rax= vmm_call_handler(reg_ptr->rcx, reg_ptr->rdx, reg_ptr->r8, reg_ptr->r9);
		break;
	}
	case EXIT_FOR_VMCLEAR:
	case EXIT_FOR_VMLAUNCH:
	case EXIT_FOR_VMPTRLD:
	case EXIT_FOR_VMPTRST:
	case EXIT_FOR_VMREAD:
	case EXIT_FOR_VMRESUME:
	case EXIT_FOR_VMWRITE:
	case EXIT_FOR_VMXON:
	case EXIT_FOR_VMXOFF:
		{
		ULONG64 flags = 0;
		__vmx_vmread(GUEST_RFLAGS, &flags);
		__vmx_vmwrite(GUEST_RFLAGS, flags | 0x01);//置1表示失败
		break;
		}
	case EXIT_FOR_CPUID:
	{
		int cpu_info[4] = { 0,0,0,0 };
		//DbgPrint("execute cpuid rax %p, rcx %p\n", regs->rax, regs->rcx);
		__cpuidex(cpu_info, (reg_ptr->rax) & 0xffffffff, (reg_ptr->rcx) & 0xffffffff);
		if (reg_ptr->rax == 1)
		{
			//
			// Set the Hypervisor Present-bit in RCX, which Intel and AMD have both
			// reserved for this indication
			//
			cpu_info[2] |= 0x80000000;
		}

		else if (reg_ptr->rax == 0x40000001)
		{
			//
			// Return our interface identifier
			//

			cpu_info[0] = ((int)'L') + (((int)'S') << 8) + (((int)'M') << 16);
		}
		reg_ptr->rax = cpu_info[0];
		reg_ptr->rbx = cpu_info[1];
		reg_ptr->rcx = cpu_info[2];
		reg_ptr->rdx = cpu_info[3];
		break;
	}
	}
	ULONG64 ResumeRIP = 0;
	ULONG64 CurrentRIP = 0;
	ULONG   ExitInstructionLength = 0;

	__vmx_vmread(GUEST_RIP, &CurrentRIP);
	__vmx_vmread(VM_EXIT_INSTRUCTION_LEN, (size_t*)( & ExitInstructionLength));
	ResumeRIP = CurrentRIP + ExitInstructionLength;
	__vmx_vmwrite(GUEST_RIP, ResumeRIP);//指向下一条指令
}

VOID resume_guest() {
	__vmx_vmresume();
	DbgBreakPoint();
}
BOOLEAN setup_resource() {
	KIRQL old = 0;
	BOOLEAN ret = TRUE;
	__try {
		for (ULONG i = 0; i < g_cpu_count; i++) {
			KAFFINITY ka = (KAFFINITY)(1ULL << i);
			KeSetSystemAffinityThread(ka);
			old = KeRaiseIrqlToDpcLevel();//防止处理器切换进程
			//因为在高IRQL中保存了各种寄存器，因此VMM root是在高IRQL中
			//执行的，因此无法调用各种内核函数
			UCHAR status = __vmx_vmclear(&(g_vmm_state_ptr[i].vmcs_pa));
			if(status >0){
				//Log("clear fail, code is %d", status);
				ExRaiseStatus(STATUS_UNSUCCESSFUL);
			}
			status = __vmx_vmptrld(&(g_vmm_state_ptr[i].vmcs_pa));
			if (status >0) {
				//Log("load fail, code is %d", status);
				ExRaiseStatus(STATUS_UNSUCCESSFUL);
			}
			if (!save_state_and_virtualize(i)) {
				ExRaiseStatus(STATUS_UNSUCCESSFUL);
			}
			KeLowerIrql(old);
			KeRevertToUserAffinityThread();
		}
	}

	__except(EXCEPTION_EXECUTE_HANDLER){
		KeLowerIrql(old);
		KeRevertToUserAffinityThread();
		ret = FALSE;
		
	}
	
	return ret;
}
BOOLEAN init_vmm() {
	if (!support_vmx()) {
		Log("do not support vmx");
		return FALSE;
	}
	if (!enable_vmx()) {
		Log("enable vmx fail");
		return FALSE;
	}
	if (!allocate_resource()) {
		Log("allocate context fail");
		return FALSE;
	}
	if (!setup_resource()) {
		Log("setup context fail");
		return FALSE;
	}
	return TRUE;
	
}