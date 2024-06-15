// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"
HHOOK ret;
LRESULT CALLBACK myfunc(int code, WPARAM wParam, LPARAM lParam) {
    if (code >= 0) {
        OutputDebugString(L"press a key\n");
        
    }
    return CallNextHookEx(ret, code, wParam, lParam);
}

BOOLEAN NEED_HOOK = TRUE;
VOID HOOK() {
     ret= SetWindowsHookEx(WH_KEYBOARD, myfunc, NULL, GetCurrentThreadId());
    if (!ret) {
        DWORD error_code = GetLastError();
        OutputDebugString(L"set hook fail,%d\n");
    }
    OutputDebugString(L"set hook success,%d\n");
   
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        HOOK();
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

