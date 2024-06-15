#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>

struct thread_param {
    LPVOID func1;
    LPVOID func2;
    WCHAR para1[20];
    WCHAR para2[20];
    WCHAR para3[20];
};

DWORD WINAPI remote_func(LPVOID param) {
    struct thread_param* p = (struct thread_param*)param;
    HMODULE(WINAPI * f_load_library)(LPCWSTR);
    FARPROC(__stdcall * f_get_proc_addr)(HMODULE, LPCSTR);
    f_load_library = (HMODULE(WINAPI*)(LPCWSTR))p->func1;
    f_get_proc_addr = (FARPROC(__stdcall*)(HMODULE, LPCSTR))p->func2;

    // Debug prints
   

    HMODULE module = f_load_library(p->para1);
  

    FARPROC message_box = f_get_proc_addr(module, (LPCSTR)p->para2);
  

    int (WINAPI * message_box1)(HWND, LPCWSTR, LPCWSTR, UINT) = (int (WINAPI*)(HWND, LPCWSTR, LPCWSTR, UINT))message_box;
    message_box1(NULL, p->para3, p->para3, MB_OK);

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
            printf("debug success\n");
            return;
        }
    }
    printf("debug fail\n");
}

int main() {
    OutputDebugString(L"test\n");
    HWND hwnd = FindWindow(NULL, L"");
   
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

    DWORD thread_id;
    struct thread_param param;
    param.func1 = (LPVOID)GetProcAddress(GetModuleHandle(L"kernel32.dll"), "LoadLibraryW");
    param.func2 = (LPVOID)GetProcAddress(GetModuleHandle(L"kernel32.dll"), "GetProcAddress");
    if (!param.func1) {
        printf("fail to find\n");
    }
    if (!param.func2) {
        printf("fail to find\n");
    }
    wcscpy(param.para1, L"user32.dll");
    wcscpy(param.para2, L"MessageBoxW");
    wcscpy(param.para3, L"testbox");

    LPVOID param_remote = VirtualAllocEx(target_proc, NULL, sizeof(struct thread_param), MEM_COMMIT, PAGE_READWRITE);
    if (!param_remote) {
        printf("allocate remote mem fail\n");
        return -1;
    }
    if (!WriteProcessMemory(target_proc, param_remote, &param, sizeof(param), NULL)) {
        printf("write remote mem fail\n");
        return -1;
    }

    //远程线程代码和参数都要通过分配内存和写入，才能使用
    HANDLE hThread = CreateRemoteThread(target_proc, NULL, 4096, remote_func, param_remote, 0, &thread_id);
    if (!hThread) {
        printf("create remote thread fail: %d\n", GetLastError());
        return -1;
    }
    else {
        printf("remote thread created successfully\n");
    }

    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);
    VirtualFreeEx(target_proc, param_remote, 0, MEM_RELEASE);
    CloseHandle(target_proc);
    return 0;
}
