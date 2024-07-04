// HookWindowMsg.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
#include <stdio.h>
#include <windows.h>
int main()
{
	HWND wind = FindWindow(NULL, L"新建文本文档.txt - 记事本");
	if (!wind) {
		printf("no such window\n");
		return - 1;
	}
	DWORD pid, tid;
	tid = GetWindowThreadProcessId(wind, &pid);
	HMODULE md = LoadLibraryW(L"D:\\proj\\HOOKMSG\\x64\\Release\\hookmsg.dll");
	if (!md) {
		printf("can't load module\n");
		return -1;
	}
	LRESULT(CALLBACK * pfunc)(int, WPARAM, LPARAM);
	LRESULT(CALLBACK * pfunc_mouse)(int, WPARAM, LPARAM); 
	pfunc_mouse= (LRESULT(CALLBACK*)(int, WPARAM, LPARAM))GetProcAddress(md, "myfunc_mouse");
	pfunc = (LRESULT(CALLBACK*)(int, WPARAM, LPARAM))GetProcAddress(md, "myfunc");
	if (!pfunc_mouse) {
		printf("can't find func\n");
		return -1;
	}
	HHOOK h=SetWindowsHookEx(WH_KEYBOARD, pfunc, md, tid);
	h = SetWindowsHookEx(WH_MOUSE, pfunc_mouse, md, tid);
	if (!h) {
		printf("set hook fail,code %d\n", GetLastError());
		return -1;
	}
	printf("success\n");
	MSG msg; 
	while (GetMessage(&msg, 0,0,0)!=0x1234) {
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
	return 0;
}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门使用技巧: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件
