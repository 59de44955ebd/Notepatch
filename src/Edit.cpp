#include <windows.h>
#include <shlwapi.h>
#include <strsafe.h>
#include <math.h>

#include "Dark.h"
#include "Edit.h"
#include "Helpers.h"
#include "Notepatch.h"

#define EDIT_SUBCLASS_ID 10
#define UNITS_PER_INCH 2540
#define NON_WORD L" \t'\".,:!?-()[]{}+-*=/\\<>#$§%&|`´^"

//##########################################################
//
//##########################################################
Edit::Edit(HWND hWndParent, long m_lControlID, LOGFONT logFont, BOOL bWordWrap)
{
	LONG lStyle = WS_VISIBLE | WS_CHILD | WS_TABSTOP | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_NOHIDESEL;
	if (!bWordWrap)
		lStyle = lStyle | WS_HSCROLL;

	m_hWnd = CreateWindowEx(
		0,
		WC_EDIT,
		NULL,
		lStyle,
		0, 0, 0, 0,
		hWndParent,
		NULL,
		NULL,
		NULL
	);

	m_lControlID = m_lControlID;
	SetWindowLong(m_hWnd, GWL_ID, m_lControlID);

	SendMessage(m_hWnd, EM_SETLIMITTEXT, EDIT_MAX_TEXT_LEN, 0);

	m_iMargin = 4;
	SendMessage(m_hWnd, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, (LPARAM)MAKELONG(m_iMargin, m_iMargin));

	SetWindowSubclass(m_hWnd, &__EditSubClassProc, EDIT_SUBCLASS_ID, (DWORD_PTR)this);

	HDC hdc = GetDC(NULL);
	m_iDpiY = GetDeviceCaps(hdc, LOGPIXELSY);
	ReleaseDC(NULL, hdc);

	SetFont(logFont);

	UINT uiTabWidth = 4 * m_iIndentSize;
	SendMessage(m_hWnd, EM_SETTABSTOPS, 1, (LPARAM)&uiTabWidth);
}

//##########################################################
//
//##########################################################
void Edit::SetFont(LOGFONT logFont)
{
	m_logFont = logFont;
	HFONT hFont = CreateFontIndirect(&logFont);
	m_iFontSize = -MulDiv(logFont.lfHeight, 72, m_iDpiY);
	SendMessage(m_hWnd, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(1, 0));

    HDC hDc = GetDC(NULL);
    SelectObject(hDc, hFont);
    TEXTMETRIC tm = {};
    GetTextMetrics(hDc, &tm);
    ReleaseDC(NULL, hDc);
    m_iCharWidth = tm.tmAveCharWidth;
}

//##########################################################
// Must return base font, not zoomed font
//##########################################################
LOGFONT Edit::GetFont(void)
{
	LOGFONT logFont = {};
	wcscpy_s(logFont.lfFaceName, LF_FACESIZE, m_logFont.lfFaceName);
	logFont.lfHeight = -MulDiv(m_iFontSize, m_iDpiY, 72);
	logFont.lfWeight = m_logFont.lfWeight;
	logFont.lfItalic = m_logFont.lfItalic;
	logFont.lfCharSet = m_logFont.lfCharSet;
	return logFont;
}

//##########################################################
// Text must use CRLF as EOL
//##########################################################
void Edit::SetText(const wchar_t* pwszText)
{
	SendMessage(m_hWnd, WM_SETTEXT, 0, (LPARAM)pwszText);
}

//##########################################################
// Returns text with CRLF as EOL
//##########################################################
wchar_t* Edit::GetText(void)
{
    int iBufSize = (int)SendMessage(m_hWnd, WM_GETTEXTLENGTH, 0, 0) + 1;
    wchar_t *pwszBuf = (LPWSTR)malloc(iBufSize * sizeof(wchar_t));
    SendMessage(m_hWnd, WM_GETTEXT, iBufSize, (LPARAM)pwszBuf);
    return pwszBuf;
}

//##########################################################
//
//##########################################################
BOOL Edit::HasSelection(void)
{
	int iPosStart, iPosEnd;
	SendMessage(m_hWnd, EM_GETSEL, (WPARAM)&iPosStart, (LPARAM)&iPosEnd);
	return iPosEnd > iPosStart;
}

//##########################################################
//
//##########################################################
wchar_t* Edit::GetSelectedText(void)
{
	int iPosStart, iPosEnd;
	SendMessage(m_hWnd, EM_GETSEL, (WPARAM)&iPosStart, (LPARAM)&iPosEnd);
	if (iPosEnd <= iPosStart)
		return NULL;

	HLOCAL hMem = (HLOCAL)SendMessage(m_hWnd, EM_GETHANDLE, 0, 0);
	wchar_t *pch = (wchar_t*)LocalLock(hMem);
	pch += iPosStart;

	wchar_t *buf = (wchar_t*)malloc((iPosEnd - iPosStart + 1) * sizeof(wchar_t));
	wcscpy_s(buf, iPosEnd - iPosStart, pch);
	buf[iPosEnd - iPosStart] = L'\0';
	LocalUnlock(hMem);
	return buf;
}

