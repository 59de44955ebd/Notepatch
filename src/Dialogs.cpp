#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <commdlg.h>
#include <string.h>
#include <strsafe.h>

#include "Dark.h"
#include "Dialogs.h"
#include "Edit.h"
#include "Helpers.h"
#include "Notepatch.h"
#include "Resource.h"

#define MSGBOX_BOTTOM_HEIGHT 42
#define MSGBOX_SUBCLASS_ID 1

HHOOK g_hHook;

extern HWND			g_hWndMain;
extern HINSTANCE 	g_hInstance;
extern HMODULE 		g_hModule;
extern BOOL 		g_bIsDark;
extern Edit*		g_pEdit;

extern wchar_t 		g_wszFindWhat[MAX_SEARCH_REPLACE_LEN];
extern wchar_t 		g_wszReplaceWith[MAX_SEARCH_REPLACE_LEN];

enum DialogType
{
	DIALOG_OTHER = 0,
	DIALOG_CUSTOM_PROC,
	DIALOG_OFN
};

// Used in DialogHookProc to handle dialogs in dark mode according to their flavor
static DialogType		g_iDialogType = DIALOG_OTHER;


//##########################################################
//
//##########################################################
void CenterDialog(HWND hDlg)
{
	RECT rcDlg;
	RECT rcParent;
	MONITORINFO mi;
	HMONITOR hMonitor;
	int xMin, yMin, xMax, yMax, x, y;

	GetWindowRect(hDlg, &rcDlg);
	GetWindowRect(g_hWndMain, &rcParent);

	hMonitor = MonitorFromRect(&rcParent, MONITOR_DEFAULTTONEAREST);
	mi.cbSize = sizeof(mi);
	GetMonitorInfo(hMonitor, &mi);

	xMin = mi.rcWork.left;
	yMin = mi.rcWork.top;

	xMax = (mi.rcWork.right) - (rcDlg.right - rcDlg.left);
	yMax = (mi.rcWork.bottom) - (rcDlg.bottom - rcDlg.top);

	if ((rcParent.right - rcParent.left) - (rcDlg.right - rcDlg.left) > 20)
		x = rcParent.left + (((rcParent.right - rcParent.left) - (rcDlg.right - rcDlg.left)) / 2);
	else
		x = rcParent.left + 70;

	if ((rcParent.bottom - rcParent.top) - (rcDlg.bottom - rcDlg.top) > 20)
		y = rcParent.top + (((rcParent.bottom - rcParent.top) - (rcDlg.bottom - rcDlg.top)) / 2);
	else
		y = rcParent.top + 60;
	SetWindowPos(hDlg, NULL, max(xMin, min(xMax, x)), max(yMin, min(yMax, y)), 0, 0, SWP_NOZORDER | SWP_NOSIZE);
}

//##########################################################
//
//##########################################################
UINT_PTR ChooseFontDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_INITDIALOG:
			g_iDialogType = DIALOG_CUSTOM_PROC;
			if (g_bIsDark)
				DarkDialogInit(hDlg);
			CenterDialog(hDlg);
			break;

		DIALOG_HANDLE_CONTROL_MESSAGES()
	}

	return 0;
}

//##########################################################
//
//##########################################################
UINT_PTR PageSetupDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_INITDIALOG:
			g_iDialogType = DIALOG_CUSTOM_PROC;
			if (g_bIsDark)
				DarkDialogInit(hDlg);
			CenterDialog(hDlg);
			break;

		DIALOG_HANDLE_CONTROL_MESSAGES()
	}

	return 0;
}

//##########################################################
//
//##########################################################
UINT_PTR PrintDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_INITDIALOG:
			g_iDialogType = DIALOG_CUSTOM_PROC;
			if (g_bIsDark)
				DarkDialogInit(hDlg);
			CenterDialog(hDlg);
			break;

		DIALOG_HANDLE_CONTROL_MESSAGES()
	}

	return 0;
}

