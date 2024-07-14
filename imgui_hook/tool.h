#pragma once
#include <windows.h>

typedef struct _vec2 {
	int x;
	int y;
}VEC2;

typedef struct _vec3 {
	int x;
	int y;
	int z;
}VEC3;


typedef struct _vec2f {
	float x;
	float y;
}VEC2f;


typedef struct _vec3f {
	float x;
	float y;
	float z;
}VEC3f;

BOOLEAN suspend_or_resume_other_threads(BOOLEAN);
LPVOID get_module_base_addr(LPCWSTR module_name);
LPVOID get_func_in_module(LPCWSTR module, LPCSTR func_name);
UINT_PTR resolve_addr_in_offset_mem_instruction(UINT_PTR instruction_addr, UINT32 instruction_len, UINT32 offset_start);
UINT_PTR pattern_scan(const wchar_t* module_name, const char* signature);
VOID LogWithInfo(const char* file, int line, const WCHAR* format, ...);
VOID LogWithInfoA(const char* file, int line, const char* foramt, ...);


#define Log(format, ...) LogWithInfo(__FILE__, __LINE__, format, __VA_ARGS__)
#define LogA(format, ...) LogWithInfoA(__FILE__, __LINE__, format, __VA_ARGS__)
