#include <windows.h>
#include <string>

#include "Edit.h"

#define MAX_TEXT_LEN MAX_PATH
extern wchar_t wszText[MAX_TEXT_LEN];

wchar_t* CodePageToUtf16(const char *utf8, UINT CodePage);
char* Utf16ToCodePage (const wchar_t* utf16, UINT CodePage);

enum Encoding DetectEncoding(const unsigned char* lpData, DWORD dwFileSize);
enum Eol DetectEol(const wchar_t* lpData);
enum IndentType DetectIndentType(const wchar_t* pwszText);

BOOL PathResolveLnk(wchar_t* pszPath);

wchar_t* GetFormattedMessage(wchar_t* wszMessage, ...);

void GetText(UINT uID);

std::wstring ReplaceAll(std::wstring str, const std::wstring& from, const std::wstring& to, BOOL *pbFound);
