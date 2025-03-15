#include <windows.h>
#include <shlwapi.h>
#include <VersionHelpers.h>

#include "Dark.h"
#include "Dialogs.h"
#include "Edit.h"
#include "Helpers.h"
#include "Notepatch.h"
#include "Resource.h"
#include "Version.h"

enum Theme
{
	THEME_AUTO,
	THEME_DARK,
	THEME_LIGHT
};

//##########################################################
// Some globals (sure, globals are evil, but well, it's just a simple application)
//##########################################################
HINSTANCE 		g_hInstance = NULL;
HMODULE 		g_hModule = NULL;

Edit*			g_pEdit = NULL;

HWND			g_hWndMain;
HWND			g_hWndStatusbar;

HWND			g_hDlgFind = NULL;
HWND			g_hDlgReplace = NULL;

wchar_t			g_wszWindowTitle[MAX_WIN_TITLE];
wchar_t			g_wszCurrentFile[MAX_PATH];

BOOL 			g_bIsDark = TRUE;
BOOL			g_bShowStatusbar = TRUE;
BOOL			g_bFullscreen = FALSE;
BOOL			g_bAlwaysOnTop = FALSE;
BOOL			g_bTransparentMode = FALSE;
BOOL			g_bModified = FALSE;
BOOL			g_bWordWrap = FALSE;
BOOL			g_bShowLinenumbers = FALSE;

HMENU 			g_hMenuMain;
HMENU 			g_hMenuPopup;

enum Eol		g_iEol = EOL_CRLF;
enum Encoding 	g_iEncoding = ENCODING_UTF8;
enum IndentType g_iIndentType = INDENT_TYPE_TAB;
int				g_iIndentSize = 4;

int				g_iTheme = THEME_AUTO;

POINT 			g_ptPrintPaperSize = { 21000, 29700 };  		// in mm/100 (Din A4)
RECT 			g_rcPrintMargins = { 1000, 1500, 1000, 1500 };  // in mm/100

// Resource Strings
wchar_t			g_wszStatusCaretTempl[MAX_CARET_INFO_LEN];
wchar_t 		g_wszFilter[MAX_FILTER_LEN];
wchar_t 		g_wszUntitled[MAX_RESOURCE_TEXT_LEN];
wchar_t 		g_wszOpen[MAX_RESOURCE_TEXT_LEN];
wchar_t 		g_wszSaveAs[MAX_RESOURCE_TEXT_LEN];

wchar_t 		g_wszFindWhat[MAX_SEARCH_REPLACE_LEN] = L"";
wchar_t 		g_wszReplaceWith[MAX_SEARCH_REPLACE_LEN] = L"";

extern HHOOK 	g_hDetectDialogsHook;

//##########################################################
//
//##########################################################
void SetEol(int eol)
{
	StatusBarUpdateEol(eol);
	CheckMenuItem(GetSubMenu(g_hMenuPopup, 0), IDM_LINEENDINGS_CRLF + g_iEol, MF_BYCOMMAND|MF_UNCHECKED);
	CheckMenuItem(GetSubMenu(g_hMenuPopup, 0), IDM_LINEENDINGS_CRLF + eol, MF_BYCOMMAND|MF_CHECKED);
	g_iEol = (Eol)eol;
}

//##########################################################
//
//##########################################################
void SetEncoding(int encoding)
{
	StatusBarUpdateEncoding(encoding);
	CheckMenuItem(GetSubMenu(g_hMenuPopup, 1), IDM_ENCODING_ANSI + g_iEncoding, MF_BYCOMMAND|MF_UNCHECKED);
	CheckMenuItem(GetSubMenu(g_hMenuPopup, 1), IDM_ENCODING_ANSI  + encoding, MF_BYCOMMAND|MF_CHECKED);
	g_iEncoding = (Encoding)encoding;
}

//##########################################################
//
//##########################################################
void SetIndentType(int indentType)
{
	StatusBarUpdateIndentType(indentType);
	CheckMenuItem(GetSubMenu(g_hMenuPopup, 3), IDM_INDENT_TAB + g_iIndentType, MF_BYCOMMAND|MF_UNCHECKED);
	CheckMenuItem(GetSubMenu(g_hMenuPopup, 3), IDM_INDENT_TAB + indentType, MF_BYCOMMAND|MF_CHECKED);
	g_iIndentType = (IndentType)indentType;
}

//##########################################################
//
//##########################################################
void SetIndentSize(int indentSize)
{
	//StatusBarUpdateIndentSize(indentSize);
	CheckMenuItem(GetSubMenu(g_hMenuPopup, 4), IDM_INDENT_BASE + g_iIndentSize, MF_BYCOMMAND|MF_UNCHECKED);
	CheckMenuItem(GetSubMenu(g_hMenuPopup, 4), IDM_INDENT_BASE + indentSize, MF_BYCOMMAND|MF_CHECKED);
	g_iIndentSize = indentSize;
}

//##########################################################
//
//##########################################################
void SetTheme(int theme)
{
	CheckMenuItem(g_hMenuMain, IDM_VIEW_THEME_AUTO + g_iTheme, MF_BYCOMMAND|MF_UNCHECKED);
    CheckMenuItem(g_hMenuMain, IDM_VIEW_THEME_AUTO + theme, MF_BYCOMMAND|MF_CHECKED);
	g_iTheme = theme;

	BOOL bIsDark;
	if (g_iTheme == THEME_AUTO)
		bIsDark = AppsUseDarkTheme();
	else
		bIsDark = g_iTheme == THEME_DARK;
	if (bIsDark != g_bIsDark)
		SetDark(bIsDark);
}

//##########################################################
//
//##########################################################
void SetDark(BOOL bIsDark)
{
	g_bIsDark = bIsDark;

	DarkWindowTitleBar(g_hWndMain, g_bIsDark);

	if (g_bIsDark)
		SetWindowSubclass(g_hWndStatusbar, &DarkStatusBarClassProc, STATUSBAR_SUBCLASS_ID, 0);
	else
		RemoveWindowSubclass(g_hWndStatusbar, &DarkStatusBarClassProc, STATUSBAR_SUBCLASS_ID);

	g_pEdit->SetDark(bIsDark);

	DarkMenus(g_bIsDark);

	RedrawWindow(g_hWndMain, 0, 0, RDW_ERASE | RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);

	// Destroy and recreate existing Find/Replace dialogs, restore previous state
	if (g_hDlgFind)
	{
		BOOL bVisible = IsWindowVisible(g_hDlgFind);
		BOOL bCaseSensitive = SendDlgItemMessage(g_hDlgFind, IDC_FIND_MATCH_CASE, BM_GETCHECK, 0, 0) == BST_CHECKED;
		BOOL bForward = SendDlgItemMessage(g_hDlgFind, IDC_FIND_DOWN, BM_GETCHECK, 0, 0) == BST_CHECKED;
		BOOL bWrap = GetDlgItem(g_hDlgFind, IDC_FIND_WRAP_AROUND) && SendDlgItemMessage(g_hDlgFind, IDC_FIND_WRAP_AROUND, BM_GETCHECK, 0, 0) == BST_CHECKED;

		DestroyWindow(g_hDlgFind);
		CreateFindDialog();

		// Restore previous state
		SetDlgItemTextW(g_hDlgFind, IDC_FIND_EDIT, g_wszFindWhat);
		if (bCaseSensitive)
			SendDlgItemMessage(g_hDlgFind, IDC_FIND_MATCH_CASE, BM_SETCHECK, BST_CHECKED, 0);
		if (!bForward)
		{
			SendDlgItemMessage(g_hDlgFind, IDC_FIND_UP, BM_SETCHECK, BST_CHECKED, 0);
			SendDlgItemMessage(g_hDlgFind, IDC_FIND_DOWN, BM_SETCHECK, BST_UNCHECKED, 0);
		}
		if (bWrap)
			SendDlgItemMessage(g_hDlgFind, IDC_FIND_WRAP_AROUND, BM_SETCHECK, BST_CHECKED, 0);
		if (bVisible)
			ShowWindow(g_hDlgFind, SW_SHOW);
	}

	if (g_hDlgReplace)
	{
		BOOL bVisible = IsWindowVisible(g_hDlgReplace);
		BOOL bCaseSensitive = SendDlgItemMessage(g_hDlgReplace, IDC_FIND_MATCH_CASE, BM_GETCHECK, 0, 0) == BST_CHECKED;
		BOOL bWrap = GetDlgItem(g_hDlgReplace, IDC_FIND_WRAP_AROUND) && SendDlgItemMessage(g_hDlgReplace, IDC_FIND_WRAP_AROUND, BM_GETCHECK, 0, 0) == BST_CHECKED;

		DestroyWindow(g_hDlgReplace);
		CreateReplaceDialog();

		// Restore previous state
		SetDlgItemTextW(g_hDlgReplace, IDC_FIND_EDIT, g_wszFindWhat);
		SetDlgItemTextW(g_hDlgReplace, IDC_REPLACE_EDIT, g_wszReplaceWith);
		if (bCaseSensitive)
			SendDlgItemMessage(g_hDlgReplace, IDC_FIND_MATCH_CASE, BM_SETCHECK, BST_CHECKED, 0);
		if (bWrap)
			SendDlgItemMessage(g_hDlgReplace, IDC_FIND_WRAP_AROUND, BM_SETCHECK, BST_CHECKED, 0);
		if (bVisible)
			ShowWindow(g_hDlgReplace, SW_SHOW);
	}
}

