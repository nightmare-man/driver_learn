#include <windows.h>
#include <stdio.h>
#include "hook.h"
#define JMP_CODE_LEN 5
static unsigned char origin_start_code[JMP_CODE_LEN];
LPVOID hook_func(LPVOID old_func, LPVOID new_func) {
	UINT_PTR tmp1 = 0, tmp2 = 0;
	UINT_PTR ret_addr = ((UINT_PTR)old_func - 0x2000);
	LPVOID tmp_ret = NULL;
	while (!tmp_ret) {
		tmp_ret = VirtualAlloc((LPVOID)ret_addr, 32, MEM_COMMIT|MEM_RESERVE, PAGE_EXECUTE_READWRITE);
		ret_addr += 0x200;
	}
	ret_addr = (UINT_PTR)tmp_ret;
	

	
	
	tmp1 = ret_addr;
	memcpy_s(origin_start_code, JMP_CODE_LEN, old_func, JMP_CODE_LEN);
	memcpy_s((LPVOID)ret_addr, JMP_CODE_LEN, origin_start_code, JMP_CODE_LEN);
	ret_addr += JMP_CODE_LEN;
	*((unsigned char*)ret_addr) = 0xe9;//jmp rel 32
	ret_addr++;
	*((INT32*)(ret_addr)) = (INT32)(((UINT_PTR)old_func+5)-(ret_addr+4));//jmp的相对跳转是相对jmp的下一条地址而言
	ret_addr += 4;

	tmp2 = ret_addr;
	//再来一个间接跳转，这个间接跳转比较特别，第一级的地址是offset形式给出的，相当第一级是间接，第二级是直接
#ifdef _WIN64
	unsigned char tmp_jmp_code[] = { 0xff, 0x25, 0x00,0x00,0x00,0x00 };
	memcpy_s((LPVOID)ret_addr, sizeof(tmp_jmp_code), tmp_jmp_code, sizeof(tmp_jmp_code));
	//这里给出的第一级地址是相对rip offset0的地址，也就是接下来的地址
	//因此往这里写入想跳转的地址
	ret_addr += sizeof(tmp_jmp_code);
	*((UINT_PTR*)ret_addr) = (UINT_PTR)new_func;
#else
	//32位下用相对跳转
	* ((unsigned char*)ret_addr) = 0xe9;
	ret_addr++;
	*((INT32*)ret_addr) = (INT32)((UINT_PTR)new_func - (ret_addr + 4));
#endif

	//原来的页面估计有保护
	DWORD old_pro;
	if (!VirtualProtect(old_func, JMP_CODE_LEN, PAGE_EXECUTE_READWRITE, &old_pro)) {
		return NULL;
	}
	* ((unsigned char*)old_func) = 0xe9;
	*((INT32*)((UINT_PTR)old_func + 1)) = (INT32)(tmp2 - ((UINT_PTR)old_func+5));
	VirtualProtect(old_func, JMP_CODE_LEN, old_pro, NULL);
	return (LPVOID)tmp1;
}