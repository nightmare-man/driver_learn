#include "tool.h"
#include "tlhelp32.h"
BOOLEAN suspend_or_resume_other_threads(BOOLEAN suspend) {
	DWORD current_tid = GetCurrentThreadId();
	DWORD current_pid = GetCurrentProcessId();
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (!hSnap) return FALSE;
	THREADENTRY32 te;
	te.dwSize = sizeof(te);
	if (Thread32First(hSnap, &te)) {
		while (1) {
			if (te.th32OwnerProcessID == current_pid && te.th32ThreadID != current_tid) {
				HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
				if (!hThread) {
					OutputDebugString(L"fail to open one thread");
					continue;
				}
				if (suspend) {

#ifdef _WIN64
					if (Wow64SuspendThread(hThread) == -1) {
						OutputDebugString(L"suspend thread fail\n");
					}
#else
					if (SuspendThread(hThread) == -1) {
						OutputDebugString(L"suspend thread fail\n");
					}
#endif
					OutputDebugString(L"suspend thread success\n");


				}
				else {
					if (ResumeThread(hThread) == -1) {
						OutputDebugString(L"resume thread fail\n");
					}

					OutputDebugString(L"resume thread success\n");
				}

			}
			if (!Thread32Next(hSnap, &te)) {
				break;
			}
		}
	}
	else {
		return FALSE;
	}
	return TRUE;
}