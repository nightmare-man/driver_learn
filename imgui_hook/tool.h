#pragma once
#include <windows.h>

BOOLEAN suspend_or_resume_other_threads(BOOLEAN);
LPVOID get_module_base_addr(LPCWSTR module_name);
LPVOID resolve_addr_in_offset_mem_instruction(LPVOID instruction_addr, UINT32 instruction_len, UINT32 offset_start);
VOID LogWithInfo(const char* file, int line, const WCHAR* format, ...);
VOID LogWithInfoA(const char* file, int line, const char* foramt, ...);
#define Log(format, ...) LogWithInfo(__FILE__, __LINE__, format, __VA_ARGS__)
#define LogA(format, ...) LogWithInfoA(__FILE__, __LINE__, format, __VA_ARGS__)
