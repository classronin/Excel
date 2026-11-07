// ============================================================================
//  api_p8b_app.h - module contract: state + interfaces of p8b_app.
//  Included only by translation units that reference this module.
// ============================================================================
#pragma once
#include "core.h"

MenuItem MItem(int id, const wchar_t* text, const wchar_t* shortcut = nullptr,                       bool enabled = true, bool checked = false);
MenuItem MSep();
MenuItem MSub(const wchar_t* text, std::vector<MenuItem> sub, bool enabled = true);
MenuItem MCheck(int id, const wchar_t* text, bool checked);
std::vector<MenuItem> BuildImportMenu();
std::vector<MenuItem> BuildInsertMenu();
std::vector<MenuItem> BuildFileMenu();
std::vector<MenuItem> BuildOpMenu();
std::vector<MenuItem> BuildEditMenu();
std::vector<MenuItem> BuildTopMenuItems(int index);
std::vector<MenuItem> BuildContextMenu();
void OpenTopMenu(int index);
void PaintWindow();
std::wstring SessionDirPath();
std::wstring SessionListPath();
void CleanupSessionFiles(const std::vector<std::wstring>& keep);
void SaveSessionState();
bool RestoreSessionState();
int ResizeEdgeAt(HWND h, POINT pt);
void SetResizeCursor(int e);
UiMods CurrentMods(int button = 1);
LRESULT CALLBACK MainWndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp);
int APIENTRY wWinMain(HINSTANCE inst, HINSTANCE, LPWSTR, int show);