//##########################################################
//
//##########################################################
UINT_PTR CALLBACK FindDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static HWND s_hwndWrap = NULL;

	switch (uMsg)
	{
		case WM_INITDIALOG:
			{
				g_iDialogType = DIALOG_CUSTOM_PROC;
				s_hwndWrap = GetDlgItem(hDlg, IDC_FIND_WRAP_AROUND); // Win 11 only?
				if (s_hwndWrap)
				{
					EnableWindow(s_hwndWrap, SW_SHOW);
					ShowWindow(s_hwndWrap, SW_SHOW);
				}

				ShowWindow(GetDlgItem(hDlg, IDC_FIND_HELP), SW_HIDE);
				ShowWindow(GetDlgItem(hDlg, IDC_FIND_MATCH_WORD), SW_HIDE);
				SendDlgItemMessage(hDlg, IDC_FIND_DOWN, BM_SETCHECK, BST_CHECKED, 0);

				if (g_bIsDark)
					DarkDialogInit(hDlg);
				CenterDialog(hDlg);
			}
			break;

		DIALOG_HANDLE_CONTROL_MESSAGES()

		case WM_SHOWWINDOW:
			SetFocus(GetDlgItem(hDlg, IDC_FIND_EDIT));
			break;

		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case IDOK:
					{
						wchar_t _wszFindWhat[MAX_SEARCH_REPLACE_LEN];
						GetDlgItemText(hDlg, IDC_FIND_EDIT, _wszFindWhat, MAX_PATH);

						std::wstring wszFindWhat = ReplaceAll(_wszFindWhat, L"^t", L"\t", NULL);
						wszFindWhat = ReplaceAll(wszFindWhat, L"^n", L"\r\n", NULL);

						BOOL bCaseSensitive = SendDlgItemMessage(hDlg, IDC_FIND_MATCH_CASE, BM_GETCHECK, 0, 0) == BST_CHECKED;
						BOOL bForward = SendDlgItemMessage(hDlg, IDC_FIND_DOWN, BM_GETCHECK, 0, 0) == BST_CHECKED;
						BOOL bWrap = s_hwndWrap ? SendDlgItemMessage(hDlg, IDC_FIND_WRAP_AROUND, BM_GETCHECK, 0, 0) == BST_CHECKED : FALSE;
						BOOL bFound = g_pEdit->Find(wszFindWhat.c_str(), bCaseSensitive, bForward, bWrap, FALSE);
						if (!bFound)
						{
						  	GetText(IDS_CANNOT_FIND);
							LPWSTR pwszMsg = GetFormattedMessage(wszText, wszFindWhat);
							MessageBox(g_hWndMain, pwszMsg, APP_NAME, MB_OK | MB_ICONINFORMATION);
							LocalFree(pwszMsg);
						}
					}
					break;

				case IDCANCEL:
					EndDialog(hDlg, LOWORD(wParam));
					SetFocus(g_pEdit->m_hWnd);
					break;

				case IDC_FIND_EDIT:
					if (HIWORD(wParam) == EN_CHANGE)
					{
						wchar_t wszFindWhat[MAX_SEARCH_REPLACE_LEN];
						GetDlgItemText(hDlg, IDC_FIND_EDIT, wszFindWhat, MAX_SEARCH_REPLACE_LEN);
						wcscpy_s(g_wszFindWhat, MAX_SEARCH_REPLACE_LEN, wszFindWhat);
					}
					break;
			}
			return TRUE;
	}

	return 0;
}

