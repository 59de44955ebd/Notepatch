#include <stdint.h>

#include "Dark.h"

extern HINSTANCE 		g_hInstance;
extern BOOL 			g_bIsDark;

using fnSetPreferredAppMode = PreferredAppMode(WINAPI*)(PreferredAppMode appMode);
using fnFlushMenuThemes = void (WINAPI*)();

//##########################################################
// DarkModeInit
//##########################################################
void DarkMenus(BOOL flag)
{
	// Dark menus and dark scrollbars
	HMODULE hUxtheme = LoadLibraryEx(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
	if (hUxtheme)
	{
		fnSetPreferredAppMode SetPreferredAppMode = (fnSetPreferredAppMode)GetProcAddress(hUxtheme, MAKEINTRESOURCEA(135));
		SetPreferredAppMode(flag ? ForceDark : ForceLight);

		fnFlushMenuThemes FlushMenuThemes = (fnFlushMenuThemes)GetProcAddress(hUxtheme, MAKEINTRESOURCEA(136));
		FlushMenuThemes();

		FreeLibrary(hUxtheme);
	}
}

//##########################################################
//
//##########################################################
void DarkWindowTitleBar(HWND hWnd, BOOL flag)
{
	INT value = flag ? 1 : 0;
	DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));

	// For Windows 10
	SendMessage(hWnd, WM_NCACTIVATE, FALSE, 0);
	SendMessage(hWnd, WM_NCACTIVATE, TRUE, 0);
}

