#include <stdio.h>
#include <windows.h>

int  main() {
	HANDLE device = CreateFile(L"\\\\.\\ZeroBuffer", GENERIC_WRITE | GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (!device) {
		printf("can't open device\n");
		return -1;
	}

	BYTE buffer[64];
	for (int i = 0; i < sizeof(buffer); i++) {
		buffer[i] = i + 1;
	}
	DWORD readed = 0;
	BOOL ok = ReadFile(device, buffer, 64, &readed, NULL);
	if (!ok) {
		printf("read buffer fail\n");
		return -1;
	}
	long sum = 0;
	for (int i = 0; i < sizeof(buffer); i++) {
		sum += buffer[i];
	}
	if (sum != 0) {
		printf("wrong data by driver read\n");
		return -1;
	}
	BYTE buffer1[64];
	DWORD writed = 0;
	ok = WriteFile(device, buffer1, sizeof(buffer1), &writed, NULL);
	if (!ok) {
		printf("write buffer fail\n");
		return -1;
	}
	if (writed != sizeof(buffer1)) {
		printf("wrong size write by driver write\n");
		return -1;
	}
	CloseHandle(device);
	return 0;
}