#pragma once

#include <windows.h>
#include "settings.h"
#include <vector>

// Shows the settings dialog (modal). Returns true if settings were changed.
bool ShowSettingsDialog(HWND parentHwnd, ClockSettings& settings, std::vector<TodoItem>* todos = nullptr);

// Returns the active settings dialog handle if open, or nullptr
HWND GetSettingsDialogHwnd();

