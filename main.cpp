//──────────────────────────────────────────────────────────────────────
// Windows Clock Widget — main.cpp
// Lightweight desktop clock overlay with GDI+ rendering
//──────────────────────────────────────────────────────────────────────
#include <windows.h>
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
#include "tray_icon.h"
#include "settings_dialog.h"

// ── Globals ──
static HWND           g_hwnd          = nullptr;
static ClockSettings  g_settings;
static ClockRenderer  g_renderer;
static TrayIcon       g_trayIcon;
static ULONG_PTR      g_gdiplusToken  = 0;
static HANDLE         g_mutex         = nullptr;
static const UINT_PTR TIMER_CLOCK     = 1;
static bool           g_settingsOpen  = false;
static bool           g_iconsHidden   = false;

// ── Forward declarations ──
static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
static void ApplyWindowStyle();
static void RefreshClock();

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
    g_mutex = CreateMutexW(nullptr, TRUE, L"WindowsClockWidgetMutex_v1");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(g_mutex);
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

    // ── Register window class ──
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance      = hInstance;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = L"WindowsClockWidget";
    wc.hIcon         = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hbrBackground = nullptr;  // We render via UpdateLayeredWindow
    RegisterClassExW(&wc);

    // ── Determine initial window position ──
    g_renderer.UpdateSettings(g_settings);
    int wndW = 0, wndH = 0;
    g_renderer.GetWindowSize(wndW, wndH);

    int posX = g_settings.posX;
    int posY = g_settings.posY;
    if (posX < 0 || posY < 0) {
        // Auto-center on primary monitor
        int scrW = GetSystemMetrics(SM_CXSCREEN);
        int scrH = GetSystemMetrics(SM_CYSCREEN);
        posX = (scrW - wndW) / 2;
        posY = scrH / 6;  // Upper third
    }

    // ── Create the layered window ──
    DWORD exStyle = WS_EX_LAYERED | WS_EX_TOOLWINDOW;
    if (g_settings.alwaysOnTop) exStyle |= WS_EX_TOPMOST;
    if (g_settings.clickThrough) exStyle |= WS_EX_TRANSPARENT;

    g_hwnd = CreateWindowExW(
        exStyle,
        L"WindowsClockWidget",
        L"Clock",
        WS_POPUP,
        posX, posY, wndW, wndH,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!g_hwnd) {
        Gdiplus::GdiplusShutdown(g_gdiplusToken);
        return 1;
    }

    // ── Initialize renderer and tray icon ──
    g_renderer.Initialize(g_hwnd);
    g_renderer.UpdateSettings(g_settings);

    g_trayIcon.Create(g_hwnd, hInstance);

    // ── Apply Desktop Icon state ──
    if (g_settings.hideDesktopIcons) {
        ToggleDesktopIcons(true);
    }

    // ── Initial render ──
    RefreshClock();

    // ── Show window ──
    ShowWindow(g_hwnd, SW_SHOWNOACTIVATE);

    // ── Start timer (1 second) ──
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
    LONG_PTR exStyle = GetWindowLongPtrW(g_hwnd, GWL_EXSTYLE);

    // Always on top
    if (g_settings.alwaysOnTop) {
        exStyle |= WS_EX_TOPMOST;
        SetWindowPos(g_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    } else {
        exStyle &= ~WS_EX_TOPMOST;
        SetWindowPos(g_hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }

    // Click-through
    if (g_settings.clickThrough) {
        exStyle |= WS_EX_TRANSPARENT;
    } else {
        exStyle &= ~WS_EX_TRANSPARENT;
    }

    SetWindowLongPtrW(g_hwnd, GWL_EXSTYLE, exStyle);
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
// Open settings dialog
// ────────────────────────────────────────────────────────────────────
static void OpenSettings() {
    if (g_settingsOpen) return;
    g_settingsOpen = true;

    ClockSettings prevSettings = g_settings;
    bool changed = ShowSettingsDialog(g_hwnd, g_settings);

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
        SaveSettings(g_settings);
    }

    g_settingsOpen = false;
}

// ────────────────────────────────────────────────────────────────────
// Window Procedure
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
        // Keep renderer internal settings in sync so it doesn't snap back on next timer tick
        g_renderer.UpdateSettings(g_settings);
        return 0;
    }

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
            // Save position before exit
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
                SaveSettings(g_settings);
            }
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
        SaveSettings(g_settings);
        return 0;

    case WM_DESTROY:
        SaveSettings(g_settings);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
