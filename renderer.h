#pragma once

#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include "settings.h"

class ClockRenderer {
public:
    ClockRenderer();
    ~ClockRenderer();

    void Initialize(HWND hwnd);
    void UpdateSettings(const ClockSettings& settings);
    void Render();
    void RenderToGraphics(Gdiplus::Graphics& graphics, bool clearBackground = true);
    void GetWindowSize(int& width, int& height) const;

private:
    HWND             m_hwnd       = nullptr;
    ClockSettings    m_settings;
    int              m_width      = 400;
    int              m_height     = 120;
    int              m_padding    = 20;

    void CalculateSize();
    void DrawRoundedRect(Gdiplus::Graphics& g, Gdiplus::RectF rect,
                         float radius, Gdiplus::Color fillColor);
    void DrawGlowText(Gdiplus::Graphics& g, const std::wstring& text,
                      const Gdiplus::Font& font, Gdiplus::PointF origin,
                      Gdiplus::Color textColor, Gdiplus::Color glowColor,
                      int glowRadius);
};