//##########################################################
//
//##########################################################
BOOL Edit::Print(HDC hDC, POINT ptPrintPaperSize, RECT rcPrintMargins, wchar_t *pwszDocName)
{
	DOCINFO di = {};
	di.cbSize = sizeof(DOCINFO);
	di.lpszDocName = pwszDocName;
	int job_id = StartDoc(hDC, &di);
	if (job_id <= 0)
		return FALSE;

	// Get printer resolution
	POINT ptDpi;
	ptDpi.x = GetDeviceCaps(hDC, LOGPIXELSX);
	ptDpi.y = GetDeviceCaps(hDC, LOGPIXELSY);

	// Set current font
    HFONT hfont = CreateFont(
    	-MulDiv(m_iFontSize, ptDpi.y, 72),
    	0, 0, 0,
    	m_logFont.lfWeight,
    	m_logFont.lfItalic,
    	FALSE, FALSE,
    	m_logFont.lfCharSet,
    	OUT_TT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        m_logFont.lfFaceName
    );

	SelectObject(hDC, hfont);

	// Set page rect in logical units, rcPage is the rectangle {0, 0, maxX, maxY} where
	// maxX+1 and maxY+1 are the number of physically printable pixels in x and y.
	// rc is the rectangle to render the text in (which will, of course, fit within the
	// rectangle defined by rcPage).
	RECT rcPage = {
		0,
		0,
		(LONG)round(ptPrintPaperSize.x * ptDpi.x / UNITS_PER_INCH),
		(LONG)round(ptPrintPaperSize.y * ptDpi.x / UNITS_PER_INCH)
	};

	RECT rcDraw = {
		(LONG)round(rcPrintMargins.left * ptDpi.x / UNITS_PER_INCH),
		(LONG)round(rcPrintMargins.top * ptDpi.x / UNITS_PER_INCH),
		rcPage.right - (LONG)round(rcPrintMargins.right * ptDpi.x / UNITS_PER_INCH),
		rcPage.bottom - (LONG)round(rcPrintMargins.bottom * ptDpi.x / UNITS_PER_INCH)
	};

	int iRcTop = rcDraw.top;
	int iRcBottom = rcDraw.bottom;

	int iFirstPage = 1;
	int iLastPage = 0xFFFFFF;

	BOOL bSuccess = StartPage(hDC);
	int iPage = 1;
	int iHeight;

	wchar_t *pwszText = GetText();
	wchar_t* pwszBuf;
	wchar_t* pwszLine = wcstok_s(pwszText, L"\n", &pwszBuf);
	int iLineLen;

	RECT rcCalc = { rcDraw.left, 0, rcDraw.right, 0 };

	while (pwszLine)
	{
		iLineLen = (int)wcslen(pwszLine);

		iHeight = DrawText(
			hDC,
			iLineLen ? pwszLine : L" ",
			-1,
			&rcCalc,
			DT_CALCRECT | DT_WORDBREAK
		);

        if (rcDraw.top + iHeight > iRcBottom)
        {
            iPage++;
            if (iPage > iLastPage)
                break;
            else if (iPage > iFirstPage)
            {
                EndPage(hDC);
                StartPage(hDC);
            }
			rcDraw.top = iRcTop;
        }

        if (iPage < iFirstPage)
        {
			rcDraw.top += iHeight;
            continue;
		}

		// remove trailing CR
		pwszLine[iLineLen - 1] = L'\0';

        iHeight = DrawText(
            hDC,
            iLineLen ? pwszLine : L"\n",
            -1,
            &rcDraw,
           	iLineLen ? DT_WORDBREAK : DT_SINGLELINE
        );
		rcDraw.top += iHeight;

		pwszLine = wcstok_s(nullptr, L"\n", &pwszBuf);
	}

	if (bSuccess)
		EndDoc(hDC);
	else
		AbortDoc(hDC);

	free(pwszText);

	return bSuccess;
}

//##########################################################
//
//##########################################################
void Edit::Undo(void)
{
	SendMessage(m_hWnd, EM_UNDO, 0, 0);
}

//##########################################################
//
//##########################################################
void Edit::Cut(void)
{
	SendMessage(m_hWnd, WM_CUT, 0, 0);
}

//##########################################################
//
//##########################################################
void Edit::Copy(void)
{
	SendMessage(m_hWnd, WM_COPY, 0, 0);
}

//##########################################################
// WM_PASTE can't handle Unix/macOS EOL
//##########################################################
void Edit::Paste(void)
{
	if (!IsClipboardFormatAvailable(CF_UNICODETEXT) || !OpenClipboard(NULL))
		return;

	wchar_t *lpwPastedText = NULL;
	Eol eol;

	HANDLE hdata = GetClipboardData(CF_UNICODETEXT);
	wchar_t *pch = (wchar_t*)GlobalLock(hdata);
	if (pch)
	{
		eol = DetectEol(pch);
		if (eol != EOL_CRLF)
			lpwPastedText = _wcsdup(pch);
	}
	GlobalUnlock(hdata);
	CloseClipboard();

	if (lpwPastedText)
	{
		std::wstring wsText;
		if (eol == EOL_LF)
			wsText = ReplaceAll(lpwPastedText, L"\n", L"\r\n", NULL);
		else //if (eol == EOL_CR)
			wsText = ReplaceAll(lpwPastedText, L"\r", L"\r\n", NULL);
		SendMessage(m_hWnd, EM_REPLACESEL, TRUE, (LPARAM)wsText.c_str());
		free(lpwPastedText);
	}
	else
		SendMessage(m_hWnd, WM_PASTE, 0, 0);
}

