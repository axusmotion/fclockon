#pragma once

#include <windows.h>
#include <shellapi.h>
#include "settings.h"

class TrayIcon {
public:
    TrayIcon();
    ~TrayIcon();

    void Create(HWND hwnd, HINSTANCE hInstance);
    void Destroy();
    void ShowContextMenu(HWND hwnd, const ClockSettings& settings);
    int  GetPresetIndexFromMenuId(int menuId) const;
};
