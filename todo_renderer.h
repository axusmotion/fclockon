#pragma once

#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <vector>
#include "settings.h"

class TodoRenderer {
public:
    TodoRenderer();
    ~TodoRenderer();

    void Initialize(HWND hwnd);
    void UpdateSettings(const ClockSettings& settings);
    void SetTodoItems(const std::vector<TodoItem>& items);
    void Render();
    void GetWindowSize(int& width, int& height) const;

    // Returns index of the item whose checkbox was clicked, or -1
    int HitTestCheckbox(int x, int y) const;

    // Returns index of item at client-area (x, y), or -1
    int HitTestItem(int x, int y) const;

    void SetHoveredIndex(int index) { m_hoveredIndex = index; }
    int GetHoveredIndex() const { return m_hoveredIndex; }

private:
    HWND                  m_hwnd = nullptr;
    ClockSettings         m_settings;
    std::vector<TodoItem> m_todos;
    int                   m_width  = 0;
    int                   m_height = 0;
    int                   m_hoveredIndex = -1;

    // Layout metrics (computed in CalculateSize)
    float m_lineH        = 28.0f;
    float m_headerH      = 34.0f;
    float m_padX         = 14.0f;
    float m_padY         = 10.0f;
    float m_firstItemY   = 0.0f;
    float m_checkboxSize = 16.0f;

    void CalculateSize();
    void DrawRoundedRect(Gdiplus::Graphics& g, Gdiplus::RectF rect,
                         float radius, Gdiplus::Color fillColor, Gdiplus::Color borderColor);
};
