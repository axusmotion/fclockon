#include "settings.h"
#include "json_util.h"
#include <shlobj.h>
#include <fstream>
#include <cstdio>

// ────────────────────────────────────────────────────────────────────
// Built-in theme presets
// ────────────────────────────────────────────────────────────────────
static const std::vector<ThemePreset> g_presets = {
    {
        L"Midnight Neon",
        L"Segoe UI",
        52, true,
        RGB(0, 255, 255),       // cyan text
        RGB(0, 180, 255),       // blue glow
        RGB(12, 12, 20),        // near-black bg
        180, 5, 16
    },
    {
        L"Frosted Glass",
        L"Segoe UI Light",
        48, false,
        RGB(240, 240, 250),     // white text
        RGB(180, 200, 255),     // soft blue glow
        RGB(200, 210, 230),     // light gray-blue bg
        100, 3, 15
    },
    {
        L"Minimal White",
        L"Segoe UI Semibold",
        56, true,
        RGB(255, 255, 255),     // white text
        RGB(255, 255, 255),     // white glow
        RGB(0, 0, 0),           // black bg (invisible with low alpha)
        0, 1, 16
    },
    {
        L"Sunset Warm",
        L"Georgia",
        50, true,
        RGB(255, 180, 50),      // orange-gold text
        RGB(255, 100, 30),      // deep orange glow
        RGB(40, 15, 10),        // dark warm bg
        190, 6, 15
    },
    {
        L"Matrix Green",
        L"Consolas",
        48, true,
        RGB(0, 255, 65),        // bright green text
        RGB(0, 200, 40),        // green glow
        RGB(5, 10, 5),          // very dark green bg
        200, 7, 14
    },
    {
        L"Rose Gold",
        L"Segoe UI Light",
        50, false,
        RGB(240, 185, 170),     // rose-pink text
        RGB(220, 150, 130),     // warm rose glow
        RGB(35, 18, 22),        // dark wine bg
        185, 4, 15
    },
    {
        L"Forest Glass",
        L"Segoe UI",
        96, false,
        RGB(210, 240, 210),     // text (light green)
        RGB(50, 180, 80),       // glow (forest green)
        RGB(20, 40, 20),        // bg (dark green)
        140, 4, 28
    },
    {
        L"Cyberpunk",
        L"Segoe UI",
        110, true,
        RGB(255, 255, 0),       // text (neon yellow)
        RGB(255, 0, 255),       // glow (neon pink)
        RGB(10, 10, 25),        // bg (dark purple)
        180, 6, 32
    },
    {
        L"Ocean Blue",
        L"Segoe UI",
        100, false,
        RGB(230, 240, 255),     // text (ice white)
        RGB(0, 120, 255),       // glow (deep blue)
        RGB(15, 25, 45),        // bg (navy)
        100, 5, 30
    },
    {
        L"Vintage Warm",
        L"Segoe UI",
        90, true,
        RGB(255, 215, 150),     // text (warm white)
        RGB(200, 100, 20),      // glow (orange)
        RGB(40, 20, 10),        // bg (brown)
        160, 3, 26
    }
};

const std::vector<ThemePreset>& GetPresets() {
    return g_presets;
}

// ────────────────────────────────────────────────────────────────────
// Config directory
// ────────────────────────────────────────────────────────────────────
std::wstring GetConfigDir() {
    wchar_t* appDataPath = nullptr;
    std::wstring dir;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appDataPath))) {
        dir = appDataPath;
        std::wstring newDir = dir + L"\\FClockOn";
        std::wstring oldDir = dir + L"\\WindowsClock";
        if (GetFileAttributesW(newDir.c_str()) != INVALID_FILE_ATTRIBUTES) {
            dir = newDir;
        } else if (GetFileAttributesW(oldDir.c_str()) != INVALID_FILE_ATTRIBUTES) {
            dir = oldDir;
        } else {
            dir = newDir;
        }
        CoTaskMemFree(appDataPath);
    } else {
        // Fallback to current directory
        wchar_t buf[MAX_PATH];
        GetModuleFileNameW(nullptr, buf, MAX_PATH);
        dir = buf;
        size_t pos = dir.find_last_of(L'\\');
        if (pos != std::wstring::npos) dir = dir.substr(0, pos);
    }
    // Create directory if it doesn't exist
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir;
}