//##########################################################
//
//##########################################################
UINT_PTR CALLBACK ReplaceDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static HWND s_hwndWrap = NULL;

	switch (uMsg)
	{
		case WM_INITDIALOG:
			{
				g_iDialogType = DIALOG_CUSTOM_PROC;

				s_hwndWrap = GetDlgItem(hDlg, IDC_FIND_WRAP_AROUND); // Win 11 only?
				if (s_hwndWrap)
				{
					// There is a little flaw in the system replace dialog (at least in Windows 11),
					// the (by default hidden) wrap around checkbox doesn't horizontally align with
					// the other checkboxes, but is 2 px off. So let's fix this.
					HWND hwndMatchCase = GetDlgItem(hDlg, IDC_FIND_MATCH_CASE);
					RECT rc1, rc2;
					GetWindowRect(hwndMatchCase, &rc1);
					MapWindowPoints(NULL, hDlg, (LPPOINT)&rc1, 2);
					GetWindowRect(s_hwndWrap, &rc2);
					MapWindowPoints(NULL, hDlg, (LPPOINT)&rc2, 2);
					SetWindowPos(s_hwndWrap, NULL, rc1.left, rc2.top, 0, 0, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOSIZE);

					EnableWindow(s_hwndWrap, SW_SHOW);
					ShowWindow(s_hwndWrap, SW_SHOW);
				}

				if (g_bIsDark)
					DarkDialogInit(hDlg);

				ShowWindow(GetDlgItem(hDlg, IDC_FIND_HELP), SW_HIDE);
				ShowWindow(GetDlgItem(hDlg, IDC_FIND_MATCH_WORD), SW_HIDE);
				SendDlgItemMessage(hDlg, IDC_FIND_DOWN, BM_SETCHECK, BST_CHECKED, 0);

				CenterDialog(hDlg);
			}
			break;

		DIALOG_HANDLE_CONTROL_MESSAGES()

		case WM_SHOWWINDOW:
			SetFocus(GetDlgItem(hDlg, IDC_FIND_EDIT));
			break;

		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case IDOK:
					{
						wchar_t _wszFindWhat[MAX_SEARCH_REPLACE_LEN];
						GetDlgItemText(hDlg, IDC_FIND_EDIT, _wszFindWhat, MAX_SEARCH_REPLACE_LEN);

						std::wstring wszFindWhat = ReplaceAll(_wszFindWhat, L"^t", L"\t", NULL);
						wszFindWhat = ReplaceAll(wszFindWhat, L"^n", L"\r\n", NULL);

						BOOL bCaseSensitive = SendDlgItemMessage(hDlg, IDC_FIND_MATCH_CASE, BM_GETCHECK, 0, 0) == BST_CHECKED;
						BOOL bWrap = s_hwndWrap ? SendDlgItemMessage(hDlg, IDC_FIND_WRAP_AROUND, BM_GETCHECK, 0, 0) == BST_CHECKED : FALSE;
						BOOL bFound = g_pEdit->Find(wszFindWhat.c_str(), bCaseSensitive, TRUE, bWrap, FALSE);
						if (!bFound)
						{
						  	GetText(IDS_CANNOT_FIND);
							LPWSTR pwszMsg = GetFormattedMessage(wszText, wszFindWhat);
							MessageBox(g_hWndMain, pwszMsg, APP_NAME, MB_OK | MB_ICONINFORMATION);
							LocalFree(pwszMsg);
						}
					}
					break;

				case IDC_BTN_REPLACE:
				case IDC_BTN_REPLACE_ALL:
					{
						wchar_t _wszFindWhat[MAX_SEARCH_REPLACE_LEN];
						GetDlgItemText(hDlg, IDC_FIND_EDIT, _wszFindWhat, MAX_SEARCH_REPLACE_LEN);

						std::wstring wszFindWhat = ReplaceAll(_wszFindWhat, L"^t", L"\t", NULL);
						wszFindWhat = ReplaceAll(wszFindWhat, L"^n", L"\r\n", NULL);

						wchar_t _wszReplaceWith[MAX_SEARCH_REPLACE_LEN];
						GetDlgItemText(hDlg, IDC_REPLACE_EDIT, _wszReplaceWith, MAX_SEARCH_REPLACE_LEN);

						std::wstring wszReplaceWith = ReplaceAll(_wszReplaceWith, L"^t", L"\t", NULL);
						wszReplaceWith = ReplaceAll(wszReplaceWith, L"^n", L"\r\n", NULL);

						BOOL bCaseSensitive = SendDlgItemMessage(hDlg, IDC_FIND_MATCH_CASE, BM_GETCHECK, 0, 0) == BST_CHECKED;
						BOOL bWrap = SendDlgItemMessage(hDlg, IDC_FIND_WRAP_AROUND, BM_GETCHECK, 0, 0) == BST_CHECKED;
						BOOL bReplaceAll = LOWORD(wParam) == IDC_BTN_REPLACE_ALL;

						BOOL bFound = g_pEdit->Replace(wszFindWhat.c_str(), wszReplaceWith.c_str(), bCaseSensitive, bWrap, bReplaceAll);
						if (!bFound)
						{
						  	GetText(IDS_CANNOT_FIND);
							LPWSTR pwszMsg = GetFormattedMessage(wszText, wszFindWhat);
							MessageBox(g_hWndMain, pwszMsg, APP_NAME, MB_OK | MB_ICONINFORMATION);
							LocalFree(pwszMsg);
						}
					}
					break;

				case IDCANCEL:
					EndDialog(hDlg, LOWORD(wParam));
					SetFocus(g_pEdit->m_hWnd);
					break;

				case IDC_FIND_EDIT:
					if (HIWORD(wParam) == EN_CHANGE)
					{
						wchar_t wszFindWhat[MAX_SEARCH_REPLACE_LEN];
						GetDlgItemText(hDlg, IDC_FIND_EDIT, wszFindWhat, MAX_SEARCH_REPLACE_LEN);
						wcscpy_s(g_wszFindWhat, MAX_SEARCH_REPLACE_LEN, wszFindWhat);
					}
					break;

				case IDC_REPLACE_EDIT:
					if (HIWORD(wParam) == EN_CHANGE)
					{
						wchar_t wszReplaceWith[MAX_SEARCH_REPLACE_LEN];
						GetDlgItemText(hDlg, IDC_REPLACE_EDIT, wszReplaceWith, MAX_SEARCH_REPLACE_LEN);
						wcscpy_s(g_wszReplaceWith, MAX_SEARCH_REPLACE_LEN, wszReplaceWith);
					}
					break;
			}
			return TRUE;
	}

	return 0;
}