//##########################################################
//
//##########################################################
BOOL AppsUseDarkTheme(void)
{
	BOOL bDark = FALSE;
    HKEY hkey;
    if (RegOpenKey(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", &hkey) == ERROR_SUCCESS)
    {
		DWORD dwData, cbData;
	    if (RegQueryValueEx(hkey, L"AppsUseLightTheme", NULL, NULL, (BYTE *)&dwData, &cbData) == ERROR_SUCCESS)
	    	bDark = !(BOOL)dwData;
	    RegCloseKey(hkey);
    }
    return bDark;
}

//##########################################################
//
//##########################################################
BOOL LoadFile(wchar_t *pwszFile, int encoding)
{
	DWORD dwLastIOError;

	if (lstrcmpi(PathFindExtension(pwszFile), L".lnk") == 0)
		PathResolveLnk(pwszFile);

	HANDLE hFile = CreateFile(
		pwszFile,
		GENERIC_READ,
		FILE_SHARE_READ|FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);

	if (hFile == INVALID_HANDLE_VALUE)
	{
		dwLastIOError = GetLastError();

		// If file doesn't exist, check if its directory exists
		wchar_t wszDir[MAX_PATH];
		wcscpy_s(wszDir, MAX_PATH, pwszFile);
		//PathCchRemoveFileSpec(wszDir, MAX_PATH); // Would require additional lib
		PathRemoveFileSpec(wszDir);

		if (PathIsDirectory(wszDir))
		{
			GetText(IDS_CANNOT_FIND_FILE);
			LPWSTR pwszMsg = GetFormattedMessage(wszText, pwszFile);
			int res = MessageBox(g_hWndMain, pwszMsg, APP_NAME, MB_YESNOCANCEL | MB_ICONWARNING);
			LocalFree(pwszMsg);
			SetFocus(g_pEdit->m_hWnd);

			if (res == IDYES)
			{
				hFile = CreateFile(
					pwszFile,
					GENERIC_READ,
					FILE_SHARE_READ|FILE_SHARE_WRITE,
					NULL,
					CREATE_NEW,
					FILE_ATTRIBUTE_NORMAL,
					NULL
				);
				if (hFile == INVALID_HANDLE_VALUE)
				{
					ShowSysErrorMsgBox(GetLastError());
					SetFocus(g_pEdit->m_hWnd);
					return FALSE;
				}
			}
			else
				return FALSE;
		}
		else
		{
			ShowSysErrorMsgBox(dwLastIOError);
			SetFocus(g_pEdit->m_hWnd);
			return FALSE;
		}
	}

	// calculate buffer limit
	DWORD dwFileSize = GetFileSize(hFile, NULL);
	if (dwFileSize > EDIT_MAX_TEXT_LEN)
	{
		GetText(IDS_FILE_TOO_LARGE);
		LPWSTR pwszMsg = GetFormattedMessage(wszText, pwszFile);
		MessageBox(g_hWndMain, pwszMsg, APP_NAME, MB_OK | MB_ICONWARNING);
		LocalFree(pwszMsg);
		SetFocus(g_pEdit->m_hWnd);
		CloseHandle(hFile);
		return FALSE;
	}

	char *lpData = (char *)GlobalAlloc(GPTR, dwFileSize + 1);

	DWORD dwData;
	BOOL bReadSuccess = ReadFile(hFile, lpData, dwFileSize, &dwData, NULL);
	dwLastIOError = GetLastError();
	CloseHandle(hFile);

	if (bReadSuccess)
	{
		lpData[dwFileSize] = 0;

		//enum Encoding
		if (encoding < 0)
			encoding = DetectEncoding((unsigned char*)lpData, dwFileSize);
		enum Eol eol;
		enum IndentType indentType;

		wchar_t *lpwText;

		BOOL bFree = FALSE;

		if (encoding == ENCODING_ANSI || encoding == ENCODING_UTF8 || encoding == ENCODING_UTF8_BOM)
		{
			lpwText = CodePageToUtf16(lpData, encoding == ENCODING_ANSI ? CP_ACP : CP_UTF8);
			bFree = TRUE;
		}
		else
		{
			if (encoding == ENCODING_UTF16_BE)
			{
				// Swap all bytes
				char *ptr = (char*)lpData;
				char b;
				for (DWORD i = 0; i < dwFileSize; i += 2)
				{
					b = ptr[i];
					ptr[i] = ptr[i + 1];
					ptr[i + 1] = b;
				}
			}

			lpwText = (wchar_t*)lpData;
		}

		eol = DetectEol(lpwText);

		if (eol == EOL_LF)
		{
			std::wstring wsText = ReplaceAll(lpwText, L"\n", L"\r\n", NULL);
			indentType = DetectIndentType(wsText.c_str());
			g_pEdit->SetText(wsText.c_str());
		}
		else if (eol == EOL_CR)
		{
			std::wstring wsText = ReplaceAll(lpwText, L"\r", L"\r\n", NULL);
			indentType = DetectIndentType(wsText.c_str());
			g_pEdit->SetText(wsText.c_str());
		}
		else
		{
			indentType = DetectIndentType(lpwText);
			g_pEdit->SetText(lpwText);
		}

		if (bFree)
			free(lpwText);

		SetIndentType(indentType);

		g_pEdit->m_bIndentUseSpaces = indentType == INDENT_TYPE_SPACES;

		SetEol(eol);
		SetEncoding(encoding);

		wcscpy_s(g_wszCurrentFile, MAX_PATH, pwszFile);
		g_bModified = FALSE;
		UpdateWindowTitle();
	}

	GlobalFree(lpData);

	if (!bReadSuccess)
	{
		ShowSysErrorMsgBox(dwLastIOError);
		SetFocus(g_pEdit->m_hWnd);
	}

	return bReadSuccess;
}

//##########################################################
//
//##########################################################
BOOL SaveFile(wchar_t *pwszFile, int encoding, int eol)
{
	FILE * f; // = _wfopen(pwszFile, L"wb");
	_wfopen_s(&f, pwszFile, L"wb");
	if (!f)
	{
		ShowSysErrorMsgBox(GetLastError());
		SetFocus(g_pEdit->m_hWnd);
		return FALSE;
	}

	wchar_t *utf16 = g_pEdit->GetText();
	BOOL bFree = TRUE;

	if (eol == EOL_LF)
	{
		std::wstring wsText = ReplaceAll(utf16, L"\r\n", L"\n", NULL);
		free(utf16);
		bFree = FALSE;
		utf16 = (wchar_t*)wsText.c_str();
	}
	else if (eol == EOL_CR)
	{
		std::wstring wsText = ReplaceAll(utf16, L"\r\n", L"\r", NULL);
		free(utf16);
		bFree = FALSE;
		utf16 = (wchar_t*)wsText.c_str();
	}

	switch(encoding)
	{
	case ENCODING_ANSI:
		{
			char * text = Utf16ToCodePage(utf16, CP_ACP);
			fwrite(text, sizeof(char), strlen(text), f);
			free(text);
		}
		break;

	case ENCODING_UTF8:
		{
			char * text = Utf16ToCodePage(utf16, CP_UTF8);
			fwrite(text, sizeof(char), strlen(text), f);
			free(text);
		}
		break;

	case ENCODING_UTF8_BOM:
		{
			char * text = Utf16ToCodePage(utf16, CP_UTF8);  // BOM UTF-8: EF BB BF
			fwrite("\xEF\xBB\xBF", sizeof(char), 3, f);
			fwrite(text, sizeof(char), strlen(text), f);
			free(text);
		}
		break;

	case ENCODING_UTF16_LE:
		fwrite("\xFF\xFE", sizeof(char), 2, f);  // BOM LE: FF FE
		fwrite(utf16, sizeof(wchar_t), wcslen(utf16), f);
		break;

	case ENCODING_UTF16_BE:
		fwrite("\xFE\xFF", sizeof(char), 2, f);  // BOM BE: FE FF
		// Swap all bytes
		char *ptr = (char*)utf16;
		char b;
		size_t dwDataSize = wcslen(utf16) * sizeof(wchar_t);
		for (DWORD i = 0; i < dwDataSize; i += 2)
		{
			b = ptr[i];
			ptr[i] = ptr[i + 1];
			ptr[i + 1] = b;
		}
		fwrite((char*)utf16, sizeof(char), dwDataSize, f);
		break;
	}

	if (bFree)
		free(utf16);

	fclose(f);

	return TRUE;
}

//##########################################################
//
//##########################################################
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInst, LPWSTR lpCmdLine, int nCmdShow)
{
	g_hInstance = hInstance;

	int X = CW_USEDEFAULT, Y = CW_USEDEFAULT, nWidth = CW_USEDEFAULT, nHeight = CW_USEDEFAULT;

	LOGFONT logFont = {};
	wcscpy_s(logFont.lfFaceName, LF_FACESIZE, DEFAULT_FONT);
	logFont.lfHeight = DEFAULT_FONT_SIZE;
	logFont.lfWeight = FW_NORMAL;
	logFont.lfItalic = 0;
	logFont.lfCharSet = DEFAULT_FONT_CHARSET;

	LoadSettings(&X, &Y, &nWidth, &nHeight, &logFont);

	if (IsWindows10OrGreater())
	{
		if (g_iTheme == THEME_AUTO)
			g_bIsDark = AppsUseDarkTheme();
		else
			g_bIsDark = g_iTheme == THEME_DARK;
	}
	else
		g_bIsDark = FALSE;

	// Init Common Controls
	INITCOMMONCONTROLSEX icex;
	icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
	icex.dwICC	= ICC_WIN95_CLASSES|ICC_COOL_CLASSES|ICC_BAR_CLASSES|ICC_USEREX_CLASSES;
	InitCommonControlsEx(&icex);

	if (g_bIsDark)
		DarkMenus(TRUE);

	{
		WNDCLASS	 		wc;
		wc.style			= CS_BYTEALIGNWINDOW | CS_DBLCLKS;
		wc.lpfnWndProc		= (WNDPROC)MainWndProc;
		wc.cbClsExtra		= 0;
		wc.cbWndExtra		= 0;
		wc.hInstance		= hInstance;
		wc.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP));
		wc.hCursor			= LoadCursor(NULL, IDC_ARROW);
		wc.hbrBackground 	= (HBRUSH)(COLOR_3DFACE + 1);
		wc.lpszMenuName		= MAKEINTRESOURCE(IDR_MAINMENU);
		wc.lpszClassName 	= APP_CLASS;
		if (!RegisterClass(&wc))
			return FALSE;
	}

	g_hWndMain = CreateWindowEx(
		0,
		APP_CLASS,
		APP_NAME,
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
		X,
		Y,
		nWidth,
		nHeight,
		NULL,
		NULL,
		hInstance,
		NULL
	);
	if (!g_hWndMain)
		return FALSE;

	if (g_bIsDark)
		DarkWindowTitleBar(g_hWndMain, TRUE);

	LoadResourceStrings();
	HACCEL hAccMain = LoadAccelerators(g_hInstance, L"GLOBALACC");
	g_hModule = LoadMuiFile();
	CreateMenus();
	CreateStatusbar();

	// Create edit control
	g_pEdit = new Edit(g_hWndMain, IDC_EDIT, logFont, g_bWordWrap, g_bShowLinenumbers);
	if (g_bIsDark)
		g_pEdit->SetDark(TRUE);

	StatusBarUpdateCaret(0, 0);
	StatusBarUpdateZoom(100);
	StatusBarUpdateIndentType(g_iIndentType);

	// Hook for finding and adjusting dialogs
	g_hDetectDialogsHook = SetWindowsHookEx(WH_CALLWNDPROCRET, (HOOKPROC)DialogHookProc, 0, GetCurrentThreadId());

	SetFocus(g_pEdit->m_hWnd);

	ShowWindow(g_hWndMain, SW_SHOWNORMAL);

	BOOL bFileLoaded = FALSE;

	if (wcslen(lpCmdLine))
	{
		int nArgs;
		LPWSTR *szArglist = CommandLineToArgvW(lpCmdLine, &nArgs);

		// If first argument either ends on "notepad" or "notepad.exe", we assume that this is caused
		// by running Notepatch in "Hijack mode" ("Debugger" entry in "Image File Execution Options"),
		// and ignore this argument.
		size_t len = wcslen(szArglist[0]);
		if ((len >= 7 && _wcsicmp(szArglist[0] + len - 7, L"notepad") == 0) || (len >= 11 && _wcsicmp(szArglist[0] + len - 11, L"notepad.exe") == 0))
		{
			szArglist++;
			nArgs--;
		}

		// Those are the 4 command line arguments supported by classic Notepad:
		// notepad /a file.txt => ANSI
		// notepad /w file.txt => UNICODE (UTF-16 LE)
		// notepad /p file.txt
		// notepad /pt "file.txt" "<printername>"

		if (nArgs > 1 && szArglist[0][0] == L'/')
		{

			// Print mode - fail silently, always quit immediately
			if (_wcsnicmp(szArglist[0], L"/p", 2) == 0)
			{
				wchar_t lpwPrinter[MAX_PATH];

				if (_wcsicmp(szArglist[0], L"/pt") == 0 && nArgs > 2)
					wcscpy_s(lpwPrinter, MAX_PATH, szArglist[2]);
				else
				{
					DWORD pcchBuffer = MAX_PATH;
					GetDefaultPrinter(lpwPrinter, &pcchBuffer);
				}
				if (LoadFile(szArglist[1], -1))
				{
					HDC hDC = CreateDC(NULL, lpwPrinter, NULL, NULL);
					if (hDC)
					{
						g_pEdit->Print(hDC, g_ptPrintPaperSize, g_rcPrintMargins, PathFindFileName(g_wszCurrentFile));
						DeleteDC(hDC);
					}
				}
				DestroyMenu(g_hMenuPopup);
				UnregisterClass(APP_CLASS, hInstance);
				return 0;
			}

			else if (_wcsicmp(szArglist[0], L"/a") == 0)
				bFileLoaded = LoadFile(szArglist[1], ENCODING_ANSI);

			else if (_wcsicmp(szArglist[0], L"/w") == 0)
				bFileLoaded = LoadFile(szArglist[1], ENCODING_UTF16_LE);
		}
		else if (nArgs > 0)
			bFileLoaded = LoadFile(szArglist[0], -1);
	}

	if (!bFileLoaded)
	{
		UpdateWindowTitle();
		StatusBarUpdateEol(EOL_CRLF);
		StatusBarUpdateEncoding(ENCODING_UTF8);
	}

	DragAcceptFiles(g_hWndMain, TRUE);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator(g_hWndMain, hAccMain, &msg))
		{
			if ((g_hDlgFind && IsDialogMessage(g_hDlgFind, &msg)) || (g_hDlgReplace && IsDialogMessage(g_hDlgReplace, &msg)))
				continue;
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	DestroyMenu(g_hMenuPopup);

	UnregisterClass(APP_CLASS, hInstance);

	return (int)(msg.wParam);
}

