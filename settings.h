#pragma once

#include <windows.h>
#include <string>
#include <vector>

// ── Color helper ──
inline std::string ColorToHex(COLORREF c) {
    char buf[8];
    snprintf(buf, sizeof(buf), "#%02X%02X%02X", GetRValue(c), GetGValue(c), GetBValue(c));
    return buf;
}

inline COLORREF HexToColor(const std::string& hex) {
    if (hex.size() < 7 || hex[0] != '#') return RGB(255, 255, 255);
    int r = (int)strtol(hex.substr(1, 2).c_str(), nullptr, 16);
    int g = (int)strtol(hex.substr(3, 2).c_str(), nullptr, 16);
    int b = (int)strtol(hex.substr(5, 2).c_str(), nullptr, 16);
    return RGB(r, g, b);
}

// ── Narrow ↔ Wide string conversion ──
inline std::string WideToNarrow(const std::wstring& ws) {
    if (ws.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), nullptr, 0, nullptr, nullptr);
    std::string s(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), &s[0], len, nullptr, nullptr);
    return s;
}

inline std::wstring NarrowToWide(const std::string& s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring ws(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &ws[0], len);
    return ws;
}

// ── Clock Settings ──
struct ClockSettings {
    // Time format
    bool use24Hour      = false;
    bool showSeconds    = true;
    bool showDate       = true;
    bool showDayOfWeek  = true;

    // Appearance
    std::wstring fontFamily = L"Segoe UI";
    int fontSize        = 52;
    bool fontBold       = true;
    COLORREF textColor  = RGB(0, 255, 255);
    COLORREF glowColor  = RGB(0, 180, 255);
    COLORREF bgColor    = RGB(12, 12, 20);
    int bgAlpha         = 180;        // 0–255
    int glowIntensity   = 5;          // 0–10
    int dateFontSize    = 16;

    // Behavior
    bool alwaysOnTop    = false;
    bool clickThrough   = false;
    bool autoStart      = false;
    bool snapToEdges    = true;
    bool hideDesktopIcons = false;

    // Window position
    int posX            = -1;  // -1 = auto-center
    int posY            = -1;

    // Active preset name
    std::wstring presetName = L"Midnight Neon";
};

// ── Theme Preset ──
struct ThemePreset {
    std::wstring name;
    std::wstring fontFamily;
    int fontSize;
    bool fontBold;
    COLORREF textColor;
    COLORREF glowColor;
    COLORREF bgColor;
    int bgAlpha;
    int glowIntensity;
    int dateFontSize;
};

// ── Functions ──
void            LoadSettings(ClockSettings& settings);
void            SaveSettings(const ClockSettings& settings);
void            ApplyPreset(ClockSettings& settings, const std::wstring& presetName);
const std::vector<ThemePreset>& GetPresets();
void            SetAutoStart(bool enable);
std::wstring    GetConfigDir();
