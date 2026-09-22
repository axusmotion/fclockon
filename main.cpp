//──────────────────────────────────────────────────────────────────────
// Windows Clock Widget — main.cpp
// Lightweight desktop clock overlay with GDI+ rendering
//──────────────────────────────────────────────────────────────────────
#include <windows.h>
#include <windowsx.h>
#include <objidl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <exdisp.h>
#include <gdiplus.h>
#include <commctrl.h>
#include <dwmapi.h>

#include "resource.h"
#include "settings.h"
#include "renderer.h"
#include "todo_renderer.h"
#include "tray_icon.h"
#include "settings_dialog.h"

// ── Globals ──
static HWND           g_hwnd          = nullptr;
static HWND           g_hwndTodo      = nullptr;
static ClockSettings  g_settings;
static ClockRenderer  g_renderer;
static TodoRenderer   g_todoRenderer;
static TrayIcon       g_trayIcon;
static ULONG_PTR      g_gdiplusToken  = 0;
static HANDLE         g_mutex         = nullptr;
static const UINT_PTR TIMER_CLOCK     = 1;
static bool           g_settingsOpen  = false;
static bool           g_iconsHidden   = false;
static std::vector<TodoItem> g_todos;

// ── Forward declarations ──
static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK TodoWndProc(HWND, UINT, WPARAM, LPARAM);
static void ApplyWindowStyle();
static void RefreshClock();
static void RefreshTodoWindow();
static void OpenSettings();

// ────────────────────────────────────────────────────────────────────
// Toggle Desktop Icons (robust Win32 method)
// ────────────────────────────────────────────────────────────────────
static void ToggleDesktopIcons(bool hide) {
    if (hide == g_iconsHidden) return;

    HWND hProgman = FindWindowW(L"Progman", L"Program Manager");
    HWND hDesktopWnd = FindWindowExW(hProgman, nullptr, L"SHELLDLL_DefView", nullptr);
    if (!hDesktopWnd) {
        HWND hWorker = nullptr;
        do {
            hWorker = FindWindowExW(nullptr, hWorker, L"WorkerW", nullptr);
            if (hWorker) {
                hDesktopWnd = FindWindowExW(hWorker, nullptr, L"SHELLDLL_DefView", nullptr);
                if (hDesktopWnd) break;
            }
        } while (hWorker);
    }
    
    if (hDesktopWnd) {
        HWND hListView = FindWindowExW(hDesktopWnd, nullptr, L"SysListView32", L"FolderView");
        if (hListView) {
            ShowWindow(hListView, hide ? SW_HIDE : SW_SHOW);
            g_iconsHidden = hide;
        }
    }
}

