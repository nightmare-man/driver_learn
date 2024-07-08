#include "tool.h"
#include <tlhelp32.h>
#include <psapi.h>
#include <stdio.h>
BOOLEAN suspend_or_resume_other_threads(BOOLEAN suspend) {
	DWORD current_tid = GetCurrentThreadId();
	DWORD current_pid = GetCurrentProcessId();
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (!hSnap) return FALSE;
	THREADENTRY32 te;
	te.dwSize = sizeof(te);
	if (Thread32First(hSnap, &te)) {
		while (1) {
			if (te.th32OwnerProcessID == current_pid && te.th32ThreadID != current_tid) {
				HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
				if (!hThread) {
					//OutputDebugString(L"fail to open one thread");
					continue;
				}
				if (suspend) {

#ifdef _WIN64
					if (Wow64SuspendThread(hThread) == -1) {
					//	OutputDebugString(L"suspend thread fail\n");
					}
#else
					if (SuspendThread(hThread) == -1) {
						//OutputDebugString(L"suspend thread fail\n");
					}
#endif
					//OutputDebugString(L"suspend thread success\n");


				}
				else {
					if (ResumeThread(hThread) == -1) {
						//OutputDebugString(L"resume thread fail\n");
					}

					//OutputDebugString(L"resume thread success\n");
				}

			}
			if (!Thread32Next(hSnap, &te)) {
				break;
			}
		}
	}
	else {
		return FALSE;
	}
	return TRUE;
}

LPCWSTR get_file_name_in_path(LPCWSTR path) {
	DWORD path_len = wcslen(path);
	LPCWSTR ret = path;
	for (INT32 p = (INT32)path_len - 1; p >= 0; p--) {
		if (path[p] == '\\') {
			ret = path + p + 1;
			break;
		}
	}
	return ret;
}

LPVOID get_module_base_addr(LPCWSTR module_name) {
	HANDLE process = GetCurrentProcess();
	HMODULE module_list[1024] = { 0 };
	DWORD receviced = 0;
	LPVOID ret = NULL;
	if (EnumProcessModules(process, module_list, sizeof(module_list), &receviced)) {
		DWORD list_size = receviced / sizeof(HMODULE);
		for (DWORD i = 0; i < list_size; i++) {
			TCHAR tmp_module_name[512];
			ZeroMemory(tmp_module_name, sizeof(tmp_module_name));
			if (GetModuleBaseNameW(process, module_list[i], tmp_module_name, 512)) {
				LPCWSTR file_name = get_file_name_in_path(tmp_module_name);
				//OutputDebugString(file_name);
				if (wcscmp(file_name, module_name) == 0) {
					ret = module_list[i];
					break;
				}
			}
		}
	}
	return ret;
}

LPVOID resolve_addr_in_offset_mem_instruction(LPVOID instruction_addr,UINT32 instruction_len, UINT32 offset_start) {
	INT32 offset = *(INT32*)((UINT_PTR)instruction_addr + offset_start);
	LPVOID next_instruction = (LPVOID)((UINT_PTR)instruction_addr + instruction_len);
	LPVOID target_addr = (LPVOID)((INT_PTR)next_instruction + (INT_PTR)offset);
	return target_addr;
}

VOID LogWithInfo(const char* file, int line, const WCHAR* format, ...) {
	va_list args;
	va_start(args, format);

	// Buffer to hold the log message
	WCHAR buffer[256] = { 0 };

	// Create a format string that includes the file name and line number
	WCHAR logFormat[256] = { 0 };
	swprintf_s(logFormat, 256, L"[prefix:%S:%d] %s\n", file, line, format);

	// Format the log message
	vswprintf_s(buffer, 256, logFormat, args);

	// Output the log message
	OutputDebugString(buffer);
	va_end(args);
}
VOID LogWithInfoA(const char* file, int line, const char* format, ...) {
	va_list args;
	va_start(args, format);

	// Buffer to hold the log message
	char buffer[256] = { 0 };

	// Create a format string that includes the file name and line number
	char logFormat[256] = { 0 };
	sprintf_s(logFormat, 256, "[prefix:%s:%d] %s\n", file, line, format);
	// Format the log message
	vsprintf_s(buffer, 256, logFormat, args);

	// Output the log message
	OutputDebugStringA(buffer);
	va_end(args);
}