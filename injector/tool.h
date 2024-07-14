#pragma once
#include "pch.h"


VOID LogWithInfo(const char* file, int line, const WCHAR* format, ...);
VOID LogWithInfoA(const char* file, int line, const char* foramt, ...);
#define Log(format, ...) LogWithInfo(__FILE__, __LINE__, format, __VA_ARGS__)
#define LogA(format, ...) LogWithInfoA(__FILE__, __LINE__, format, __VA_ARGS__)