//##########################################################
//
//##########################################################
LRESULT CALLBACK MainWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (g_bIsDark && DarkHandleMenuMessages(hWnd, uMsg, wParam, lParam))
		return TRUE;

	switch(uMsg)
	{
		case WM_COMMAND:
			return OnCommand(hWnd, wParam, lParam);

		case WM_NOTIFY:
			return OnNotify(hWnd, wParam, lParam);

		case WM_INITMENU:
			OnInitMenu(hWnd, wParam, lParam);
			break;

		case WM_SIZE:
			OnSize(hWnd, wParam, lParam);
			break;

		case WM_DESTROY:
		case WM_ENDSESSION:
			if (uMsg == WM_DESTROY)
				PostQuitMessage(0);
			break;

		case WM_CLOSE:
			if (!MaybeSave())
				return 0;
			SaveSettings();
			DestroyWindow(hWnd);
			break;

		case WM_CTLCOLOREDIT:
	        if (g_bIsDark && (HWND)lParam == g_pEdit->m_hWnd)
	        {
	            SetTextColor((HDC)wParam, DARK_EDITOR_TEXT_COLOR);
	            SetBkColor((HDC)wParam, DARK_EDITOR_BG_COLOR);
	            SetDCBrushColor((HDC)wParam, DARK_EDITOR_BG_COLOR);
	            return (LRESULT)GetStockObject(DC_BRUSH);
	        }
	        else
	        	return DefWindowProc(hWnd, uMsg, wParam, lParam);
			break;

		case WM_DROPFILES:
			{
				WCHAR szBuf[MAX_PATH];
				HDROP hDrop = (HDROP)wParam;

				if (IsIconic(hWnd))
					ShowWindow(hWnd, SW_RESTORE);

				SetForegroundWindow(hWnd);

				DragQueryFile(hDrop, 0, szBuf, MAX_PATH);

				if (!PathIsDirectory(szBuf) && MaybeSave())
					LoadFile(szBuf, -1);

				DragFinish(hDrop);
			}
			break;

		case WM_SETTINGCHANGE:
			if (g_iTheme == THEME_AUTO && wcscmp((LPCWSTR)lParam, L"ImmersiveColorSet") == 0)
			{
				BOOL bIsDark = AppsUseDarkTheme();
				if (bIsDark != g_bIsDark)
					SetDark(bIsDark);
			}
			return 0;

		default:
			return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}

	return 0;
}

