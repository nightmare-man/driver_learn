// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"
#include "tool.h"
// MyCFuncs.h
#ifdef __cplusplus
extern "C" {  // only need to export C interface if
    // used by C++ source code
#endif

    __declspec(dllimport) int inject_dll_to_window(const char* windows_name, const char* dll_name);
    __declspec(dllimport) int find_window(const char* windows_name);
   
#ifdef __cplusplus
}
#endif


typedef HMODULE(WINAPI* FuncLoadLibraryA)(LPCSTR);
typedef HMODULE(WINAPI* FuncGetModuleA)(LPCSTR);
typedef BOOL(WINAPI* FuncFreeModule)(HMODULE);

struct thread_param {
    LPVOID func1;
    LPVOID func2;
    CHAR msg[40];
};

DWORD WINAPI inject(LPVOID param) {
    struct thread_param* p = (struct thread_param*)param;
    FuncLoadLibraryA f_load_lib = (FuncLoadLibraryA)(p->func1);
    HMODULE module = f_load_lib(p->msg);
    return 0;
}

DWORD WINAPI free_module(LPVOID param) {
    struct thread_param* p = (struct thread_param*)param;
    FuncGetModuleA f_get_module = (FuncGetModuleA)(p->func1);
    FuncFreeModule f_free_lib = (FuncFreeModule)p->func2;
    HMODULE target = f_get_module(p->msg);
    f_free_lib(target);
    return 0;
}
BOOLEAN adjust_privilege(HANDLE token) {
    LUID luid;
    if (LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid)) {
        TOKEN_PRIVILEGES tp;
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        if (AdjustTokenPrivileges(token, FALSE, &tp, sizeof(tp), NULL, NULL)) {
            return TRUE;
        }
        else {
            return FALSE;
        }
    }
    return FALSE;
}

int inject_dll_to_window(const char* windows_name, const char* dll_name) {
    HWND hwnd = FindWindowA(NULL,windows_name);
    if (!hwnd) {
        LogA(" ");
        return -1;
    }
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);

    HANDLE token;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &token)) {
        adjust_privilege(token);
        CloseHandle(token);
    }
    else {
        LogA(" ");
        return -1;
    }
    HANDLE target_proc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!target_proc) {
   
        LogA(" ");
        return -1;
    }

    struct thread_param param;
    
    HMODULE target_module = GetModuleHandleA("kernel32.dll");
    if (!target_module) {
        LogA(" ");
        return -1;
    }
    LPVOID target_func_a = (LPVOID)GetProcAddress(target_module, "LoadLibraryA");
    if (!target_func_a) {
        LogA(" ");
        return -1;
    }
    param.func1 = target_func_a;
    memcpy_s(param.msg,40,dll_name, strnlen_s(dll_name,100));

    LPVOID param_remote = VirtualAllocEx(target_proc, NULL, sizeof(param), MEM_COMMIT, PAGE_READWRITE);
    if (!param_remote) {
        LogA(" ");
        return -1;
    }
    if (!WriteProcessMemory(target_proc, param_remote, &param, sizeof(param), NULL)) {
        
        LogA(" ");
        return -1;
    }

    LPVOID func_remote = VirtualAllocEx(target_proc, NULL, 512, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!func_remote) {
        LogA(" ");
        return -1;
    }
    
    if (!WriteProcessMemory(target_proc, func_remote, inject, 512, NULL)) {
        LogA(" ");
        return -1;
    }
    
    //远程线程代码和参数都要通过分配内存和写入，才能使用
    HANDLE hThread = CreateRemoteThread(target_proc, NULL, 1024 * 1024, (LPTHREAD_START_ROUTINE)func_remote, param_remote, 0, NULL);
    if (!hThread) {
        LogA(" ");
        return -1;
    }
    WaitForSingleObject(hThread, INFINITE);
    return 0;
}

int find_window(const char* windows_name) {
    HWND hwnd = FindWindowA(NULL, windows_name);
    if (hwnd) return 0;
    return -1;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

