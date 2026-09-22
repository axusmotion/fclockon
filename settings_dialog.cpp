#include "settings_dialog.h"
#include "resource.h"
#include "renderer.h"
#include <commctrl.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <vector>
#include <string>
#include <algorithm>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")

// ────────────────────────────────────────────────────────────────────
// Dialog state
// ────────────────────────────────────────────────────────────────────
struct DialogState {
    ClockSettings* settings;
    ClockSettings  workingCopy;
    bool           changed;
    HBRUSH         hDarkBrush;
    HBRUSH         hGroupBrush;
    HFONT          hFont;
    HFONT          hBoldFont;
    HFONT          hTitleFont;
    std::vector<TodoItem>* todos;       // pointer to g_todos in main
    std::vector<TodoItem>  todoCopy;    // working copy for editing
};

// Helper: refresh the TODO listbox from todoCopy
static void RefreshTodoListbox(HWND hDlg, const std::vector<TodoItem>& items) {
    HWND hList = GetDlgItem(hDlg, IDC_TODO_LISTBOX);
    if (!hList) return;
    SendMessageW(hList, LB_RESETCONTENT, 0, 0);
    for (const auto& item : items) {
        std::wstring display = (item.done ? L"\x2713  " : L"\x25CB  ");
        display += item.text;
        SendMessageW(hList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(display.c_str()));
    }
}

// Helper: enable/disable TODO controls based on checkbox
static void EnableTodoControls(HWND hDlg, bool enabled) {
    EnableWindow(GetDlgItem(hDlg, IDC_TODO_LISTBOX), enabled);
    EnableWindow(GetDlgItem(hDlg, IDC_TODO_INPUT), enabled);
    EnableWindow(GetDlgItem(hDlg, IDC_BTN_TODO_ADD), enabled);
    EnableWindow(GetDlgItem(hDlg, IDC_BTN_TODO_REMOVE), enabled);
    EnableWindow(GetDlgItem(hDlg, IDC_BTN_TODO_CHECK), enabled);
    EnableWindow(GetDlgItem(hDlg, IDC_TODO_SIZE_SLIDER), enabled);
    EnableWindow(GetDlgItem(hDlg, IDC_TODO_STYLE_COMBO), enabled);
}

static const COLORREF DARK_BG     = RGB(24, 24, 32);
static const COLORREF DARK_GROUP  = RGB(32, 32, 44);
static const COLORREF DARK_TEXT   = RGB(220, 220, 235);
static const COLORREF ACCENT      = RGB(80, 160, 255);

// ── Font enumeration callback ──
static int CALLBACK EnumFontProc(const LOGFONTW* lf, const TEXTMETRICW*, DWORD, LPARAM lParam) {
    HWND combo = reinterpret_cast<HWND>(lParam);
    if (lf->lfFaceName[0] != L'@') {
        if (SendMessageW(combo, CB_FINDSTRINGEXACT, static_cast<WPARAM>(-1),
                         reinterpret_cast<LPARAM>(lf->lfFaceName)) == CB_ERR) {
            SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(lf->lfFaceName));
        }
    }
    return 1;
}

// ── Helper: create a control ──
static HWND MakeCtrl(HWND parent, const wchar_t* cls, const wchar_t* text,
                     DWORD style, int x, int y, int w, int h, int id, HFONT font) {
    HWND hwnd = CreateWindowExW(0, cls, text, style | WS_CHILD | WS_VISIBLE,
                                 x, y, w, h, parent,
                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                 nullptr, nullptr);
    if (hwnd) {
        if (font) SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        
        // Remove Windows visual styles from checkboxes/radios so our custom colors work
        if ((style & BS_AUTOCHECKBOX) || (style & BS_AUTORADIOBUTTON)) {
            SetWindowTheme(hwnd, L"", L"");
        }
    }
    return hwnd;
}

// ── Color picker button ──
static COLORREF PickColor(HWND owner, COLORREF current) {
    static COLORREF customColors[16] = {};
    CHOOSECOLORW cc = {};
    cc.lStructSize  = sizeof(cc);
    cc.hwndOwner    = owner;
    cc.rgbResult    = current;
    cc.lpCustColors = customColors;
    cc.Flags        = CC_FULLOPEN | CC_RGBINIT;
    if (ChooseColorW(&cc)) {
        return cc.rgbResult;
    }
    return current;
}

// ── Update slider value labels ──
static void UpdateSliderLabels(HWND hDlg, const ClockSettings& s) {
    wchar_t buf[16];
    swprintf(buf, 16, L"%d px", s.fontSize);
    SetDlgItemTextW(hDlg, IDC_SIZE_VALUE, buf);

    swprintf(buf, 16, L"%d px", s.dateFontSize);
    SetDlgItemTextW(hDlg, IDC_DATE_SIZE_VALUE, buf);

    int pct = (s.bgAlpha * 100) / 255;
    swprintf(buf, 16, L"%d%%", pct);
    SetDlgItemTextW(hDlg, IDC_OPACITY_VALUE, buf);

    swprintf(buf, 16, L"%d", s.glowIntensity);
    SetDlgItemTextW(hDlg, IDC_GLOW_VALUE, buf);

    swprintf(buf, 16, L"%d px", s.todoFontSize > 0 ? s.todoFontSize : 20);
    SetDlgItemTextW(hDlg, IDC_TODO_SIZE_VALUE, buf);
}

