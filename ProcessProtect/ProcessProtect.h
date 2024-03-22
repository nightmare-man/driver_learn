#pragma once
#include "ntddk.h"
#define PROCESS_TERMINATE 1
#define MAX_PIDS 256
struct Globals {
	int pid_count;
	ULONG pids[MAX_PIDS];
	FAST_MUTEX lock;
	PVOID reg_handle;
};