static std::string GetConfigFilePath() {
    std::wstring dir = GetConfigDir();
    dir += L"\\config.json";
    return WideToNarrow(dir);
}

// ────────────────────────────────────────────────────────────────────
// Load / Save
// ────────────────────────────────────────────────────────────────────
void LoadSettings(ClockSettings& settings) {
    std::string path = GetConfigFilePath();
    JsonConfig cfg;
    if (!cfg.loadFromFile(path)) {
        // No config file yet — use defaults (Midnight Neon)
        return;
    }

    settings.use24Hour     = cfg.getBool("use24Hour", settings.use24Hour);
    settings.showSeconds   = cfg.getBool("showSeconds", settings.showSeconds);
    settings.showDate      = cfg.getBool("showDate", settings.showDate);
    settings.showDayOfWeek = cfg.getBool("showDayOfWeek", settings.showDayOfWeek);

    std::string font = cfg.getString("fontFamily");
    if (!font.empty()) settings.fontFamily = NarrowToWide(font);

    settings.fontSize      = cfg.getInt("fontSize", settings.fontSize);
    settings.fontBold      = cfg.getBool("fontBold", settings.fontBold);

    std::string tc = cfg.getString("textColor");
    if (!tc.empty()) settings.textColor = HexToColor(tc);
    std::string gc = cfg.getString("glowColor");
    if (!gc.empty()) settings.glowColor = HexToColor(gc);
    std::string bc = cfg.getString("bgColor");
    if (!bc.empty()) settings.bgColor = HexToColor(bc);

    settings.bgAlpha       = cfg.getInt("bgAlpha", settings.bgAlpha);
    settings.glowIntensity = cfg.getInt("glowIntensity", settings.glowIntensity);
    settings.dateFontSize  = cfg.getInt("dateFontSize", settings.dateFontSize);

    settings.alwaysOnTop   = cfg.getBool("alwaysOnTop", settings.alwaysOnTop);
    settings.clickThrough  = cfg.getBool("clickThrough", settings.clickThrough);
    settings.autoStart     = cfg.getBool("autoStart", settings.autoStart);
    settings.snapToEdges   = cfg.getBool("snapToEdges", settings.snapToEdges);
    settings.hideDesktopIcons = cfg.getBool("hideDesktopIcons", settings.hideDesktopIcons);
    settings.todoEnabled   = cfg.getBool("todoEnabled", settings.todoEnabled);
    settings.todoFontSize  = cfg.getInt("todoFontSize", settings.todoFontSize);
    settings.todoWidth     = cfg.getInt("todoWidth", settings.todoWidth);
    settings.todoPosX      = cfg.getInt("todoPosX", settings.todoPosX);
    settings.todoPosY      = cfg.getInt("todoPosY", settings.todoPosY);
    settings.todoStyle     = cfg.getInt("todoStyle", settings.todoStyle);

    settings.posX          = cfg.getInt("posX", settings.posX);
    settings.posY          = cfg.getInt("posY", settings.posY);

    std::string preset = cfg.getString("presetName");
    if (!preset.empty()) settings.presetName = NarrowToWide(preset);
}