// ── Populate controls from settings ──
static void SettingsToControls(HWND hDlg, const ClockSettings& s) {
    // Radio buttons
    CheckDlgButton(hDlg, IDC_RADIO_12H, s.use24Hour ? BST_UNCHECKED : BST_CHECKED);
    CheckDlgButton(hDlg, IDC_RADIO_24H, s.use24Hour ? BST_CHECKED : BST_UNCHECKED);

    // Checkboxes
    CheckDlgButton(hDlg, IDC_CHECK_SECONDS,  s.showSeconds  ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_CHECK_DATE,     s.showDate     ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_CHECK_DAY, s.showDayOfWeek ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_CHECK_ONTOP, s.alwaysOnTop ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_CHECK_CLICKTHRU, s.clickThrough ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_CHECK_AUTOSTART, s.autoStart ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_CHECK_SNAP, s.snapToEdges ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_CHECK_HIDE_ICONS, s.hideDesktopIcons ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_CHECK_TODO_ENABLED, s.todoEnabled ? BST_CHECKED : BST_UNCHECKED);

    // Font combo
    HWND hFontCombo = GetDlgItem(hDlg, IDC_FONT_COMBO);
    int idx = static_cast<int>(SendMessageW(hFontCombo, CB_FINDSTRINGEXACT,
                                             static_cast<WPARAM>(-1),
                                             reinterpret_cast<LPARAM>(s.fontFamily.c_str())));
    if (idx != CB_ERR) SendMessageW(hFontCombo, CB_SETCURSEL, idx, 0);

    // Preset combo
    HWND hPresetCombo = GetDlgItem(hDlg, IDC_PRESET_COMBO);
    const auto& presets = GetPresets();
    for (size_t i = 0; i < presets.size(); i++) {
        if (presets[i].name == s.presetName) {
            SendMessageW(hPresetCombo, CB_SETCURSEL, i, 0);
            break;
        }
    }

    // Sliders
    SendDlgItemMessageW(hDlg, IDC_SIZE_SLIDER, TBM_SETPOS, TRUE, s.fontSize);
    SendDlgItemMessageW(hDlg, IDC_DATE_SIZE_SLIDER, TBM_SETPOS, TRUE, s.dateFontSize);
    SendDlgItemMessageW(hDlg, IDC_OPACITY_SLIDER, TBM_SETPOS, TRUE, s.bgAlpha);
    SendDlgItemMessageW(hDlg, IDC_GLOW_SLIDER, TBM_SETPOS, TRUE, s.glowIntensity);
    SendDlgItemMessageW(hDlg, IDC_TODO_SIZE_SLIDER, TBM_SETPOS, TRUE, s.todoFontSize > 0 ? s.todoFontSize : 20);

    // Card style combo
    HWND hStyleCombo = GetDlgItem(hDlg, IDC_TODO_STYLE_COMBO);
    if (hStyleCombo) {
        int curSel = std::max(0, std::min(2, s.todoStyle));
        SendMessageW(hStyleCombo, CB_SETCURSEL, curSel, 0);
    }

    UpdateSliderLabels(hDlg, s);

    // TODO controls
    EnableTodoControls(hDlg, s.todoEnabled);
}

// ── Read controls into settings ──
static void ControlsToSettings(HWND hDlg, ClockSettings& s) {
    s.use24Hour    = IsDlgButtonChecked(hDlg, IDC_RADIO_24H) == BST_CHECKED;
    s.showSeconds  = IsDlgButtonChecked(hDlg, IDC_CHECK_SECONDS) == BST_CHECKED;
    s.showDate     = IsDlgButtonChecked(hDlg, IDC_CHECK_DATE) == BST_CHECKED;
    s.showDayOfWeek = IsDlgButtonChecked(hDlg, IDC_CHECK_DAY) == BST_CHECKED;
    s.alwaysOnTop  = IsDlgButtonChecked(hDlg, IDC_CHECK_ONTOP) == BST_CHECKED;
    s.clickThrough = IsDlgButtonChecked(hDlg, IDC_CHECK_CLICKTHRU) == BST_CHECKED;
    s.autoStart    = IsDlgButtonChecked(hDlg, IDC_CHECK_AUTOSTART) == BST_CHECKED;
    s.snapToEdges  = IsDlgButtonChecked(hDlg, IDC_CHECK_SNAP) == BST_CHECKED;
    s.hideDesktopIcons = IsDlgButtonChecked(hDlg, IDC_CHECK_HIDE_ICONS) == BST_CHECKED;
    s.todoEnabled  = IsDlgButtonChecked(hDlg, IDC_CHECK_TODO_ENABLED) == BST_CHECKED;

    // Font
    HWND hFontCombo = GetDlgItem(hDlg, IDC_FONT_COMBO);
    int idx = static_cast<int>(SendMessageW(hFontCombo, CB_GETCURSEL, 0, 0));
    if (idx != CB_ERR) {
        wchar_t buf[LF_FACESIZE];
        SendMessageW(hFontCombo, CB_GETLBTEXT, idx, reinterpret_cast<LPARAM>(buf));
        s.fontFamily = buf;
    }

    // Sliders
    s.fontSize      = static_cast<int>(SendDlgItemMessageW(hDlg, IDC_SIZE_SLIDER, TBM_GETPOS, 0, 0));
    s.dateFontSize  = static_cast<int>(SendDlgItemMessageW(hDlg, IDC_DATE_SIZE_SLIDER, TBM_GETPOS, 0, 0));
    s.bgAlpha       = static_cast<int>(SendDlgItemMessageW(hDlg, IDC_OPACITY_SLIDER, TBM_GETPOS, 0, 0));
    s.glowIntensity = static_cast<int>(SendDlgItemMessageW(hDlg, IDC_GLOW_SLIDER, TBM_GETPOS, 0, 0));
    s.todoFontSize  = static_cast<int>(SendDlgItemMessageW(hDlg, IDC_TODO_SIZE_SLIDER, TBM_GETPOS, 0, 0));

    HWND hStyleCombo = GetDlgItem(hDlg, IDC_TODO_STYLE_COMBO);
    if (hStyleCombo) {
        int sIdx = static_cast<int>(SendMessageW(hStyleCombo, CB_GETCURSEL, 0, 0));
        if (sIdx != CB_ERR) {
            s.todoStyle = sIdx;
        }
    }
}

