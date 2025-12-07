#include <windows.h>
#include <shellapi.h>

// 定义消息ID
#define WM_TRAY_MSG (WM_USER + 1001)
#define IDM_EXIT    1000
#define IDM_PAUSE   1001
#define IDM_RESUME  1002

// 函数前置声明
BOOL InstallKeyboardHook();
void UninstallKeyboardHook();
void SendWinSpace();
BOOL CreateTrayIcon(HWND hWnd);
void DeleteTrayIcon();
BOOL IsRunningAsAdmin();
BOOL RestartAsAdmin();

// 全局钩子句柄
HHOOK g_hKeyboardHook = NULL;

// 记录按键状态
BOOL g_isCtrlPressed = FALSE;
BOOL g_isAltPressed = FALSE;
BOOL g_isShiftPressed = FALSE;
BOOL g_isSpacePressed = FALSE;

// 程序状态
BOOL g_isPaused = FALSE;

// 窗口相关
HWND g_hWnd = NULL;
NOTIFYICONDATA g_nid;
HMENU g_hPopupMenu = NULL;

// 发送Win+空格组合键
void SendWinSpace() {
    // 模拟按下Win键
    keybd_event(VK_LWIN, 0, 0, 0);
    // 模拟按下空格键
    keybd_event(VK_SPACE, 0, 0, 0);
    // 模拟释放空格键
    keybd_event(VK_SPACE, 0, KEYEVENTF_KEYUP, 0);
    // 模拟释放Win键
    keybd_event(VK_LWIN, 0, KEYEVENTF_KEYUP, 0);
}

// 创建托盘图标
BOOL CreateTrayIcon(HWND hWnd) {
    // 初始化托盘图标数据
    ZeroMemory(&g_nid, sizeof(NOTIFYICONDATA));
    g_nid.cbSize = sizeof(NOTIFYICONDATA);
    g_nid.hWnd = hWnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAY_MSG;
    
    // 设置默认图标（使用系统默认应用程序图标）
    g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    
    // 设置提示文本
    lstrcpy(g_nid.szTip, TEXT("键盘钩子程序"));
    
    // 添加托盘图标
    if (!Shell_NotifyIcon(NIM_ADD, &g_nid)) {
        return FALSE;
    }
    
    // 创建右键菜单
    g_hPopupMenu = CreatePopupMenu();
    if (g_hPopupMenu == NULL) {
        return FALSE;
    }
    
    // 添加菜单项
    AppendMenu(g_hPopupMenu, MF_STRING, IDM_PAUSE, TEXT("暂停"));
    AppendMenu(g_hPopupMenu, MF_STRING, IDM_EXIT, TEXT("退出"));
    
    return TRUE;
}

// 删除托盘图标
void DeleteTrayIcon() {
    Shell_NotifyIcon(NIM_DELETE, &g_nid);
    if (g_hPopupMenu != NULL) {
        DestroyMenu(g_hPopupMenu);
        g_hPopupMenu = NULL;
    }
}

// 窗口过程函数
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE:
            // 创建托盘图标
            CreateTrayIcon(hWnd);
            return 0;

        case WM_DESTROY:
            // 删除托盘图标
            DeleteTrayIcon();
            // 卸载键盘钩子
            UninstallKeyboardHook();
            // 退出消息循环
            PostQuitMessage(0);
            return 0;

        case WM_TRAY_MSG:
            {
                UINT uMouseMsg = (UINT)lParam;
                switch (uMouseMsg) {
                    case WM_RBUTTONUP:
                        {
                            // 获取鼠标位置
                            POINT pt;
                            GetCursorPos(&pt);
                            
                            // 设置菜单状态（暂停/恢复）
                            ModifyMenu(g_hPopupMenu, IDM_PAUSE, MF_BYCOMMAND | MF_STRING, 
                                      g_isPaused ? IDM_RESUME : IDM_PAUSE, 
                                      g_isPaused ? TEXT("恢复") : TEXT("暂停"));
                            
                            // 显示右键菜单
                            SetForegroundWindow(hWnd);
                            TrackPopupMenu(g_hPopupMenu, TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD,
                                          pt.x, pt.y, 0, hWnd, NULL);
                            PostMessage(hWnd, WM_NULL, 0, 0);
                        }
                        break;
                    
                    case WM_LBUTTONDBLCLK:
                        // 双击托盘图标可以显示/隐藏主窗口（如果需要）
                        break;
                }
            }
            return 0;

        case WM_COMMAND:
            {
                WORD wId = LOWORD(wParam);
                switch (wId) {
                    case IDM_EXIT:
                        // 退出程序
                        DestroyWindow(hWnd);
                        break;
                    
                    case IDM_PAUSE:
                        // 暂停钩子
                        g_isPaused = TRUE;
                        break;
                    
                    case IDM_RESUME:
                        // 恢复钩子
                        g_isPaused = FALSE;
                        break;
                }
            }
            return 0;

        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
}

