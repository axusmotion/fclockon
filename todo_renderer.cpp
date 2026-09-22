#include "todo_renderer.h"
#include <algorithm>
#include <memory>

#pragma comment(lib, "gdiplus.lib")

// Helper: font family fallback
static std::unique_ptr<Gdiplus::FontFamily> GetTodoFontFamily(const std::wstring& name) {
    auto family = std::make_unique<Gdiplus::FontFamily>(name.c_str());
    if (!family->IsAvailable()) {
        family = std::make_unique<Gdiplus::FontFamily>(L"Segoe UI");
    }
    return family;
}

static Gdiplus::Color MakeColor(COLORREF cr, int alpha = 255) {
    return Gdiplus::Color(
        static_cast<BYTE>(alpha),
        GetRValue(cr), GetGValue(cr), GetBValue(cr)
    );
}

TodoRenderer::TodoRenderer() {}
TodoRenderer::~TodoRenderer() {}

void TodoRenderer::Initialize(HWND hwnd) {
    m_hwnd = hwnd;
}

void TodoRenderer::UpdateSettings(const ClockSettings& settings) {
    m_settings = settings;
    CalculateSize();
}

void TodoRenderer::SetTodoItems(const std::vector<TodoItem>& items) {
    m_todos = items;
    CalculateSize();
}

void TodoRenderer::GetWindowSize(int& width, int& height) const {
    width  = m_width;
    height = m_height;
}

void TodoRenderer::CalculateSize() {
    Gdiplus::Bitmap measureBmp(1, 1, PixelFormat32bppARGB);
    Gdiplus::Graphics g(&measureBmp);

    auto family = GetTodoFontFamily(m_settings.fontFamily);
    float textFontSize = static_cast<float>((std::max)(14, (std::min)(45, m_settings.todoFontSize > 0 ? m_settings.todoFontSize : 20)));
    Gdiplus::Font itemFont(family.get(), textFontSize, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);

    m_checkboxSize = std::max(16.0f, textFontSize * 0.85f);
    m_lineH = textFontSize + std::max(10.0f, textFontSize * 0.55f);
    m_padX = std::max(16.0f, textFontSize * 0.85f);
    m_padY = std::max(12.0f, textFontSize * 0.60f);
    m_headerH = std::max(26.0f, textFontSize * 1.25f);
    m_firstItemY = m_padY + m_headerH + 6.0f;

    float maxTextW = 180.0f;
    for (const auto& item : m_todos) {
        Gdiplus::RectF bounds;
        g.MeasureString(item.text.c_str(), -1, &itemFont, Gdiplus::PointF(0, 0), &bounds);
        if (bounds.Width > maxTextW) maxTextW = bounds.Width;
    }

    float calculatedW = m_padX * 2.0f + m_checkboxSize + 14.0f + maxTextW + 16.0f;
    if (m_settings.todoWidth > 0) {
        m_width = (std::max)(220, (std::min)(700, m_settings.todoWidth));
    } else {
        m_width = static_cast<int>(std::max(280.0f, std::min(600.0f, calculatedW)));
    }

    size_t count = m_todos.empty() ? 1 : m_todos.size();
    float totalH = m_firstItemY + count * m_lineH + m_padY + 2.0f;
    m_height = static_cast<int>(totalH);
}