//##########################################################
//
//##########################################################
void Edit::Clear(void)
{
	SendMessage(m_hWnd, WM_CLEAR, 0, 0);
}

//##########################################################
//
//##########################################################
void Edit::SelectAll(void)
{
	SendMessage(m_hWnd, EM_SETSEL, 0, -1);
}

//##########################################################
// WS_HSCROLL can't be changed after edit control was created, so the control needs to be replaced with a new instance.
// (MS Notepad does the same thing).
//##########################################################
void Edit::SetWordWrap(BOOL bFlag)
{
	wchar_t *pwszText = GetText();

	int iPosStart, iPosEnd;
	SendMessage(m_hWnd, EM_GETSEL, (WPARAM)&iPosStart, (LPARAM)&iPosEnd);

	LONG lStyle = GetWindowLong(m_hWnd, GWL_STYLE);
	lStyle = bFlag ? lStyle & ~WS_HSCROLL : lStyle | WS_HSCROLL;

	HWND hwndParent = GetParent(m_hWnd);

	RECT rc;
	GetWindowRect(m_hWnd, &rc);
	MapWindowPoints(NULL, hwndParent, (LPPOINT)&rc, 2);

	DestroyWindow(m_hWnd);

	m_hWnd = CreateWindowEx(
		0,
		WC_EDIT,
		NULL,
		lStyle,
		rc.left, rc.top,
		rc.right - rc.left, rc.bottom - rc.top,
		hwndParent,
		NULL,
		NULL,
		NULL
	);

	SetWindowLong(m_hWnd, GWL_ID, m_lControlID);

	SetFont(m_logFont);

	SendMessage(m_hWnd, WM_SETTEXT, 0, (LPARAM)pwszText);
	SendMessage(m_hWnd, EM_SETSEL, iPosStart, iPosEnd);

	SetFocus(m_hWnd);

	free(pwszText);
}

//##########################################################
//
//##########################################################
int Edit::ZoomIn(void)
{
	m_iFontSizeZoom += 1;
	__UpdateFont();
	return (int)(100 * (m_iFontSize + m_iFontSizeZoom) / m_iFontSize);
}

//##########################################################
//
//##########################################################
int Edit::ZoomOut(void)
{
	m_iFontSizeZoom -= 1;
	__UpdateFont();
	return (int)(100 * (m_iFontSize + m_iFontSizeZoom) / m_iFontSize);
}

//##########################################################
//
//##########################################################
void Edit::ZoomReset(void)
{
	m_iFontSizeZoom = 0;
	__UpdateFont();
}

//##########################################################
// Sets caret at beginning of line and scrolls line in view
//##########################################################
void Edit::GotoLine(UINT uiLineno)
{
    int iPos = (int)SendMessage(m_hWnd, EM_LINEINDEX, uiLineno - 1, 0);
    SendMessage(m_hWnd, EM_SETSEL, iPos, iPos);
    SendMessage(m_hWnd, EM_SCROLLCARET, 0, 0);
    //__CheckCaretPos();
	EditNotification en = {
		m_hWnd,
		(UINT_PTR)m_lControlID,
		EVENT_CARET_POS_CHANGED,
		(int)(uiLineno - 1),  // line index
		0  // column index
	};
	SendMessage(GetParent(m_hWnd), WM_NOTIFY, m_lControlID, (LPARAM)&en);
}

//##########################################################
//
//##########################################################
PWSTR StrRStrW(PCWSTR pszSource, PCWSTR pszLast, PCWSTR pszSrch)
{
	wchar_t *pwszRes = StrRStrIW(pszSource, pszLast, pszSrch);
	while (pwszRes)
	{
		if (wcsncmp(pwszRes, pszSrch, wcslen(pszSrch)) == 0)
			return pwszRes;
		pwszRes = StrRStrIW(pszSource, pwszRes, pszSrch);
	}
	return NULL;
}

