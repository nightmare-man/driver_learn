#include <stdio.h>
#include <windows.h>

int main() {
	HANDLE dev = CreateFile(L"\\\\.\\VMMNEW", GENERIC_WRITE, FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
	if (dev== INVALID_HANDLE_VALUE) {
		printf("open dev fail\n");
		return -1;
	}
	system("pause");
	CloseHandle(dev);
	return 0;
}