//##########################################################
// Handles WM_SIZE
//##########################################################
void OnSize(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
	if (wParam == SIZE_MINIMIZED)
		return;

	int cx = LOWORD(lParam);
	int cy = HIWORD(lParam);

	if (g_bShowStatusbar & !g_bFullscreen)
	{
		RECT rc;
		SendMessage(g_hWndStatusbar, WM_SIZE, 0, 0);
		GetWindowRect(g_hWndStatusbar, &rc);
		cy -= (rc.bottom - rc.top);
	}

	SetWindowPos(g_pEdit->m_hWnd, NULL, 0, 0, cx, cy, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

	//if (g_pEdit->m_bShowLinenos)
	//{
	//	RECT rc = { g_pEdit->m_iLinenoWidth + g_pEdit->m_iMargin, 0, cx, cy };
	//	SendMessage(g_pEdit->m_hWnd, EM_SETRECT, 0, (LPARAM)&rc);
	//}

	// Right align parts in statusbar
	SendMessage(g_hWndStatusbar, WM_SIZE, 0, 0);
    int sb_parts[STATUSBAR_NUM_PARTS] = { cx - 480, cx - 340, cx - 290, cx - 170, cx - 70, -1 };
	SendMessage(g_hWndStatusbar, SB_SETPARTS, 6, (LPARAM)sb_parts);
}

//##########################################################
// Handles WM_COMMAND
//##########################################################
LRESULT OnCommand(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
	const WORD wCommandID = LOWORD(wParam);
	const WORD wNotificationCode = HIWORD(wParam);

	if (wNotificationCode > 1)
	{
		if (wCommandID == IDC_EDIT)
		{
			switch (wNotificationCode)
			{
				case EN_CHANGE:
					{
						BOOL modified = wcscmp(g_wszCurrentFile, g_wszUntitled) == 0 && SendMessage(g_pEdit->m_hWnd, WM_GETTEXTLENGTH, 0, 0) == 0 ? FALSE : TRUE;
						if (modified != g_bModified)
						{
							g_bModified = modified;
							UpdateWindowTitle();
						}

						// Update caret info in statusbar
				        int iLineIndex = (int)SendMessage(g_pEdit->m_hWnd, EM_LINEFROMCHAR, -1, 0);
						int iPosStart, iPosEnd;
						SendMessage(g_pEdit->m_hWnd, EM_GETSEL, (WPARAM)&iPosStart, (LPARAM)&iPosEnd);
						int iPos = (int)SendMessage(g_pEdit->m_hWnd, EM_LINEINDEX, -1, 0);
						int iColIndex = iPosStart - iPos;
						StatusBarUpdateCaret(iLineIndex, iColIndex);
					}
					break;

				case EN_VSCROLL:
					if (g_pEdit->m_bShowLinenos)
						g_pEdit->OnVScroll(0, 0);
					//{
					//	RECT rc;
					//	GetClientRect(g_pEdit->m_hWnd, &rc);
					//	rc.right = g_pEdit->m_iLinenoWidth;
					//	InvalidateRect(g_pEdit->m_hWnd, &rc, TRUE);
					//}
					break;
			}

		}
		return 0;
	}

	switch (wCommandID)
	{

		//##########################################################
		//	File
		//##########################################################

		case IDM_FILE_NEW:
			if (!MaybeSave())
				return 0;

			g_pEdit->SetText(L"");
			SetEncoding(ENCODING_UTF8);
			SetEol(EOL_CRLF);
			g_bModified = FALSE;
			wcscpy_s(g_wszCurrentFile, MAX_PATH, g_wszUntitled);
			UpdateWindowTitle();
			break;

		case IDM_FILE_NEWWINDOW:
			{
				SaveSettings();
				wchar_t exe_file[MAX_PATH];
				GetModuleFileName(NULL, exe_file, MAX_PATH);
				ShellExecute(hwnd, NULL, exe_file, NULL, NULL, SW_SHOWNORMAL);
			}
			break;

		case IDM_FILE_OPEN:
			{
				if (!MaybeSave())
					return 0;
				wchar_t *filename = GetOpenFilename(g_wszOpen, NULL, L".txt", g_wszFilter);
				if (filename)
				{
					LoadFile(filename, -1);
					free(filename);
				}
			}
			break;

		case IDM_FILE_SAVE:
			if (wcscmp(g_wszCurrentFile, g_wszUntitled) != 0)
			{
				if (SaveFile(g_wszCurrentFile, g_iEncoding, g_iEol))
				{
					g_bModified = FALSE;
					UpdateWindowTitle();
				}
				break;
			}
			// else fall through
		case IDM_FILE_SAVEAS:
			{
				wchar_t *filename = GetSaveFilename(g_wszSaveAs, g_wszCurrentFile, L".txt", g_wszFilter);
				if (filename)
				{
					if (SaveFile(filename, g_iEncoding, g_iEol))
					{
						wcscpy_s(g_wszCurrentFile, MAX_PATH, filename);
						g_bModified = FALSE;
						UpdateWindowTitle();
					}
					free(filename);
				}
			}
			break;

		case IDM_LINEENDINGS_CRLF:
		case IDM_LINEENDINGS_LF:
		case IDM_LINEENDINGS_CR:
			if (wCommandID - IDM_LINEENDINGS_CRLF == g_iEol)
				break;
			SetEol(wCommandID - IDM_LINEENDINGS_CRLF);
			if (!g_bModified)
			{
				g_bModified = SendMessage(g_pEdit->m_hWnd, EM_GETLINECOUNT, 0, 0) > 1;
				if (g_bModified)
					UpdateWindowTitle();
			}
			break;

		case IDM_ENCODING_ANSI:
		case IDM_ENCODING_UTF8:
		case IDM_ENCODING_UTF8BOM:
		case IDM_ENCODING_UTF16BE:
		case IDM_ENCODING_UTF16LE:
			if (wCommandID - IDM_ENCODING_ANSI == g_iEncoding)
				break;
			SetEncoding(wCommandID - IDM_ENCODING_ANSI);
			if (!g_bModified)
			{
				g_bModified = SendMessage(g_pEdit->m_hWnd, WM_GETTEXTLENGTH, 0, 0) > 0;
				if (g_bModified)
					UpdateWindowTitle();
			}
			break;

		case IDM_FILE_PAGESETUP:
			{
		        PAGESETUPDLG psd = {};
		        psd.lStructSize = sizeof(PAGESETUPDLG);
		        psd.hwndOwner = hwnd;
		        psd.Flags = PSD_INHUNDREDTHSOFMILLIMETERS | PSD_MARGINS | PSD_ENABLEPAGESETUPHOOK;
		        psd.ptPaperSize = g_ptPrintPaperSize;
		        psd.rtMargin = g_rcPrintMargins;
		        psd.lpfnPageSetupHook = PageSetupDlgProc; // LPPAGESETUPHOOK
				if (PageSetupDlg(&psd))
				{
					g_ptPrintPaperSize.x = psd.ptPaperSize.x;
					g_ptPrintPaperSize.y = psd.ptPaperSize.y;
					SetRect(&g_rcPrintMargins, psd.rtMargin.left, psd.rtMargin.top, psd.rtMargin.right, psd.rtMargin.bottom);
				}
				SetFocus(g_pEdit->m_hWnd);
		    }
			break;

		case IDM_FILE_PRINT:
			{
				PRINTDLG pdlg = {};
				pdlg.lStructSize = sizeof(PRINTDLG);
				// When the following line is commented out, the legacy (non-dark) print dialog is shown instead
				//pdlg.hwndOwner = this->hwnd;
				pdlg.lpfnPrintHook = PrintDlgProc; // LPPRINTHOOKPROC
				pdlg.Flags = PD_RETURNDC | PD_USEDEVMODECOPIES | PD_NOSELECTION | PD_ENABLEPRINTHOOK;  // | PD_PRINTSETUP
				pdlg.nFromPage = 1;
				pdlg.nToPage = 1;
				pdlg.nMinPage = 1;
				pdlg.nMaxPage = 0xffff;
				BOOL bOk = PrintDlg(&pdlg);
				if (bOk)
					g_pEdit->Print(pdlg.hDC, g_ptPrintPaperSize, g_rcPrintMargins, PathFindFileName(g_wszCurrentFile));
				SetFocus(g_pEdit->m_hWnd);
			}
			break;

		case IDM_FILE_EXIT:
			SendMessage(g_hWndMain, WM_CLOSE, 0, 0);
			break;

		//##########################################################
		//	Edit
		//##########################################################

		case IDM_EDIT_UNDO:
			g_pEdit->Undo();
			break;

		case IDM_EDIT_CUT:
			g_pEdit->Cut();
			break;

		case IDM_EDIT_COPY:
			g_pEdit->Copy();
			break;

		case IDM_EDIT_PASTE:
			g_pEdit->Paste();
			break;

		case IDM_EDIT_CLEAR:
			g_pEdit->Clear();
			break;

		case IDM_EDIT_SELECTALL:
			g_pEdit->SelectAll();
			break;

		case IDM_EDIT_FIND:
			{
				if (g_hDlgReplace && IsWindowVisible(g_hDlgReplace))
					ShowWindow(g_hDlgReplace, SW_HIDE);
				if (!g_hDlgFind)
					CreateFindDialog();
				else
					CenterDialog(g_hDlgFind);

				wchar_t *pwszSel = g_pEdit->GetSelectedText();
				if (pwszSel)
				{
					SetDlgItemTextW(g_hDlgFind, IDC_FIND_EDIT, pwszSel);
					free(pwszSel);
				}
				else
					SetDlgItemTextW(g_hDlgFind, IDC_FIND_EDIT, g_wszFindWhat);

				ShowWindow(g_hDlgFind, SW_SHOW);
			}
			break;

		case IDM_EDIT_REPLACE:
			{
				if (g_hDlgFind && IsWindowVisible(g_hDlgFind))
					ShowWindow(g_hDlgFind, SW_HIDE);
				if (!g_hDlgReplace)
					CreateReplaceDialog();
				else
					CenterDialog(g_hDlgReplace);

				wchar_t *pwszSel = g_pEdit->GetSelectedText();
				if (pwszSel)
				{
					SetDlgItemTextW(g_hDlgReplace, IDC_FIND_EDIT, pwszSel);
					free(pwszSel);
				}
				else
					SetDlgItemTextW(g_hDlgReplace, IDC_FIND_EDIT, g_wszFindWhat);

				ShowWindow(g_hDlgReplace, SW_SHOW);
			}
			break;

		case IDM_EDIT_FINDNEXT:
		case IDM_EDIT_FINDPREV:
			{
				if (!g_hDlgFind)
					CreateFindDialog();

				wchar_t *pwszSel = g_pEdit->GetSelectedText();
				BOOL bHasText = pwszSel != NULL;
				if (pwszSel)
				{
					SetDlgItemTextW(g_hDlgFind, IDC_FIND_EDIT, pwszSel);
					free(pwszSel);
				}
				else
				{
					// Check if search edit contains text
					wchar_t wszFindWhat[MAX_PATH];
					GetDlgItemText(g_hDlgFind, IDC_FIND_EDIT, wszFindWhat, MAX_PATH);
					bHasText = wcslen(wszFindWhat) > 0;
				}

				SendDlgItemMessage(g_hDlgFind, wCommandID == IDM_EDIT_FINDNEXT ? IDC_FIND_DOWN : IDC_FIND_UP, BM_SETCHECK, BST_CHECKED, 0);
				SendDlgItemMessage(g_hDlgFind, wCommandID == IDM_EDIT_FINDNEXT ? IDC_FIND_UP : IDC_FIND_DOWN, BM_SETCHECK, BST_UNCHECKED, 0);

				if (bHasText)
					SendMessage(g_hDlgFind, WM_COMMAND, IDOK, 0);
				else
					ShowWindow(g_hDlgFind, SW_SHOW);
			}
			break;

		case IDM_EDIT_GOTO:
			if (g_hDlgFind && IsWindowVisible(g_hDlgFind))
				ShowWindow(g_hDlgFind, SW_HIDE);
			else if (g_hDlgReplace && IsWindowVisible(g_hDlgReplace))
				ShowWindow(g_hDlgReplace, SW_HIDE);
			DialogBoxParam(
				g_hModule,
				MAKEINTRESOURCE(IDD_GOTO),
				g_hWndMain,
				GotoDlgProc,
				0
			);
			break;

		case IDM_EDIT_TIMEDATE:
			{
    			SYSTEMTIME lt = {};
      			GetLocalTime(&lt);
				wchar_t wszTime[64];
				GetTimeFormat(
					LOCALE_NAME_USER_DEFAULT,
					TIME_NOSECONDS,
					&lt,
					NULL,
					wszTime,
					64
				);
				wchar_t wszDate[64];
				GetDateFormat(
					LOCALE_NAME_USER_DEFAULT,
					DATE_SHORTDATE,
					&lt,
					NULL,
					wszDate,
					64
				);
				wsprintf(wszText, L"%ws %ws", wszTime, wszDate);
				SendMessage(g_pEdit->m_hWnd, EM_REPLACESEL, TRUE, (LPARAM)wszText);
			}
			break;

		//##########################################################
		//	Format
		//##########################################################

		case IDM_FORMAT_WORDWRAP:
			{
				g_bWordWrap = !g_bWordWrap;
				g_pEdit->SetWordWrap(g_bWordWrap);
				SetWindowTheme(g_pEdit->m_hWnd, g_bIsDark ? L"DarkMode_Explorer" : L"Explorer", NULL);
				CheckMenuItem(g_hMenuMain, IDM_FORMAT_WORDWRAP, MF_BYCOMMAND | (g_bWordWrap ? MF_CHECKED : MF_UNCHECKED));
				if (g_bWordWrap && g_bShowLinenumbers)
				{
					g_bShowLinenumbers = FALSE;
					g_pEdit->m_bShowLinenos = FALSE;
					CheckMenuItem(g_hMenuMain, IDM_VIEW_LINENUMBERS, MF_BYCOMMAND | MF_UNCHECKED);
				}
			}
			break;

		case IDM_FORMAT_FONT:
			{
				CHOOSEFONT cf = {};
				cf.lStructSize = sizeof(CHOOSEFONT);
				cf.hwndOwner = hwnd;
				cf.Flags = CF_INITTOLOGFONTSTRUCT | CF_ENABLEHOOK;
				LOGFONT lf = g_pEdit->GetFont();
				cf.lpLogFont = &lf;
				cf.lpfnHook = ChooseFontDlgProc;  // LPCFHOOKPROC
		       	if (ChooseFont(&cf))
					g_pEdit->SetFont(*cf.lpLogFont);
				SetFocus(g_pEdit->m_hWnd);
			}
			break;

		case IDM_INDENT_TAB:
		case IDM_INDENT_SPACES:
			SetIndentType(wCommandID - IDM_INDENT_TAB);
			g_pEdit->SetUseSpaces(wCommandID != IDM_INDENT_TAB);
			break;

		case IDM_INDENT_2:
		case IDM_INDENT_4:
		case IDM_INDENT_8:
		case IDM_INDENT_16:
			SetIndentSize(wCommandID - IDM_INDENT_BASE);
			g_pEdit->SetTabSize(wCommandID - IDM_INDENT_BASE);
			break;

		//##########################################################
		//	View
		//##########################################################

		case IDM_VIEW_LINENUMBERS:
			{
				g_bShowLinenumbers = !g_bShowLinenumbers;

				if (g_bShowLinenumbers && g_bWordWrap)
				{
					g_bWordWrap = FALSE;
					g_pEdit->SetWordWrap(FALSE);
					CheckMenuItem(g_hMenuMain, IDM_FORMAT_WORDWRAP, MF_BYCOMMAND | MF_UNCHECKED);
				}

				g_pEdit->ShowLinenumbers(g_bShowLinenumbers);
				CheckMenuItem(g_hMenuMain, IDM_VIEW_LINENUMBERS, MF_BYCOMMAND | (g_bShowLinenumbers ? MF_CHECKED : MF_UNCHECKED));
			}
			break;

		case IDM_VIEW_ZOOMIN:
			StatusBarUpdateZoom(g_pEdit->ZoomIn());
			break;

		case IDM_VIEW_ZOOMOUT:
			StatusBarUpdateZoom(g_pEdit->ZoomOut());
			break;

		case IDM_VIEW_ZOOMRESET:
			g_pEdit->ZoomReset();
			StatusBarUpdateZoom(100);
			break;

		case IDM_VIEW_STATUSBAR:
			{
				RECT rc;
				GetClientRect(g_hWndMain, &rc);
				int y = 0;
				int cx = rc.right;
				int cy = rc.bottom;
				g_bShowStatusbar = !g_bShowStatusbar;
				if (g_bShowStatusbar)
				{
					GetWindowRect(g_hWndStatusbar, &rc);
					cy -= (rc.bottom - rc.top);
				}
				ShowWindow(g_hWndStatusbar, g_bShowStatusbar ? SW_SHOW : SW_HIDE);
				SetWindowPos(g_pEdit->m_hWnd, NULL, 0, y, cx, cy, SWP_NOZORDER | SWP_NOACTIVATE);
				CheckMenuItem(g_hMenuMain, IDM_VIEW_STATUSBAR, MF_BYCOMMAND | (g_bShowStatusbar ? MF_CHECKED : MF_UNCHECKED));
			}
			break;

		case IDM_VIEW_FULLSCREEN:
			{
				long style = GetWindowLongA(g_hWndMain, GWL_STYLE);
				if (g_bFullscreen) //exstyle & WS_EX_LAYERED)
				{
					g_bFullscreen = FALSE;
					style |= (WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
					SetWindowLongA(g_hWndMain, GWL_STYLE, style);

					SetMenu(g_hWndMain, g_hMenuMain);
					if (g_bShowStatusbar)
						ShowWindow(g_hWndStatusbar, SW_SHOW);
					ShowWindow(g_hWndMain, SW_NORMAL);
				}
				else
				{
					g_bFullscreen = TRUE;
					style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
					SetWindowLongA(g_hWndMain, GWL_STYLE, style);
//					g_hMenuMain = GetMenu(g_hWndMain);
					SetMenu(g_hWndMain, NULL);
					if (g_bShowStatusbar)
						ShowWindow(g_hWndStatusbar, SW_HIDE);
					ShowWindow(g_hWndMain, SW_SHOWMAXIMIZED);
				}
			}
			break;

		case IDM_VIEW_TRANSPARENT:
			{
				long exstyle = GetWindowLongA(g_hWndMain, GWL_EXSTYLE);
				if (g_bTransparentMode) //exstyle & WS_EX_LAYERED)
				{
					SetWindowLong(g_hWndMain, GWL_EXSTYLE, exstyle & ~WS_EX_LAYERED);
					g_bTransparentMode = FALSE;
				}
				else
				{
					SetWindowLong(g_hWndMain, GWL_EXSTYLE, exstyle | WS_EX_LAYERED);
					SetLayeredWindowAttributes(g_hWndMain, 0, 200, LWA_ALPHA);
					g_bTransparentMode = TRUE;
				}
				RedrawWindow(g_hWndMain, 0, 0, RDW_FRAME);
				CheckMenuItem(g_hMenuMain, IDM_VIEW_TRANSPARENT, MF_BYCOMMAND | (g_bTransparentMode ? MF_CHECKED : MF_UNCHECKED));
			}
			break;

		case IDM_VIEW_ALWAYSONTOP:
			if (g_bAlwaysOnTop)
			{
				SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE);
				g_bAlwaysOnTop = FALSE;
			}
			else
			{
				SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE);
				g_bAlwaysOnTop = TRUE;
			}
			CheckMenuItem(g_hMenuMain, IDM_VIEW_ALWAYSONTOP, MF_BYCOMMAND | (g_bAlwaysOnTop ? MF_CHECKED : MF_UNCHECKED));
			break;

		case IDM_VIEW_THEME_AUTO:
		case IDM_VIEW_THEME_DARK:
		case IDM_VIEW_THEME_LIGHT:
			SetTheme(wCommandID - IDM_VIEW_THEME_AUTO);
	    	break;

		//##########################################################
		//	Help
		//##########################################################

		case IDM_HELP_ABOUT:
			MessageBox(
				g_hWndMain,
				VERSION_FILEVERSION_LONG L"\n"
				VERSION_LEGALCOPYRIGHT_LONG L"\n\n"
				VERSION_DESCRIPTION,
				APP_NAME,
				MB_OK | MB_ICONINFORMATION
			);
			SetFocus(g_pEdit->m_hWnd);
			break;
	}
	return 0;
}

