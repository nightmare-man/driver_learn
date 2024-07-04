#include <stdio.h>
#include <windows.h>
#include <d3d11.h>
#include "hook.h"
#pragma comment(lib, "d3d11.lib")
typedef int(WINAPI* MessageBoxFunc)(HWND, LPCWSTR, LPCWSTR, UINT);
MessageBoxFunc ret_func;

int my_message_box(HWND h, LPCWSTR x1, LPCWSTR x2, UINT t) {
    printf("hooked by dll\n");
    return ret_func(h, x1, x2, t);
}
DWORD WINAPI thread_start(LPVOID param) {
    OutputDebugString(L"dll thread start\n");
    ret_func = (MessageBoxFunc)hook_func(MessageBoxW, my_message_box);
    if (ret_func == NULL) {
        OutputDebugString(L"hook fail\n");
    }
    OutputDebugString(L"dll thread end\n");
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
        CreateThread(NULL, 4096, thread_start, NULL, NULL, NULL);
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