int TodoRenderer::HitTestCheckbox(int x, int y) const {
    if (m_todos.empty()) return -1;
    for (size_t i = 0; i < m_todos.size(); i++) {
        float itemY = m_firstItemY + static_cast<float>(i) * m_lineH;
        float hitLeft   = 0.0f;
        float hitRight  = m_padX + m_checkboxSize + 16.0f;
        float hitTop    = itemY;
        float hitBottom = itemY + m_lineH;
        if (x >= hitLeft && x <= hitRight && y >= hitTop && y <= hitBottom) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int TodoRenderer::HitTestItem(int x, int y) const {
    if (m_todos.empty()) return -1;
    for (size_t i = 0; i < m_todos.size(); i++) {
        float itemY = m_firstItemY + static_cast<float>(i) * m_lineH;
        if (x >= 0 && x <= m_width && y >= itemY && y < itemY + m_lineH) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void TodoRenderer::DrawRoundedRect(Gdiplus::Graphics& g, Gdiplus::RectF rect,
                                   float radius, Gdiplus::Color fillColor, Gdiplus::Color borderColor) {
    Gdiplus::GraphicsPath path;
    float d = radius * 2.0f;
    path.AddArc(rect.X, rect.Y, d, d, 180.0f, 90.0f);
    path.AddArc(rect.X + rect.Width - d, rect.Y, d, d, 270.0f, 90.0f);
    path.AddArc(rect.X + rect.Width - d, rect.Y + rect.Height - d, d, d, 0.0f, 90.0f);
    path.AddArc(rect.X, rect.Y + rect.Height - d, d, d, 90.0f, 90.0f);
    path.CloseFigure();

    if (fillColor.GetA() > 0) {
        Gdiplus::SolidBrush brush(fillColor);
        g.FillPath(&brush, &path);
    }

    if (borderColor.GetA() > 0) {
        Gdiplus::Pen pen(borderColor, 1.0f);
        g.DrawPath(&pen, &path);
    }
}

void TodoRenderer::Render() {
    if (!m_hwnd) return;

    CalculateSize();
    if (m_width <= 0 || m_height <= 0) return;

    // Create 32bpp ARGB bitmap
    Gdiplus::Bitmap bmp(m_width, m_height, PixelFormat32bppARGB);
    Gdiplus::Graphics g(&bmp);

    g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
    g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
    g.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);
    g.Clear(Gdiplus::Color(0, 0, 0, 0));

    float textFontSize = static_cast<float>((std::max)(14, (std::min)(45, m_settings.todoFontSize > 0 ? m_settings.todoFontSize : 20)));

    // ── Card Style Customization ──
    Gdiplus::RectF bgRect(2.0f, 2.0f,
                           static_cast<float>(m_width - 4),
                           static_cast<float>(m_height - 4));

    if (m_settings.todoStyle == 0) {
        // Style 0: Clock Matched (Fill)
        if (m_settings.bgAlpha > 0) {
            Gdiplus::Color fillColor = MakeColor(m_settings.bgColor, m_settings.bgAlpha);
            Gdiplus::Color borderColor(
                static_cast<BYTE>((std::min)(fillColor.GetA() + 30, 255)),
                static_cast<BYTE>((std::min)((int)fillColor.GetR() + 40, 255)),
                static_cast<BYTE>((std::min)((int)fillColor.GetG() + 40, 255)),
                static_cast<BYTE>((std::min)((int)fillColor.GetB() + 40, 255))
            );
            DrawRoundedRect(g, bgRect, 14.0f, fillColor, borderColor);
        }
    } else if (m_settings.todoStyle == 1) {
        // Style 1: Frosted Glass
        // Base translucent dark tint
        Gdiplus::Color baseColor(140, 16, 18, 26);
        Gdiplus::Color glassBorder(70, 255, 255, 255);
        DrawRoundedRect(g, bgRect, 14.0f, baseColor, glassBorder);

        // Specular vertical linear gradient highlight
        Gdiplus::GraphicsPath glassPath;
        float d = 14.0f * 2.0f;
        glassPath.AddArc(bgRect.X, bgRect.Y, d, d, 180.0f, 90.0f);
        glassPath.AddArc(bgRect.X + bgRect.Width - d, bgRect.Y, d, d, 270.0f, 90.0f);
        glassPath.AddArc(bgRect.X + bgRect.Width - d, bgRect.Y + bgRect.Height - d, d, d, 0.0f, 90.0f);
        glassPath.AddArc(bgRect.X, bgRect.Y + bgRect.Height - d, d, d, 90.0f, 90.0f);
        glassPath.CloseFigure();

        Gdiplus::LinearGradientBrush glassGrad(
            Gdiplus::PointF(0.0f, bgRect.Y),
            Gdiplus::PointF(0.0f, bgRect.Y + bgRect.Height),
            Gdiplus::Color(52, 255, 255, 255),
            Gdiplus::Color(8, 255, 255, 255)
        );
        g.FillPath(&glassGrad, &glassPath);
    }
    // Style 2: Transparent / Borderless — no card background drawn

    // ── Header (Tasks · Status) ──
    auto family = GetTodoFontFamily(m_settings.fontFamily);
    float headerTitleSize = std::max(12.0f, textFontSize * 0.70f);
    float headerCountSize = std::max(11.0f, textFontSize * 0.60f);
    Gdiplus::Font headerTitleFont(family.get(), headerTitleSize, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
    Gdiplus::Font headerCountFont(family.get(), headerCountSize, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);

    int remaining = 0;
    for (const auto& item : m_todos) {
        if (!item.done) remaining++;
    }

    std::wstring titleText = L"Tasks";
    std::wstring countText;
    if (m_todos.empty()) {
        countText = L"· 0 items";
    } else if (remaining == 0) {
        countText = L"· All done \x2713";
    } else {
        countText = L"· " + std::to_wstring(remaining) + L" left";
    }

    Gdiplus::RectF titleBounds;
    g.MeasureString(titleText.c_str(), -1, &headerTitleFont, Gdiplus::PointF(0, 0), &titleBounds);

    if (m_settings.todoStyle == 2) {
        // Drop shadow for header in Transparent mode
        Gdiplus::SolidBrush shadowBrush(Gdiplus::Color(140, 0, 0, 0));
        g.DrawString(titleText.c_str(), -1, &headerTitleFont,
                     Gdiplus::PointF(m_padX + 1.0f, m_padY + 2.0f), &shadowBrush);
        g.DrawString(countText.c_str(), -1, &headerCountFont,
                     Gdiplus::PointF(m_padX + titleBounds.Width + 9.0f, m_padY + 3.5f), &shadowBrush);
    }

    Gdiplus::SolidBrush titleBrush(MakeColor(m_settings.textColor, 230));
    g.DrawString(titleText.c_str(), -1, &headerTitleFont,
                 Gdiplus::PointF(m_padX, m_padY + 1.0f), &titleBrush);

    Gdiplus::SolidBrush countBrush(MakeColor(m_settings.textColor, 130));
    g.DrawString(countText.c_str(), -1, &headerCountFont,
                 Gdiplus::PointF(m_padX + titleBounds.Width + 8.0f, m_padY + 2.5f), &countBrush);

    // Subtle divider line
    float sepY = m_padY + m_headerH;
    if (m_settings.todoStyle == 2) {
        Gdiplus::Pen shadowPen(Gdiplus::Color(50, 0, 0, 0), 1.0f);
        g.DrawLine(&shadowPen, m_padX, sepY + 1.0f, static_cast<float>(m_width) - m_padX, sepY + 1.0f);
        Gdiplus::Pen sepPen(MakeColor(m_settings.textColor, 20), 1.0f);
        g.DrawLine(&sepPen, m_padX, sepY, static_cast<float>(m_width) - m_padX, sepY);
    } else {
        Gdiplus::Pen sepPen(MakeColor(m_settings.textColor, 28), 1.0f);
        g.DrawLine(&sepPen, m_padX, sepY, static_cast<float>(m_width) - m_padX, sepY);
    }

    // ── Task List ──
    Gdiplus::Font itemFont(family.get(), textFontSize, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);

    if (m_todos.empty()) {
        if (m_settings.todoStyle == 2) {
            Gdiplus::SolidBrush shadowBrush(Gdiplus::Color(140, 0, 0, 0));
            g.DrawString(L"No tasks added", -1, &itemFont,
                         Gdiplus::PointF(m_padX + 1.0f, m_firstItemY + 3.0f), &shadowBrush);
        }
        Gdiplus::SolidBrush emptyBrush(MakeColor(m_settings.textColor, 100));
        g.DrawString(L"No tasks added", -1, &itemFont,
                     Gdiplus::PointF(m_padX, m_firstItemY + 2.0f), &emptyBrush);
    } else {
        for (size_t i = 0; i < m_todos.size(); i++) {
            const auto& item = m_todos[i];
            float itemY = m_firstItemY + static_cast<float>(i) * m_lineH;
            bool isHovered = (static_cast<int>(i) == m_hoveredIndex);

            // Subtle row hover highlight pill
            if (isHovered) {
                Gdiplus::RectF hoverRowRect(m_padX - 8.0f, itemY + 2.0f,
                                            static_cast<float>(m_width) - (m_padX - 8.0f) * 2.0f,
                                            m_lineH - 4.0f);
                DrawRoundedRect(g, hoverRowRect, 6.0f,
                                Gdiplus::Color(18, 255, 255, 255),
                                Gdiplus::Color(25, 255, 255, 255));
            }

            // Checkbox position
            float cbX = m_padX;
            float cbY = itemY + (m_lineH - m_checkboxSize) / 2.0f;
            Gdiplus::RectF cbRect(cbX, cbY, m_checkboxSize, m_checkboxSize);

            // In transparent mode, draw soft drop shadow for checkbox circle
            if (m_settings.todoStyle == 2) {
                Gdiplus::Pen shadowCirclePen(Gdiplus::Color(90, 0, 0, 0), 1.5f);
                g.DrawEllipse(&shadowCirclePen, Gdiplus::RectF(cbX + 1.0f, cbY + 1.0f, m_checkboxSize, m_checkboxSize));
            }

            float checkThickness = std::max(1.6f, textFontSize * 0.08f);

            if (item.done) {
                // Completed: soft outline + crisp checkmark
                Gdiplus::Color fillCol = isHovered ? MakeColor(m_settings.textColor, 38)
                                                   : MakeColor(m_settings.textColor, 25);
                Gdiplus::SolidBrush cbFill(fillCol);
                g.FillEllipse(&cbFill, cbRect);

                Gdiplus::Color penCol = isHovered ? MakeColor(m_settings.textColor, 220)
                                                  : MakeColor(m_settings.textColor, 110);
                Gdiplus::Pen cbPen(penCol, 1.4f);
                g.DrawEllipse(&cbPen, cbRect);

                // High-contrast checkmark matching the text color
                Gdiplus::Color checkCol = isHovered ? Gdiplus::Color(255, 255, 255, 255)
                                                    : MakeColor(m_settings.textColor, 225);
                Gdiplus::Pen checkPen(checkCol, checkThickness);
                checkPen.SetStartCap(Gdiplus::LineCapRound);
                checkPen.SetEndCap(Gdiplus::LineCapRound);
                g.DrawLine(&checkPen,
                           cbX + m_checkboxSize * 0.28f, cbY + m_checkboxSize * 0.52f,
                           cbX + m_checkboxSize * 0.46f, cbY + m_checkboxSize * 0.72f);
                g.DrawLine(&checkPen,
                           cbX + m_checkboxSize * 0.46f, cbY + m_checkboxSize * 0.72f,
                           cbX + m_checkboxSize * 0.74f, cbY + m_checkboxSize * 0.30f);
            } else {
                // Active: clean circular outline
                if (isHovered) {
                    // Hover state: circle illuminates + faint interior glow
                    Gdiplus::SolidBrush cbHoverFill(MakeColor(m_settings.textColor, 20));
                    g.FillEllipse(&cbHoverFill, cbRect);

                    Gdiplus::Pen cbPen(MakeColor(m_settings.textColor, 245), 1.8f);
                    g.DrawEllipse(&cbPen, cbRect);

                    // Ghost checkmark (delicate preview of checking it off)
                    Gdiplus::Pen ghostPen(MakeColor(m_settings.textColor, 115), checkThickness);
                    ghostPen.SetStartCap(Gdiplus::LineCapRound);
                    ghostPen.SetEndCap(Gdiplus::LineCapRound);
                    g.DrawLine(&ghostPen,
                               cbX + m_checkboxSize * 0.28f, cbY + m_checkboxSize * 0.52f,
                               cbX + m_checkboxSize * 0.46f, cbY + m_checkboxSize * 0.72f);
                    g.DrawLine(&ghostPen,
                               cbX + m_checkboxSize * 0.46f, cbY + m_checkboxSize * 0.72f,
                               cbX + m_checkboxSize * 0.74f, cbY + m_checkboxSize * 0.30f);
                } else {
                    Gdiplus::Pen cbPen(MakeColor(m_settings.textColor, 140), 1.5f);
                    g.DrawEllipse(&cbPen, cbRect);
                }
            }

            // Task text
            float textX = cbX + m_checkboxSize + 12.0f;
            float textY = itemY + (m_lineH - textFontSize) / 2.0f - 1.0f;

            if (m_settings.todoStyle == 2) {
                // Drop shadow in transparent mode for maximum readability
                Gdiplus::SolidBrush shadowBrush(Gdiplus::Color(140, 0, 0, 0));
                g.DrawString(item.text.c_str(), -1, &itemFont, Gdiplus::PointF(textX + 1.0f, textY + 1.2f), &shadowBrush);
            }

            if (item.done) {
                // Completed: gently muted text + crisp, thin strikethrough
                Gdiplus::SolidBrush doneBrush(MakeColor(m_settings.textColor, 95));
                g.DrawString(item.text.c_str(), -1, &itemFont, Gdiplus::PointF(textX, textY), &doneBrush);

                Gdiplus::RectF itemTextBounds;
                g.MeasureString(item.text.c_str(), -1, &itemFont, Gdiplus::PointF(0, 0), &itemTextBounds);

                float strikeY = textY + itemTextBounds.Height * 0.52f;
                float strikeThickness = std::max(1.1f, textFontSize * 0.05f);
                Gdiplus::Pen strikePen(MakeColor(m_settings.textColor, 85), strikeThickness);
                g.DrawLine(&strikePen, textX, strikeY, textX + itemTextBounds.Width, strikeY);
            } else {
                // Active: crisp primary contrast
                Gdiplus::Color textCol = isHovered ? Gdiplus::Color(255, 255, 255, 255)
                                                   : MakeColor(m_settings.textColor, 245);
                Gdiplus::SolidBrush textBrush(textCol);
                g.DrawString(item.text.c_str(), -1, &itemFont, Gdiplus::PointF(textX, textY), &textBrush);
            }
        }
    }

    // ── Resize Grip Indicator (very subtle corner marks) ──
    Gdiplus::Pen gripPen(MakeColor(m_settings.textColor, 35), 1.2f);
    g.DrawLine(&gripPen, static_cast<float>(m_width) - 12.0f, static_cast<float>(m_height) - 6.0f,
                         static_cast<float>(m_width) - 6.0f,  static_cast<float>(m_height) - 12.0f);
    g.DrawLine(&gripPen, static_cast<float>(m_width) - 8.0f,  static_cast<float>(m_height) - 6.0f,
                         static_cast<float>(m_width) - 6.0f,  static_cast<float>(m_height) - 8.0f);

    // ── Update Layered Window ──
    HDC screenDC = GetDC(nullptr);
    HDC memDC    = CreateCompatibleDC(screenDC);
    HBITMAP hBmp = nullptr;
    bmp.GetHBITMAP(Gdiplus::Color(0, 0, 0, 0), &hBmp);
    HGDIOBJ oldBmp = SelectObject(memDC, hBmp);

    POINT ptSrc = { 0, 0 };
    SIZE  size  = { m_width, m_height };

    BLENDFUNCTION blend = {};
    blend.BlendOp             = AC_SRC_OVER;
    blend.BlendFlags          = 0;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat         = AC_SRC_ALPHA;

    UpdateLayeredWindow(
        m_hwnd,
        screenDC,
        nullptr,
        &size,
        memDC,
        &ptSrc,
        0,
        &blend,
        ULW_ALPHA
    );

    SelectObject(memDC, oldBmp);
    DeleteObject(hBmp);
    DeleteDC(memDC);
    ReleaseDC(nullptr, screenDC);
}
