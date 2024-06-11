#include <windows.h>
#include <stdio.h>

DWORD WINAPI MyFunc(
	LPVOID lpThreadParameter
) {
	int a = 1;
	while (1) {
		//printf("12\n");
		a++;
	}
}

void adjust_priviledge(HANDLE token) {
	LUID luid;
	if (LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid)) {
		TOKEN_PRIVILEGES tp;
		tp.PrivilegeCount = 1;
		tp.Privileges[0].Luid = luid;
		tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
		if (AdjustTokenPrivileges(token, FALSE, &tp, sizeof(tp), NULL, NULL)) {
			printf("debug sucess\n");
			return;
		}
	}
	printf("debug fail\n");
}

int main() {
	HANDLE token=GetCurrentProcessToken();
	DWORD pid=1;
	scanf_s("%d", &pid);
	
	if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &token)) {
		printf("open token sucess\n");
		adjust_priviledge(token);
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
	if (CreateRemoteThread(target_proc, NULL, 1024, NULL, NULL, 0, NULL)) {
		printf("inject sucess\n");
	}
	else {
		printf("last error %d\n", GetLastError());
	}
	return 0;
}