void SaveSettings(const ClockSettings& settings) {
    JsonConfig cfg;

    cfg.setBool("use24Hour", settings.use24Hour);
    cfg.setBool("showSeconds", settings.showSeconds);
    cfg.setBool("showDate", settings.showDate);
    cfg.setBool("showDayOfWeek", settings.showDayOfWeek);

    cfg.setString("fontFamily", WideToNarrow(settings.fontFamily));
    cfg.setInt("fontSize", settings.fontSize);
    cfg.setBool("fontBold", settings.fontBold);

    cfg.setString("textColor", ColorToHex(settings.textColor));
    cfg.setString("glowColor", ColorToHex(settings.glowColor));
    cfg.setString("bgColor", ColorToHex(settings.bgColor));
    cfg.setInt("bgAlpha", settings.bgAlpha);
    cfg.setInt("glowIntensity", settings.glowIntensity);
    cfg.setInt("dateFontSize", settings.dateFontSize);

    cfg.setBool("alwaysOnTop", settings.alwaysOnTop);
    cfg.setBool("clickThrough", settings.clickThrough);
    cfg.setBool("autoStart", settings.autoStart);
    cfg.setBool("snapToEdges", settings.snapToEdges);
    cfg.setBool("hideDesktopIcons", settings.hideDesktopIcons);
    cfg.setBool("todoEnabled", settings.todoEnabled);
    cfg.setInt("todoFontSize", settings.todoFontSize);
    cfg.setInt("todoWidth", settings.todoWidth);
    cfg.setInt("todoPosX", settings.todoPosX);
    cfg.setInt("todoPosY", settings.todoPosY);
    cfg.setInt("todoStyle", settings.todoStyle);

    cfg.setInt("posX", settings.posX);
    cfg.setInt("posY", settings.posY);

    cfg.setString("presetName", WideToNarrow(settings.presetName));

    std::string path = GetConfigFilePath();
    cfg.saveToFile(path);
}

// ────────────────────────────────────────────────────────────────────
// Apply Preset
// ────────────────────────────────────────────────────────────────────
void ApplyPreset(ClockSettings& settings, const std::wstring& presetName) {
    for (const auto& p : g_presets) {
        if (p.name == presetName) {
            settings.fontFamily    = p.fontFamily;
            settings.fontSize      = p.fontSize;
            settings.fontBold      = p.fontBold;
            settings.textColor     = p.textColor;
            settings.glowColor     = p.glowColor;
            settings.bgColor       = p.bgColor;
            settings.bgAlpha       = p.bgAlpha;
            settings.glowIntensity = p.glowIntensity;
            settings.dateFontSize  = p.dateFontSize;
            settings.presetName    = presetName;
            return;
        }
    }
}

// ────────────────────────────────────────────────────────────────────
// Auto-start (registry)
// ────────────────────────────────────────────────────────────────────
void SetAutoStart(bool enable) {
    HKEY hKey = nullptr;
    LONG result = RegOpenKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0, KEY_SET_VALUE | KEY_QUERY_VALUE, &hKey
    );
    if (result != ERROR_SUCCESS) return;

    if (enable) {
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        RegSetValueExW(hKey, L"WindowsClock", 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(path),
                       static_cast<DWORD>((wcslen(path) + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(hKey, L"WindowsClock");
    }
    RegCloseKey(hKey);
}

// ────────────────────────────────────────────────────────────────────
// TODO Items — simple line-based file ("0|text" or "1|text")
// ────────────────────────────────────────────────────────────────────
static std::string GetTodosFilePath() {
    std::wstring dir = GetConfigDir();
    dir += L"\\todos.txt";
    return WideToNarrow(dir);
}

void LoadTodos(std::vector<TodoItem>& items) {
    items.clear();
    std::string path = GetTodosFilePath();
    std::ifstream file(path);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line) && items.size() < 20) {
        if (line.size() < 3) continue;  // minimum: "0|x"
        if (line[1] != '|') continue;
        bool done = (line[0] == '1');
        std::string text = line.substr(2);
        if (text.empty()) continue;
        TodoItem item;
        item.done = done;
        item.text = NarrowToWide(text);
        items.push_back(std::move(item));
    }
    file.close();
}

void SaveTodos(const std::vector<TodoItem>& items) {
    std::string path = GetTodosFilePath();
    std::ofstream file(path);
    if (!file.is_open()) return;

    for (const auto& item : items) {
        file << (item.done ? '1' : '0') << '|' << WideToNarrow(item.text) << '\n';
    }
    file.close();
}