//##########################################################
//
//##########################################################
INT_PTR CALLBACK GotoDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_INITDIALOG:
			g_iDialogType = DIALOG_CUSTOM_PROC;
			if (g_bIsDark)
				DarkDialogInit(hDlg);
			SetDlgItemInt(hDlg, IDC_GOTO_LINENO, (int)SendMessage(g_pEdit->m_hWnd, EM_LINEFROMCHAR, -1, 0) + 1, FALSE);
			CenterDialog(hDlg);
			break;

		DIALOG_HANDLE_CONTROL_MESSAGES()

		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case IDOK:
					{
						UINT lineno = GetDlgItemInt(hDlg, IDC_GOTO_LINENO, NULL, FALSE);

			            UINT total_lines = (UINT)SendMessage(g_pEdit->m_hWnd, EM_GETLINECOUNT, 0, 0);
			            if (lineno < 1 || lineno > total_lines)
						{
							wchar_t wszMsg[MAX_PATH];
							LoadString(g_hModule, IDS_LINENO_BEYOND, wszMsg, MAX_PATH);
							MessageBox(g_hWndMain, wszMsg, APP_NAME, MB_OK | MB_ICONINFORMATION);

							SetDlgItemInt(hDlg, IDC_GOTO_LINENO, total_lines, FALSE);
							return 0;
						}
						g_pEdit->GotoLine(lineno);
					}
					// fall through
				case IDCANCEL:
					EndDialog(hDlg, LOWORD(wParam));
					SetFocus(g_pEdit->m_hWnd);
					break;
			}
			return TRUE;
	}

	return 0;
}

