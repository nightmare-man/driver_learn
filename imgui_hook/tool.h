#pragma once
#include <windows.h>
BOOLEAN suspend_or_resume_other_threads(BOOLEAN);
LPVOID get_module_base_addr(LPCWSTR module_name);