//##########################################################
//
//##########################################################
BOOL Edit::Find(const wchar_t *pwszFindWhat, BOOL bCaseSensitive, BOOL bForward, BOOL bWrap, BOOL bAtSelStart)
{
	BOOL bFound = FALSE;
	int iPosStart, iPosEnd;
	SendMessage(m_hWnd, EM_GETSEL, (WPARAM)&iPosStart, (LPARAM)&iPosEnd);

	HLOCAL hMem = (HLOCAL)SendMessage(m_hWnd, EM_GETHANDLE, 0, 0);
	wchar_t *pch = (wchar_t*)LocalLock(hMem);

	wchar_t *pwszRes;

	if (bForward)
	{
		int iStartPos = bAtSelStart ? iPosStart : iPosEnd;
		pwszRes = bCaseSensitive ? StrStrW(pch + iStartPos, pwszFindWhat) : StrStrIW(pch + iStartPos, pwszFindWhat);
		if (pwszRes)
		{
			bFound = TRUE;
			SendMessage(m_hWnd, EM_SETSEL, (WPARAM)(pwszRes - pch), (LPARAM)(pwszRes - pch + wcslen(pwszFindWhat)));
		}
		else if (bWrap)
		{
			pwszRes = bCaseSensitive ? StrStrW(pch, pwszFindWhat) : StrStrIW(pch, pwszFindWhat);
			if (pwszRes)
			{
				bFound = TRUE;
				SendMessage(m_hWnd, EM_SETSEL, pwszRes - pch, pwszRes - pch + wcslen(pwszFindWhat));
			}
		}
	}
	else
	{
		//Searches for the last occurrence of a specified substring within a string. The comparison is not case-sensitive.
		pwszRes = bCaseSensitive ? StrRStrW(pch, pch + iPosStart, pwszFindWhat) : StrRStrIW(pch, pch + iPosStart, pwszFindWhat);
		if (pwszRes)
		{
			bFound = TRUE;
			SendMessage(m_hWnd, EM_SETSEL,
				(WPARAM)(pwszRes - pch),
				(LPARAM)(pwszRes - pch + wcslen(pwszFindWhat))
			);
		}
		else if (bWrap)
		{
			pwszRes = bCaseSensitive ? StrRStrW(pch, NULL, pwszFindWhat) : StrRStrIW(pch, NULL, pwszFindWhat);
			if (pwszRes)
			{
				bFound = TRUE;
				SendMessage(m_hWnd, EM_SETSEL,
					(WPARAM)(pwszRes - pch),
					(LPARAM)(pwszRes - pch + wcslen(pwszFindWhat))
				);
			}
		}
	}

	LocalUnlock(hMem);
	return bFound;
}

//##########################################################
//
//##########################################################
BOOL Edit::Replace(const wchar_t *pwszFindWhat, const wchar_t *wszReplaceWith, BOOL bCaseSensitive, BOOL bWrap, BOOL bReplaceAll)
{
	BOOL bFound = FALSE;
	if (bReplaceAll)
	{

		// This doesn't allow UNDO
		//bWrap = TRUE;
		//SendMessage(m_hWnd, WM_SETREDRAW, FALSE, 0);
		//while (Find(pwszFindWhat, bCaseSensitive, TRUE, bWrap, TRUE))
		//{
		//	bFound = TRUE;
		//	SendMessage(m_hWnd, EM_REPLACESEL, TRUE, (LPARAM)wszReplaceWith);
		//}
		//SendMessage(m_hWnd, WM_SETREDRAW, TRUE, 0);

		wchar_t *pwszText = GetText();
		std::wstring wsTextNew = ReplaceAll(pwszText, pwszFindWhat, wszReplaceWith, &bFound);

		if (bFound)
		{
			int iPosStart, iPosEnd;
			SendMessage(m_hWnd, EM_GETSEL, (WPARAM)&iPosStart, (LPARAM)&iPosEnd);

			SendMessage(m_hWnd, WM_SETREDRAW, FALSE, 0);

			SendMessage(m_hWnd, EM_SETSEL, 0, -1);
			SendMessage(m_hWnd, EM_REPLACESEL, TRUE, (LPARAM)(wchar_t*)wsTextNew.c_str());
			SendMessage(m_hWnd, EM_SETSEL, iPosStart, iPosStart);

			SendMessage(m_hWnd, WM_SETREDRAW, TRUE, 0);
		}
		free(pwszText);
	}
	else
	{
		if (Find(pwszFindWhat, bCaseSensitive, TRUE, bWrap, TRUE))
		{
			bFound = TRUE;
			SendMessage(m_hWnd, EM_REPLACESEL, TRUE, (LPARAM)wszReplaceWith);
			Find(pwszFindWhat, bCaseSensitive, TRUE, bWrap, FALSE);
		}
	}
	return bFound;
}

