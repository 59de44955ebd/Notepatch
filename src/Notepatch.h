//==== General defines ========================================================

#define APP_NAME L"Notepatch"
#define APP_CLASS L"Notepatch"

// In Win 7 and earlier Notepad's default font was Lucida Console (10).
// In Win 8 and later it's Consolas 11.
#define DEFAULT_FONT L"Consolas"
#define DEFAULT_FONT_SIZE -15
#define DEFAULT_FONT_CHARSET ANSI_CHARSET

#define MAX_WIN_TITLE 300
#define MAX_CARET_INFO_LEN 32
#define MAX_FILTER_LEN 128
#define MAX_RESOURCE_TEXT_LEN 32
#define MAX_SEARCH_REPLACE_LEN MAX_PATH

//==== Control Ids ============================================================

#define IDC_STATUSBAR    0xFB00
#define IDC_EDIT         0xFB03

//==== Main Menu ==============================================================

#define MENU_INDEX_FILE 0
#define MENU_INDEX_EDIT 1
#define MENU_INDEX_FORMAT 2
#define MENU_INDEX_VIEW 3
#define MENU_INDEX_HELP 4

//==== Statusbar ==============================================================

#define STATUSBAR_NUM_PARTS 6
#define STATUSBAR_PART_CARET 1
#define STATUSBAR_PART_ZOOM 2
#define STATUSBAR_PART_EOL 3
#define STATUSBAR_PART_ENCODING 4
#define STATUSBAR_PART_INDENT 5

//==== Function Declarations ==================================================

void 		LoadSettings(int *X, int *Y, int *nWidth, int *nHeight, LOGFONT *logFont);
void 		SaveSettings();

LRESULT		CALLBACK MainWndProc(HWND, UINT, WPARAM, LPARAM);

void		OnSize(HWND, WPARAM, LPARAM);
void		OnInitMenu(HWND, WPARAM, LPARAM);
LRESULT		OnCommand(HWND, WPARAM, LPARAM);
LRESULT		OnNotify(HWND, WPARAM, LPARAM);

void 		LoadResourceStrings(void);
HMODULE 	LoadMuiFile(void);

void		CreateMenus(void);
void		CreateStatusbar(void);
void 		CreateFindDialog(void);
void 		CreateReplaceDialog(void);

void		StatusBarUpdateCaret(int ln, int col);
void		StatusBarUpdateZoom(int pct);
void		StatusBarUpdateEol(int eol);
void		StatusBarUpdateEncoding(int encoding);
void 		StatusBarUpdateIndentType(int indentType);

void		UpdateWindowTitle();

BOOL 		MaybeSave(void);

void 		SetEol(int eol);
void 		SetEncoding(int encoding);
void 		SetIndentType(int indentType);
void 		SetIndentSize(int indentSize);
void 		SetTheme(WORD theme_id);

void 		SetDark(BOOL bIsDark);
BOOL 		AppsUseDarkTheme(void);
