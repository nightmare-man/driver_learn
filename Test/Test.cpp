// Test.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
#include <windows.h>
#include <stdio.h>

#define OFFSET_HANDLE_RESOLVE 0x1a12f10
UINT_PTR g_module_client_addr = 0x7FF855AA0000;
UINT_PTR resolve_hanle(UINT32 handle_idx) {
	if (handle_idx > 0x7ffe) return NULL;
	UINT32 tmp1 = handle_idx >> 9;
	if (tmp1 > 0x3f) return NULL;
	UINT_PTR a1 = *(UINT_PTR*)((UINT_PTR)g_module_client_addr + OFFSET_HANDLE_RESOLVE);
	UINT_PTR v2 = *(UINT_PTR*)(a1 + 8 * tmp1 + 16);
	if (v2 == 0) return NULL;
	UINT_PTR v3 = 120 * (handle_idx & 0x1ff) + v2;
	if (v3 == 0) return NULL;
	UINT32 tmp2 = *(UINT32*)(v3 + 16);
	if (tmp2 != handle_idx) return NULL;
	UINT_PTR ret = *(UINT_PTR*)v3;
	return ret;
}
int main()
{
   
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
