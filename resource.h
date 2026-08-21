#pragma once

// Application
#define IDI_APPICON             101

// Tray icon
#define WM_TRAYICON             (WM_USER + 1)
#define ID_TRAY_SETTINGS        2001
#define ID_TRAY_EXIT            2002
#define ID_TRAY_TOGGLE_ONTOP    2003
#define ID_TRAY_TOGGLE_CLICK    2004
#define ID_TRAY_PRESET_FIRST    2100
#define ID_TRAY_PRESET_LAST     2119

// Settings dialog control IDs
#define IDC_PRESET_COMBO        3001
#define IDC_PRESET_APPLY        3002
#define IDC_RADIO_12H           3003
#define IDC_RADIO_24H           3004
#define IDC_CHECK_SECONDS       3005
#define IDC_CHECK_DATE          3006
#define IDC_CHECK_DAY           3007
#define IDC_FONT_COMBO          3008
#define IDC_SIZE_SLIDER         3009
#define IDC_SIZE_VALUE          3010
#define IDC_BTN_TEXT_COLOR      3011
#define IDC_BTN_GLOW_COLOR      3012
#define IDC_BTN_BG_COLOR        3013
#define IDC_OPACITY_SLIDER      3014
#define IDC_OPACITY_VALUE       3015
#define IDC_GLOW_SLIDER         3016
#define IDC_GLOW_VALUE          3017
#define IDC_CHECK_ONTOP         3018
#define IDC_CHECK_CLICKTHRU     3019
#define IDC_CHECK_AUTOSTART     3020
#define IDC_BTN_OK              3021
#define IDC_BTN_CANCEL          3022
#define IDC_BTN_APPLY           3023
#define IDC_DATE_SIZE_SLIDER    3024
#define IDC_DATE_SIZE_VALUE     3025
#define IDC_CHECK_SNAP          3026
#define IDC_CHECK_HIDE_ICONS    3027
#define IDC_ALIGN_TL            3028
#define IDC_ALIGN_TR            3029
#define IDC_ALIGN_BL            3030
#define IDC_ALIGN_BR            3031

// Custom messages
#define WM_SETTINGS_CHANGED     (WM_USER + 100)
