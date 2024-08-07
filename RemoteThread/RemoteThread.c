﻿
#include <windows.h>
#include <stdio.h>


typedef HMODULE(WINAPI* FuncLoadLibraryW)(LPCWSTR);
typedef HMODULE(WINAPI* FuncGetModuleW)(LPCWSTR);
typedef BOOL(WINAPI* FuncFreeModule)(HMODULE);



struct thread_param {
    LPVOID func1;
    LPVOID func2;
    WCHAR msg[40];
};

DWORD WINAPI inject(LPVOID param) {
    struct thread_param* p = (struct thread_param*)param;
    FuncLoadLibraryW f_load_lib = (FuncLoadLibraryW)(p->func1);
    HMODULE module = f_load_lib(p->msg);
    return 0;
}

DWORD WINAPI free_module(LPVOID param) {
    struct thread_param* p= (struct thread_param*)param;
    FuncGetModuleW f_get_module = (FuncGetModuleW)(p->func1);
    FuncFreeModule f_free_lib = (FuncFreeModule)p->func2;
    HMODULE target = f_get_module(p->msg);
    f_free_lib(target);
    return 0;
}
void adjust_privilege(HANDLE token) {
    LUID luid;
    if (LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid)) {
        TOKEN_PRIVILEGES tp;
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        if (AdjustTokenPrivileges(token, FALSE, &tp, sizeof(tp), NULL, NULL)) {
            printf("access debug priviledges success\n");
            return;
        }
    }
    printf("access debug priviledges fail\n");
}

int main(int argc,char* argv[]) {
    printf("argc is %d\n", argc);
    if (argc < 2) {
        printf("param invalid\n");
        return -1;
    }
    HWND hwnd = FindWindowA(NULL, argv[1]);
    if (!hwnd) {
        printf("找不到窗口\n");
        return -1;
    }
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
   
    HANDLE token;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &token)) {
        printf("open token success\n");
        adjust_privilege(token);
        CloseHandle(token);
    }
    else {
        printf("open token fail\n");
    }

    HANDLE target_proc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (target_proc) {
        printf("open proc success\n");
    }
    else {
        printf("last error %d\n", GetLastError());
        return -1;
    }

    
    struct thread_param param;
    if (argc >= 3) {
      
        param.func1 = (LPVOID)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetModuleHandleW");
        param.func2 = (LPVOID)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "FreeLibrary");
    }
    else {
        param.func1 = (LPVOID)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
    }
   
    if (!param.func1) {
        printf("fail to find\n");
    }
    wcscpy_s(param.msg, 40, L"imgui_hook.dll");
  
    LPVOID param_remote = VirtualAllocEx(target_proc, NULL, sizeof(param), MEM_COMMIT, PAGE_READWRITE);
    if (!param_remote) {
        printf("allocate remote param mem fail\n");
        return -1;
    }
    if (!WriteProcessMemory(target_proc, param_remote, &param, sizeof(param), NULL)) {
        printf("write remote param mem fail\n");
        return -1;
    }

    LPVOID func_remote = VirtualAllocEx(target_proc, NULL, 512, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!func_remote) {
        printf("allocate remote func mem fail\n");
        return -1;
    }
    if (argc >= 3) {
        if (!WriteProcessMemory(target_proc, func_remote, free_module, 512, NULL)) {
            printf("write remote func mem fail\n");
            return -1;
        }
    }
    else {
        if (!WriteProcessMemory(target_proc, func_remote, inject, 512, NULL)) {
            printf("write remote func mem fail\n");
            return -1;
        }
    }
    
    //远程线程代码和参数都要通过分配内存和写入，才能使用
    HANDLE hThread = CreateRemoteThread(target_proc, NULL, 1024*1024, func_remote, param_remote, 0, NULL);
    if (!hThread) {
        printf("create remote thread fail: %d\n", GetLastError());
        return -1;
    }
    else {
        printf("remote thread created successfully\n");
    }
    WaitForSingleObject(hThread, INFINITE);
    
    return 0;
}
