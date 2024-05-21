#pragma once
#include <ntddk.h>
ULONG64 vitual_to_physical(ULONG64 va);
ULONG64 physical_to_vitual(ULONG64 pa);
BOOLEAN is_vmx_support();