//##########################################################
//
//##########################################################
void DarkDialogInit(HWND hDlg)
{
	// Dark window titlebar
	INT value = 1;
	DwmSetWindowAttribute(hDlg, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));

	HWND hWnd = ::GetTopWindow(hDlg);
	wchar_t lpClassName[64];

	wchar_t szWinText[MAX_WIN_TEXT_LEN];
	LONG style, ex_style, nButtonType;

	while (hWnd)
	{
		style = GetWindowLong(hWnd, GWL_STYLE);
		if (style & WS_VISIBLE)
		{
			GetClassName(hWnd, lpClassName, 64);

			if (wcscmp(lpClassName, WC_BUTTON) == 0)
			{
				SetWindowTheme(hWnd, L"DarkMode_Explorer", NULL);

				nButtonType = style & BS_TYPEMASK;

				if (nButtonType == BS_GROUPBOX || nButtonType == BS_CHECKBOX || nButtonType == BS_AUTOCHECKBOX || nButtonType == BS_RADIOBUTTON || nButtonType == BS_AUTORADIOBUTTON || nButtonType == BS_AUTO3STATE)
				{
					RECT rc;
					GetWindowRect(hWnd, &rc);
					MapWindowPoints(NULL, hDlg, (LPPOINT)&rc, 2);

					//wchar_t szWinText[MAX_WIN_TEXT_LEN];
					GetWindowText(hWnd, szWinText, MAX_WIN_TEXT_LEN);

					// Remove single "&", replace "&&" with "&"
					wchar_t szWinTextClean[MAX_WIN_TEXT_LEN] = L"";
					wchar_t* buffer;
					wchar_t* token = wcstok_s(szWinText, L"&", &buffer);
					while (token)
					{
						// This should be safe, right?
						if (token > szWinText + 1 && *(token - 1) == L'&')
							wcscat_s(szWinTextClean, L"&");
						wcscat_s(szWinTextClean, token);
						token = wcstok_s(nullptr, L"&", &buffer);
					}

					// Add static with original button text and original font - but this one can have a custom color (white) which is set in WM_CTLCOLORSTATIC
					HWND hWndStatic = CreateWindowEx(
						0, //WS_EX_TRANSPARENT,
						WC_STATIC,
						szWinTextClean,
						WS_CHILD | SS_SIMPLE | WS_VISIBLE,

						//17, //
						nButtonType == BS_GROUPBOX ? 9 : 17,
						nButtonType == BS_GROUPBOX ? 0 : 3,

						nButtonType == BS_GROUPBOX ? rc.right - rc.left - 16 : rc.right - rc.left,
						nButtonType == BS_GROUPBOX ? 16 : rc.bottom - rc.top,
						hWnd,

						nullptr,
						g_hInstance,
						nullptr
					);

					SendMessage(hWndStatic, WM_SETFONT,
						SendMessage(hWnd, WM_GETFONT, 0, 0),
						MAKELPARAM(1, 0));

					SetWindowPos(hWndStatic, HWND_TOPMOST, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE);

					// Hide original (black) checkbox/radiobutton text by resizing the control
					if (nButtonType != BS_GROUPBOX)
						//SetWindowPos(hWnd, 0, 0, 0, 16, rc.bottom - rc.top, SWP_NOZORDER | SWP_NOMOVE);
						SetWindowText(hWnd, L"");

					SetWindowSubclass(hWnd, &DarkCheckBoxClassProc, CHECKBOX_SUBCLASS_ID, 0);
				}
			}

			else if (wcscmp(lpClassName, WC_COMBOBOX) == 0)
			{
				SetWindowTheme(hWnd, L"DarkMode_CFD", NULL);

		        // find internal listbox (ComboLBox)
		        COMBOBOXINFO ci;
		        ci.cbSize = sizeof(COMBOBOXINFO);
		        SendMessage(hWnd, CB_GETCOMBOBOXINFO, 0, (LPARAM)&ci);
		        if (ci.hwndList)
		        {
		        	//DBGS(L"ComboBox->ComboLBox");

					// Dark scrollbars for the internal Listbox (ComboLBox)
                	SetWindowTheme(ci.hwndList, L"DarkMode_Explorer", NULL);

					ex_style = GetWindowLong(ci.hwndList, GWL_EXSTYLE);
					if (ex_style & WS_EX_CLIENTEDGE)
					{
						SetWindowLong(ci.hwndList, GWL_EXSTYLE, ex_style & ~WS_EX_CLIENTEDGE   & ~WS_EX_STATICEDGE);
						SetWindowLong(ci.hwndList, GWL_STYLE, GetWindowLong(ci.hwndList, GWL_STYLE) | WS_BORDER);
					}
                }

//		        if (ci.hwndItem)
//		        {
//		        	DBGS(L"ComboBox->Edit");
//
//					SetWindowTheme(ci.hwndItem, L"DarkMode_Explorer", NULL);
//
//					ex_style = GetWindowLong(ci.hwndItem, GWL_EXSTYLE);
//					if (ex_style & WS_EX_CLIENTEDGE)
//					{
//						SetWindowLong(ci.hwndItem, GWL_EXSTYLE, ex_style & ~WS_EX_CLIENTEDGE   & ~WS_EX_STATICEDGE);
//						//SetWindowLong(ci.hwndItem, GWL_STYLE, GetWindowLong(ci.hwndItem, GWL_STYLE) | WS_BORDER);
//						SetWindowLong(ci.hwndItem, GWL_STYLE, WS_VISIBLE | WS_CHILD);
//					}
//					SetWindowLong(ci.hwndItem, GWL_STYLE, WS_VISIBLE | WS_CHILD);
//
//					SetWindowSubclass(ci.hwndItem, &__DarkComboBoxEditClassProc, COMBOBOX_SUBCLASS_ID, 0);
//		        }

				SetWindowSubclass(hWnd, &DarkComboBoxClassProc, COMBOBOX_SUBCLASS_ID, 0);
			}

			else if (wcscmp(lpClassName, WC_COMBOBOXEX) == 0)
			{
		        // find internal combobox control
		        HWND hWndComboBox = (HWND)SendMessage(hWnd, CBEM_GETCOMBOCONTROL, 0, 0);
				SetWindowTheme(hWndComboBox, L"DarkMode_CFD", NULL);
				SetWindowSubclass(hWndComboBox, &DarkComboBoxExClassProc, COMBOBOXEX_SUBCLASS_ID, 0);

		        // find internal edit control
//		        COMBOBOXINFO ci;
//		        ci.cbSize = sizeof(COMBOBOXINFO);
//		        SendMessage(hwnd_combobox, CB_GETCOMBOBOXINFO, 0, (LPARAM)&ci);
//		        SetWindowTheme(ci.hwndItem, L"DarkMode_Explorer", NULL);
			}

			else if (wcscmp(lpClassName, WC_LISTVIEW) == 0)
			{
				SetWindowTheme(hWnd, L"DarkMode_Explorer", NULL);

				SendMessage(hWnd, LVM_SETTEXTCOLOR, 0, DARK_TEXT_COLOR);
				SendMessage(hWnd, LVM_SETTEXTBKCOLOR, 0, DARK_CONTROL_BG_COLOR);
				SendMessage(hWnd, LVM_SETBKCOLOR, 0, DARK_CONTROL_BG_COLOR);

				// Unfortunately I couldn't find a way to invert the colors of a SysHeader32,
				// it's always black on white. But without theming removed it looks slightly
				// better inside a dark mode ListView.
				HWND hWndHeader = (HWND)SendMessage(hWnd, LVM_GETHEADER, 0, 0);
				if (hWndHeader)
					SetWindowTheme(hWndHeader, L"", L"");
			}

			else if (wcscmp(lpClassName, WC_EDIT) == 0)
			{
				ex_style = GetWindowLong(hWnd, GWL_EXSTYLE);
				if (ex_style & WS_EX_STATICEDGE || ex_style & WS_EX_CLIENTEDGE)
				{
					SetWindowLong(hWnd, GWL_EXSTYLE, ex_style & ~WS_EX_CLIENTEDGE & ~WS_EX_STATICEDGE);
					SetWindowLong(hWnd, GWL_STYLE, style | WS_BORDER);

					// make a little smaller and move to the bottom
					RECT rc;
					GetWindowRect(hWnd, &rc);
					MapWindowPoints(NULL, hDlg, (LPPOINT)&rc, 2);
					SetWindowPos(hWnd, 0,

//						rc.left, rc.top + 2,
//						rc.right - rc.left, rc.bottom - rc.top - 4,

						rc.left + 1, rc.top,
						rc.right - rc.left - 2, rc.bottom - rc.top,

						SWP_NOZORDER | SWP_FRAMECHANGED
					);
				}
			}

			else if (wcscmp(lpClassName, WC_STATIC) == 0)
			{
				ex_style = GetWindowLong(hWnd, GWL_EXSTYLE);
				if (ex_style & WS_EX_STATICEDGE || ex_style & WS_EX_CLIENTEDGE)
				{
					SetWindowLong(hWnd, GWL_EXSTYLE, ex_style & ~WS_EX_STATICEDGE & ~WS_EX_CLIENTEDGE);
					SetWindowLong(hWnd, GWL_STYLE, style | WS_BORDER);
				}
			}

			else if (wcscmp(lpClassName, WC_LISTBOX) == 0)
			{
				//SetWindowTheme(hwnd, L"DarkMode_Explorer", NULL);

				ex_style = GetWindowLong(hWnd, GWL_EXSTYLE);
				if (ex_style & WS_EX_CLIENTEDGE)
				{
					SetWindowLong(hWnd, GWL_EXSTYLE, ex_style & ~WS_EX_CLIENTEDGE);
					//SetWindowLong(hwnd, GWL_STYLE, style | WS_BORDER);
				}
			}

			else if (wcscmp(lpClassName, WC_TREEVIEW) == 0)
			{
				SetWindowTheme(hWnd, L"DarkMode_Explorer", NULL);

				//SetWindowLongPtrA(hwnd, GWL_EXSTYLE, GetWindowLongPtrA(hwnd, GWL_EXSTYLE) & ~WS_EX_NOPARENTNOTIFY);
				SendMessage(hWnd, TVM_SETBKCOLOR, 0, (LPARAM)DARK_CONTROL_BG_COLOR);
				SendMessage(hWnd, TVM_SETTEXTCOLOR, 0, (LPARAM)DARK_TEXT_COLOR);
				//TreeView_SetTextColor(hwnd, TEXT_COLOR_DARK);
			}

			else if (wcscmp(lpClassName, UPDOWN_CLASS) == 0)
			{
				SetWindowTheme(hWnd, L"DarkMode_Explorer", NULL);
			}

		}

		hWnd = ::GetNextWindow(hWnd, GW_HWNDNEXT);
	}
}

