#include "renderer.h"
#include <ctime>
#include <string>
#include <algorithm>
#include <memory>

#pragma comment(lib, "gdiplus.lib")

// Helper: get a valid font family, falling back to Segoe UI
static std::unique_ptr<Gdiplus::FontFamily> GetFontFamily(const std::wstring& name) {
    auto family = std::make_unique<Gdiplus::FontFamily>(name.c_str());
    if (!family->IsAvailable()) {
        family = std::make_unique<Gdiplus::FontFamily>(L"Segoe UI");
    }
    return family;
}

// ── Helper: COLORREF → GDI+ Color ──
static Gdiplus::Color MakeColor(COLORREF cr, int alpha = 255) {
    return Gdiplus::Color(
        static_cast<BYTE>(alpha),
        GetRValue(cr), GetGValue(cr), GetBValue(cr)
    );
}

// ────────────────────────────────────────────────────────────────────
ClockRenderer::ClockRenderer() {}
ClockRenderer::~ClockRenderer() {}

void ClockRenderer::Initialize(HWND hwnd) {
    m_hwnd = hwnd;
}

void ClockRenderer::UpdateSettings(const ClockSettings& settings) {
    m_settings = settings;
    CalculateSize();
}

void ClockRenderer::GetWindowSize(int& width, int& height) const {
    width  = m_width;
    height = m_height;
}

// ────────────────────────────────────────────────────────────────────
// Calculate the window size based on current font/format settings
// ────────────────────────────────────────────────────────────────────
void ClockRenderer::CalculateSize() {
    // Create a temporary bitmap to measure text
    Gdiplus::Bitmap measureBmp(1, 1, PixelFormat32bppARGB);
    Gdiplus::Graphics measureGfx(&measureBmp);

    int fontStyle = m_settings.fontBold ? Gdiplus::FontStyleBold : Gdiplus::FontStyleRegular;

    auto family = GetFontFamily(m_settings.fontFamily);
    Gdiplus::Font timeFont(family.get(), static_cast<Gdiplus::REAL>(m_settings.fontSize),
                           fontStyle, Gdiplus::UnitPixel);
    Gdiplus::Font dateFont(family.get(), static_cast<Gdiplus::REAL>(m_settings.dateFontSize),
                           Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);

    // Measure worst-case time string
    std::wstring worstTime;
    if (m_settings.use24Hour) {
        worstTime = m_settings.showSeconds ? L"23:59:59" : L"23:59";
    } else {
        worstTime = m_settings.showSeconds ? L"12:59:59 PM" : L"12:59 PM";
    }

    Gdiplus::RectF timeRect;
    measureGfx.MeasureString(worstTime.c_str(), -1, &timeFont, Gdiplus::PointF(0, 0), &timeRect);

    float totalWidth  = timeRect.Width;
    float totalHeight = timeRect.Height;

    // Measure date string if enabled
    if (m_settings.showDate || m_settings.showDayOfWeek) {
        std::wstring worstDate = L"Wednesday, September 30, 2026";
        Gdiplus::RectF dateRect;
        measureGfx.MeasureString(worstDate.c_str(), -1, &dateFont, Gdiplus::PointF(0, 0), &dateRect);
        totalWidth = (std::max)(totalWidth, dateRect.Width);
        totalHeight += dateRect.Height + 4.0f; // 4px gap between time and date
    }

    int glowMargin = m_settings.glowIntensity * 2;
    m_padding = 20 + glowMargin;

    m_width  = static_cast<int>(totalWidth) + m_padding * 2;
    m_height = static_cast<int>(totalHeight) + m_padding * 2;

    // Minimum size
    if (m_width < 200) m_width = 200;
    if (m_height < 60) m_height = 60;
}

// ────────────────────────────────────────────────────────────────────
// Draw a rounded rectangle
// ────────────────────────────────────────────────────────────────────
void ClockRenderer::DrawRoundedRect(Gdiplus::Graphics& g, Gdiplus::RectF rect,
                                     float radius, Gdiplus::Color fillColor) {
    Gdiplus::GraphicsPath path;
    float d = radius * 2.0f;

    path.AddArc(rect.X, rect.Y, d, d, 180, 90);
    path.AddArc(rect.X + rect.Width - d, rect.Y, d, d, 270, 90);
    path.AddArc(rect.X + rect.Width - d, rect.Y + rect.Height - d, d, d, 0, 90);
    path.AddArc(rect.X, rect.Y + rect.Height - d, d, d, 90, 90);
    path.CloseFigure();

    Gdiplus::SolidBrush brush(fillColor);
    g.FillPath(&brush, &path);

    // Subtle border
    Gdiplus::Color borderColor(
        static_cast<BYTE>((std::min)(fillColor.GetA() + 30, 255)),
        static_cast<BYTE>((std::min)((int)fillColor.GetR() + 40, 255)),
        static_cast<BYTE>((std::min)((int)fillColor.GetG() + 40, 255)),
        static_cast<BYTE>((std::min)((int)fillColor.GetB() + 40, 255))
    );
    Gdiplus::Pen borderPen(borderColor, 1.0f);
    g.DrawPath(&borderPen, &path);
}

