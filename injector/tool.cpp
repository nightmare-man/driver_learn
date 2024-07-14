#include "pch.h"
#include "tool.h"
#include <tlhelp32.h>
#include <psapi.h>
#include <stdio.h>
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