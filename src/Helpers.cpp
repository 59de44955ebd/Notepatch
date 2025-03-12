#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <wchar.h>

#include "Dark.h"
#include "Helpers.h"
#include "Resource.h"

extern HMODULE 			g_hModule;
extern BOOL				g_bIsDark;

enum Bom
{
	BOM_NONE = 0,
	BOM_UTF_8,
	BOM_UTF_16_LE,
	BOM_UTF_16_BE
};

//##########################################################
// Checks bytes for Byte-Order-Mark (BOM), returns BOM type or 0 (BOM_NONE)
//##########################################################
enum Bom __GetBom(const unsigned char* lpData, DWORD dwFileSize)
{
	if (dwFileSize < 2)
		return BOM_NONE;
	if (dwFileSize > 2 && lpData[0] == 0xEF && lpData[1] == 0xBB && lpData[2] == 0xBF)
		return BOM_UTF_8;
	if (lpData[0] == 0xFF && lpData[1] == 0xFE)
		return BOM_UTF_16_LE;
	if (lpData[0] == 0xFE && lpData[1] == 0xFF)
		return BOM_UTF_16_BE;
	return BOM_NONE;
}

//##########################################################
//´Checks bytes for invalid UTF-8 sequences
// Notice: since ASCII is a UTF-8 subset, function also returns TRUE for pure ASCII data
//##########################################################
BOOL __IsUtf8(const char* lpData)
{
	size_t data_len = strlen(lpData);
	unsigned char o, n, c;
	size_t i = -1;
    while (TRUE)
    {
		i++;
        if (i >= data_len)
            break;
        o = lpData[i];
        if (o < 128)
            continue;
        else if ((o & 224) == 192 && o > 193)
            n = 1;
        else if ((o & 240) == 224)
            n = 2;
        else if ((o & 248) == 240 && o < 245)
            n = 3;
        else
            return FALSE;  // invalid UTF-8 sequence
        for (c = 0; c < n; c++)
       	{
            i++;
            if (i >= data_len || (lpData[i] & 192) != 128)
                return FALSE;  // invalid UTF-8 sequence
        }
    }
    return TRUE;
}

//##########################################################
// Converts 8-bit text (CP_ACP or CP_UTF8) to Unicode
//##########################################################
wchar_t* CodePageToUtf16(const char* utf8, UINT CodePage)
{
	int utf8len = (int)strlen(utf8) + 1;
	int utf16len = MultiByteToWideChar(CodePage, 0, utf8, -1, NULL, 0);
	wchar_t *utf16 = (wchar_t*)malloc((utf16len + 1) * sizeof(wchar_t));
	int result = MultiByteToWideChar(
		CodePage,
		0,
		utf8,
		utf8len,
		utf16,
		utf16len
	);
	return utf16;
}

//##########################################################
// Converts Unicode (UTF-16 LE) text to CP_ACP or CP_UTF8
//##########################################################
char* Utf16ToCodePage (const wchar_t* utf16, UINT CodePage)
{
	int utf16len = (int)wcslen(utf16) + 1;
	int utf8len = WideCharToMultiByte(CodePage, 0, utf16, -1, NULL, 0, NULL, NULL);
	char * utf8  = (char*)malloc(utf8len + 1);
	int result = WideCharToMultiByte(
		CodePage,
		0,
		utf16,
		utf16len,
		utf8,
		utf8len,
		NULL,
		NULL
	);
	return utf8;
}

//##########################################################
// Tries to detect the encoding of the specified bytes
//##########################################################
enum Encoding DetectEncoding(const unsigned char *lpData, DWORD dwFileSize)
{
	enum Bom bom = __GetBom(lpData, dwFileSize);
	switch (bom)
	{
		case BOM_UTF_8: return ENCODING_UTF8_BOM;
		case BOM_UTF_16_LE: return ENCODING_UTF16_LE;
		case BOM_UTF_16_BE: return ENCODING_UTF16_BE;
	}
	if (dwFileSize < 2)
		return ENCODING_UTF8;
	if (lpData[0] == 0)
		return ENCODING_UTF16_BE;
	if (lpData[1] == 0)
		return ENCODING_UTF16_LE;
	if (__IsUtf8((char*)lpData))
		return ENCODING_UTF8;
	return ENCODING_ANSI;
}