// 键盘钩子回调函数
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    // 如果程序暂停，直接传递事件
    if (g_isPaused) {
        return CallNextHookEx(g_hKeyboardHook, nCode, wParam, lParam);
    }
    
    if (nCode >= 0) {
        KBDLLHOOKSTRUCT* pKeyboardHookStruct = (KBDLLHOOKSTRUCT*)lParam;
        DWORD vkCode = pKeyboardHookStruct->vkCode;
        BOOL isKeyDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
        BOOL isKeyUp = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);

        // 更新按键状态
        if (vkCode == VK_CONTROL || vkCode == VK_LCONTROL || vkCode == VK_RCONTROL) {
            g_isCtrlPressed = isKeyDown;
        } else if (vkCode == VK_MENU || vkCode == VK_LMENU || vkCode == VK_RMENU) {
            g_isAltPressed = isKeyDown;
        } else if (vkCode == VK_SHIFT || vkCode == VK_LSHIFT || vkCode == VK_RSHIFT) {
            g_isShiftPressed = isKeyDown;
        } else if (vkCode == VK_SPACE) {
            g_isSpacePressed = isKeyDown;

            // 检查组合键
            if (isKeyDown) {
                // Ctrl+空格
                if (g_isCtrlPressed && !g_isAltPressed && !g_isShiftPressed) {
                    return 1; // 拦截并丢弃
                }
                // Alt+Shift+空格
                else if (g_isAltPressed && g_isShiftPressed && !g_isCtrlPressed) {
                    return 1; // 拦截并丢弃
                }
                // Ctrl+Shift+空格
                else if (g_isCtrlPressed && g_isShiftPressed && !g_isAltPressed) {
                    // 拦截并丢弃
                    // 发送Win+空格
                    SendWinSpace();
                    return 1;
                }
            }
        }
    }

    // 调用下一个钩子
    return CallNextHookEx(g_hKeyboardHook, nCode, wParam, lParam);
}

// 检查是否以管理员权限运行
BOOL IsRunningAsAdmin() {
    BOOL bIsElevated = FALSE;
    HANDLE hToken = NULL;
    
    // 打开当前进程的令牌
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation;
        DWORD dwSize = sizeof(TOKEN_ELEVATION);
        
        // 获取令牌的提权信息
        if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &dwSize)) {
            bIsElevated = elevation.TokenIsElevated;
        }
        
        CloseHandle(hToken);
    }
    
    return bIsElevated;
}

// 自动提权函数
BOOL RestartAsAdmin() {
    TCHAR szPath[MAX_PATH];
    
    // 获取当前程序的路径
    if (GetModuleFileName(NULL, szPath, ARRAYSIZE(szPath)) == 0) {
        return FALSE;
    }
    
    // 设置ShellExecuteEx的参数
    SHELLEXECUTEINFO sei = { sizeof(SHELLEXECUTEINFO) };
    sei.lpVerb = TEXT("runas"); // 以管理员身份运行
    sei.lpFile = szPath;         // 程序路径
    sei.nShow = SW_HIDE;         // 隐藏新窗口
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    
    // 启动新的进程
    if (!ShellExecuteEx(&sei)) {
        DWORD dwError = GetLastError();
        // 如果用户拒绝了UAC提示，dwError会是ERROR_CANCELLED
        if (dwError != ERROR_CANCELLED) {
            MessageBox(NULL, TEXT("需要管理员权限才能运行！"), TEXT("错误"), MB_OK | MB_ICONERROR);
        }
        return FALSE;
    }
    
    // 关闭新进程的句柄
    CloseHandle(sei.hProcess);
    return TRUE;
}

// 安装键盘钩子
BOOL InstallKeyboardHook() {
    g_hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(NULL), 0);
    return (g_hKeyboardHook != NULL);
}

// 卸载键盘钩子
void UninstallKeyboardHook() {
    if (g_hKeyboardHook != NULL) {
        UnhookWindowsHookEx(g_hKeyboardHook);
        g_hKeyboardHook = NULL;
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 检查是否以管理员权限运行
    if (!IsRunningAsAdmin()) {
        // 如果不是管理员，尝试以管理员身份重新启动
        if (RestartAsAdmin()) {
            return 0;
        } else {
            return 1;
        }
    }
    
    // 注册窗口类
    WNDCLASSEX wc = {
        sizeof(WNDCLASSEX),
        CS_HREDRAW | CS_VREDRAW,
        WndProc,
        0,
        0,
        hInstance,
        NULL,
        NULL,
        NULL,
        NULL,
        TEXT("KeyboardHookClass"),
        NULL
    };
    
    if (!RegisterClassEx(&wc)) {
        MessageBox(NULL, TEXT("Failed to register window class!"), TEXT("Error"), MB_OK | MB_ICONERROR);
        return 1;
    }
    
    // 创建隐藏窗口
    g_hWnd = CreateWindowEx(
        0,
        TEXT("KeyboardHookClass"),
        TEXT("键盘钩子程序"),
        WS_POPUP, // 弹出窗口样式，不显示在任务栏
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        NULL,
        NULL,
        hInstance,
        NULL
    );
    
    if (g_hWnd == NULL) {
        MessageBox(NULL, TEXT("Failed to create window!"), TEXT("Error"), MB_OK | MB_ICONERROR);
        return 1;
    }
    
    // 安装键盘钩子
    if (!InstallKeyboardHook()) {
        MessageBox(NULL, TEXT("Failed to install keyboard hook!"), TEXT("Error"), MB_OK | MB_ICONERROR);
        DestroyWindow(g_hWnd);
        return 1;
    }
    
    // 消息循环
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return (int)msg.wParam;
}
