#include <windows.h>
#include <stdio.h>
#include "hook.h"
#include "tool.h"
#include <unordered_map>


#define JMP_CODE_LEN 5
typedef struct _hook_record {
	char origin_star_code[JMP_CODE_LEN];
	UINT_PTR virtual_alloc_addr;
}hook_record,*p_hook_record;
std::unordered_map<INT_PTR, p_hook_record> hook_map;

LPVOID hook_func(LPVOID old_func, LPVOID new_func) {
	
	UINT_PTR tmp1 = 0, tmp2 = 0;
	UINT_PTR ret_addr = ((UINT_PTR)old_func - 0x2000);
	LPVOID tmp_ret = NULL;
	while (!tmp_ret) {
		tmp_ret = VirtualAlloc((LPVOID)ret_addr, 32, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
		ret_addr += 0x200;
	}
	ret_addr = (UINT_PTR)tmp_ret;
	
	tmp1 = ret_addr;
	memcpy_s((LPVOID)ret_addr, JMP_CODE_LEN, old_func, JMP_CODE_LEN);
	ret_addr += JMP_CODE_LEN;
	*((unsigned char*)ret_addr) = 0xe9;//jmp rel 32
	ret_addr++;
	*((INT32*)(ret_addr)) = (INT32)(((UINT_PTR)old_func+5)-(ret_addr+4));//jmp�������ת�����jmp����һ����ַ����
	ret_addr += 4;

	tmp2 = ret_addr;
	//����һ�������ת����������ת�Ƚ��ر𣬵�һ���ĵ�ַ��offset��ʽ�����ģ��൱��һ���Ǽ�ӣ��ڶ�����ֱ��
#ifdef _WIN64
	unsigned char tmp_jmp_code[] = { 0xff, 0x25, 0x00,0x00,0x00,0x00 };
	memcpy_s((LPVOID)ret_addr, sizeof(tmp_jmp_code), tmp_jmp_code, sizeof(tmp_jmp_code));
	//��������ĵ�һ����ַ�����rip offset0�ĵ�ַ��Ҳ���ǽ������ĵ�ַ
	//���������д������ת�ĵ�ַ
	ret_addr += sizeof(tmp_jmp_code);
	*((UINT_PTR*)ret_addr) = (UINT_PTR)new_func;
#else
	//32λ���������ת
	* ((unsigned char*)ret_addr) = 0xe9;
	ret_addr++;
	*((INT32*)ret_addr) = (INT32)((UINT_PTR)new_func - (ret_addr + 4));
#endif

	
	//ԭ����ҳ������б���
	DWORD old_pro;
	if (!VirtualProtect(old_func, JMP_CODE_LEN, PAGE_EXECUTE_READWRITE, &old_pro)) {
		return NULL;
	}

	suspend_or_resume_other_threads(TRUE);
	* ((unsigned char*)old_func) = 0xe9;
	*((INT32*)((UINT_PTR)old_func + 1)) = (INT32)(tmp2 - ((UINT_PTR)old_func+5));
	suspend_or_resume_other_threads(FALSE);
	VirtualProtect(old_func, JMP_CODE_LEN, old_pro, NULL);


	p_hook_record new_record = (p_hook_record)malloc(sizeof(hook_record));
	hook_map[(UINT_PTR)old_func] = new_record;
	new_record->virtual_alloc_addr = tmp1;
	memcpy_s(new_record->origin_star_code, JMP_CODE_LEN, (LPVOID)tmp1, JMP_CODE_LEN);
	return (LPVOID)tmp1;
}
LPVOID reset_func(LPVOID old_func) {
	if (hook_map.find((UINT_PTR)old_func) == hook_map.end()) return NULL;
	p_hook_record target_record = hook_map[(UINT_PTR)old_func];
	suspend_or_resume_other_threads(TRUE);
	memcpy_s(old_func, JMP_CODE_LEN, target_record->origin_star_code, JMP_CODE_LEN);
	suspend_or_resume_other_threads(FALSE);
	VirtualFree((LPVOID)target_record->virtual_alloc_addr, 32, MEM_DECOMMIT);
	hook_map.erase((UINT_PTR)old_func);
	free(target_record);
	return old_func;
}