// ────────────────────────────────────────────────────────────────────
// Dialog WndProc & Preview Canvas
// ────────────────────────────────────────────────────────────────────
static LRESULT CALLBACK PreviewWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_ERASEBKGND) {
        return 1;
    }

    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        
        RECT rc;
        GetClientRect(hwnd, &rc);
        int clientW = rc.right - rc.left;
        int clientH = rc.bottom - rc.top;

        if (clientW > 0 && clientH > 0) {
            // Double-buffer offscreen bitmap
            Gdiplus::Bitmap memBmp(clientW, clientH, PixelFormat32bppARGB);
            Gdiplus::Graphics memGfx(&memBmp);
            memGfx.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
            memGfx.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
            memGfx.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);

            // Draw wallpaper background
            WCHAR wallpaper[MAX_PATH];
            bool drewWallpaper = false;
            if (SystemParametersInfoW(SPI_GETDESKWALLPAPER, MAX_PATH, wallpaper, 0)) {
                Gdiplus::Image bgImage(wallpaper);
                if (bgImage.GetLastStatus() == Gdiplus::Ok) {
                    float imgW = static_cast<float>(bgImage.GetWidth());
                    float imgH = static_cast<float>(bgImage.GetHeight());
                    float scale = (std::max)(static_cast<float>(clientW) / imgW, static_cast<float>(clientH) / imgH);
                    float drawW = imgW * scale;
                    float drawH = imgH * scale;
                    float drawX = (clientW - drawW) / 2.0f;
                    float drawY = (clientH - drawH) / 2.0f;
                    
                    memGfx.DrawImage(&bgImage, drawX, drawY, drawW, drawH);
                    drewWallpaper = true;
                }
            }
            
            if (!drewWallpaper) {
                memGfx.Clear(Gdiplus::Color(255, 22, 24, 32));
            }
            
            // Draw the clock using workingCopy settings
            DialogState* state = reinterpret_cast<DialogState*>(GetWindowLongPtrW(GetParent(hwnd), GWLP_USERDATA));
            if (state) {
                ClockRenderer previewRenderer;
                previewRenderer.Initialize(hwnd);
                previewRenderer.UpdateSettings(state->workingCopy);
                
                int w = 0, h = 0;
                previewRenderer.GetWindowSize(w, h);
                
                if (w > 0 && h > 0) {
                    Gdiplus::Bitmap bmp(w, h, PixelFormat32bppARGB);
                    Gdiplus::Graphics bmpGfx(&bmp);
                    previewRenderer.RenderToGraphics(bmpGfx, true);
                    
                    // Proportional scale to fit nicely in preview panel without clipping
                    float maxAllowedW = clientW * 0.86f;
                    float maxAllowedH = clientH * 0.50f;
                    float scaleFactor = 1.0f;
                    if (static_cast<float>(w) > maxAllowedW || static_cast<float>(h) > maxAllowedH) {
                        scaleFactor = (std::min)(maxAllowedW / static_cast<float>(w),
                                                 maxAllowedH / static_cast<float>(h));
                    }
                    
                    float finalW = static_cast<float>(w) * scaleFactor;
                    float finalH = static_cast<float>(h) * scaleFactor;
                    float x = (clientW - finalW) / 2.0f;
                    float y = (clientH - finalH) / 2.0f;
                    
                    memGfx.DrawImage(&bmp, x, y, finalW, finalH);
                }
            }

            // Blit offscreen buffer directly to screen DC
            Gdiplus::Graphics screenGfx(hdc);
            screenGfx.DrawImage(&memBmp, 0, 0);
        }
        
        EndPaint(hwnd, &ps);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static HWND s_hSettingsDlg = nullptr;

HWND GetSettingsDialogHwnd() {
    return s_hSettingsDlg;
}

