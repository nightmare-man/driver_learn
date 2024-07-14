#include "tool.h"
#include <tlhelp32.h>
#include <psapi.h>
#include <stdio.h>
#include <vector>
#include <math.h>
#define my_pi 3.1415926
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

UINT_PTR resolve_addr_in_offset_mem_instruction(UINT_PTR instruction_addr,UINT32 instruction_len, UINT32 offset_start) {
	INT32 offset = *(INT32*)(instruction_addr + offset_start);
	LPVOID next_instruction = (LPVOID)((UINT_PTR)instruction_addr + instruction_len);
	UINT_PTR target_addr = (UINT_PTR)((INT_PTR)next_instruction + (INT_PTR)offset);
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


LPVOID get_func_in_module(LPCWSTR module, LPCSTR func_name) {
	HMODULE m =GetModuleHandleW(module);
	if (!m) return NULL;
	return GetProcAddress(m, func_name);
}


static std::vector<std::uint32_t> pattern_to_byte(const char* pattern)
{
	std::vector<std::uint32_t> bytes;
	char* start = const_cast<char*>(pattern);
	char* end = const_cast<char*>(pattern) + std::strlen(pattern);

	for (char* current = start; current < end; current++)
	{
		if (*current == '?')
		{
			current++;

			if (*current == '?')
			{
				current++;
			}

			bytes.push_back(-1);
		}
		else
		{
			bytes.push_back(std::strtoul(current, &current, 16));
		}
	}

	return bytes;
}

UINT_PTR pattern_scan(const wchar_t* module_name, const char* signature)
{
	HMODULE module_handle = GetModuleHandle(module_name);

	if (!module_handle)
	{
		return NULL;
	}

	PIMAGE_DOS_HEADER dos_header = reinterpret_cast<PIMAGE_DOS_HEADER>(module_handle);
	PIMAGE_NT_HEADERS nt_headers = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<std::uint8_t*>(module_handle) + dos_header->e_lfanew);

	std::size_t size_of_image = nt_headers->OptionalHeader.SizeOfImage;
	std::vector<std::uint32_t> pattern_bytes = pattern_to_byte(signature);
	std::uint8_t* image_base = reinterpret_cast<std::uint8_t*>(module_handle);

	std::size_t pattern_size = pattern_bytes.size();
	std::uint32_t* array_of_bytes = pattern_bytes.data();

	for (std::size_t i = 0; i < size_of_image - pattern_size; i++)
	{
		bool found = true;

		for (std::size_t j = 0; j < pattern_size; j++)
		{
			if (image_base[i + j] != array_of_bytes[j] && array_of_bytes[j] != -1)
			{
				found = false;
				break;
			}
		}

		if (found)
		{
			return (UINT_PTR)(& image_base[i]);
		}
	}

	return NULL;
}
