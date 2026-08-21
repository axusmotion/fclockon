#include "tray_icon.h"
#include "resource.h"
#include <vector>

static NOTIFYICONDATAW g_nid = {};

TrayIcon::TrayIcon() {}
TrayIcon::~TrayIcon() { Destroy(); }

void TrayIcon::Create(HWND hwnd, HINSTANCE hInstance) {
    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize           = sizeof(NOTIFYICONDATAW);
    g_nid.hWnd             = hwnd;
    g_nid.uID              = 1;
    g_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon            = LoadIconW(nullptr, IDI_APPLICATION);
    wcscpy_s(g_nid.szTip, L"Windows Clock Widget");

    Shell_NotifyIconW(NIM_ADD, &g_nid);

    // Set version for modern behavior
    g_nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &g_nid);
}

void TrayIcon::Destroy() {
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
}

void TrayIcon::ShowContextMenu(HWND hwnd, const ClockSettings& settings) {
    POINT pt;
    GetCursorPos(&pt);

    HMENU hMenu    = CreatePopupMenu();
    HMENU hPresets = CreatePopupMenu();

    // Build presets submenu
    const auto& presets = GetPresets();
    for (size_t i = 0; i < presets.size(); i++) {
        UINT flags = MF_STRING;
        if (presets[i].name == settings.presetName) {
            flags |= MF_CHECKED;
        }
        AppendMenuW(hPresets, flags,
                    ID_TRAY_PRESET_FIRST + static_cast<UINT>(i),
                    presets[i].name.c_str());
    }

    // Main menu
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_SETTINGS, L"⚙  Settings...");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hPresets), L"🎨  Presets");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

    // Toggle items
    UINT onTopFlags = MF_STRING | (settings.alwaysOnTop ? MF_CHECKED : 0);
    AppendMenuW(hMenu, onTopFlags, ID_TRAY_TOGGLE_ONTOP, L"📌  Always on Top");

    UINT clickFlags = MF_STRING | (settings.clickThrough ? MF_CHECKED : 0);
    AppendMenuW(hMenu, clickFlags, ID_TRAY_TOGGLE_CLICK, L"👆  Click-Through");

    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"❌  Exit");

    // Required for the menu to close properly when clicking elsewhere
    SetForegroundWindow(hwnd);

    int cmd = TrackPopupMenu(hMenu,
                             TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
                             pt.x, pt.y, 0, hwnd, nullptr);

    DestroyMenu(hMenu); // This also destroys the submenus

    if (cmd > 0) {
        PostMessageW(hwnd, WM_COMMAND, static_cast<WPARAM>(cmd), 0);
    }
}

int TrayIcon::GetPresetIndexFromMenuId(int menuId) const {
    int idx = menuId - ID_TRAY_PRESET_FIRST;
    const auto& presets = GetPresets();
    if (idx >= 0 && idx < static_cast<int>(presets.size())) {
        return idx;
    }
    return -1;
}
