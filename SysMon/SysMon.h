#pragma once
#include <ntddk.h>
#include "common.h"

struct FullProcessExitInfo {
	LIST_ENTRY entry;
	struct ProcessExitInfo info;
};

struct FullProcessCreateInfo {
	LIST_ENTRY entry;
	struct ProcessCreateInfo info;
};

struct Globals {
	LIST_ENTRY item_header;
	int item_count;
	FAST_MUTEX mutex;
};