//##########################################################
// Handles WM_NOTIFY
//##########################################################
LRESULT OnNotify(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
	LPNMHDR pnmh = (LPNMHDR)lParam;

	switch (pnmh->idFrom)
	{
		case IDC_EDIT:
		{
			switch (pnmh->code)
			{
			case EVENT_CARET_POS_CHANGED:
				{
					EditNotification *en = (EditNotification*)lParam;
					StatusBarUpdateCaret(en->iArg1, en->iArg2);
				}
				break;
			}
		}
		break;

		case IDC_STATUSBAR:
		{
			switch (pnmh->code)
			{
				case NM_RCLICK:
				case NM_DBLCLK:
					{
						LPNMMOUSE lpnm = (LPNMMOUSE) lParam;
						switch (lpnm->dwItemSpec)
						{
							case STATUSBAR_PART_CARET:
								{
								if (pnmh->code == NM_DBLCLK)
									PostMessage(g_hWndMain, WM_COMMAND, IDM_EDIT_GOTO, 0);
								}
								break;
							case STATUSBAR_PART_EOL:
								{
									POINT pt;
									GetCursorPos(&pt);
					                WORD wCommandID = TrackPopupMenuEx(
					                	GetSubMenu(g_hMenuPopup, 0),
					                	TPM_LEFTBUTTON | TPM_RETURNCMD, pt.x, pt.y, g_hWndMain, NULL);
					                PostMessage(g_hWndMain, WM_NULL, 0, 0);
									if (wCommandID && wCommandID - IDM_LINEENDINGS_CRLF != g_iEol)
									{
										SetEol((Eol)(wCommandID - IDM_LINEENDINGS_CRLF));
										if (!g_bModified)
										{
											g_bModified = SendMessage(g_pEdit->m_hWnd, EM_GETLINECOUNT, 0, 0) > 1;
											if (g_bModified)
												UpdateWindowTitle();
										}
									}
								}
								break;
							case STATUSBAR_PART_ENCODING:
								{
									POINT pt;
									GetCursorPos(&pt);
					                WORD wCommandID = TrackPopupMenuEx(
					                	GetSubMenu(g_hMenuPopup, 1),
					                	TPM_LEFTBUTTON | TPM_RETURNCMD, pt.x, pt.y, g_hWndMain, NULL);
					                PostMessage(g_hWndMain, WM_NULL, 0, 0);
									if (wCommandID && wCommandID - IDM_ENCODING_ANSI != g_iEncoding)
									{
										SetEncoding((Encoding)(wCommandID - IDM_ENCODING_ANSI));
										if (!g_bModified)
										{
											g_bModified = SendMessage(g_pEdit->m_hWnd, WM_GETTEXTLENGTH, 0, 0) > 0;
											if (g_bModified)
												UpdateWindowTitle();
										}
									}
								}
								break;
							case STATUSBAR_PART_ZOOM:
								if (pnmh->code == NM_DBLCLK)
									PostMessage(g_hWndMain, WM_COMMAND, IDM_VIEW_ZOOMRESET, 0);
								break;
							case STATUSBAR_PART_INDENT:
								{
									POINT pt;
									GetCursorPos(&pt);
					                WORD wCommandID = TrackPopupMenuEx(
					                	GetSubMenu(g_hMenuPopup, 3),
					                	TPM_LEFTBUTTON | TPM_RETURNCMD, pt.x, pt.y, g_hWndMain, NULL);
					                PostMessage(g_hWndMain, WM_NULL, 0, 0);
									if (wCommandID && wCommandID - IDM_INDENT_TAB != g_iIndentType)
									{
										SetIndentType(wCommandID - IDM_INDENT_TAB);
										g_pEdit->SetUseSpaces(wCommandID != IDM_INDENT_TAB);
										if (!g_bModified)
										{
											g_bModified = SendMessage(g_pEdit->m_hWnd, WM_GETTEXTLENGTH, 0, 0) > 0;
											if (g_bModified)
												UpdateWindowTitle();
										}
									}
								}
								break;
						}
					}
					break;
			}
		}
		break;
	}

	return 0;
}