//##########################################################
//
//##########################################################
LRESULT DialogHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode < 0)
        return CallNextHookEx(g_hHook, nCode, wParam, lParam);
	CWPRETSTRUCT *msg = (CWPRETSTRUCT*)lParam;
    HHOOK hook = g_hHook;
	wchar_t wszClassName[10];
	GetClassName(msg->hwnd, wszClassName, 10);

	if (wcscmp(wszClassName, L"#32770") == 0)
	{
		switch (msg->message)
		{
		case WM_INITDIALOG:
			if (g_iDialogType == DIALOG_OFN)
			{
				if (g_bIsDark)
				{
					DarkMenus(TRUE);
					SendMessage(msg->hwnd, WM_SETTINGCHANGE, 0, (LPARAM)L"ImmersiveColorSet");
				}
			}
			else if (g_iDialogType == DIALOG_OTHER)
			{
				CenterDialog(msg->hwnd);
				if (g_bIsDark)
				{
					DarkDialogInit(msg->hwnd);
					SetWindowSubclass(msg->hwnd, &MsgBoxClassProc, MSGBOX_SUBCLASS_ID, NULL);
				}
			}
			g_iDialogType = DIALOG_OTHER;
			break;
		}
	}

    return CallNextHookEx(hook, nCode, wParam, lParam);
}

//##########################################################
//
//##########################################################
LRESULT CALLBACK MsgBoxClassProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	switch (uMsg)
	{
	case WM_ERASEBKGND:
		{
			RECT rc;
			GetClientRect(hDlg, &rc);
			HBRUSH hbr = CreateSolidBrush(DARK_CONTROL_BG_COLOR);
			FillRect((HDC)wParam, &rc, hbr);
			DeleteObject(hbr);
		}
		return TRUE;

	MSGBOX_HANDLE_CONTROL_MESSAGES()

    case WM_PAINT:
        {
        	// Make lower part with buttons darker
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hDlg, &ps);
            HBRUSH hbr = CreateSolidBrush(DARK_BG_COLOR);
			ps.rcPaint.top = ps.rcPaint.bottom - MSGBOX_BOTTOM_HEIGHT;
            FillRect(hdc, &ps.rcPaint, hbr);
            DeleteObject(hbr);
            EndPaint(hDlg, &ps);
        }
        return 0;

	}

	return DefSubclassProc(hDlg, uMsg, wParam, lParam);
}

//##########################################################
//
//##########################################################
void ShowSysErrorMsgBox(DWORD dwError)
{
	LPWSTR pwszMsg;
	size_t size = FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, dwError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&pwszMsg, 0, NULL
	);
	MessageBox(g_hWndMain, pwszMsg, APP_NAME, MB_OK | MB_ICONWARNING);
	LocalFree(pwszMsg);
}

//##########################################################
//
//##########################################################
wchar_t *GetOpenFilename(wchar_t* title, wchar_t* default_name, wchar_t* default_ext, wchar_t* filter)
{
	wchar_t *file_buffer = (wchar_t *)malloc(MAX_PATH * sizeof(wchar_t));
	wcscpy_s(file_buffer, MAX_PATH, default_name ? default_name : L"");

	OPENFILENAME ofn = {};
	ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = g_hWndMain;
	ofn.lpstrTitle = title;
    ofn.lpstrFile = file_buffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = default_ext;
    ofn.lpstrFilter = filter;
    ofn.Flags = OFN_ENABLESIZING | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
	g_iDialogType = DIALOG_OFN;
	BOOL bOk = GetOpenFileName(&ofn);
	if (bOk)
		return file_buffer;

	free(file_buffer);
    return NULL;
}

//##########################################################
//
//##########################################################
wchar_t *GetSaveFilename(wchar_t* title, wchar_t* default_name, wchar_t* default_ext, wchar_t* filter)
{
	wchar_t *file_buffer = (wchar_t *)malloc(MAX_PATH * sizeof(wchar_t));
	wcscpy_s(file_buffer, MAX_PATH, default_name ? default_name : L"");

	OPENFILENAME ofn = {};
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = g_hWndMain;
	ofn.lpstrTitle = title;
    ofn.lpstrFile = file_buffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = default_ext;
    ofn.lpstrFilter = filter;
    ofn.Flags = OFN_ENABLESIZING | OFN_OVERWRITEPROMPT | OFN_EXPLORER;
	g_iDialogType = DIALOG_OFN;
	BOOL bOk = GetSaveFileName(&ofn);
	if (bOk)
		return file_buffer;

	free(file_buffer);
    return NULL;
}
