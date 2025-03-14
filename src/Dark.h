#pragma once

#include <windows.h>
#include <uxtheme.h>
#include <dwmapi.h>

#define DIALOG_HANDLE_CONTROL_MESSAGES() \
		case WM_CTLCOLORDLG: \
			if (g_bIsDark) \
				return DarkOnCtlColorDlg((HDC)wParam); \
			break; \
		case WM_CTLCOLORSTATIC: \
			if (g_bIsDark) \
				return DarkOnCtlColorStatic(hDlg, (HDC)wParam); \
			break; \
		case WM_CTLCOLORBTN: \
			if (g_bIsDark) \
				return DarkOnCtlColorBtn((HDC)wParam); \
			break; \
		case WM_CTLCOLOREDIT: \
			if (g_bIsDark) \
				return DarkOnCtlColorEdit((HDC)wParam); \
			break;

#define MSGBOX_HANDLE_CONTROL_MESSAGES() \
		case WM_CTLCOLORDLG: \
			if (g_bIsDark) \
				return DarkOnCtlColorDlg((HDC)wParam); \
			break; \
		case WM_CTLCOLORSTATIC: \
			if (g_bIsDark) \
				return DarkOnCtlColorStaticMsgBox(hDlg, (HDC)wParam); \
			break; \
		case WM_CTLCOLORBTN: \
			if (g_bIsDark) \
				return DarkOnCtlColorBtn((HDC)wParam); \
			break;

#define MAX_WIN_TEXT_LEN 256
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20

#define STATUSBAR_SUBCLASS_ID 1
#define CHECKBOX_SUBCLASS_ID 2
#define COMBOBOX_SUBCLASS_ID 3
#define COMBOBOXEX_SUBCLASS_ID 4

// Colors for dark mode
#define DARK_BG_COLOR 0x202020
#define DARK_TEXT_COLOR 0XE0E0E0
#define DARK_TEXT_COLOR_DISABLED 0x646464
#define DARK_TEXT_COLOR_LINK 0xE9BD5B
#define DARK_MENUBAR_BG_COLOR 0x202020
#define DARK_MENUBAR_BG_COLOR_HOT 0X3E3E3E
#define DARK_CONTROL_BG_COLOR 0x2B2B2B
#define DARK_CONTROL_BG_COLOR_HOT 0x454545
#define DARK_CONTROL_BG_COLOR_SELECTED 0x666666
#define DARK_CONTROL_BORDER_COLOR 0x9B9B9B
#define DARK_EDITOR_BG_COLOR 0x141414
#define DARK_EDITOR_TEXT_COLOR 0xFFFFFF
#define DARK_SEPARATOR_COLOR 0x636363
#define DARK_EDIT_BG_COLOR 0x383838

// window messages related to menu bar drawing
#define WM_UAHDESTROYWINDOW    0x0090	// handled by DefWindowProc
#define WM_UAHDRAWMENU         0x0091	// lParam is UAHMENU
#define WM_UAHDRAWMENUITEM     0x0092	// lParam is UAHDRAWMENUITEM
#define WM_UAHINITMENU         0x0093	// handled by DefWindowProc
#define WM_UAHMEASUREMENUITEM  0x0094	// lParam is UAHMEASUREMENUITEM
#define WM_UAHNCPAINTMENUPOPUP 0x0095	// handled by DefWindowProc

// describes the sizes of the menu bar or menu item
typedef union tagUAHMENUITEMMETRICS
{
	// cx appears to be 14 / 0xE less than rcItem's width!
	// cy 0x14 seems stable, i wonder if it is 4 less than rcItem's height which is always 24 atm
	struct
	{
		DWORD cx;
		DWORD cy;
	} rgsizeBar[2];
	struct
	{
		DWORD cx;
		DWORD cy;
	} rgsizePopup[4];
} UAHMENUITEMMETRICS;

// not really used in our case but part of the other structures
typedef struct tagUAHMENUPOPUPMETRICS
{
	DWORD rgcx[4];
	DWORD fUpdateMaxWidths : 2; // from kernel symbols, padded to full dword
} UAHMENUPOPUPMETRICS;

// hmenu is the main window menu; hdc is the context to draw in
typedef struct tagUAHMENU
{
	HMENU hmenu;
	HDC hdc;
	DWORD dwFlags; // no idea what these mean, in my testing it's either 0x00000a00 or sometimes 0x00000a10
} UAHMENU;

// menu items are always referred to by iPosition here
typedef struct tagUAHMENUITEM
{
	int iPosition; // 0-based position of menu item in menubar
	UAHMENUITEMMETRICS umim;
	UAHMENUPOPUPMETRICS umpm;
} UAHMENUITEM;

// the DRAWITEMSTRUCT contains the states of the menu items, as well as
// the position index of the item in the menu, which is duplicated in
// the UAHMENUITEM's iPosition as well
typedef struct UAHDRAWMENUITEM
{
	DRAWITEMSTRUCT dis; // itemID looks uninitialized
	UAHMENU um;
	UAHMENUITEM umi;
} UAHDRAWMENUITEM;

// the MEASUREITEMSTRUCT is intended to be filled with the size of the item
// height appears to be ignored, but width can be modified
typedef struct tagUAHMEASUREMENUITEM
{
	MEASUREITEMSTRUCT mis;
	UAHMENU um;
	UAHMENUITEM umi;
} UAHMEASUREMENUITEM;

enum PreferredAppMode
{
	Default,
	AllowDark,
	ForceDark,
	ForceLight,
	Max
};

#ifdef __cplusplus
extern "C" {
#endif

void DarkMenus(BOOL flag);
void DarkWindowTitleBar(HWND hWnd, BOOL flag);
void DarkDialogInit(HWND hDlg);

LRESULT DarkOnCtlColorDlg(HDC hdc);
LRESULT DarkOnCtlColorStatic(HWND hWnd, HDC hdc);
LRESULT DarkOnCtlColorStaticMsgBox(HWND hWnd, HDC hdc);
LRESULT DarkOnCtlColorBtn(HDC hdc);
LRESULT DarkOnCtlColorEdit(HDC hdc);

LRESULT CALLBACK DarkStatusBarClassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
LRESULT CALLBACK DarkCheckBoxClassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
LRESULT CALLBACK DarkComboBoxClassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
LRESULT CALLBACK DarkComboBoxExClassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

LRESULT DarkHandleMenuMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

#ifdef __cplusplus
}
#endif