//##########################################################
// Handles WM_INITMENU
//##########################################################
void OnInitMenu(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
	UINT uEnable = MF_BYCOMMAND | (g_pEdit->HasSelection() ? MF_ENABLED : MF_DISABLED);
	EnableMenuItem(g_hMenuMain, IDM_EDIT_CUT, uEnable);
	EnableMenuItem(g_hMenuMain, IDM_EDIT_COPY, uEnable);
	EnableMenuItem(g_hMenuMain, IDM_EDIT_CLEAR, uEnable);

	uEnable = MF_BYCOMMAND | (IsClipboardFormatAvailable(CF_TEXT) ? MF_ENABLED : MF_DISABLED);
	EnableMenuItem(g_hMenuMain, IDM_EDIT_PASTE, uEnable);

	uEnable = MF_BYCOMMAND | (SendMessage(g_pEdit->m_hWnd, EM_CANUNDO, 0, 0) ? MF_ENABLED : MF_DISABLED);
	EnableMenuItem(g_hMenuMain, IDM_EDIT_UNDO, uEnable);

	uEnable = MF_BYCOMMAND | (g_bWordWrap ? MF_DISABLED : MF_ENABLED);
	EnableMenuItem(g_hMenuMain, IDM_EDIT_GOTO, uEnable);
}

//##########################################################
//
//##########################################################
void LoadResourceStrings(void)
{
	LoadString(g_hModule, IDS_STATUS_CARET_INFO, g_wszStatusCaretTempl, MAX_CARET_INFO_LEN);

	wchar_t txt[MAX_RESOURCE_TEXT_LEN];
	LoadString(g_hModule, IDS_TEXT_FILES, txt, MAX_RESOURCE_TEXT_LEN);
	wchar_t all[MAX_RESOURCE_TEXT_LEN];
	LoadString(g_hModule, IDS_ALL_FILES, all, MAX_RESOURCE_TEXT_LEN);

	swprintf(g_wszFilter, MAX_FILTER_LEN, L"%wsX*.txtX%ws (*.*)X*.*X", txt, all);
	size_t len = wcslen(g_wszFilter);
	g_wszFilter[wcslen(txt)] = L'\0';
	g_wszFilter[wcslen(txt) + 6] = L'\0';
	g_wszFilter[len - 5] = L'\0';
	g_wszFilter[len - 1] = L'\0';

	LoadString(g_hModule, IDS_UNTITLED, g_wszUntitled, MAX_RESOURCE_TEXT_LEN);
	wcscpy_s(g_wszCurrentFile, MAX_PATH, g_wszUntitled);

  	LoadString(g_hModule, IDS_OPEN, g_wszOpen, MAX_RESOURCE_TEXT_LEN);
  	LoadString(g_hModule, IDS_SAVE_AS, g_wszSaveAs, MAX_RESOURCE_TEXT_LEN);
}

//##########################################################
//
//##########################################################
HMODULE LoadMuiFile(void)
{
	HMODULE hMod = NULL;
	// Find preferred UI language and try to load a corresponding notepad.exe.mui file from the Windows directory
	ULONG ulNumLanguages;
	ULONG cchLanguagesBuffer = 0;
	if (GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &ulNumLanguages, NULL, &cchLanguagesBuffer) && cchLanguagesBuffer)
	{
		wchar_t *pwszLocale = (wchar_t*)malloc(cchLanguagesBuffer * sizeof(wchar_t));
		GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &ulNumLanguages, pwszLocale, &cchLanguagesBuffer);
		wchar_t muipath[MAX_PATH];
		GetWindowsDirectory(muipath, MAX_PATH);
		wcscat_s(muipath, MAX_PATH, L"\\");
		wcscat_s(muipath, MAX_PATH, pwszLocale);
		wcscat_s(muipath, MAX_PATH, L"\\notepad.exe.mui");
		hMod = LoadLibrary(muipath);
		free(pwszLocale);
	}
	return hMod;
}