//##########################################################
//
//##########################################################
LRESULT DarkOnCtlColorDlg(HDC hdc)
{
	SetBkColor(hdc, DARK_BG_COLOR);
	return (LRESULT)CreateSolidBrush(DARK_BG_COLOR);
}

//##########################################################
//
//##########################################################
LRESULT DarkOnCtlColorStatic(HWND hWnd, HDC hdc)
{
	wchar_t lpClassName[64];
	GetClassName(hWnd, lpClassName, 64);
	if (wcscmp(lpClassName, WC_STATIC) == 0)
	{
		LONG style = GetWindowLong(hWnd, GWL_STYLE);
		if (style & WS_DISABLED)
		{
			// Couldn't find a better way to make disabled static controls look decent in dark mode
			// than enabling them, but changing the text color to grey.
			SetWindowLong(hWnd, GWL_STYLE, style & ~WS_DISABLED);
			SetTextColor(hdc, DARK_TEXT_COLOR_DISABLED);
		}
		else
			SetTextColor(hdc, style & SS_NOTIFY ? DARK_TEXT_COLOR_LINK : DARK_TEXT_COLOR);
	}
	else
		SetTextColor(hdc, DARK_TEXT_COLOR);

	SetBkColor(hdc, DARK_BG_COLOR);
	return (LRESULT)CreateSolidBrush(DARK_BG_COLOR);
}

