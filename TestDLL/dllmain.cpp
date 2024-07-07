// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"
DWORD WINAPI my_func(LPVOID p) {
    while (1) {
        OutputDebugString(L"running\n");
        Sleep(1000);
    }
    return 0;
}
BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH: {
        CreateThread(NULL, 8192, my_func, NULL, NULL, NULL);
        break;
        }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