//##########################################################
//
//##########################################################
void CreateMenus(void)
{
	g_hMenuPopup = LoadMenu(g_hInstance, MAKEINTRESOURCE(IDR_POPUPMENU));
	g_hMenuMain = GetMenu(g_hWndMain);

	if (g_hModule)
	{
		// If we successfully loaded a notepad.exe.mui file, replace embedded main menu
		// with the menu loaded from this file. Dialog and string resources will also
		// be loaded from this file.
		DestroyMenu(g_hMenuMain);
		g_hMenuMain = LoadMenu(g_hModule, MAKEINTRESOURCE(1));
		SetMenu(g_hWndMain, g_hMenuMain);
	}
	else
	{
		// Otherwise use the embedded resources
		g_hModule = g_hInstance;
	}

	HMENU hSubMenu;

	// File
	hSubMenu = GetSubMenu(g_hMenuMain, MENU_INDEX_FILE);
	int pos = IsWindows10OrGreater() ? 5 : 4;
	InsertMenu(hSubMenu, pos, MF_BYPOSITION | MF_SEPARATOR, NULL, NULL);
	InsertMenu(hSubMenu, pos + 1, MF_BYPOSITION | MF_POPUP, (UINT_PTR)GetSubMenu(g_hMenuPopup, 0), L"EOL");
	InsertMenu(hSubMenu, pos + 2, MF_BYPOSITION | MF_POPUP, (UINT_PTR)GetSubMenu(g_hMenuPopup, 1), L"Encoding");
	InsertMenu(hSubMenu, pos + 3, MF_BYPOSITION | MF_POPUP, (UINT_PTR)GetSubMenu(g_hMenuPopup, 3), L"Indent");

	// Edit
	hSubMenu = GetSubMenu(g_hMenuMain, MENU_INDEX_EDIT);
	// Remove "Search with Bing..." item
	RemoveMenu(hSubMenu, IDM_EDIT_SEARCH_BING, MF_BYCOMMAND);

	// Format
	hSubMenu = GetSubMenu(g_hMenuMain, MENU_INDEX_FORMAT);
	AppendMenu(hSubMenu, MF_SEPARATOR, NULL, NULL);
	AppendMenu(hSubMenu, MF_POPUP, (UINT_PTR)GetSubMenu(g_hMenuPopup, 4), L"Indent Size");

	// View
	hSubMenu = GetSubMenu(g_hMenuMain, MENU_INDEX_VIEW);
	AppendMenu(hSubMenu, MF_STRING, IDM_VIEW_LINENUMBERS, L"Line Numbers\tAlt+N");
	AppendMenu(hSubMenu, MF_SEPARATOR, NULL, NULL);
	AppendMenu(hSubMenu, MF_STRING, IDM_VIEW_FULLSCREEN, L"Fullscreen\tF11");
	AppendMenu(hSubMenu, MF_STRING, IDM_VIEW_TRANSPARENT, L"Transparent\tAlt+T");
	AppendMenu(hSubMenu, MF_STRING, IDM_VIEW_ALWAYSONTOP, L"Always on Top\tAlt+A");

	// Add "Theme" submenu only in Win 10 and later
	if (IsWindows10OrGreater())
	{
		AppendMenu(hSubMenu, MF_SEPARATOR, NULL, NULL);
		AppendMenu(hSubMenu, MF_POPUP, (UINT_PTR)GetSubMenu(g_hMenuPopup, 2), L"Theme");
		CheckMenuItem(hSubMenu, IDM_VIEW_THEME_AUTO + g_iTheme, MF_BYCOMMAND | MF_CHECKED);
	}

	// Help
	hSubMenu = GetSubMenu(g_hMenuMain, MENU_INDEX_HELP);

	// Change About menu item's text (in engl. it's e.g. "About Notepad") to neutral "Info"
	MENUITEMINFO mmi = {};
	mmi.cbSize = sizeof(MENUITEMINFO);
	mmi.fMask = MIIM_STRING;
	mmi.dwTypeData = L"Info\tF1";
	SetMenuItemInfo(hSubMenu, IDM_HELP_ABOUT, MF_BYCOMMAND, &mmi);

	// Remove "Help", "Feedback" and separator, so only the "About" item is left
	RemoveMenu(hSubMenu, IDM_HELP_HELP, MF_BYCOMMAND);
	RemoveMenu(hSubMenu, IDM_HELP_FEEDBACK, MF_BYCOMMAND);
	RemoveMenu(hSubMenu, 0, MF_BYPOSITION);

	if (g_bShowStatusbar)
		CheckMenuItem(g_hMenuMain, IDM_VIEW_STATUSBAR, MF_BYCOMMAND | MF_CHECKED);
	if (g_bWordWrap)
		CheckMenuItem(g_hMenuMain, IDM_FORMAT_WORDWRAP, MF_BYCOMMAND |  MF_CHECKED);
}

//##########################################################
//
//##########################################################
void CreateStatusbar(void)
{
	DWORD dwStatusbarStyle = WS_CHILD | WS_CLIPSIBLINGS;
	if (g_bShowStatusbar)
		dwStatusbarStyle |= WS_VISIBLE;

	g_hWndStatusbar = CreateStatusWindow(dwStatusbarStyle, NULL, g_hWndMain, IDC_STATUSBAR);

	int sb_parts[STATUSBAR_NUM_PARTS] = { 0, 0, 0, 0, 0, 0 };
	SendMessage(g_hWndStatusbar, SB_SETPARTS, 6, (LPARAM)sb_parts);

	if (g_bIsDark)
		SetWindowSubclass(g_hWndStatusbar, &DarkStatusBarClassProc, STATUSBAR_SUBCLASS_ID, 0);
}

//##########################################################
//
//##########################################################
void CreateFindDialog(void)
{
	FINDREPLACE *fr = (FINDREPLACE *)malloc(sizeof(FINDREPLACE));
	memset(fr, 0, sizeof(FINDREPLACE));
	fr->lStructSize = sizeof(FINDREPLACE);
	fr->hwndOwner = g_hWndMain;
	fr->Flags = FR_DOWN | FR_FINDNEXT | FR_HIDEWHOLEWORD | FR_ENABLEHOOK;
	fr->lpstrFindWhat = g_wszFindWhat;
	fr->wFindWhatLen = MAX_SEARCH_REPLACE_LEN;
	fr->lpfnHook = FindDlgProc; // LPFRHOOKPROC
	g_hDlgFind = FindText(fr);
}

//##########################################################
//
//##########################################################
void CreateReplaceDialog(void)
{
	FINDREPLACE *fr = (FINDREPLACE *)malloc(sizeof(FINDREPLACE));
	memset(fr, 0, sizeof(FINDREPLACE));
	fr->lStructSize = sizeof(FINDREPLACE);
	fr->hwndOwner = g_hWndMain;
	fr->Flags = FR_DOWN | FR_FINDNEXT | FR_HIDEWHOLEWORD | FR_ENABLEHOOK;
	fr->lpstrFindWhat = g_wszFindWhat;
	fr->wFindWhatLen = MAX_SEARCH_REPLACE_LEN;
	fr->lpstrReplaceWith = g_wszReplaceWith;
	fr->wReplaceWithLen = MAX_SEARCH_REPLACE_LEN;
	fr->lpfnHook = ReplaceDlgProc; // LPFRHOOKPROC
	g_hDlgReplace = ReplaceText(fr);
}

//##########################################################
//
//##########################################################
void StatusBarUpdateCaret(int ln, int col)
{
	wchar_t caret_info[MAX_CARET_INFO_LEN];
	wsprintf(caret_info, g_wszStatusCaretTempl, ln + 1, col + 1);
	SendMessage(g_hWndStatusbar, SB_SETTEXT, STATUSBAR_PART_CARET, (LPARAM)caret_info);
}

//##########################################################
//
//##########################################################
void StatusBarUpdateZoom(int pct)
{
	wchar_t zoom_info[12];
	wsprintf(zoom_info, L"%i%%", pct);
	SendMessage(g_hWndStatusbar, SB_SETTEXT, STATUSBAR_PART_ZOOM, (LPARAM)zoom_info);
}

//##########################################################
//
//##########################################################
void StatusBarUpdateEol(int eol) //enum Eol eol)
{
	switch(eol)
	{
	case EOL_CRLF:
		SendMessage(g_hWndStatusbar, SB_SETTEXT, STATUSBAR_PART_EOL, (LPARAM)L"Windows (CRLF)");
		break;
	case EOL_LF:
		SendMessage(g_hWndStatusbar, SB_SETTEXT, STATUSBAR_PART_EOL, (LPARAM)L"Linux (LF)");
		break;
	case EOL_CR:
		SendMessage(g_hWndStatusbar, SB_SETTEXT, STATUSBAR_PART_EOL, (LPARAM)L"Mac (CR)");
		break;
	}
}