// ────────────────────────────────────────────────────────────────────
// Draw text with glow effect using GraphicsPath
// ────────────────────────────────────────────────────────────────────
void ClockRenderer::DrawGlowText(Gdiplus::Graphics& g, const std::wstring& text,
                                  const Gdiplus::Font& font, Gdiplus::PointF origin,
                                  Gdiplus::Color textColor, Gdiplus::Color glowColor,
                                  int glowRadius) {
    Gdiplus::FontFamily family;
    font.GetFamily(&family);

    Gdiplus::GraphicsPath path;
    Gdiplus::StringFormat format;
    format.SetAlignment(Gdiplus::StringAlignmentCenter);
    format.SetLineAlignment(Gdiplus::StringAlignmentNear);

    Gdiplus::RectF layoutRect(0, origin.Y, static_cast<Gdiplus::REAL>(m_width), 
                               static_cast<Gdiplus::REAL>(m_height));

    path.AddString(text.c_str(), -1, &family,
                   font.GetStyle(), font.GetSize(),
                   layoutRect, &format);

    // Draw glow layers (outer to inner, increasing opacity)
    if (glowRadius > 0) {
        for (int i = glowRadius; i >= 1; i--) {
            int alpha = static_cast<int>(glowColor.GetA()) * (glowRadius - i + 1) / (glowRadius * 4);
            if (alpha < 1) alpha = 1;
            if (alpha > 255) alpha = 255;

            Gdiplus::Color layerColor(
                static_cast<BYTE>(alpha),
                glowColor.GetR(), glowColor.GetG(), glowColor.GetB()
            );
            Gdiplus::Pen pen(layerColor, static_cast<Gdiplus::REAL>(i * 2));
            pen.SetLineJoin(Gdiplus::LineJoinRound);
            g.DrawPath(&pen, &path);
        }
    }

    // Draw the text fill
    Gdiplus::SolidBrush textBrush(textColor);
    g.FillPath(&textBrush, &path);
}

