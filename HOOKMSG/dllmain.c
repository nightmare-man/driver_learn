// dllmain.cpp : 定义 DLL 应用程序的入口点。



#include "pch.h"
#include <stdio.h>
HWND target_wind;
__declspec(dllexport) LRESULT  CALLBACK myfunc(int code, WPARAM wParam, LPARAM lParam) {

    if (code >= 0) {
        BOOLEAN up = (((ULONG32)lParam) >> 31);
        if (up) {
            OutputDebugString("up\n");
            SendMessage(target_wind, WM_KEYUP, wParam, lParam);
        }
        else {
            SendMessage(target_wind, WM_KEYDOWN, wParam, lParam);
        }
        
    }
    return CallNextHookEx(NULL, code, wParam, lParam);
}

__declspec(dllexport) LRESULT  CALLBACK myfunc_mouse(int code, WPARAM wParam, LPARAM lParam) {

    if (code >= 0) {
        MOUSEHOOKSTRUCT* pMouseStruct = (MOUSEHOOKSTRUCT*)lParam;

       
            SendMessage(target_wind, wParam, wParam, lParam);
        

    }
    return CallNextHookEx(NULL, code, wParam, lParam);
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        target_wind = FindWindow(NULL, L"魔兽世界");
        break;
    }
    
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