//##########################################################
// Tries to detect the EOL mode of the specified bytes
// Save ONLY if lpData is 0-terminated
//##########################################################
enum Eol DetectEol(const wchar_t* lpData)
{
	const wchar_t * pch = wcschr(lpData, L'\n');
	if (pch)
	{
		if (pch > lpData && *(pch - 1) == L'\r')
			return EOL_CRLF;
		else
			return EOL_LF;
	}
	pch = wcschr(lpData, L'\r');
	if (pch)
		return EOL_CR;
	// Neither CR nor LF found, default to CRLF
	return EOL_CRLF;
}

//##########################################################
// Tries to detect the indentation (tab or spaces).
// Does NOT detect the most likely indent size in case of spaces (TODO).
// Text has to be normalized to CRLF (or LF) as EOL
//##########################################################
enum IndentType DetectIndentType(const wchar_t* pwszText)
{
	// If any leading tabs are found, return INDENT_TYPE_TAB
	wchar_t * pwszTabIndent = StrStrW(pwszText, L"\n\t");
	if (pwszTabIndent)
		return INDENT_TYPE_TAB;
	// Otherwise if neither any leading spaces are found, also default to INDENT_TYPE_TAB
	wchar_t * pwszSpacesIndent = StrStrW(pwszText, L"\n "); // L"\n    "
	return pwszSpacesIndent ? INDENT_TYPE_SPACES : INDENT_TYPE_TAB;
}

//##########################################################
// Tries to get the path to which a lnk-file is linked
//##########################################################
BOOL PathResolveLnk(wchar_t* pszPath)
{
	IShellLink*			psl;
	WIN32_FIND_DATA		fd;
	BOOL				bSucceeded = FALSE;
	wchar_t 			szGotPath[MAX_PATH];
//    wchar_t 			szDescription[MAX_PATH];

	if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*)&psl)))
	{
		IPersistFile *ppf;
		if (SUCCEEDED(psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf)))
		{
			if (SUCCEEDED(ppf->Load(pszPath, STGM_READ)))
			{
                if (SUCCEEDED(psl->Resolve(NULL, 0)))
                {
                    if (SUCCEEDED(psl->GetPath(szGotPath, MAX_PATH, &fd, 0)))
                    {
//                        if (SUCCEEDED(psl->GetDescription(szDescription, MAX_PATH)))
//                            ok = wcscpy(lpszPath, pszResPath) != NULL;
						bSucceeded = TRUE;
                    }
                }
			}
			ppf->Release();
		}
		psl->Release();
	}

	if (bSucceeded)
		PathCanonicalize(pszPath, szGotPath);
	return (bSucceeded);
}

//##########################################################
// Formats a message string using the specified message and variable list of arguments.
// "Do you want to save changes to %1?"
// "Cannot find \"%%\""
//##########################################################
wchar_t* GetFormattedMessage(wchar_t* wszMessage, ...)
{
	// Replace (first) "%%" with "%1"
  	wchar_t* pch = StrChrW(wszMessage, L'%');
  	if (pch && *(pch + 1) == L'%')
  		*(pch + 1) = L'1';

    wchar_t* pwszBuffer = NULL;
    va_list args = NULL;
    va_start(args, wszMessage);
    FormatMessage(
    	FORMAT_MESSAGE_FROM_STRING | FORMAT_MESSAGE_ALLOCATE_BUFFER,
		wszMessage,
		0,
		0,
		(LPWSTR)&pwszBuffer,
		0,
		&args
	);
    va_end(args);
    return pwszBuffer;
}

//##########################################################
//
//##########################################################
wchar_t wszText[MAX_TEXT_LEN];

void GetText(UINT uID)
{
	LoadString(g_hModule, uID, wszText, MAX_TEXT_LEN);
}

//##########################################################
//
//##########################################################
std::wstring ReplaceAll(std::wstring str, const std::wstring& from, const std::wstring& to, BOOL *pbFound)
{
    auto&& pos = str.find(from, size_t{});
    if (pbFound)
    	*pbFound = pos != std::wstring::npos;
    while (pos != std::wstring::npos)
    {
        str.replace(pos, from.length(), to);
        pos = str.find(from, pos + to.length());
    }
    return str;
}