//##########################################################
//
//##########################################################
void Edit::SetTabSize(int iNewTabSize)
{
	if (iNewTabSize == m_iIndentSize)
		return;

	if (iNewTabSize > MAX_INDENT_SIZE)
		iNewTabSize = MAX_INDENT_SIZE;

	if (m_bIndentUseSpaces)
	{
		// Replace all leading old-tabs with new-tabs

		wchar_t *pwszText = GetText();
		std::wstring wsText = pwszText;
		free(pwszText);

		BOOL bFound = FALSE;
		size_t pos = 0;

		std::wstring wsLeadingSpaces = L"\r\n";
		wsLeadingSpaces += m_wszIndentSpaces;

		wchar_t wszNewTab[MAX_INDENT_SIZE];
		for (int i = 0; i < iNewTabSize; i++)
			wszNewTab[i] = L' ';
		wszNewTab[iNewTabSize] = L'\0';

		std::wstring wsNewLeadingSpaces = L"\r\n";
		wsNewLeadingSpaces += wszNewTab;

		// First line
		while (wsText.compare(pos, m_iIndentSize, m_wszIndentSpaces) == 0)
		{
			wsText.replace(pos, m_iIndentSize, wszNewTab);
			pos += iNewTabSize;
			bFound = TRUE;
		}

		// Other lines
		pos = wsText.find(wsLeadingSpaces, size_t{});
		bFound = bFound || pos != std::wstring::npos;
		while (pos != std::wstring::npos)
		{
			wsText.replace(pos, wsLeadingSpaces.length(), wsNewLeadingSpaces);
			pos += wsNewLeadingSpaces.length();

			while (wsText.compare(pos, m_iIndentSize, m_wszIndentSpaces) == 0)
			{
				wsText.replace(pos, m_iIndentSize, wszNewTab);
				pos += iNewTabSize;
			}

			pos = wsText.find(wsLeadingSpaces, pos); // + wsLeadingTab.length());
		}
		if (bFound)
		{
			int iPosStart, iPosEnd;
			SendMessage(m_hWnd, EM_GETSEL, (WPARAM)&iPosStart, (LPARAM)&iPosEnd);

			SendMessage(m_hWnd, WM_SETREDRAW, FALSE, 0);

			SendMessage(m_hWnd, EM_SETSEL, 0, -1);
			SendMessage(m_hWnd, EM_REPLACESEL, FALSE, (LPARAM)(wchar_t*)wsText.c_str()); // No UNDO!
			SendMessage(m_hWnd, EM_SETSEL, iPosStart, iPosStart);

			SendMessage(m_hWnd, WM_SETREDRAW, TRUE, 0);
		}
	}
	else
	{
		// Just update the tab stops to new width
		UINT uiTabWidth = 4 * iNewTabSize;
		SendMessage(m_hWnd, EM_SETTABSTOPS, 1, (LPARAM)&uiTabWidth);
	}

	m_iIndentSize = iNewTabSize;
	for (int i = 0; i < m_iIndentSize; i++)
		m_wszIndentSpaces[i] = L' ';
	m_wszIndentSpaces[m_iIndentSize] = L'\0';
}

//##########################################################
//
//##########################################################
void Edit::SetUseSpaces(BOOL m_bIndentUseSpaces)
{
	if (m_bIndentUseSpaces == m_bIndentUseSpaces)
		return;

	m_bIndentUseSpaces = m_bIndentUseSpaces;

	wchar_t *pwszText = GetText();
	std::wstring wsText = pwszText;
	free(pwszText);

	BOOL bFound = FALSE;
	size_t pos = 0;

	std::wstring wsLeadingTab = L"\r\n\t";
	std::wstring wsLeadingSpaces = L"\r\n";
	wsLeadingSpaces += m_wszIndentSpaces;

	if (m_bIndentUseSpaces)
	{
		// Replace all leading tabs with spaces

		// First line
		while (wsText[pos] == L'\t')
		{
			wsText.replace(pos, 1, m_wszIndentSpaces);
			pos += m_iIndentSize;
			bFound = TRUE;
		}

		// Other lines
		pos = wsText.find(wsLeadingTab, size_t{});
		bFound = bFound || pos != std::wstring::npos;
		while (pos != std::wstring::npos)
		{
			wsText.replace(pos, wsLeadingTab.length(), wsLeadingSpaces);
			pos += wsLeadingSpaces.length();
			while (wsText[pos] == L'\t')
			{
				wsText.replace(pos, 1, m_wszIndentSpaces);
				pos += m_iIndentSize;
			}
			pos = wsText.find(wsLeadingTab, pos + wsLeadingSpaces.length());
		}
	}
	else
	{
		// Replace all leading spaces with tabs

		// First line
		while (wsText.compare(pos, m_iIndentSize, m_wszIndentSpaces) == 0)
		{
			wsText.replace(pos, m_iIndentSize, L"\t");
			pos += 1;
			bFound = TRUE;
		}

		// Other lines
		pos = wsText.find(wsLeadingSpaces, size_t{});
		bFound = bFound || pos != std::wstring::npos;
		while (pos != std::wstring::npos)
		{
			wsText.replace(pos, wsLeadingSpaces.length(), wsLeadingTab);
			pos += wsLeadingTab.length();
			while (wsText.compare(pos, m_iIndentSize, m_wszIndentSpaces) == 0)
			{
				wsText.replace(pos, m_iIndentSize, L"\t");
				pos += 1;
			}
			pos = wsText.find(wsLeadingSpaces, pos + wsLeadingTab.length());
		}
	}

	if (bFound)
	{
		int iPosStart, iPosEnd;
		SendMessage(m_hWnd, EM_GETSEL, (WPARAM)&iPosStart, (LPARAM)&iPosEnd);

		SendMessage(m_hWnd, WM_SETREDRAW, FALSE, 0);

		SendMessage(m_hWnd, EM_SETSEL, 0, -1);
		SendMessage(m_hWnd, EM_REPLACESEL, FALSE, (LPARAM)(wchar_t*)wsText.c_str()); // No UNDO!
		SendMessage(m_hWnd, EM_SETSEL, iPosStart, iPosStart);

		SendMessage(m_hWnd, WM_SETREDRAW, TRUE, 0);
	}
}

