#include <stdio.h>
#include <windows.h>

// 鼠标钩子的句柄
HHOOK hMouseHook;

// 主窗口的句柄
HWND hwnd_main;

// 鼠标钩子的回调函数
LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        // 获取鼠标消息
        MOUSEHOOKSTRUCT* pMouseStruct = (MOUSEHOOKSTRUCT*)lParam;
     
        // 检查鼠标消息是否来自主窗口
        if (pMouseStruct->hwnd == hwnd_main ) {
            // 遍历所有的窗口，并在这些窗口上模拟鼠标点击操作
            printf("main window click\n");
        }
    }

    // 调用下一个钩子
    return CallNextHookEx(hMouseHook, nCode, wParam, lParam);
}



int main() {
    // 获取主窗口的句柄
    hwnd_main = FindWindow(NULL, L"新建文本文档.txt - 记事本");

    // 安装鼠标钩子
    hMouseHook = SetWindowsHookEx(WH_MOUSE, MouseProc, GetModuleHandle(NULL), 0);
    printf("%d\n", GetLastError());
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 卸载鼠标钩子
    UnhookWindowsHookEx(hMouseHook);

    return 0;
}
