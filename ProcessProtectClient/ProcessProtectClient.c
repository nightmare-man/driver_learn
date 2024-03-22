#include <stdio.h>
#include "common.h"

int main() {
	HANDLE dev = CreateFileW(L"\\\\.\\ProcessProtect", GENERIC_ALL, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	DWORD error_code = GetLastError();
	printf_s("error code 0x%08X", error_code);
	if (!dev) {
		printf("open dev file fail\n");
		return -1;
	}
	ULONG pid = 0;
	ULONG code = 0;
	DWORD writed = 0;
	while (TRUE) {
		scanf_s("%lu", &code);
		scanf_s("%lu", &pid);
		switch (code)
		{
		case (0): {
			BOOL ret = DeviceIoControl(dev, IOCTL_PROCESS_PROTECT_BY_PID, &pid, sizeof(ULONG), NULL, 0, &writed, NULL);
			
			if (!ret) {
				printf("write file fail\n");
				DWORD error_code= GetLastError();
				printf_s("error code 0x%08X", error_code);
				return -1;
			}
			//对于非字符串参数，不用指定长度
			//printf_s("%s", buff_size, buff);
			printf_s("[insert]\t%08lu\n", pid);
			break;
		};
		case(1): {
			BOOL ret = DeviceIoControl(dev, IOCTL_PROCESS_UNPROTECT_BY_PID, &pid, sizeof(ULONG), NULL, 0, &writed, NULL);
			if (!ret) {
				printf("write file fail\n");
				return -1;
			}
			//对于非字符串参数，不用指定长度
			//printf_s("%s", buff_size, buff);
			printf_s("[delete]\t%08lu\n", pid);
			break;
		};
		case (2): {
			BOOL ret = DeviceIoControl(dev, IOCTL_PROCESS_PROTECT_CLEAR, NULL,0, NULL, 0, NULL, NULL);
			if (!ret) {
				printf("write file fail\n");
				return -1;
			}
			//对于非字符串参数，不用指定长度
			//printf_s("%s", buff_size, buff);
			printf_s("[clear]\n");
			break;
		};
		case (3): {
			return 0;
		}
		default:
			break;
		}
	}
}