static LRESULT CALLBACK SettingsDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    DialogState* state = reinterpret_cast<DialogState*>(GetWindowLongPtrW(hDlg, GWLP_USERDATA));

    switch (msg) {
    case WM_CREATE: {
        s_hSettingsDlg = hDlg;
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        state = reinterpret_cast<DialogState*>(cs->lpCreateParams);
        SetWindowLongPtrW(hDlg, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));

        // Enable dark title bar on Windows 10/11
        BOOL darkMode = TRUE;
        DwmSetWindowAttribute(hDlg, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &darkMode, sizeof(darkMode));

        // Create fonts
        state->hFont = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                    DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        state->hBoldFont = CreateFontW(-14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                        DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        state->hTitleFont = CreateFontW(-13, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                         DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        state->hDarkBrush  = CreateSolidBrush(DARK_BG);
        state->hGroupBrush = CreateSolidBrush(DARK_GROUP);

        HFONT hf  = state->hFont;
        HFONT hbf = state->hBoldFont;
        HFONT htf = state->hTitleFont;

        int W = 430;
        int lm = 18;    // left margin
        int cw = W - lm * 2; // content width
        int y = 12;

        // ── Preset section ──
        MakeCtrl(hDlg, L"STATIC", L"PRESET", WS_VISIBLE | SS_LEFT, lm, y, cw, 16, 0, htf);
        y += 22;
        HWND hPresetCombo = MakeCtrl(hDlg, L"COMBOBOX", L"",
            CBS_DROPDOWNLIST | WS_VSCROLL, lm, y, cw - 80, 200, IDC_PRESET_COMBO, hf);
        MakeCtrl(hDlg, L"BUTTON", L"Apply", BS_PUSHBUTTON, W - lm - 70, y, 70, 26, IDC_PRESET_APPLY, hf);

        // Populate presets
        const auto& presets = GetPresets();
        for (const auto& p : presets) {
            SendMessageW(hPresetCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(p.name.c_str()));
        }
        y += 36;

        // ── Time Format section ──
        MakeCtrl(hDlg, L"STATIC", L"TIME FORMAT", WS_VISIBLE | SS_LEFT, lm, y, cw, 16, 0, htf);
        y += 22;
        MakeCtrl(hDlg, L"BUTTON", L"12-hour", BS_AUTORADIOBUTTON | WS_GROUP, lm, y, 90, 20, IDC_RADIO_12H, hf);
        MakeCtrl(hDlg, L"BUTTON", L"24-hour", BS_AUTORADIOBUTTON, lm + 100, y, 90, 20, IDC_RADIO_24H, hf);
        y += 26;
        MakeCtrl(hDlg, L"BUTTON", L"Show seconds", BS_AUTOCHECKBOX, lm, y, 130, 20, IDC_CHECK_SECONDS, hf);
        MakeCtrl(hDlg, L"BUTTON", L"Show date", BS_AUTOCHECKBOX, lm + 140, y, 110, 20, IDC_CHECK_DATE, hf);
        MakeCtrl(hDlg, L"BUTTON", L"Show day", BS_AUTOCHECKBOX, lm + 260, y, 110, 20, IDC_CHECK_DAY, hf);
        y += 32;

        // ── Appearance section ──
        MakeCtrl(hDlg, L"STATIC", L"APPEARANCE", WS_VISIBLE | SS_LEFT, lm, y, cw, 16, 0, htf);
        y += 24;

        // Font
        MakeCtrl(hDlg, L"STATIC", L"Font:", WS_VISIBLE | SS_LEFT, lm, y + 3, 40, 16, 0, hf);
        HWND hFontCombo = MakeCtrl(hDlg, L"COMBOBOX", L"",
            CBS_DROPDOWNLIST | WS_VSCROLL | CBS_SORT, lm + 45, y, cw - 45, 300, IDC_FONT_COMBO, hf);
        HDC hdc = GetDC(nullptr);
        EnumFontFamiliesW(hdc, nullptr, EnumFontProc, reinterpret_cast<LPARAM>(hFontCombo));
        ReleaseDC(nullptr, hdc);
        y += 32;

        // Font size slider
        MakeCtrl(hDlg, L"STATIC", L"Time Size:", WS_VISIBLE | SS_LEFT, lm, y + 3, 70, 16, 0, hf);
        MakeCtrl(hDlg, TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_NOTICKS,
                 lm + 75, y, cw - 130, 24, IDC_SIZE_SLIDER, hf);
        SendDlgItemMessageW(hDlg, IDC_SIZE_SLIDER, TBM_SETRANGE, TRUE, MAKELONG(16, 150));
        MakeCtrl(hDlg, L"STATIC", L"", WS_VISIBLE | SS_RIGHT, W - lm - 50, y + 3, 50, 16, IDC_SIZE_VALUE, hf);
        y += 30;

        // Date font size slider
        MakeCtrl(hDlg, L"STATIC", L"Date Size:", WS_VISIBLE | SS_LEFT, lm, y + 3, 70, 16, 0, hf);
        MakeCtrl(hDlg, TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_NOTICKS,
                 lm + 75, y, cw - 130, 24, IDC_DATE_SIZE_SLIDER, hf);
        SendDlgItemMessageW(hDlg, IDC_DATE_SIZE_SLIDER, TBM_SETRANGE, TRUE, MAKELONG(10, 40));
        MakeCtrl(hDlg, L"STATIC", L"", WS_VISIBLE | SS_RIGHT, W - lm - 50, y + 3, 50, 16, IDC_DATE_SIZE_VALUE, hf);
        y += 32;

        // Color buttons
        MakeCtrl(hDlg, L"STATIC", L"Text:", WS_VISIBLE | SS_LEFT, lm, y + 4, 35, 16, 0, hf);
        MakeCtrl(hDlg, L"BUTTON", L"", BS_PUSHBUTTON | BS_OWNERDRAW,
                 lm + 40, y, 40, 24, IDC_BTN_TEXT_COLOR, hf);
        MakeCtrl(hDlg, L"STATIC", L"Glow:", WS_VISIBLE | SS_LEFT, lm + 100, y + 4, 40, 16, 0, hf);
        MakeCtrl(hDlg, L"BUTTON", L"", BS_PUSHBUTTON | BS_OWNERDRAW,
                 lm + 145, y, 40, 24, IDC_BTN_GLOW_COLOR, hf);
        MakeCtrl(hDlg, L"STATIC", L"Background:", WS_VISIBLE | SS_LEFT, lm + 205, y + 4, 85, 16, 0, hf);
        MakeCtrl(hDlg, L"BUTTON", L"", BS_PUSHBUTTON | BS_OWNERDRAW,
                 lm + 295, y, 40, 24, IDC_BTN_BG_COLOR, hf);
        y += 34;

        // Opacity slider
        MakeCtrl(hDlg, L"STATIC", L"Opacity:", WS_VISIBLE | SS_LEFT, lm, y + 3, 60, 16, 0, hf);
        MakeCtrl(hDlg, TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_NOTICKS,
                 lm + 75, y, cw - 130, 24, IDC_OPACITY_SLIDER, hf);
        SendDlgItemMessageW(hDlg, IDC_OPACITY_SLIDER, TBM_SETRANGE, TRUE, MAKELONG(0, 255));
        MakeCtrl(hDlg, L"STATIC", L"", WS_VISIBLE | SS_RIGHT, W - lm - 50, y + 3, 50, 16, IDC_OPACITY_VALUE, hf);
        y += 30;

        // Glow slider
        MakeCtrl(hDlg, L"STATIC", L"Glow:", WS_VISIBLE | SS_LEFT, lm, y + 3, 60, 16, 0, hf);
        MakeCtrl(hDlg, TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_NOTICKS,
                 lm + 75, y, cw - 130, 24, IDC_GLOW_SLIDER, hf);
        SendDlgItemMessageW(hDlg, IDC_GLOW_SLIDER, TBM_SETRANGE, TRUE, MAKELONG(0, 10));
        MakeCtrl(hDlg, L"STATIC", L"", WS_VISIBLE | SS_RIGHT, W - lm - 50, y + 3, 50, 16, IDC_GLOW_VALUE, hf);
        y += 36;

        // ── Behavior section ──
        MakeCtrl(hDlg, L"STATIC", L"BEHAVIOR", WS_VISIBLE | SS_LEFT, lm, y, cw, 16, 0, htf);
        y += 22;
        MakeCtrl(hDlg, L"BUTTON", L"Always on top", BS_AUTOCHECKBOX, lm, y, 130, 20, IDC_CHECK_ONTOP, hf);
        MakeCtrl(hDlg, L"BUTTON", L"Click-through", BS_AUTOCHECKBOX, lm + 130, y, 130, 20, IDC_CHECK_CLICKTHRU, hf);
        MakeCtrl(hDlg, L"BUTTON", L"Snap to edges", BS_AUTOCHECKBOX, lm + 260, y, 130, 20, IDC_CHECK_SNAP, hf);
        y += 26;
        MakeCtrl(hDlg, L"BUTTON", L"Start with Windows", BS_AUTOCHECKBOX, lm, y, 130, 20, IDC_CHECK_AUTOSTART, hf);
        MakeCtrl(hDlg, L"BUTTON", L"Hide desktop icons", BS_AUTOCHECKBOX, lm + 130, y, 160, 20, IDC_CHECK_HIDE_ICONS, hf);
        y += 32;

        // ── TODO List section ──
        MakeCtrl(hDlg, L"STATIC", L"TODO LIST", WS_VISIBLE | SS_LEFT, lm, y, cw, 16, 0, htf);
        y += 22;
        MakeCtrl(hDlg, L"BUTTON", L"Enable TODO widget", BS_AUTOCHECKBOX, lm, y, 180, 20, IDC_CHECK_TODO_ENABLED, hf);
        y += 26;
        // Listbox for TODO items
        MakeCtrl(hDlg, L"LISTBOX", L"",
            LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_VSCROLL | WS_BORDER,
            lm, y, cw, 90, IDC_TODO_LISTBOX, hf);
        SetWindowTheme(GetDlgItem(hDlg, IDC_TODO_LISTBOX), L"", L"");
        y += 94;
        // Input + Add/Remove/Check buttons
        MakeCtrl(hDlg, L"EDIT", L"",
            ES_AUTOHSCROLL | WS_BORDER,
            lm, y, cw - 96, 24, IDC_TODO_INPUT, hf);
        SetWindowTheme(GetDlgItem(hDlg, IDC_TODO_INPUT), L"", L"");
        MakeCtrl(hDlg, L"BUTTON", L"+", BS_PUSHBUTTON, lm + cw - 92, y, 28, 24, IDC_BTN_TODO_ADD, hbf);
        MakeCtrl(hDlg, L"BUTTON", L"\x2212", BS_PUSHBUTTON, lm + cw - 62, y, 28, 24, IDC_BTN_TODO_REMOVE, hf);
        MakeCtrl(hDlg, L"BUTTON", L"\x2713", BS_PUSHBUTTON, lm + cw - 32, y, 28, 24, IDC_BTN_TODO_CHECK, hf);
        y += 30;

        // Task Size slider
        MakeCtrl(hDlg, L"STATIC", L"Task Size:", WS_VISIBLE | SS_LEFT, lm, y + 3, 70, 16, 0, hf);
        MakeCtrl(hDlg, TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_NOTICKS,
                 lm + 75, y, cw - 130, 24, IDC_TODO_SIZE_SLIDER, hf);
        SendDlgItemMessageW(hDlg, IDC_TODO_SIZE_SLIDER, TBM_SETRANGE, TRUE, MAKELONG(14, 45));
        MakeCtrl(hDlg, L"STATIC", L"", WS_VISIBLE | SS_RIGHT, W - lm - 50, y + 3, 50, 16, IDC_TODO_SIZE_VALUE, hf);
        y += 30;

        // Card Style dropdown
        MakeCtrl(hDlg, L"STATIC", L"Card Style:", WS_VISIBLE | SS_LEFT, lm, y + 3, 70, 16, 0, hf);
        HWND hStyleCombo = MakeCtrl(hDlg, L"COMBOBOX", L"",
            CBS_DROPDOWNLIST | WS_VSCROLL, lm + 75, y, cw - 75, 120, IDC_TODO_STYLE_COMBO, hf);
        SendMessageW(hStyleCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Clock Matched (Fill)"));
        SendMessageW(hStyleCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Frosted Glass"));
        SendMessageW(hStyleCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Transparent / Borderless"));
        y += 34;

        // ── Alignment section ──
        MakeCtrl(hDlg, L"STATIC", L"SNAP ALIGNMENT", WS_VISIBLE | SS_LEFT, lm, y, cw, 16, 0, htf);
        MakeCtrl(hDlg, L"BUTTON", L"\x2196", BS_PUSHBUTTON, W - lm - 100, y - 4, 22, 22, IDC_ALIGN_TL, hf);
        MakeCtrl(hDlg, L"BUTTON", L"\x2197", BS_PUSHBUTTON, W - lm - 75, y - 4, 22, 22, IDC_ALIGN_TR, hf);
        MakeCtrl(hDlg, L"BUTTON", L"\x2199", BS_PUSHBUTTON, W - lm - 50, y - 4, 22, 22, IDC_ALIGN_BL, hf);
        MakeCtrl(hDlg, L"BUTTON", L"\x2198", BS_PUSHBUTTON, W - lm - 25, y - 4, 22, 22, IDC_ALIGN_BR, hf);
        y += 40;

        // ── Bottom buttons ──
        int btnW = 80;
        int btnH = 30;
        // Placed at 0,0 for now; WM_SIZE will move them
        MakeCtrl(hDlg, L"BUTTON", L"Apply", BS_PUSHBUTTON, 0, 0, btnW, btnH, IDC_BTN_APPLY, hbf);
        MakeCtrl(hDlg, L"BUTTON", L"Cancel", BS_PUSHBUTTON, 0, 0, btnW, btnH, IDC_BTN_CANCEL, hf);
        MakeCtrl(hDlg, L"BUTTON", L"OK", BS_PUSHBUTTON, 0, 0, btnW, btnH, IDC_BTN_OK, hbf);

        // ── Preview Canvas ──
        MakeCtrl(hDlg, L"ClockPreviewClass", L"", WS_VISIBLE | WS_CHILD, 0, 0, 10, 10, 4000, nullptr);

        // ── Populate controls ──
        SettingsToControls(hDlg, state->workingCopy);

        // ── Populate TODO listbox ──
        RefreshTodoListbox(hDlg, state->todoCopy);

        return 0;
    }

    case WM_SIZE: {
        int w = LOWORD(lParam);
        int h = HIWORD(lParam);
        
        int leftPaneW = 430;
        
        HWND hPreview = GetDlgItem(hDlg, 4000);
        if (hPreview) {
            SetWindowPos(hPreview, nullptr, leftPaneW, 0, w - leftPaneW, h - 50, SWP_NOZORDER);
            InvalidateRect(hPreview, nullptr, TRUE);
        }
        
        int btnW = 80, btnH = 30;
        int btnY = h - 40;
        HWND hApply = GetDlgItem(hDlg, IDC_BTN_APPLY);
        HWND hCancel = GetDlgItem(hDlg, IDC_BTN_CANCEL);
        HWND hOk = GetDlgItem(hDlg, IDC_BTN_OK);
        
        if (hApply) SetWindowPos(hApply, nullptr, w - btnW - 15, btnY, btnW, btnH, SWP_NOZORDER);
        if (hCancel) SetWindowPos(hCancel, nullptr, w - btnW * 2 - 25, btnY, btnW, btnH, SWP_NOZORDER);
        if (hOk) SetWindowPos(hOk, nullptr, w - btnW * 3 - 35, btnY, btnW, btnH, SWP_NOZORDER);
        
        return 0;
    }

    case WM_DRAWITEM: {
        auto* di = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        COLORREF color = RGB(255, 255, 255);
        if (state) {
            if (di->CtlID == IDC_BTN_TEXT_COLOR) color = state->workingCopy.textColor;
            else if (di->CtlID == IDC_BTN_GLOW_COLOR) color = state->workingCopy.glowColor;
            else if (di->CtlID == IDC_BTN_BG_COLOR) color = state->workingCopy.bgColor;
        }

        // Fill with color
        HBRUSH hBr = CreateSolidBrush(color);
        FillRect(di->hDC, &di->rcItem, hBr);
        DeleteObject(hBr);

        // Draw border
        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(100, 100, 120));
        HPEN hOldPen = static_cast<HPEN>(SelectObject(di->hDC, hPen));
        HBRUSH hOldBr = static_cast<HBRUSH>(SelectObject(di->hDC, GetStockObject(NULL_BRUSH)));
        Rectangle(di->hDC, di->rcItem.left, di->rcItem.top, di->rcItem.right, di->rcItem.bottom);
        SelectObject(di->hDC, hOldPen);
        SelectObject(di->hDC, hOldBr);
        DeleteObject(hPen);

        // Focus rect
        if (di->itemState & ODS_FOCUS) {
            RECT r = di->rcItem;
            InflateRect(&r, -2, -2);
            DrawFocusRect(di->hDC, &r);
        }
        return TRUE;
    }

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        HDC hdcCtrl = reinterpret_cast<HDC>(wParam);
        SetTextColor(hdcCtrl, DARK_TEXT);
        SetBkColor(hdcCtrl, DARK_BG);
        if (state) return reinterpret_cast<LRESULT>(state->hDarkBrush);
        break;
    }

    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        HDC hdcCtrl = reinterpret_cast<HDC>(wParam);
        SetTextColor(hdcCtrl, DARK_TEXT);
        SetBkColor(hdcCtrl, DARK_GROUP);
        if (state) return reinterpret_cast<LRESULT>(state->hGroupBrush);
        break;
    }

    case WM_ERASEBKGND: {
        HDC hdc2 = reinterpret_cast<HDC>(wParam);
        RECT rc;
        GetClientRect(hDlg, &rc);
        HBRUSH hBr = CreateSolidBrush(DARK_BG);
        FillRect(hdc2, &rc, hBr);
        DeleteObject(hBr);

        // Draw section separator lines
        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(55, 55, 70));
        HPEN hOldPen = static_cast<HPEN>(SelectObject(hdc2, hPen));
        // Lines are drawn at approximate section boundaries
        int separators[] = {66, 162, 372};
        for (int sy : separators) {
            MoveToEx(hdc2, 18, sy, nullptr);
            LineTo(hdc2, rc.right - 18, sy);
        }
        SelectObject(hdc2, hOldPen);
        DeleteObject(hPen);
        return TRUE;
    }

    case WM_HSCROLL: {
        if (!state) break;
        // Update slider values
        state->workingCopy.fontSize = static_cast<int>(
            SendDlgItemMessageW(hDlg, IDC_SIZE_SLIDER, TBM_GETPOS, 0, 0));
        state->workingCopy.dateFontSize = static_cast<int>(
            SendDlgItemMessageW(hDlg, IDC_DATE_SIZE_SLIDER, TBM_GETPOS, 0, 0));
        state->workingCopy.bgAlpha = static_cast<int>(
            SendDlgItemMessageW(hDlg, IDC_OPACITY_SLIDER, TBM_GETPOS, 0, 0));
        state->workingCopy.glowIntensity = static_cast<int>(
            SendDlgItemMessageW(hDlg, IDC_GLOW_SLIDER, TBM_GETPOS, 0, 0));
        state->workingCopy.todoFontSize = static_cast<int>(
            SendDlgItemMessageW(hDlg, IDC_TODO_SIZE_SLIDER, TBM_GETPOS, 0, 0));
        UpdateSliderLabels(hDlg, state->workingCopy);
        return 0;
    }

    case WM_COMMAND: {
        if (!state) break;
        int id = LOWORD(wParam);

        if (HIWORD(wParam) == CBN_SELCHANGE && (id == IDC_FONT_COMBO || id == IDC_TODO_STYLE_COMBO)) {
            ControlsToSettings(hDlg, state->workingCopy);
            InvalidateRect(GetDlgItem(hDlg, 4000), nullptr, FALSE);
            return 0;
        }

        switch (id) {
        case IDC_RADIO_12H:
        case IDC_RADIO_24H:
        case IDC_CHECK_SECONDS:
        case IDC_CHECK_DATE:
        case IDC_CHECK_DAY:
        case IDC_CHECK_ONTOP:
        case IDC_CHECK_CLICKTHRU:
        case IDC_CHECK_AUTOSTART:
        case IDC_CHECK_SNAP:
        case IDC_CHECK_HIDE_ICONS:
        case IDC_CHECK_TODO_ENABLED: {
            ControlsToSettings(hDlg, state->workingCopy);
            InvalidateRect(GetDlgItem(hDlg, 4000), nullptr, FALSE);
            if (id == IDC_CHECK_HIDE_ICONS) {
                if (IsDlgButtonChecked(hDlg, IDC_CHECK_HIDE_ICONS) == BST_CHECKED) {
                    MessageBoxW(hDlg, L"Warning: This will hide your desktop icons while the clock is running. They will be restored when the clock exits.",
                                L"Hide Desktop Icons", MB_ICONWARNING | MB_OK);
                }
            }
            if (id == IDC_CHECK_TODO_ENABLED) {
                EnableTodoControls(hDlg, state->workingCopy.todoEnabled);
            }
            return 0;
        }

        case IDC_PRESET_APPLY: {
            int idx = static_cast<int>(SendDlgItemMessageW(hDlg, IDC_PRESET_COMBO, CB_GETCURSEL, 0, 0));
            if (idx != CB_ERR) {
                const auto& presets = GetPresets();
                if (idx < static_cast<int>(presets.size())) {
                    ApplyPreset(state->workingCopy, presets[idx].name);
                    SettingsToControls(hDlg, state->workingCopy);
                    // Invalidate color buttons and preview
                    InvalidateRect(GetDlgItem(hDlg, IDC_BTN_TEXT_COLOR), nullptr, TRUE);
                    InvalidateRect(GetDlgItem(hDlg, IDC_BTN_GLOW_COLOR), nullptr, TRUE);
                    InvalidateRect(GetDlgItem(hDlg, IDC_BTN_BG_COLOR), nullptr, TRUE);
                    InvalidateRect(GetDlgItem(hDlg, 4000), nullptr, FALSE);
                }
            }
            return 0;
        }

        case IDC_ALIGN_TL:
        case IDC_ALIGN_TR:
        case IDC_ALIGN_BL:
        case IDC_ALIGN_BR: {
            // Send special message to parent to snap window now
            PostMessageW(GetParent(hDlg), WM_COMMAND, id, 0);
            return 0;
        }

        // ── TODO list buttons ──
        case IDC_BTN_TODO_ADD: {
            wchar_t buf[256] = {};
            GetDlgItemTextW(hDlg, IDC_TODO_INPUT, buf, 256);
            std::wstring text(buf);
            // Trim whitespace
            while (!text.empty() && text.back() == L' ') text.pop_back();
            while (!text.empty() && text.front() == L' ') text.erase(text.begin());
            if (!text.empty() && state->todoCopy.size() < 20) {
                TodoItem item;
                item.text = text;
                item.done = false;
                state->todoCopy.push_back(std::move(item));
                RefreshTodoListbox(hDlg, state->todoCopy);
                SetDlgItemTextW(hDlg, IDC_TODO_INPUT, L"");
                InvalidateRect(GetDlgItem(hDlg, 4000), nullptr, FALSE);
            }
            return 0;
        }
        case IDC_BTN_TODO_REMOVE: {
            HWND hList = GetDlgItem(hDlg, IDC_TODO_LISTBOX);
            int sel = static_cast<int>(SendMessageW(hList, LB_GETCURSEL, 0, 0));
            if (sel != LB_ERR && sel < static_cast<int>(state->todoCopy.size())) {
                state->todoCopy.erase(state->todoCopy.begin() + sel);
                RefreshTodoListbox(hDlg, state->todoCopy);
                InvalidateRect(GetDlgItem(hDlg, 4000), nullptr, FALSE);
            }
            return 0;
        }
        case IDC_BTN_TODO_CHECK: {
            HWND hList = GetDlgItem(hDlg, IDC_TODO_LISTBOX);
            int sel = static_cast<int>(SendMessageW(hList, LB_GETCURSEL, 0, 0));
            if (sel != LB_ERR && sel < static_cast<int>(state->todoCopy.size())) {
                state->todoCopy[sel].done = !state->todoCopy[sel].done;
                RefreshTodoListbox(hDlg, state->todoCopy);
                // Re-select the same index
                SendMessageW(hList, LB_SETCURSEL, sel, 0);
                InvalidateRect(GetDlgItem(hDlg, 4000), nullptr, FALSE);
            }
            return 0;
        }

        case IDC_BTN_TEXT_COLOR: {
            COLORREF c = PickColor(hDlg, state->workingCopy.textColor);
            state->workingCopy.textColor = c;
            InvalidateRect(GetDlgItem(hDlg, id), nullptr, TRUE);
            InvalidateRect(GetDlgItem(hDlg, 4000), nullptr, FALSE);
            return 0;
        }
        case IDC_BTN_GLOW_COLOR: {
            COLORREF c = PickColor(hDlg, state->workingCopy.glowColor);
            state->workingCopy.glowColor = c;
            InvalidateRect(GetDlgItem(hDlg, id), nullptr, TRUE);
            InvalidateRect(GetDlgItem(hDlg, 4000), nullptr, FALSE);
            return 0;
        }
        case IDC_BTN_BG_COLOR: {
            COLORREF c = PickColor(hDlg, state->workingCopy.bgColor);
            state->workingCopy.bgColor = c;
            InvalidateRect(GetDlgItem(hDlg, id), nullptr, TRUE);
            InvalidateRect(GetDlgItem(hDlg, 4000), nullptr, FALSE);
            return 0;
        }

        case IDC_BTN_OK:
            ControlsToSettings(hDlg, state->workingCopy);
            *(state->settings) = state->workingCopy;
            if (state->todos) *(state->todos) = state->todoCopy;
            SaveTodos(state->todoCopy);
            state->changed = true;
            DestroyWindow(hDlg);
            return 0;

        case IDC_BTN_APPLY:
            ControlsToSettings(hDlg, state->workingCopy);
            *(state->settings) = state->workingCopy;
            if (state->todos) *(state->todos) = state->todoCopy;
            SaveTodos(state->todoCopy);
            state->changed = true;
            // Send message to main window to update immediately
            PostMessageW(GetParent(hDlg), WM_SETTINGS_CHANGED, 0, 0);
            return 0;

        case IDC_BTN_CANCEL:
            DestroyWindow(hDlg);
            return 0;
        }
        break;
    }

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            DestroyWindow(hDlg);
            return 0;
        }
        break;

    case WM_DESTROY:
        if (state) {
            DeleteObject(state->hDarkBrush);
            DeleteObject(state->hGroupBrush);
            DeleteObject(state->hFont);
            DeleteObject(state->hBoldFont);
            DeleteObject(state->hTitleFont);
        }
        s_hSettingsDlg = nullptr;
        PostQuitMessage(0);
        return 0;

    case WM_USER + 101: {
        if (state && state->todos) {
            state->todoCopy = *(state->todos);
            RefreshTodoListbox(hDlg, state->todoCopy);
        }
        return 0;
    }

    case WM_CLOSE:
        DestroyWindow(hDlg);
        return 0;
    }

    return DefWindowProcW(hDlg, msg, wParam, lParam);
}

