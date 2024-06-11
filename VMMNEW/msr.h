#pragma once
#include <ntddk.h>
#define IA32_VMX_TRUE_PINBASED_CTLS 0x48d
#define IA32_VMX_PINBASED_CTLS 0x481
#define IA32_VMX_BASIC_MSR 0x480

#define IA32_VMX_PROCBASED_CTLS 0x482
#define IA32_VMX_TRUE_PROCBASED_CTLS 0x48e

#define IA32_VMX_PROCBASED_CTLS2 0x48b

#define IA32_VMX_EXIT_CTLS 0x483
#define IA32_VMX_TRUE_EXIT_CTLS 0x48f

#define IA32_VMX_ENTRY_CTLS 0x484
#define IA32_VMX_TRUE_ENTRY_CTLS 0x490

#define IA32_FEATURE_CONTROL_MSR 0x3a 
#define IA32_SYSENTER_CS  0x174
#define IA32_SYSENTER_ESP 0x175
#define IA32_SYSENTER_EIP 0x176
#define IA32_DEBUGCTL     0x1D9

union FEATURE_CONTROL_MSR {
	ULONG64 all;
	struct {
		ULONG64 lock : 1;
		ULONG64 enable_vmx_in_smx : 1;
		ULONG64 enable_vmx_out_smx : 1;
		ULONG64 unused : 61;
	} fields;
};
union VMX_BASE_MSR {
	ULONG64 all;
	struct {
		ULONG32 revision : 31;   // [0-30]
		ULONG32 Reserved1 : 1;             // [31]
		ULONG32 RegionSize : 12;           // [32-43]
		ULONG32 RegionClear : 1;           // [44]
		ULONG32 Reserved2 : 3;             // [45-47]
		ULONG32 SupportedIA64 : 1;         // [48]
		ULONG32 SupportedDualMoniter : 1;  // [49]
		ULONG32 MemoryType : 4;            // [50-53]
		ULONG32 VmExitReport : 1;          // [54]
		ULONG32 VmxCapabilityHint : 1;     // [55]
		ULONG32 Reserved3 : 8;             // [56-63]
	} fields;
};
struct CPUID_RESULT {
	int eax;
	int ebx;
	int ecx;
	int edx;
};
