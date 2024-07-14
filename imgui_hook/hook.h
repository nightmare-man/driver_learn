#pragma once
#include <Windows.h>
LPVOID hook_func(LPVOID old_func, LPVOID new_func);
LPVOID reset_func(LPVOID old_func);