// ────────────────────────────────────────────────────────────────────
// Public API
// ────────────────────────────────────────────────────────────────────
bool ShowSettingsDialog(HWND parentHwnd, ClockSettings& settings, std::vector<TodoItem>* todos) {
    // Ensure common controls are loaded
    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(icc);
    icc.dwICC  = ICC_BAR_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    // Register preview canvas
    static bool previewRegistered = false;
    if (!previewRegistered) {
        WNDCLASSEXW pwc = {};
        pwc.cbSize = sizeof(pwc);
        pwc.style = CS_HREDRAW | CS_VREDRAW;
        pwc.lpfnWndProc = PreviewWndProc;
        pwc.hInstance = GetModuleHandleW(nullptr);
        pwc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        pwc.lpszClassName = L"ClockPreviewClass";
        RegisterClassExW(&pwc);
        previewRegistered = true;
    }

    // Register the settings window class
    static bool registered = false;
    const wchar_t* className = L"ClockSettingsDialog";
    if (!registered) {
        WNDCLASSEXW wc = {};
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = SettingsDlgProc;
        wc.hInstance      = GetModuleHandleW(nullptr);
        wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = className;
        wc.hbrBackground = nullptr; // We paint our own background
        RegisterClassExW(&wc);
        registered = true;
    }

    // Create dialog state
    DialogState state;
    state.settings    = &settings;
    state.workingCopy = settings;
    state.changed     = false;
    state.hDarkBrush  = nullptr;
    state.hGroupBrush = nullptr;
    state.hFont       = nullptr;
    state.hBoldFont   = nullptr;
    state.hTitleFont  = nullptr;
    state.todos       = todos;
    if (todos) {
        state.todoCopy = *todos;
    }

    // Calculate dialog position centered on screen (clamp to screen dimensions)
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int dlgW = (std::min)(900, screenW - 40);
    int dlgH = (std::min)(860, screenH - 60);
    int dlgX = (screenW - dlgW) / 2;
    int dlgY = (screenH - dlgH) / 2;

    HWND hDlg = CreateWindowExW(
        WS_EX_TOPMOST,
        className,
        L"⚙ Clock Settings",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        dlgX, dlgY, dlgW, dlgH,
        parentHwnd, nullptr,
        GetModuleHandleW(nullptr),
        &state
    );

    if (!hDlg) return false;

    // Run a modal message loop
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessageW(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    return state.changed;
}
