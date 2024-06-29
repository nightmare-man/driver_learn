#pragma once
#include <ntddk.h>
#define Log(format, ...)  \
    DbgPrint("Info (%s:%d) | " format "\n",	\
		 __FUNCTION__, __LINE__, __VA_ARGS__)

BOOLEAN test_cpuid_bit(int idx, int register_id, int bit);
ULONG64 virtual_to_physic(ULONG64);
ULONG64 physic_to_virtual(ULONG64);