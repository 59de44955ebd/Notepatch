#pragma once

#include <windows.h>

#define CUSTOM_DIALOG 666

void CenterDialog(HWND hDlg);

UINT_PTR ChooseFontDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
UINT_PTR PageSetupDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
UINT_PTR PrintDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
UINT_PTR CALLBACK FindDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
UINT_PTR CALLBACK ReplaceDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

//typedef INT_PTR(CALLBACK* DLGPROC)(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK GotoDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

LRESULT DialogHookProc(int nCode, WPARAM wParam, LPARAM lParam);

//typedef LRESULT (CALLBACK *SUBCLASSPROC)(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
LRESULT CALLBACK MsgBoxClassProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

wchar_t *GetOpenFilename(wchar_t *title, wchar_t *default_name, wchar_t *default_ext, wchar_t *filter);
wchar_t *GetSaveFilename(wchar_t *title, wchar_t *default_name, wchar_t *default_ext, wchar_t *filter);

void ShowSysErrorMsgBox(DWORD dwError);