// ────────────────────────────────────────────────────────────────────
// Entry Point
// ────────────────────────────────────────────────────────────────────
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    // ── Single instance check ──
    g_mutex = CreateMutexW(nullptr, TRUE, L"FClockOnMutex_v1");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(g_mutex);
        HWND hExisting = FindWindowW(L"FClockOnClockWidget", nullptr);
        if (!hExisting) hExisting = FindWindowW(L"WindowsClockWidget", nullptr);
        if (hExisting) {
            SetForegroundWindow(hExisting);
            PostMessageW(hExisting, WM_COMMAND, ID_TRAY_SETTINGS, 0);
        }
        return 0;
    }

    // ── DPI awareness ──
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // ── Initialize GDI+ ──
    Gdiplus::GdiplusStartupInput gdiplusInput;
    Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdiplusInput, nullptr);

    // ── Initialize common controls ──
    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(icc);
    icc.dwICC  = ICC_BAR_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    // ── Load settings ──
    LoadSettings(g_settings);
    LoadTodos(g_todos);

    // ── Register window classes ──
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance      = hInstance;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = L"FClockOnClockWidget";
    wc.hIcon         = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hbrBackground = nullptr;  // Rendered via UpdateLayeredWindow
    RegisterClassExW(&wc);

    WNDCLASSEXW twc = {};
    twc.cbSize        = sizeof(twc);
    twc.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    twc.lpfnWndProc   = TodoWndProc;
    twc.hInstance      = hInstance;
    twc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    twc.lpszClassName = L"FClockOnTodoWidget";
    twc.hIcon         = LoadIconW(nullptr, IDI_APPLICATION);
    twc.hbrBackground = nullptr;
    RegisterClassExW(&twc);

    // ── Determine initial clock window position ──
    g_renderer.UpdateSettings(g_settings);
    int wndW = 0, wndH = 0;
    g_renderer.GetWindowSize(wndW, wndH);

    int posX = g_settings.posX;
    int posY = g_settings.posY;
    int scrW = GetSystemMetrics(SM_CXSCREEN);
    int scrH = GetSystemMetrics(SM_CYSCREEN);
    if (posX < 0 || posY < 0) {
        // Auto-center on primary monitor
        posX = (scrW - wndW) / 2;
        posY = scrH / 6;  // Upper third
    }

    // ── Create clock layered window ──
    DWORD exStyle = WS_EX_LAYERED | WS_EX_TOOLWINDOW;
    if (g_settings.alwaysOnTop) exStyle |= WS_EX_TOPMOST;
    if (g_settings.clickThrough) exStyle |= WS_EX_TRANSPARENT;

    g_hwnd = CreateWindowExW(
        exStyle,
        L"FClockOnClockWidget",
        L"FClockOn",
        WS_POPUP,
        posX, posY, wndW, wndH,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!g_hwnd) {
        Gdiplus::GdiplusShutdown(g_gdiplusToken);
        return 1;
    }

    // ── Create TODO widget window ──
    g_todoRenderer.UpdateSettings(g_settings);
    g_todoRenderer.SetTodoItems(g_todos);
    int todoW = 0, todoH = 0;
    g_todoRenderer.GetWindowSize(todoW, todoH);

    int todoX = g_settings.todoPosX;
    int todoY = g_settings.todoPosY;
    if (todoX < 0 || todoY < 0) {
        todoX = (scrW - todoW) - 40;
        todoY = 60;
    }

    g_hwndTodo = CreateWindowExW(
        exStyle,
        L"FClockOnTodoWidget",
        L"FClockOn Tasks",
        WS_POPUP,
        todoX, todoY, todoW, todoH,
        nullptr, nullptr, hInstance, nullptr
    );

    // ── Initialize renderers and tray icon ──
    g_renderer.Initialize(g_hwnd);
    g_renderer.UpdateSettings(g_settings);

    if (g_hwndTodo) {
        g_todoRenderer.Initialize(g_hwndTodo);
        g_todoRenderer.UpdateSettings(g_settings);
        g_todoRenderer.SetTodoItems(g_todos);
    }

    g_trayIcon.Create(g_hwnd, hInstance);

    // ── Apply Desktop Icon state ──
    if (g_settings.hideDesktopIcons) {
        ToggleDesktopIcons(true);
    }

    // ── Initial renders ──
    RefreshClock();
    RefreshTodoWindow();

    // ── Show windows ──
    ShowWindow(g_hwnd, SW_SHOWNOACTIVATE);

    // ── Start clock timer (1 second) ──
    SetTimer(g_hwnd, TIMER_CLOCK, 1000, nullptr);

    // ── Message loop ──
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // ── Cleanup ──
    KillTimer(g_hwnd, TIMER_CLOCK);
    g_trayIcon.Destroy();
    
    // Restore desktop icons if we hid them
    ToggleDesktopIcons(false);

    if (g_hwndTodo) {
        DestroyWindow(g_hwndTodo);
        g_hwndTodo = nullptr;
    }

    Gdiplus::GdiplusShutdown(g_gdiplusToken);
    if (g_mutex) {
        ReleaseMutex(g_mutex);
        CloseHandle(g_mutex);
    }

    return static_cast<int>(msg.wParam);
}