//##########################################################
//
//##########################################################
void Edit::__UpdateFont(void)
{
	m_logFont.lfHeight = -MulDiv(m_iFontSize + m_iFontSizeZoom, m_iDpiY, 72);
	HFONT hfont = CreateFontIndirect(&m_logFont);
	SendMessage(m_hWnd, WM_SETFONT, (WPARAM)hfont, MAKELPARAM(1, 0));
}

//##########################################################
//
//##########################################################
void Edit::__CheckCaretPos(void)
{
    POINT pt;
	GetCaretPos(&pt);
    int iPos = (int)SendMessage(m_hWnd, EM_CHARFROMPOS, 0, (LPARAM)MAKELONG(pt.x, pt.y));
    if (iPos > -1)
    {
		EditNotification en = {
			m_hWnd,
			(UINT_PTR)m_lControlID,
			EVENT_CARET_POS_CHANGED,
			HIWORD(iPos),  // line index
			(int)((pt.x - m_iMargin) / m_iCharWidth)  // column index
		};
		SendMessage(GetParent(m_hWnd), WM_NOTIFY, m_lControlID, (LPARAM)&en);
    }
}

//##########################################################
//
//##########################################################
LRESULT CALLBACK __EditSubClassProc(HWND hWnd, UINT uMsg, WPARAM wParam,
		LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	switch (uMsg)
	{
		case WM_CHAR:
			return ((Edit*)dwRefData)->OnChar(wParam, lParam);

		case WM_KEYUP:
			((Edit*)dwRefData)->OnKeyUp(wParam, lParam);
			break;

		case WM_LBUTTONDBLCLK:
			return ((Edit*)dwRefData)->OnLButtonDblClk(wParam, lParam);

        case WM_LBUTTONUP:
			((Edit*)dwRefData)->OnLButtonUp(wParam, lParam);
			break;

		case WM_MOUSEMOVE:
			((Edit*)dwRefData)->OnMouseMove(wParam, lParam);
			break;
	}

	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

//##########################################################
//
//##########################################################
LRESULT Edit::__HandleTab(WPARAM wParam, LPARAM lParam)
{
	wchar_t *pwszIndentSpaces = m_bIndentUseSpaces ? m_wszIndentSpaces : L"\t";
	int iTabLen = m_bIndentUseSpaces ? m_iIndentSize : 1;

    BOOL bIsShift = GetAsyncKeyState(VK_SHIFT) < 0;

	int iPosStart, iPosEnd;
	SendMessage(m_hWnd, EM_GETSEL, (WPARAM)&iPosStart, (LPARAM)&iPosEnd);

    int iLineFrom = (int)SendMessage(m_hWnd, EM_LINEFROMCHAR, iPosStart, 0);
    int lLineTo = (int)SendMessage(m_hWnd, EM_LINEFROMCHAR, iPosEnd, 0);
	int iLineStartPos;

    if (lLineTo > iLineFrom)
	{
		// Multiple lines selected
		SendMessage(m_hWnd, WM_SETREDRAW, FALSE, 0);
        if (bIsShift)
        {
        	// Unindent block
        	wchar_t *pwszLine = (wchar_t *)malloc((iTabLen + 1) * sizeof(wchar_t));

            for (int iLineIndex=iLineFrom; iLineIndex <= lLineTo; iLineIndex++)
            {
				iLineStartPos = (int)SendMessage(m_hWnd, EM_LINEINDEX, iLineIndex, 0);
				pwszLine[0] = iTabLen + 1;
				SendMessage(m_hWnd, EM_GETLINE, iLineIndex, (LPARAM)pwszLine);
				// If line starts with a full tab, remove it
				if (wcsncmp(pwszLine, pwszIndentSpaces, iTabLen) == 0)
				{
                	SendMessage(m_hWnd, EM_SETSEL, iLineStartPos, iLineStartPos + iTabLen);
                	//SendMessage(m_hWnd, EM_REPLACESEL, FALSE, (LPARAM)L"");
                	SendMessage(m_hWnd, WM_CLEAR, 0, 0);
                }
            }

            free(pwszLine);
        }
        else
        {
            // Indent block
            for (int iLineIndex=iLineFrom; iLineIndex <= lLineTo; iLineIndex++)
            {
                iLineStartPos = (int)SendMessage(m_hWnd, EM_LINEINDEX, iLineIndex, 0);
                SendMessage(m_hWnd, EM_SETSEL, iLineStartPos, iLineStartPos);
                SendMessage(m_hWnd, EM_REPLACESEL, FALSE, (LPARAM)pwszIndentSpaces);
            }
        }

        // Select full block
        iPosStart = (int)SendMessage(m_hWnd, EM_LINEINDEX, iLineFrom, 0);
        iPosEnd = (int)SendMessage(m_hWnd, EM_LINEINDEX, lLineTo + 1, 0) - 1;

        SendMessage(m_hWnd, EM_SETSEL, iPosStart, iPosEnd);

        SendMessage(m_hWnd, WM_SETREDRAW, TRUE, 0);
    }
    else if (bIsShift)
    {
        // Jump back to preceding tab pos
        iLineStartPos = (int)SendMessage(m_hWnd, EM_LINEINDEX, iLineFrom, 0);
        if (iPosStart > iLineStartPos)
        {
	        iPosStart = iPosStart - (iPosStart % m_iIndentSize == 0 ? m_iIndentSize : iPosStart % m_iIndentSize);
	        SendMessage(m_hWnd, EM_SETSEL, iPosStart, iPosStart);
        }
	}
    else
    {
		// Single line mode
    	if (m_bIndentUseSpaces)
    	{
    		// Find number of chars before caret
    		iLineStartPos = (int)SendMessage(m_hWnd, EM_LINEINDEX, iLineFrom, 0);
    		// Calculate how many new spaces to add
    		int iAdd = (iPosStart - iLineStartPos) % m_iIndentSize == 0 ? m_iIndentSize : (m_iIndentSize - (iPosStart - iLineStartPos) % m_iIndentSize);

    		wchar_t *pwszRepl = (wchar_t *)malloc((iAdd + 1) * sizeof(wchar_t));
			wcsncpy_s(pwszRepl, iAdd + 1, m_wszIndentSpaces, iAdd);
			pwszRepl[iAdd] = L'\0';
    		SendMessage(m_hWnd, EM_REPLACESEL, TRUE, (LPARAM)pwszRepl);
			free(pwszRepl);
    	}
    	else
	    	// Replace selection with TAB
	    	SendMessage(m_hWnd, EM_REPLACESEL, TRUE, (LPARAM)L"\t");
    }

	return 0;
}

//##########################################################
// Only called if bUseSapces is TRUE
//##########################################################
LRESULT Edit::__HandleBack(WPARAM wParam, LPARAM lParam)
{
	int iPosStart, iPosEnd;
	SendMessage(m_hWnd, EM_GETSEL, (WPARAM)&iPosStart, (LPARAM)&iPosEnd);

    int iLineIndex = (int)SendMessage(m_hWnd, EM_LINEFROMCHAR, iPosStart, 0);
    int iLineStartPos = (int)SendMessage(m_hWnd, EM_LINEINDEX, iLineIndex, 0);

	if (iPosStart == iLineStartPos)
		return FALSE;

	int iRemove = (iPosStart - iLineStartPos) % m_iIndentSize == 0 ? m_iIndentSize : (iPosStart - iLineStartPos) % m_iIndentSize;

    wchar_t *pwszLine = (wchar_t *)malloc((iPosStart - iLineStartPos + 1) * sizeof(wchar_t));
    pwszLine[0] = iPosStart - iLineStartPos;
    SendMessage(m_hWnd, EM_GETLINE, iLineIndex, (LPARAM)pwszLine);
	pwszLine[iPosStart - iLineStartPos] = L'\0';

	wchar_t *pwCh = (wchar_t*)pwszLine + iPosStart - iLineStartPos - iRemove;

	// Only spaces?
	if (wcsncmp(pwCh, m_wszIndentSpaces, iRemove) == 0)
	{
		SendMessage(m_hWnd, WM_SETREDRAW, FALSE, 0);
		SendMessage(m_hWnd, EM_SETSEL, iPosStart - iRemove, iPosStart);
		//SendMessage(m_hWnd, EM_REPLACESEL, TRUE, (LPARAM)L"");
		SendMessage(m_hWnd, WM_CLEAR, 0, 0);
		SendMessage(m_hWnd, WM_SETREDRAW, TRUE, 0);
		free(pwszLine);
		return TRUE;
	}
    free(pwszLine);

	return FALSE;
}

//##########################################################
//
//##########################################################
LRESULT Edit::__HandleReturn(WPARAM wParam, LPARAM lParam)
{
	int iPosStart, iPosEnd;
	SendMessage(m_hWnd, EM_GETSEL, (WPARAM)&iPosStart, (LPARAM)&iPosEnd);
    if (iPosStart == iPosEnd)
    {
    	// Get previous line (since Enter was pressed, there always is one)
		int iLineIndex = (int)SendMessage(m_hWnd, EM_LINEFROMCHAR, iPosStart, 0) - 1;
		int iCharIndex = (int)SendMessage(m_hWnd, EM_LINEINDEX, iLineIndex, 0);
	    int iLenLine = (int)SendMessage(m_hWnd, EM_LINELENGTH, iCharIndex, 0);
	    wchar_t *pwszLine = (wchar_t *)malloc((iLenLine + 1) * sizeof(wchar_t));
	    pwszLine[0] = iLenLine + 1;
	    SendMessage(m_hWnd, EM_GETLINE, iLineIndex, (LPARAM)pwszLine);
		pwszLine[iLenLine] = L'\0';

		// Replicate indentation of previous line
		SendMessage(m_hWnd, WM_SETREDRAW, FALSE, 0);
		if (m_bIndentUseSpaces)
		{
			wchar_t *pchr = pwszLine;
			while (wcsncmp(pchr, m_wszIndentSpaces, m_iIndentSize) == 0)
			{
				SendMessage(m_hWnd, EM_REPLACESEL, FALSE, (LPARAM)m_wszIndentSpaces);
				pchr += m_iIndentSize;
			}
		}
		else
		{
			int found = 0;
			while (pwszLine[found] == L'\t')
			{
				SendMessage(m_hWnd, EM_REPLACESEL, FALSE, (LPARAM)L"\t");
				found++;
			}
		}
		SendMessage(m_hWnd, WM_SETREDRAW, TRUE, 0);

		free(pwszLine);
	}

	// WM_CHAR: An application should return zero if it processes this message.
	return 0;
}

//##########################################################
//
//##########################################################
LRESULT Edit::OnChar(WPARAM wParam, LPARAM lParam)
{
	if (wParam == 0x16) // Ctrl+V
	{
		Paste();
		return 0;
	}

	if (wParam == VK_TAB)
		return __HandleTab(wParam, lParam);

    if (m_bIndentUseSpaces && wParam == VK_BACK)
        if (__HandleBack(wParam, lParam))
        	return 0;

	DefSubclassProc(m_hWnd, WM_CHAR, wParam, lParam);

	if (wParam == VK_RETURN)
		return __HandleReturn(wParam, lParam);

	return 0;
}

//##########################################################
//
//##########################################################
void Edit::OnKeyUp(WPARAM wParam, LPARAM lParam)
{
	// 0x21...0x28, all 8 navigation keys that can change the caret position
	if (wParam >= VK_PRIOR && wParam <= VK_DOWN)
		__CheckCaretPos();
}

//##########################################################
// Fix Edit's doubleclick selection behavior
//##########################################################
LRESULT Edit::OnLButtonDblClk(WPARAM wParam, LPARAM lParam)
{
	int ch = (int)SendMessage(m_hWnd, EM_CHARFROMPOS, 0, lParam);
	WORD iCharPos = LOWORD(ch);
	WORD iLineIndex = HIWORD(ch);

    //Get text of the full line
    int iLenLine = (int)SendMessage(m_hWnd, EM_LINELENGTH, iCharPos, 0);
    wchar_t *pwszLine = (wchar_t *)malloc((iLenLine + 1) * sizeof(wchar_t));
    pwszLine[0] = iLenLine + 1;
    SendMessage(m_hWnd, EM_GETLINE, iLineIndex, (LPARAM)pwszLine);

	int iLineStartPos = (int)SendMessage(m_hWnd, EM_LINEINDEX, iLineIndex, 0);
	int iPosTo = iLineStartPos + iLenLine;

	// Increase selection forward to include all word characters
	wchar_t *pwCh = pwszLine + iCharPos - iLineStartPos;
	wchar_t *pwChRes = wcspbrk(pwCh, NON_WORD);
	if (pwChRes)
		iPosTo = iCharPos + (int)(pwChRes - pwCh);

	// Increase selection backwards to include all word characters
	pwszLine[iCharPos - iLineStartPos] = L'\0';
	_wcsrev(pwszLine);
	pwChRes = wcspbrk(pwszLine, NON_WORD);
	if (pwChRes)
		iCharPos -= (int)(pwChRes - pwszLine);
	else
		iCharPos = iLineStartPos;

	SendMessage(m_hWnd, EM_SETSEL, iCharPos, iPosTo);

    free(pwszLine);

	return 0;
}

//##########################################################
//
//##########################################################
void Edit::OnLButtonUp(WPARAM wParam, LPARAM lParam)
{
	__CheckCaretPos();
}

//##########################################################
//
//##########################################################
void Edit::OnMouseMove(WPARAM wParam, LPARAM lParam)
{
	if (wParam & MK_LBUTTON)
	{
        // Edit control only knows about selections, not about the active caret position.
        // So we have to find out the active (=changing) side of the selection by ourself.
        m_lastSel = m_currentSel;

		SendMessage(m_hWnd, EM_GETSEL, (WPARAM)&m_currentSel.iFrom, (LPARAM)&m_currentSel.iTo);

        if (m_currentSel.iFrom == m_lastSel.iFrom && m_currentSel.iTo == m_lastSel.iTo)
            return;

        int iPos = m_currentSel.iTo != m_lastSel.iTo ? m_currentSel.iTo : m_currentSel.iFrom;
        int iLineIndex = (int)SendMessage(m_hWnd, EM_LINEFROMCHAR, iPos, 0);
        int iRes = (int)SendMessage(m_hWnd, EM_POSFROMCHAR, iPos, 0);
		int iColIndex;
		if (iRes == -1)
		{
			// special case, position is EOF, so go one char back
			int iLineIndexNew = (int)SendMessage(m_hWnd, EM_LINEFROMCHAR, iPos - 1, 0);
			if (iLineIndexNew < iLineIndex)
				iColIndex = 0;
			else
			{
				iRes = (int)SendMessage(m_hWnd, EM_POSFROMCHAR, iPos - 1, 0);
				iColIndex = (int)((iRes & 0xFFFF - m_iMargin) / m_iCharWidth + 1);
			}
		}
        else
            iColIndex = (int)((iRes & 0xFFFF - m_iMargin) / m_iCharWidth);

		EditNotification en = {
			m_hWnd,
			(UINT_PTR)m_lControlID,
			EVENT_CARET_POS_CHANGED,
			iLineIndex,
			iColIndex
		};

		SendMessage(GetParent(m_hWnd), WM_NOTIFY, m_lControlID, (LPARAM)&en);
	}
}
