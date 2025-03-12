#pragma once

//##############################################################################
// Constants
//##############################################################################

#define EVENT_CARET_POS_CHANGED 10
#define EDIT_MAX_TEXT_LEN 0x80000 // 512 KB (Edit control's default: 30.000)
#define MAX_INDENT_SIZE 64

//##############################################################################
// Enums and structs
//##############################################################################
enum Encoding
{
	ENCODING_ANSI,
	ENCODING_UTF8,
	ENCODING_UTF8_BOM,
	ENCODING_UTF16_LE,
	ENCODING_UTF16_BE
};

enum Eol
{
	EOL_CRLF,
	EOL_LF,
	EOL_CR
};

enum IndentType
{
	INDENT_TYPE_TAB,
	INDENT_TYPE_SPACES
};

typedef struct tagSelection
{
	int  			iFrom;
	int  			iTo;

} Selection;

typedef struct tagEditNotification
{
	// NMHDR header
	HWND     		hwndFrom;
 	UINT_PTR 		idFrom;
 	UINT  			code;
	// custom data
	int  			iArg1;
	int  			iArg2;
} EditNotification;

//##############################################################################
//
//##############################################################################
class Edit
{

public:
	HWND 		m_hWnd;
	BOOL		m_bIndentUseSpaces = FALSE;

private:
	long		m_lControlID;
	int			m_iMargin = 4;
	int			m_iCharWidth = 0;
	Selection	m_lastSel = { 0, 0 };
	Selection	m_currentSel = { 0, 0 };
	LOGFONT		m_logFont;
	int			m_iFontSize;
	int 		m_iFontSizeZoom = 0;

	int 		m_iIndentSize = 4;
	wchar_t		m_wszIndentSpaces[MAX_INDENT_SIZE + 1] = L"    ";

	int			m_iDpiY = 96;

public:
				Edit(HWND hwndParent, long m_lControlID, LOGFONT logFont);

	void		SetFont(LOGFONT logFont);
	LOGFONT		GetFont(void);

	void 		SetText(const wchar_t *lpwText);

	wchar_t*	GetText(void);
	wchar_t* 	GetSelectedText(void);
	BOOL 		HasSelection(void);

	BOOL 		Print(HDC hDC, POINT ptPrintPaperSize, RECT rcPrintMargins, wchar_t *pwszDocName);

	void 		Undo(void);
	void 		Cut(void);
	void 		Copy(void);
	void 		Paste(void);
	void 		Clear(void);
	void 		SelectAll(void);

	void 		SetWordWrap(BOOL pFlag);

	int 		ZoomIn(void);
	int 		ZoomOut(void);
	void 		ZoomReset(void);

	void 		GotoLine(UINT lineno);

	BOOL 		Find(const wchar_t *pwszFindWhat, BOOL bCaseSensitive, BOOL bForward, BOOL bWrap, BOOL bAtSelStart);
	BOOL 		Replace(const wchar_t *pwszFindWhat, const wchar_t *wszReplaceWith, BOOL bCaseSensitive, BOOL bWrap, BOOL bReplaceAll);

	void 		SetTabSize(int iIndentSize);
	void 		SetUseSpaces(BOOL pUseSpaces);

	// can't be private, SubClassProc must have access
	LRESULT 	OnChar(WPARAM wParam, LPARAM lParam);
	void 		OnKeyUp(WPARAM wParam, LPARAM lParam);
	LRESULT 	OnLButtonDblClk(WPARAM wParam, LPARAM lParam);
	void 		OnLButtonUp(WPARAM wParam, LPARAM lParam);
	void		OnMouseMove(WPARAM wParam, LPARAM lParam);

private:
	void 		__CheckCaretPos(void);
	LRESULT		__HandleBack(WPARAM wParam, LPARAM lParam);
	LRESULT		__HandleReturn(WPARAM wParam, LPARAM lParam);
	LRESULT		__HandleTab(WPARAM wParam, LPARAM lParam);
	void		__UpdateFont(void);

};

LRESULT
CALLBACK 		__EditSubClassProc(HWND hWnd, UINT uMsg, WPARAM wParam,
						LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
