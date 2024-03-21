#include <stdio.h>
#include <windows.h>
#include <psapi.h>
DWORD pid_array[1024];
WCHAR image_name[1024];
int main(int argc, char*argv[]) {
	DWORD readed;
	EnumProcesses(pid_array, sizeof(pid_array), &readed);
	if (readed == 0) {
		printf("can't read process list\n");
		return -1;
	}
	DWORD pid_count = readed / (sizeof(DWORD));
	for (int i = 0; i < pid_count; i++) {
		DWORD current_pid = pid_array[i];
		printf("[pid]:%08d\t", current_pid);
		HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, current_pid);
		if (!process) {
			printf("open fail\n");
			continue;
		}
		
		DWORD ret = GetModuleBaseName(process, NULL, image_name, 1024);
		if (!ret) {
			printf("can't get image name\n");
			CloseHandle(process);
			continue;
		}
		wprintf(L"%s\n", image_name);

	}
	return 0;
}