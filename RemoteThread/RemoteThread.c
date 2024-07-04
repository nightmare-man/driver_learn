#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>


typedef HMODULE(WINAPI* FuncLoadLibraryW)(LPCWSTR);
typedef FARPROC(WINAPI* FuncGetProcAddress)(HMODULE, LPCSTR);
typedef int(WINAPI* FuncMessageBoxW)(HWND, LPCWSTR, LPCWSTR, UINT);
struct thread_param {
    LPVOID func1;
    WCHAR msg[20];
};

DWORD WINAPI my_func(LPVOID param) {
    struct thread_param* p = (struct thread_param*)param;
    HMODULE(WINAPI * f_load_library)(LPCWSTR);
    FARPROC(__stdcall * f_get_proc_addr)(HMODULE, LPCSTR);
    f_load_library = (HMODULE(WINAPI*)(LPCWSTR))p->func1;
    f_get_proc_addr = (FARPROC(__stdcall*)(HMODULE, LPCSTR))p->func2;

    // Debug prints
   

    HMODULE module = f_load_library(p->para1);
  
    /*
    FARPROC message_box = f_get_proc_addr(module, (LPCSTR)p->para2);
  

    int (WINAPI * message_box1)(HWND, LPCWSTR, LPCWSTR, UINT) = (int (WINAPI*)(HWND, LPCWSTR, LPCWSTR, UINT))message_box;
    message_box1(NULL, p->para3, p->para3, MB_OK);
    */
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

int main() {
    HWND hwnd = FindWindow(NULL, L"新建文本文档.txt - 记事本");
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
    param.func1 = (LPVOID)GetProcAddress(GetModuleHandle(L"kernel32.dll"), "LoadLibraryW");
    if (!param.func1) {
        printf("fail to find\n");
    }
    wcscpy(param.msg, L"imgui_hook.dll");

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
    if (!WriteProcessMemory(target_proc, func_remote,my_func , 512, NULL)) {
        printf("write remote func mem fail\n");
        return -1;
    }
    //远程线程代码和参数都要通过分配内存和写入，才能使用
    HANDLE hThread = CreateRemoteThread(target_proc, NULL, 4096, func_remote, param_remote, 0, NULL);
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