// ────────────────────────────────────────────────────────────────────
// Main render function
// ────────────────────────────────────────────────────────────────────
void ClockRenderer::RenderToGraphics(Gdiplus::Graphics& graphics, bool clearBackground) {
    // High-quality rendering
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
    graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
    graphics.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);

    // Clear to fully transparent
    if (clearBackground) {
        graphics.Clear(Gdiplus::Color(0, 0, 0, 0));
    }

    // ── Draw background ──
    if (m_settings.bgAlpha > 0) {
        Gdiplus::RectF bgRect(2.0f, 2.0f,
                               static_cast<Gdiplus::REAL>(m_width - 4),
                               static_cast<Gdiplus::REAL>(m_height - 4));
        DrawRoundedRect(graphics, bgRect, 12.0f,
                        MakeColor(m_settings.bgColor, m_settings.bgAlpha));
    }

    // ── Prepare fonts ──
    int fontStyle = m_settings.fontBold ? Gdiplus::FontStyleBold : Gdiplus::FontStyleRegular;

    auto family = GetFontFamily(m_settings.fontFamily);
    Gdiplus::Font timeFont(family.get(), static_cast<Gdiplus::REAL>(m_settings.fontSize),
                           fontStyle, Gdiplus::UnitPixel);
    Gdiplus::Font dateFont(family.get(), static_cast<Gdiplus::REAL>(m_settings.dateFontSize),
                           Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);

    // ── Get current time ──
    time_t now = time(nullptr);
    struct tm localTime;
    localtime_s(&localTime, &now);

    // ── Format time string ──
    wchar_t timeBuf[32];
    if (m_settings.use24Hour) {
        if (m_settings.showSeconds)
            swprintf(timeBuf, 32, L"%02d:%02d:%02d", localTime.tm_hour, localTime.tm_min, localTime.tm_sec);
        else
            swprintf(timeBuf, 32, L"%02d:%02d", localTime.tm_hour, localTime.tm_min);
    } else {
        int hour = localTime.tm_hour % 12;
        if (hour == 0) hour = 12;
        const wchar_t* ampm = localTime.tm_hour >= 12 ? L"PM" : L"AM";
        if (m_settings.showSeconds)
            swprintf(timeBuf, 32, L"%d:%02d:%02d %s", hour, localTime.tm_min, localTime.tm_sec, ampm);
        else
            swprintf(timeBuf, 32, L"%d:%02d %s", hour, localTime.tm_min, ampm);
    }
    std::wstring timeStr(timeBuf);

    // ── Format date string ──
    std::wstring dateStr;
    if (m_settings.showDate || m_settings.showDayOfWeek) {
        static const wchar_t* dayNames[] = {
            L"Sunday", L"Monday", L"Tuesday", L"Wednesday",
            L"Thursday", L"Friday", L"Saturday"
        };
        static const wchar_t* monthNames[] = {
            L"January", L"February", L"March", L"April",
            L"May", L"June", L"July", L"August",
            L"September", L"October", L"November", L"December"
        };

        if (m_settings.showDayOfWeek) {
            dateStr += dayNames[localTime.tm_wday];
        }
        if (m_settings.showDate) {
            if (!dateStr.empty()) dateStr += L", ";
            dateStr += monthNames[localTime.tm_mon];
            dateStr += L" ";
            dateStr += std::to_wstring(localTime.tm_mday);
            dateStr += L", ";
            dateStr += std::to_wstring(1900 + localTime.tm_year);
        }
    }

    // ── Calculate text positions ──
    Gdiplus::RectF timeRect;
    graphics.MeasureString(timeStr.c_str(), -1, &timeFont, Gdiplus::PointF(0, 0), &timeRect);

    float timeY = static_cast<float>(m_padding);
    if (dateStr.empty()) {
        // Center vertically if no date
        timeY = (m_height - timeRect.Height) / 2.0f;
    }

    Gdiplus::Color textCol  = MakeColor(m_settings.textColor, 255);
    Gdiplus::Color glowCol  = MakeColor(m_settings.glowColor, 200);

    // ── Draw time ──
    DrawGlowText(graphics, timeStr, timeFont,
                 Gdiplus::PointF(0, timeY),
                 textCol, glowCol, m_settings.glowIntensity);

    // ── Draw date ──
    float contentBottomY = timeY + timeRect.Height;
    if (!dateStr.empty()) {
        float dateY = timeY + timeRect.Height + 4.0f;

        // Date uses a dimmer version of the text color
        Gdiplus::Color dateTextCol(
            200,
            static_cast<BYTE>((std::min)((int)textCol.GetR() + 20, 255)),
            static_cast<BYTE>((std::min)((int)textCol.GetG() + 20, 255)),
            static_cast<BYTE>((std::min)((int)textCol.GetB() + 20, 255))
        );
        Gdiplus::Color dateGlowCol(
            120,
            glowCol.GetR(), glowCol.GetG(), glowCol.GetB()
        );

        DrawGlowText(graphics, dateStr, dateFont,
                     Gdiplus::PointF(0, dateY),
                     dateTextCol, dateGlowCol,
                     (std::max)(m_settings.glowIntensity / 2, 1));

        Gdiplus::RectF dateRect;
        graphics.MeasureString(dateStr.c_str(), -1, &dateFont, Gdiplus::PointF(0, 0), &dateRect);
        contentBottomY = dateY + dateRect.Height;
    }
}

void ClockRenderer::Render() {
    if (!m_hwnd) return;

    CalculateSize();

    Gdiplus::Bitmap bitmap(m_width, m_height, PixelFormat32bppARGB);
    Gdiplus::Graphics graphics(&bitmap);

    RenderToGraphics(graphics, true);

    // ── Push to screen via UpdateLayeredWindow ──
    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem    = CreateCompatibleDC(hdcScreen);

    HBITMAP hBitmap = nullptr;
    bitmap.GetHBITMAP(Gdiplus::Color(0, 0, 0, 0), &hBitmap);
    HBITMAP hOldBitmap = static_cast<HBITMAP>(SelectObject(hdcMem, hBitmap));

    BLENDFUNCTION blend = {};
    blend.BlendOp             = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat         = AC_SRC_ALPHA;

    POINT ptSrc  = { 0, 0 };
    SIZE  sizeWnd = { m_width, m_height };

    UpdateLayeredWindow(m_hwnd, hdcScreen, nullptr, &sizeWnd,
                        hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);

    // Cleanup
    SelectObject(hdcMem, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
}


