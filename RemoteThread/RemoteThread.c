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

DWORD WINAPI remote_func(LPVOID param) {
    struct thread_param* p = (struct thread_param*)param;
    FuncLoadLibraryW load = (FuncLoadLibraryW)p->func1;
    load(p->msg);
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
   
    wcscpy_s(param.msg, 20,L"hookme5.dll");
  
    LPVOID param_addr_in_remote = VirtualAllocEx(target_proc, NULL, sizeof(struct thread_param), MEM_COMMIT, PAGE_READWRITE);
    LPVOID func_addr_in_remote = VirtualAllocEx(target_proc, NULL,4096, MEM_COMMIT|MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if ((!param_addr_in_remote) || (!func_addr_in_remote)) {
        printf("allocate remote mem fail\n");
        return -1;
    }
    if (!WriteProcessMemory(target_proc, param_addr_in_remote, &param, sizeof(param), NULL)) {
        printf("write remote mem fail\n");
        return -1;
    }

    if (!WriteProcessMemory(target_proc, func_addr_in_remote, &remote_func,4096, NULL)) {
        printf("write remote mem fail\n");
        return -1;
    }
  
    printf("remote param addr is 0x%p\n", param_addr_in_remote);
    printf("remote func addr is 0x%p\n", func_addr_in_remote);
    //远程线程代码和参数都要通过分配内存和写入，才能使用
    DWORD thread_id;
    HANDLE hThread = CreateRemoteThread(target_proc, NULL, 4096, (LPTHREAD_START_ROUTINE)func_addr_in_remote, param_addr_in_remote, 0, &thread_id);
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