//##########################################################
//
//##########################################################
LRESULT DarkOnCtlColorStaticMsgBox(HWND hWnd, HDC hdc)
{
	SetTextColor(hdc, DARK_TEXT_COLOR);
	SetBkColor(hdc, DARK_CONTROL_BG_COLOR);
	return (LRESULT)CreateSolidBrush(DARK_CONTROL_BG_COLOR);
}

//##########################################################
//
//##########################################################
LRESULT DarkOnCtlColorBtn(HDC hdc)
{
	SetDCBrushColor(hdc, DARK_BG_COLOR);
	return (LRESULT)GetStockObject(DC_BRUSH);
}

//##########################################################
//
//##########################################################
LRESULT DarkOnCtlColorEdit(HDC hdc)
{
	SetTextColor(hdc, DARK_TEXT_COLOR);
	SetBkColor(hdc, DARK_BG_COLOR);
	SetDCBrushColor(hdc, DARK_BG_COLOR);
	return (LRESULT)GetStockObject(DC_BRUSH);
}

//##########################################################
//
//##########################################################
//LRESULT DarkOnCtlColorEditColorListBox(HDC hdc)
//{
//	SetTextColor(hdc, DARK_TEXT_COLOR);
//	SetBkColor(hdc, DARK_CONTROL_BG_COLOR);
//	SetDCBrushColor(hdc, DARK_CONTROL_BG_COLOR);
//	return (LRESULT)GetStockObject(DC_BRUSH);
//}

//##########################################################
//
//##########################################################
LRESULT DarkOnCtlColorListBox(HDC hdc)
{
	SetTextColor(hdc, DARK_TEXT_COLOR);
	SetBkColor(hdc, DARK_CONTROL_BG_COLOR);
	SetDCBrushColor(hdc, DARK_CONTROL_BG_COLOR);
	return (LRESULT)GetStockObject(DC_BRUSH);
}