// ────────────────────────────────────────────────────────────────────
// Apply window styles based on current settings
// ────────────────────────────────────────────────────────────────────
static void ApplyWindowStyle() {
    HWND wnds[] = { g_hwnd, g_hwndTodo };
    for (HWND h : wnds) {
        if (!h) continue;
        LONG_PTR exStyle = GetWindowLongPtrW(h, GWL_EXSTYLE);

        // Always on top
        if (g_settings.alwaysOnTop) {
            exStyle |= WS_EX_TOPMOST;
            SetWindowPos(h, HWND_TOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        } else {
            exStyle &= ~WS_EX_TOPMOST;
            SetWindowPos(h, HWND_NOTOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }

        // Click-through
        if (g_settings.clickThrough) {
            exStyle |= WS_EX_TRANSPARENT;
        } else {
            exStyle &= ~WS_EX_TRANSPARENT;
        }

        SetWindowLongPtrW(h, GWL_EXSTYLE, exStyle);
    }
}

// ────────────────────────────────────────────────────────────────────
// Refresh the clock display
// ────────────────────────────────────────────────────────────────────
static void RefreshClock() {
    g_renderer.UpdateSettings(g_settings);

    int wndW = 0, wndH = 0;
    g_renderer.GetWindowSize(wndW, wndH);

    // Resize window if needed (keep position)
    RECT rc;
    GetWindowRect(g_hwnd, &rc);
    if ((rc.right - rc.left) != wndW || (rc.bottom - rc.top) != wndH) {
        SetWindowPos(g_hwnd, nullptr, 0, 0, wndW, wndH,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    g_renderer.Render();
}

// ────────────────────────────────────────────────────────────────────
// Refresh the TODO widget display
// ────────────────────────────────────────────────────────────────────
static void RefreshTodoWindow() {
    if (!g_hwndTodo) return;

    if (!g_settings.todoEnabled) {
        ShowWindow(g_hwndTodo, SW_HIDE);
        return;
    }

    g_todoRenderer.UpdateSettings(g_settings);
    g_todoRenderer.SetTodoItems(g_todos);

    int wndW = 0, wndH = 0;
    g_todoRenderer.GetWindowSize(wndW, wndH);

    RECT rc;
    GetWindowRect(g_hwndTodo, &rc);
    if ((rc.right - rc.left) != wndW || (rc.bottom - rc.top) != wndH) {
        SetWindowPos(g_hwndTodo, nullptr, 0, 0, wndW, wndH,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    g_todoRenderer.Render();
    ShowWindow(g_hwndTodo, SW_SHOWNOACTIVATE);
}

// ────────────────────────────────────────────────────────────────────
// Open settings dialog
// ────────────────────────────────────────────────────────────────────
static void OpenSettings() {
    if (g_settingsOpen) return;
    g_settingsOpen = true;

    ClockSettings prevSettings = g_settings;
    bool changed = ShowSettingsDialog(g_hwnd, g_settings, &g_todos);

    if (changed) {
        // Handle auto-start change
        if (g_settings.autoStart != prevSettings.autoStart) {
            SetAutoStart(g_settings.autoStart);
        }
        
        // Handle desktop icons change
        if (g_settings.hideDesktopIcons != prevSettings.hideDesktopIcons) {
            ToggleDesktopIcons(g_settings.hideDesktopIcons);
        }

        ApplyWindowStyle();
        RefreshClock();
        RefreshTodoWindow();
        SaveSettings(g_settings);
    }

    g_settingsOpen = false;
}

// ────────────────────────────────────────────────────────────────────
// Window Procedure — Clock Widget
// ────────────────────────────────────────────────────────────────────
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {

    case WM_TIMER:
        if (wParam == TIMER_CLOCK) {
            g_renderer.Render();
        }
        return 0;

    case WM_NCHITTEST: {
        // Make entire window draggable (act like a title bar)
        LRESULT defResult = DefWindowProcW(hwnd, msg, wParam, lParam);
        if (defResult == HTCLIENT) {
            return HTCAPTION;
        }
        return defResult;
    }

    case WM_NCLBUTTONDBLCLK:
        // Double-click to open settings
        OpenSettings();
        return 0;

    case WM_MOVE: {
        // Save position when dragged
        RECT rc;
        GetWindowRect(hwnd, &rc);
        g_settings.posX = rc.left;
        g_settings.posY = rc.top;
        g_renderer.UpdateSettings(g_settings);
        return 0;
    }

    case WM_EXITSIZEMOVE:
        SaveSettings(g_settings);
        return 0;

    case WM_MOVING: {
        if (!g_settings.snapToEdges) return FALSE;
        RECT* prc = reinterpret_cast<RECT*>(lParam);
        int w = prc->right - prc->left;
        int h = prc->bottom - prc->top;
        
        HMONITOR hMon = MonitorFromRect(prc, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = { sizeof(mi) };
        if (GetMonitorInfo(hMon, &mi)) {
            const int snapDist = 20; // Snap threshold in pixels
            
            // Horizontal snap
            if (abs(prc->left - mi.rcWork.left) < snapDist) {
                prc->left = mi.rcWork.left;
                prc->right = prc->left + w;
            }
            else if (abs(prc->right - mi.rcWork.right) < snapDist) {
                prc->right = mi.rcWork.right;
                prc->left = prc->right - w;
            }
            
            // Vertical snap
            if (abs(prc->top - mi.rcWork.top) < snapDist) {
                prc->top = mi.rcWork.top;
                prc->bottom = prc->top + h;
            }
            else if (abs(prc->bottom - mi.rcWork.bottom) < snapDist) {
                prc->bottom = mi.rcWork.bottom;
                prc->top = prc->bottom - h;
            }
        }
        return TRUE;
    }

    case WM_TRAYICON: {
        // Handle tray icon events
        UINT trayMsg = LOWORD(lParam);
        if (trayMsg == WM_RBUTTONUP || trayMsg == WM_CONTEXTMENU) {
            g_trayIcon.ShowContextMenu(hwnd, g_settings);
        } else if (trayMsg == WM_LBUTTONDBLCLK) {
            OpenSettings();
        }
        return 0;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);

        if (id == ID_TRAY_SETTINGS) {
            OpenSettings();
        }
        else if (id == ID_TRAY_EXIT) {
            SaveSettings(g_settings);
            PostQuitMessage(0);
        }
        else if (id == ID_TRAY_TOGGLE_ONTOP) {
            g_settings.alwaysOnTop = !g_settings.alwaysOnTop;
            ApplyWindowStyle();
            SaveSettings(g_settings);
        }
        else if (id == ID_TRAY_TOGGLE_CLICK) {
            g_settings.clickThrough = !g_settings.clickThrough;
            ApplyWindowStyle();
            SaveSettings(g_settings);
        }
        else if (id >= ID_TRAY_PRESET_FIRST && id <= ID_TRAY_PRESET_LAST) {
            int idx = g_trayIcon.GetPresetIndexFromMenuId(id);
            if (idx >= 0) {
                const auto& presets = GetPresets();
                ApplyPreset(g_settings, presets[idx].name);
                RefreshClock();
                RefreshTodoWindow();
                SaveSettings(g_settings);
            }
        }
        else if (id >= ID_TRAY_TODO_STYLE_FIRST && id <= ID_TRAY_TODO_STYLE_TRANS) {
            g_settings.todoStyle = id - ID_TRAY_TODO_STYLE_FIRST;
            g_todoRenderer.UpdateSettings(g_settings);
            RefreshTodoWindow();
            SaveSettings(g_settings);
        }
        else if (id >= IDC_ALIGN_TL && id <= IDC_ALIGN_BR) {
            HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi = { sizeof(mi) };
            if (GetMonitorInfo(hMon, &mi)) {
                RECT rc; GetWindowRect(hwnd, &rc);
                int w = rc.right - rc.left;
                int h = rc.bottom - rc.top;
                int newX = rc.left, newY = rc.top;
                
                if (id == IDC_ALIGN_TL || id == IDC_ALIGN_BL) newX = mi.rcWork.left;
                if (id == IDC_ALIGN_TR || id == IDC_ALIGN_BR) newX = mi.rcWork.right - w;
                if (id == IDC_ALIGN_TL || id == IDC_ALIGN_TR) newY = mi.rcWork.top;
                if (id == IDC_ALIGN_BL || id == IDC_ALIGN_BR) newY = mi.rcWork.bottom - h;
                
                SetWindowPos(hwnd, nullptr, newX, newY, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
                g_settings.posX = newX;
                g_settings.posY = newY;
                RefreshClock();
                SaveSettings(g_settings);
            }
        }
        return 0;
    }

    case WM_SETTINGS_CHANGED:
        // Live update from settings dialog "Apply" button
        ApplyWindowStyle();
        RefreshClock();
        RefreshTodoWindow();
        SaveSettings(g_settings);
        return 0;

    case WM_DESTROY:
        SaveSettings(g_settings);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ────────────────────────────────────────────────────────────────────
// Window Procedure — TODO Widget
// ────────────────────────────────────────────────────────────────────
static LRESULT CALLBACK TodoWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {

    case WM_NCHITTEST: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        RECT rc;
        GetWindowRect(hwnd, &rc);

        // Bottom-right corner resize handle (18x18px)
        if (pt.x >= rc.right - 18 && pt.y >= rc.bottom - 18) {
            return HTBOTTOMRIGHT;
        }

        POINT ptClient = pt;
        ScreenToClient(hwnd, &ptClient);

        // Checkbox hit area: return HTCLIENT for hover feedback, hand cursor, and instant toggle!
        if (g_todoRenderer.HitTestCheckbox(ptClient.x, ptClient.y) >= 0) {
            return HTCLIENT;
        }

        // Entire rest of widget: return HTCAPTION for effortless, silky smooth native dragging!
        return HTCAPTION;
    }

    case WM_MOUSEMOVE: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        TRACKMOUSEEVENT tme = {};
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hwnd;
        TrackMouseEvent(&tme);

        int hitIdx = g_todoRenderer.HitTestCheckbox(x, y);
        if (hitIdx != g_todoRenderer.GetHoveredIndex()) {
            g_todoRenderer.SetHoveredIndex(hitIdx);
            g_todoRenderer.Render();
        }
        return 0;
    }

    case WM_MOUSELEAVE: {
        if (g_todoRenderer.GetHoveredIndex() != -1) {
            g_todoRenderer.SetHoveredIndex(-1);
            g_todoRenderer.Render();
        }
        return 0;
    }

    case WM_NCMOUSEMOVE: {
        if (g_todoRenderer.GetHoveredIndex() != -1) {
            g_todoRenderer.SetHoveredIndex(-1);
            g_todoRenderer.Render();
        }
        break;
    }

    case WM_SETCURSOR: {
        if (LOWORD(lParam) == HTCLIENT) {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hwnd, &pt);
            if (g_todoRenderer.HitTestCheckbox(pt.x, pt.y) >= 0) {
                SetCursor(LoadCursorW(nullptr, IDC_HAND));
                return TRUE;
            }
        }
        break;
    }

    case WM_SIZING: {
        RECT* prc = reinterpret_cast<RECT*>(lParam);
        int minW = 220, maxW = 700;
        int minH = 70;
        if (prc->right - prc->left < minW) prc->right = prc->left + minW;
        if (prc->right - prc->left > maxW) prc->right = prc->left + maxW;
        if (prc->bottom - prc->top < minH) prc->bottom = prc->top + minH;

        g_settings.todoWidth = prc->right - prc->left;
        g_todoRenderer.UpdateSettings(g_settings);
        g_todoRenderer.Render();
        return TRUE;
    }

    case WM_SIZE: {
        int w = LOWORD(lParam);
        if (w >= 220 && w <= 700) {
            g_settings.todoWidth = w;
            g_todoRenderer.UpdateSettings(g_settings);
            g_todoRenderer.Render();
        }
        return 0;
    }

    case WM_LBUTTONDOWN: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        int cbIdx = g_todoRenderer.HitTestCheckbox(x, y);
        if (cbIdx >= 0 && cbIdx < static_cast<int>(g_todos.size())) {
            g_todos[cbIdx].done = !g_todos[cbIdx].done;
            SaveTodos(g_todos);
            RefreshTodoWindow();

            HWND hDlg = GetSettingsDialogHwnd();
            if (hDlg && IsWindow(hDlg)) {
                PostMessageW(hDlg, WM_USER + 101, 0, 0);
            }
            return 0;
        }
        return 0;
    }

    case WM_LBUTTONDBLCLK:
    case WM_NCLBUTTONDBLCLK: {
        OpenSettings();
        return 0;
    }

    case WM_MOVE: {
        RECT rc;
        GetWindowRect(hwnd, &rc);
        g_settings.todoPosX = rc.left;
        g_settings.todoPosY = rc.top;
        return 0;
    }

    case WM_EXITSIZEMOVE:
        SaveSettings(g_settings);
        return 0;

    case WM_MOVING: {
        if (!g_settings.snapToEdges) return FALSE;
        RECT* prc = reinterpret_cast<RECT*>(lParam);
        int w = prc->right - prc->left;
        int h = prc->bottom - prc->top;

        HMONITOR hMon = MonitorFromRect(prc, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = { sizeof(mi) };
        if (GetMonitorInfo(hMon, &mi)) {
            const int snapDist = 20;

            if (abs(prc->left - mi.rcWork.left) < snapDist) {
                prc->left = mi.rcWork.left;
                prc->right = prc->left + w;
            } else if (abs(prc->right - mi.rcWork.right) < snapDist) {
                prc->right = mi.rcWork.right;
                prc->left = prc->right - w;
            }

            if (abs(prc->top - mi.rcWork.top) < snapDist) {
                prc->top = mi.rcWork.top;
                prc->bottom = prc->top + h;
            } else if (abs(prc->bottom - mi.rcWork.bottom) < snapDist) {
                prc->bottom = mi.rcWork.bottom;
                prc->top = prc->bottom - h;
            }
        }
        return TRUE;
    }

    case WM_RBUTTONUP:
    case WM_NCRBUTTONUP:
        g_trayIcon.ShowContextMenu(g_hwnd, g_settings);
        return 0;

    case WM_DESTROY:
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
