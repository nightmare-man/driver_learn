#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <iostream>
#include <set>
#include <psapi.h>
#include <stdio.h>
#include <wchar.h>
using namespace std;
const TCHAR* get_file_name_from_path(const TCHAR* path) {
    ULONG64 path_len = wcsnlen_s(path, 300);
    const TCHAR* ret = NULL;
    for (ULONG64 i = path_len - 1; i >= 0; i--) {
        if (path[i] == L'\\') {
            ret = path + i+1;
            break;
        }
    }
    return ret;
}

BOOLEAN is_same_string(const TCHAR* str1, const TCHAR* str2) {
    ULONG64 len1 = wcsnlen_s(str1, 300);
    ULONG64 len2 = wcsnlen_s(str2, 300);
    if (len1 != len2) return FALSE;
    BOOLEAN ret = TRUE;
    for (ULONG64 i = 0; i < len1; i++) {
        if (CharLowerW((LPWSTR)str1[i]) != CharLowerW((LPWSTR)str2[i])) {
            ret = FALSE;
            break;
        }
    }
    return ret;
}

DWORD_PTR GetModuleBaseAddress(HANDLE target_proc, const TCHAR* target_module) {
    DWORD_PTR dwModuleBaseAddress = 0;
    HMODULE hMods[1024];
    DWORD cbNeeded;
    unsigned int i;

  
    if (EnumProcessModules(target_proc, hMods, sizeof(hMods), &cbNeeded)) {
        for (i = 0; i < (cbNeeded / sizeof(HMODULE)); i++) {
            TCHAR szModName[MAX_PATH];
            if (GetModuleFileNameEx(target_proc, hMods[i], szModName, sizeof(szModName) / sizeof(TCHAR))) {
                
                const TCHAR* file_name = get_file_name_from_path(szModName);
                if (!file_name) continue;
                if (is_same_string(file_name, target_module)) {
                    dwModuleBaseAddress = (DWORD_PTR)hMods[i];
                    wprintf(L"find target module, name is %s\n", szModName);
                    break;
                }
            }
        }
    }
    
    
    return dwModuleBaseAddress;
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

ULONG64 read_process_u64(HANDLE target_proc,ULONG64 target_addr) {
    ULONG64 ret = 0;
    if (!ReadProcessMemory(target_proc, (LPCVOID)target_addr, &ret, sizeof(ret), NULL)) {
        printf("read mem fail\n");
    }
    return ret;
}

ULONG32 read_process_u32(HANDLE target_proc, ULONG64 target_addr) {
    ULONG64 tmp = read_process_u64(target_proc, target_addr);
    ULONG32 ret = *(ULONG32*)(&tmp);
    return ret;
}

USHORT read_process_u16(HANDLE target_proc, ULONG64 target_addr) {
    ULONG64 tmp = read_process_u64(target_proc, target_addr);
    USHORT ret = *(USHORT*)(&tmp);
    return ret;
}

FLOAT read_process_float(HANDLE target_proc, ULONG64 target_addr) {
    ULONG64 tmp = read_process_u64(target_proc, target_addr);
    FLOAT ret = *(FLOAT*)(&tmp);
    return ret;
}




int main() {
    HWND hwnd = FindWindow(NULL, L"Counter-Strike 2");
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
    const WCHAR* target_module_name = L"client.dll";
   
    ULONG64 client_base = (ULONG64)GetModuleBaseAddress(target_proc,target_module_name);
    printf("base client is 0x%p\n", client_base);
   /*
    ULONG64 table_base_addr = read_process_u64(target_proc, client_base + 0x18183e8);
    for (ULONG64 start_idx = 0; start_idx < 0xffff; start_idx++) {
        ULONG64 player_addr = read_process_u64(target_proc, table_base_addr + start_idx * 32);
        ULONG64 player_vf_addr = read_process_u64(target_proc, player_addr);
        ULONG64 func_addr = read_process_u64(target_proc, player_vf_addr + 800);
        ULONG64 player_pos1 = read_process_u64(target_proc, player_addr + 0x318);
        FLOAT player_pos2 = read_process_float(target_proc, player_pos1 + 0x268);
        if (func_addr - client_base == 0x8496d0) {
            printf("[%u] player addr is [0x%p],pos is [%f],func addr is [0x%p]\n", start_idx, player_addr, player_pos2, func_addr);
        }
    }
*/
    set<ULONG64> set1;
    
    while (1) {
        ULONG64 table_base_addr = read_process_u64(target_proc, client_base + 0x18183e8);
        USHORT start_idx = read_process_u16(target_proc, client_base + 0x18183f8);
        USHORT end_idx = read_process_u16(target_proc, client_base + 0x18183fc);
        while (start_idx != USHRT_MAX) {
            ULONG64 player_addr = read_process_u64(target_proc, table_base_addr + start_idx * 32);
            ULONG64 player_vf_addr = read_process_u64(target_proc, player_addr);
            ULONG64 func_addr = read_process_u64(target_proc, player_vf_addr + 800); 
            if (func_addr - client_base == 0x8496d0) {
                set1.insert(player_addr);
                printf("now set has %u player\n", set1.size());
                if (set1.size() == 9) {
                    goto break_here;
                }
            }
            table_base_addr = read_process_u64(target_proc, client_base + 0x18183e8);
            start_idx = read_process_u16(target_proc, table_base_addr + start_idx * 32 + 26);
            
        }
       
    }
 break_here:
    for (const ULONG64& player_addr : set1) {
        ULONG64 pos1 = read_process_u64(target_proc, player_addr+0x318);
        FLOAT p_x = read_process_float(target_proc, pos1 + 0x7e4);
        FLOAT p_y = read_process_float(target_proc, pos1 + 0x7e4 +4);
        FLOAT p_z = read_process_float(target_proc, pos1 + 0x7e4 +8);
        ULONG32 p_hp = read_process_u32(target_proc, player_addr + 0x324);
        printf("[0x%p] [%u] [%f,%f,%f]\n", player_addr, p_hp, p_x, p_y, p_z);
    }
    return 0;
}