//##########################################################
//
//##########################################################
LRESULT CALLBACK DarkStatusBarClassProc(HWND hWnd, UINT uMsg, WPARAM wParam,
		LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	switch (uMsg)
	{
		case WM_PAINT:
			{
				HFONT hfont = (HFONT)(SendMessage(hWnd, WM_GETFONT, 0, 0));
				PAINTSTRUCT ps;
				HDC hdc = BeginPaint(hWnd, &ps);

	            ps.rcPaint.right -= 1;

	            HBRUSH hbr = CreateSolidBrush(DARK_BG_COLOR);
				FillRect(hdc, &ps.rcPaint, hbr);

	            SelectObject(hdc, hfont);
	            SetBkMode(hdc, TRANSPARENT);
	            SetTextColor(hdc, DARK_TEXT_COLOR);

				RECT rc_part;

				HBRUSH hbr_sep = CreateSolidBrush(DARK_SEPARATOR_COLOR);

				int num_parts = (int)SendMessage(hWnd, SB_GETPARTS, 0, 0);
	            for (int i=0; i<num_parts; i++)
	            {
	                SendMessage(hWnd, SB_GETRECT, i, (LPARAM)&rc_part);
	                if (ps.rcPaint.left >= rc_part.right || ps.rcPaint.right < rc_part.left)
	                    continue;

					// Due to rounded window corners in Win 11 lets move text of first part a little to the right
					if (i == 0)
						rc_part.left += 5;

	                // Draw text
	                int text_len = (int)SendMessage(hWnd, SB_GETTEXTLENGTH, i, 0) + 1;
	                wchar_t* buf = (wchar_t*)(malloc(sizeof(wchar_t) * text_len));
	                SendMessage(hWnd, SB_GETTEXTW, i, (LPARAM)buf);
	                RECT rc = {rc_part.left + 5, rc_part.top + 1, rc_part.right, rc_part.bottom - 1};
	                DrawText(hdc, buf, text_len, &rc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
					free(buf);

	                // Draw separator
	                if (i < num_parts - 1)
	                {
	                	RECT rc = {rc_part.right - 1, rc_part.top + 1, rc_part.right, rc_part.bottom - 3};
	                    FillRect(hdc, &rc, hbr_sep);
	                }
				}

				DeleteObject(hbr);
				DeleteObject(hbr_sep);
	            EndPaint(hWnd, &ps);
				return 0;
	        }

		case WM_ERASEBKGND:
			{
		        RECT rc;
		        GetClientRect(hWnd, &rc);
		        HBRUSH hbr = CreateSolidBrush(DARK_BG_COLOR);
		        FillRect((HDC)wParam, &rc, hbr);
		        DeleteObject(hbr);
		        return 1;
		    }
	}

	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

//##########################################################
//
//##########################################################
LRESULT CALLBACK DarkCheckBoxClassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	switch(uMsg)
	{
		case WM_CTLCOLORSTATIC:
			return DarkOnCtlColorStatic(hWnd, (HDC)wParam);
	}
	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

//##########################################################
//
//##########################################################
LRESULT CALLBACK DarkComboBoxClassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	static HWND s_hwndListBox;

	switch(uMsg)
	{
		case WM_CTLCOLORLISTBOX:
			// 9053 = 0x235D
			//DBGI(GetWindowLong((HWND)lParam, GWL_STYLE) & LBS_OWNERDRAWFIXED);//9053);
			//DBGI(GetWindowLong((HWND)lParam, GWL_STYLE) & LBS_OWNERDRAWVARIABLE);

			// This makes the internal listbox having dark BG and light text,
			// but only if not custom drawn
			if (!(GetWindowLong((HWND)lParam, GWL_STYLE) & LBS_OWNERDRAWFIXED))
				return DarkOnCtlColorListBox((HDC)wParam);
			else
			{
				s_hwndListBox = (HWND)lParam;

				SetBkColor((HDC)wParam, 0);
				SetDCBrushColor((HDC)wParam, 0);
				return (LRESULT)GetStockObject(DC_BRUSH);
			}
			break;

		case WM_CTLCOLOREDIT:
			//return DarkOnCtlColorEdit((HDC)wParam);

			SetTextColor((HDC)wParam, DARK_TEXT_COLOR);
			SetBkColor((HDC)wParam, 0x383838);
			SetDCBrushColor((HDC)wParam, 0x383838);
			return (LRESULT)GetStockObject(DC_BRUSH);

		//case WM_NCPAINT:
		//	{
		//		DBGS(L"WM_NCPAINT");

		//		HDC hdc = GetWindowDC(hWnd);
		//		//HDC hdc = GetDCEx(hWnd, (HRGN)wParam, DCX_WINDOW | DCX_INTERSECTRGN);

		//		//   SetDCBrushColor((HDC)wParam, 0x383838);
		//		//   SetDCPenColor((HDC)wParam, 0x383838);
		//		   //SetBkColor((HDC)wParam, 0x0000FF);

		//		RECT rc = { -1, -1, 200, 200 };
		//		//GetWindowRect(hWnd, &rc);
		//		GetClientRect(hWnd, &rc);
		//		HBRUSH hbr = CreateSolidBrush(0x00FFFF); //DARK_BG_COLOR
		//		FillRect(hdc, &rc, hbr);
		//		DeleteObject(hbr);

		//		ReleaseDC(hWnd, hdc);

		//		//An application returns zero if it processes this message.
		//		return 0;
		//	}
		//	break;

		//case WM_ERASEBKGND:
		//	{
		//        RECT rc;
		//        GetClientRect(hWnd, &rc);
		//        HBRUSH hbr = CreateSolidBrush(0x0000FF); //DARK_BG_COLOR
		//        FillRect((HDC)wParam, &rc, hbr);
		//        DeleteObject(hbr);
		//        return 1;
		//    }

		case WM_DRAWITEM:
			//DBGS(L"WM_DRAWITEM");
			{
				DRAWITEMSTRUCT *di = (DRAWITEMSTRUCT*)lParam;
				if (!(di->itemState & ODS_SELECTED))
				{
					//SendMessage(s_hwndListBox, WM_SETREDRAW, FALSE, 0);

					DefSubclassProc(hWnd, uMsg, wParam, lParam);

					DRAWITEMSTRUCT *di = (DRAWITEMSTRUCT*)lParam;

			  //      wchar_t buf[MAX_PATH];
			  //      COMBOBOXEXITEM cbei = {};
			  //      cbei.mask = CBEIF_TEXT | CBEIF_IMAGE;  //| CBEIF_SELECTEDIMAGE
			  //      cbei.cchTextMax = MAX_PATH;
					//cbei.pszText = buf;
			  //      cbei.iItem = di->itemID;

					//DBGI(di->rcItem.right - di->rcItem.left);

					//SetTextColor(di->hDC, DARK_TEXT_COLOR);
					//SetBkColor(di->hDC, di->itemState & ODS_SELECTED ? GetSysColor(COLOR_HIGHLIGHT) : DARK_CONTROL_BG_COLOR);

					//SetBkColor(di->hDC, 0x0000FF);

					//DBGI(di->itemState);
					//if (!(di->itemState & ODS_SELECTED))
						InvertRect(di->hDC, &di->rcItem);

					//SendMessage(s_hwndListBox, WM_SETREDRAW, TRUE, 0);
					return TRUE;
				}
				//else //if (di->itemState & ODS_COMBOBOXEDIT)
					//DBGS(L"ODS_COMBOBOXEDIT");
					//DBGI(di->itemState);
	        }

	}
	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

//##########################################################
// Used only for ComboBoxEx, but we actually subclass the internal ComboBox, so HWND of ComboBoxEx is GetParent(hWnd)
//##########################################################
LRESULT CALLBACK DarkComboBoxExClassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	switch(uMsg)
	{
		case WM_CTLCOLORLISTBOX:
			return DarkOnCtlColorListBox((HDC)wParam);

		case WM_DRAWITEM:
		{
	        DRAWITEMSTRUCT *di = (DRAWITEMSTRUCT*)lParam;
	        wchar_t buf[MAX_PATH];
	        COMBOBOXEXITEM cbei = {};
	        cbei.mask = CBEIF_TEXT | CBEIF_IMAGE;  //| CBEIF_SELECTEDIMAGE
	        cbei.cchTextMax = MAX_PATH;
			cbei.pszText = buf;
	        cbei.iItem = di->itemID;

	        SendMessage(GetParent(hWnd), CBEM_GETITEM, 0, (LPARAM)&cbei);

	        SetTextColor(di->hDC, DARK_TEXT_COLOR);
	        SetBkColor(di->hDC, di->itemState & ODS_SELECTED ? GetSysColor(COLOR_HIGHLIGHT) : DARK_CONTROL_BG_COLOR);

	        // Calculate the vertical and horizontal position.
	        TEXTMETRICW tm = {}; //()
	        GetTextMetricsW(di->hDC, &tm);
	        int y = (di->rcItem.bottom + di->rcItem.top - tm.tmHeight) / 2;
	        int x = LOWORD(GetDialogBaseUnits()) / 4;

			#define ICON_SIZE 16
	        ExtTextOut(di->hDC, ICON_SIZE + 2 * x, y, ETO_CLIPPED | ETO_OPAQUE, &di->rcItem,  // 6 = ETO_CLIPPED | ETO_OPAQUE
	                cbei.pszText, (UINT)wcslen(cbei.pszText), NULL);

	        HIMAGELIST h_imagelist = (HIMAGELIST)SendMessage(GetParent(hWnd), CBEM_GETIMAGELIST, 0, 0);
	        if (h_imagelist && cbei.iImage >= 0)
	            ImageList_Draw(h_imagelist, cbei.iImage, di->hDC, x, y, ILD_IMAGE);

	        // If an application processes this message, it should return TRUE.
	        return TRUE;
		}
	}
	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

//##########################################################
//
//##########################################################
LRESULT DarkHandleMenuMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_NCPAINT:
		case WM_NCACTIVATE:
			{
				DefWindowProcW(hWnd, uMsg, wParam, lParam);
				RECT rcClient;
				GetClientRect(hWnd, &rcClient);
				MapWindowPoints(hWnd, NULL, (LPPOINT)(&rcClient), 2);
				RECT rcWindow;
				GetWindowRect(hWnd, &rcWindow);
				OffsetRect(&rcClient, -rcWindow.left, -rcWindow.top);
				// the rcBar is offset by the window rect
				RECT rcAnnoyingLine = rcClient;
				rcAnnoyingLine.bottom = rcAnnoyingLine.top;
				rcAnnoyingLine.top -= 1;
				HDC hdc = GetWindowDC(hWnd);
				HBRUSH hbr = CreateSolidBrush(DARK_BG_COLOR);
				FillRect(hdc, &rcAnnoyingLine, hbr);
				DeleteObject(hbr);
				ReleaseDC(hWnd, hdc);
				return TRUE;
			}

		case WM_UAHDRAWMENU:
			{
				UAHMENU* pUDM = (UAHMENU*)(lParam);
				MENUBARINFO mbi;
				mbi.cbSize = sizeof(MENUBARINFO);
				GetMenuBarInfo(hWnd, OBJID_MENU, 0, &mbi);
				RECT rc_win;
				GetWindowRect(hWnd, &rc_win);
				OffsetRect(&mbi.rcBar, -rc_win.left, -rc_win.top);
				HBRUSH hbr = CreateSolidBrush(DARK_MENUBAR_BG_COLOR);
				FillRect(pUDM->hdc, &mbi.rcBar, hbr);
				DeleteObject(hbr);
				return TRUE;
			}

		case WM_UAHDRAWMENUITEM:
			{
				UAHDRAWMENUITEM* pUDMI = (UAHDRAWMENUITEM*)(lParam);
				MENUITEMINFOW mii;
				mii.cbSize = sizeof(MENUITEMINFOW);
				mii.fMask = MIIM_STRING;
				WCHAR buf[256];
				mii.dwTypeData = buf;
				mii.cch = 256;
				GetMenuItemInfoW(pUDMI->um.hmenu, pUDMI->umi.iPosition, TRUE, &mii);
				HBRUSH hbr = CreateSolidBrush(pUDMI->dis.itemState & ODS_HOTLIGHT || pUDMI->dis.itemState & ODS_SELECTED ? DARK_MENUBAR_BG_COLOR_HOT : DARK_MENUBAR_BG_COLOR);
				FillRect(pUDMI->um.hdc, &pUDMI->dis.rcItem, hbr);
				DeleteObject(hbr);
				SetBkMode(pUDMI->um.hdc, TRANSPARENT);
				SetTextColor(pUDMI->um.hdc, DARK_TEXT_COLOR);
				DrawTextW(pUDMI->um.hdc, mii.dwTypeData, -1, &pUDMI->dis.rcItem, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
				return TRUE;
			}
	}
	return FALSE;
}
