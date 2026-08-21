#pragma once

#include <windows.h>
#include "settings.h"

// Shows the settings dialog (modal). Returns true if settings were changed.
bool ShowSettingsDialog(HWND parentHwnd, ClockSettings& settings);
