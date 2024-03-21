#include <stdio.h>
#include <windows.h>
#include "common.h"
void display_info(BYTE* buffer, DWORD buffer_size) {
	DWORD readed_size = 0;
	while (readed_size<buffer_size) {
		struct ProcessExitInfo* item = (struct ProcessExitInfo*)buffer;
		if (item->header.type == ProcessExit) {
			printf("[EXIT]:\t");
			printf("%lu\n", item->process_id);
			//printf("%lu\n", (unsigned long)item->header.time);
		}
		else if(item->header.type==ProcessCreate){
			struct ProcessCreateInfo* item1 = (struct ProcessCreateInfo*)buffer;
			printf("[CREATE]:\t");
			printf("%lu\t", item1->process_id);
			//printf("%lu\t", (unsigned long)item1->header.time);
			wprintf(L"%s\n", (unsigned short*)((unsigned char*)(item1)+item1->cmd_line_offset));
		}
		else {
			printf("bad buffer\n");
			break;
		}
		buffer += item->header.size;
		readed_size += item->header.size;
	}
}

int main() {
	HANDLE dev = CreateFile(L"\\\\.\\SysMon", GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (!dev) {
		printf("open dev fail\n");
		return -1;
	}
	BYTE buffer[1 << 16];//64kb
	while (TRUE) {
		DWORD readed = 0;
		BOOLEAN ret =ReadFile(dev, buffer, sizeof(buffer), &readed, NULL);
		if (!ret) {
			printf("read dev fail\n");
			return -1;
		}
		//printf("read bytes %d\n", readed);
		if (readed!=0) {
			display_info(buffer, readed);
		}
		Sleep(2000);
	}

	return 0;
}