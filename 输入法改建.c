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

// 中文常量定义（使用宽字符）
const WCHAR* g_szTipText = L"键盘钩子程序";
const WCHAR* g_szMenuPause = L"暂停";
const WCHAR* g_szMenuResume = L"恢复";
const WCHAR* g_szMenuExit = L"退出";
const WCHAR* g_szWindowClass = L"KeyboardHookClass";
const WCHAR* g_szWindowTitle = L"键盘钩子程序";
const WCHAR* g_szErrorTitle = L"错误";
const WCHAR* g_szErrorAdmin = L"需要管理员权限才能运行！";
const WCHAR* g_szErrorHook = L"Failed to install keyboard hook!";
const WCHAR* g_szErrorWindow = L"Failed to create window!";
const WCHAR* g_szErrorClass = L"Failed to register window class!";

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
    
    // 设置提示文本（使用宽字符版本的NOTIFYICONDATA结构体）
    // 当使用-municode编译时，NOTIFYICONDATA默认是宽字符版本
    // 所以szTip字段是WCHAR类型，可以直接赋值宽字符串
    wcscpy(g_nid.szTip, g_szTipText);
    
    // 添加托盘图标
    if (!Shell_NotifyIcon(NIM_ADD, &g_nid)) {
        return FALSE;
    }
    
    // 创建右键菜单
    g_hPopupMenu = CreatePopupMenu();
    if (g_hPopupMenu == NULL) {
        return FALSE;
    }
    
    // 添加菜单项（使用宽字符版本的函数）
    AppendMenuW(g_hPopupMenu, MF_STRING, IDM_PAUSE, g_szMenuPause);
    AppendMenuW(g_hPopupMenu, MF_STRING, IDM_EXIT, g_szMenuExit);
    
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
                            // 先移除旧的菜单项
                            RemoveMenu(g_hPopupMenu, IDM_PAUSE, MF_BYCOMMAND);
                            // 添加新的菜单项（使用宽字符版本的函数）
                            if (g_isPaused) {
                                AppendMenuW(g_hPopupMenu, MF_STRING, IDM_RESUME, g_szMenuResume);
                            } else {
                                AppendMenuW(g_hPopupMenu, MF_STRING, IDM_PAUSE, g_szMenuPause);
                            }
                            
                            // 显示右键菜单
                            SetForegroundWindow(hWnd);
                            // 移除TPM_NONOTIFY标志，这样会发送WM_COMMAND消息
                            // 或者处理返回值，手动发送WM_COMMAND消息
                            UINT uSelected = TrackPopupMenu(g_hPopupMenu, 
                                                           TPM_RIGHTBUTTON | TPM_RETURNCMD,
                                                           pt.x, pt.y, 0, hWnd, NULL);
                            
                            // 如果用户选择了菜单项，手动发送WM_COMMAND消息
                            if (uSelected != 0) {
                                PostMessage(hWnd, WM_COMMAND, uSelected, 0);
                            }
                            
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
            MessageBoxW(NULL, g_szErrorAdmin, g_szErrorTitle, MB_OK | MB_ICONERROR);
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

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    // 检查是否以管理员权限运行
    if (!IsRunningAsAdmin()) {
        // 如果不是管理员，尝试以管理员身份重新启动
        if (RestartAsAdmin()) {
            return 0;
        } else {
            return 1;
        }
    }
    
    // 注册窗口类（使用宽字符版本）
    WNDCLASSEXW wc = {
        sizeof(WNDCLASSEXW),
        CS_HREDRAW | CS_VREDRAW,
        WndProc,
        0,
        0,
        hInstance,
        NULL,
        NULL,
        NULL,
        NULL,
        g_szWindowClass,
        NULL
    };
    
    if (!RegisterClassExW(&wc)) {
        MessageBoxW(NULL, g_szErrorClass, g_szErrorTitle, MB_OK | MB_ICONERROR);
        return 1;
    }
    
    // 创建隐藏窗口（使用宽字符版本）
    g_hWnd = CreateWindowExW(
        0,
        g_szWindowClass,
        g_szWindowTitle,
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
        MessageBoxW(NULL, g_szErrorWindow, g_szErrorTitle, MB_OK | MB_ICONERROR);
        return 1;
    }
    
    // 安装键盘钩子
    if (!InstallKeyboardHook()) {
        MessageBoxW(NULL, g_szErrorHook, g_szErrorTitle, MB_OK | MB_ICONERROR);
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