//##########################################################
//
//##########################################################
void StatusBarUpdateEncoding(int encoding) //enum Encoding encoding)
{
	switch(encoding)
	{
	case ENCODING_ANSI:
		SendMessage(g_hWndStatusbar, SB_SETTEXT, STATUSBAR_PART_ENCODING, (LPARAM)L"ANSI");
		break;
	case ENCODING_UTF8:
		SendMessage(g_hWndStatusbar, SB_SETTEXT, STATUSBAR_PART_ENCODING, (LPARAM)L"UTF-8");
		break;
	case ENCODING_UTF8_BOM:
		SendMessage(g_hWndStatusbar, SB_SETTEXT, STATUSBAR_PART_ENCODING, (LPARAM)L"UTF-8 (BOM)");
		break;
	case ENCODING_UTF16_LE:
		SendMessage(g_hWndStatusbar, SB_SETTEXT, STATUSBAR_PART_ENCODING, (LPARAM)L"UTF-16");
		break;
	case ENCODING_UTF16_BE:
		SendMessage(g_hWndStatusbar, SB_SETTEXT, STATUSBAR_PART_ENCODING, (LPARAM)L"UTF-16 BE");
		break;
	}
}

//##########################################################
//
//##########################################################
void StatusBarUpdateIndentType(int indentType)
{
	switch(indentType)
	{
	case INDENT_TYPE_TAB:
		SendMessage(g_hWndStatusbar, SB_SETTEXT, STATUSBAR_PART_INDENT, (LPARAM)L"Tab");
		break;
	case INDENT_TYPE_SPACES:
		SendMessage(g_hWndStatusbar, SB_SETTEXT, STATUSBAR_PART_INDENT, (LPARAM)L"Spaces");
		break;
	}
}

//##########################################################
//
//##########################################################
void UpdateWindowTitle()
{
	swprintf(g_wszWindowTitle, MAX_WIN_TITLE, g_bModified ? L"*%ws - %ws" : L"%ws - %ws", g_wszCurrentFile, APP_NAME);
	SetWindowText(g_hWndMain, g_wszWindowTitle);
}

//##########################################################
//
//##########################################################
BOOL MaybeSave(void)
{
	if (!g_bModified)
		return TRUE;

  	GetText(IDS_SAVE_CHANGES);
	LPWSTR pwszMsg = GetFormattedMessage(wszText, g_wszCurrentFile);
	int res = MessageBox(g_hWndMain, pwszMsg, APP_NAME, MB_YESNOCANCEL);
	LocalFree(pwszMsg);
	SetFocus(g_pEdit->m_hWnd);

	if (res == IDCANCEL)
		return FALSE;
	else if (res == IDYES)
	{
		if (wcscmp(g_wszCurrentFile, g_wszUntitled) != 0)
			SaveFile(g_wszCurrentFile, g_iEncoding, g_iEol);
		else
		{
			//DWORD dwEncoding = g_iEncoding;
			wchar_t *filename = GetSaveFilename(g_wszSaveAs, g_wszCurrentFile, NULL, g_wszFilter); // , &dwEncoding);
			if (filename)
			{
				SaveFile(filename, g_iEncoding, g_iEol);
				free(filename);
			}
			else
				return FALSE;
		}
	}
	return TRUE;
}

//##########################################################
//
//##########################################################
void LoadSettings(int *X, int *Y, int *nWidth, int *nHeight, LOGFONT *lf)
{
    HKEY hkey;
    if (RegOpenKey(HKEY_CURRENT_USER, L"Software\\Notepatch", &hkey) != ERROR_SUCCESS)
    {
    	RegCreateKey(HKEY_CURRENT_USER, L"Software\\Notepatch", &hkey);
    	return;
    }

	DWORD dwData;
	DWORD cbData = sizeof(DWORD);
    if (RegQueryValueEx(hkey, L"iTheme", NULL, NULL, (BYTE *)&dwData, &cbData) == ERROR_SUCCESS)
    	g_iTheme = (int)dwData;

	if (RegQueryValueEx(hkey, L"bShowStatusbar", NULL, NULL, (BYTE *)&dwData, &cbData) == ERROR_SUCCESS)
		g_bShowStatusbar = (BOOL)dwData;
	if (RegQueryValueEx(hkey, L"bWordWrap", NULL, NULL, (BYTE *)&dwData, &cbData) == ERROR_SUCCESS)
		g_bWordWrap = (BOOL)dwData;
	if (RegQueryValueEx(hkey, L"bShowLinenumbers", NULL, NULL, (BYTE *)&dwData, &cbData) == ERROR_SUCCESS)
		g_bShowLinenumbers = (BOOL)dwData;

	if (RegQueryValueEx(hkey, L"iWindowPosLeft", NULL, NULL, (BYTE *)&dwData, &cbData) == ERROR_SUCCESS)
		*X = (int)dwData;
	if (RegQueryValueEx(hkey, L"iWindowPosTop", NULL, NULL, (BYTE *)&dwData, &cbData) == ERROR_SUCCESS)
		*Y = (int)dwData;
	if (RegQueryValueEx(hkey, L"iWindowPosRight", NULL, NULL, (BYTE *)&dwData, &cbData) == ERROR_SUCCESS)
		*nWidth = (int)dwData - *X;
	if (RegQueryValueEx(hkey, L"iWindowPosBottom", NULL, NULL, (BYTE *)&dwData, &cbData) == ERROR_SUCCESS)
		*nHeight = (int)dwData - *Y;

    cbData = LF_FACESIZE;
    wchar_t wszFaceName[LF_FACESIZE];
    if (RegQueryValueEx(hkey, L"lfFaceName", NULL, NULL, (BYTE *)&wszFaceName, &cbData) == ERROR_SUCCESS)
		wcscpy_s(lf->lfFaceName, LF_FACESIZE, wszFaceName);

	cbData = sizeof(LONG);
	if (RegQueryValueEx(hkey, L"lfWeight", NULL, NULL, (BYTE *)&dwData, &cbData) == ERROR_SUCCESS)
		lf->lfWeight = (LONG)dwData;
	if (RegQueryValueEx(hkey, L"lfHeight", NULL, NULL, (BYTE *)&dwData, &cbData) == ERROR_SUCCESS)
		lf->lfHeight = (LONG)dwData;
	if (RegQueryValueEx(hkey, L"lfItalic", NULL, NULL, (BYTE *)&dwData, &cbData) == ERROR_SUCCESS)
		lf->lfItalic = (BYTE)dwData;
	if (RegQueryValueEx(hkey, L"lfCharSet", NULL, NULL, (BYTE *)&dwData, &cbData) == ERROR_SUCCESS)
		lf->lfCharSet = (BYTE)dwData;

    RegCloseKey(hkey);
}

//##########################################################
//
//##########################################################
void SaveSettings(void)
{
    HKEY hkey;
    if (RegOpenKey(HKEY_CURRENT_USER, L"Software\\Notepatch", &hkey) != ERROR_SUCCESS)
    	if (RegCreateKey(HKEY_CURRENT_USER, L"Software\\Notepatch", &hkey) != ERROR_SUCCESS)
    		return;

    RegSetValueEx(hkey, L"iTheme", 0, REG_DWORD, (BYTE *)&g_iTheme, sizeof(g_iTheme));
    RegSetValueEx(hkey, L"bShowStatusbar", 0, REG_DWORD, (BYTE *)&g_bShowStatusbar, sizeof(g_bShowStatusbar));
	RegSetValueEx(hkey, L"bWordWrap", 0, REG_DWORD, (BYTE *)&g_bWordWrap, sizeof(g_bWordWrap));
	RegSetValueEx(hkey, L"bShowLinenumbers", 0, REG_DWORD, (BYTE *)&g_bShowLinenumbers, sizeof(g_bWordWrap));

    RECT rc;
	ShowWindow(g_hWndMain, SW_RESTORE);
    GetWindowRect(g_hWndMain, &rc);

    RegSetValueEx(hkey, L"iWindowPosLeft", 0, REG_DWORD, (BYTE *)&rc.left, sizeof(rc.left));
	RegSetValueEx(hkey, L"iWindowPosTop", 0, REG_DWORD, (BYTE *)&rc.top, sizeof(rc.top));
	RegSetValueEx(hkey, L"iWindowPosRight", 0, REG_DWORD, (BYTE *)&rc.right, sizeof(rc.right));
	RegSetValueEx(hkey, L"iWindowPosBottom", 0, REG_DWORD, (BYTE *)&rc.bottom, sizeof(rc.bottom));

	LOGFONT lf = g_pEdit->GetFont();
    RegSetValueEx(hkey, L"lfFaceName", 0, REG_SZ, (BYTE *)&lf.lfFaceName, LF_FACESIZE);
	RegSetValueEx(hkey, L"lfWeight", 0, REG_DWORD, (BYTE *)&lf.lfWeight, sizeof(LONG));
	RegSetValueEx(hkey, L"lfHeight", 0, REG_DWORD, (BYTE *)&lf.lfHeight, sizeof(LONG));
	RegSetValueEx(hkey, L"lfItalic", 0, REG_DWORD, (BYTE *)&lf.lfItalic, sizeof(LONG));
	RegSetValueEx(hkey, L"lfCharSet", 0, REG_DWORD, (BYTE *)&lf.lfCharSet, sizeof(LONG));

    RegCloseKey(hkey);
}
