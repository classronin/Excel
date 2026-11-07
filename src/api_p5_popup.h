// ============================================================================
//  api_p5_popup.h - module contract: state + interfaces of p5_popup.
//  Included only by translation units that reference this module.
//
//  Popup OBJECTS live in UiRoot, not in globals. Only the font cache (a pure
//  data cache) remains here.
// ============================================================================
#pragma once
#include "core.h"

inline std::vector<std::wstring> g_systemFonts;
void CloseAllPopups();
void CloseMenuPopup();
bool MenuPopupIsOpen();
void ShowMenuAt(const std::vector<MenuItem>& items, int x, int y);
void MenuKeyMove(int delta);
bool MenuKeyExecute();
void CloseListPopup();
void ClosePalettePopup();
// listKind: 0 = font face, 1 = point size. The object reports the picked
// VALUE; the action bus decides what that value means.
void ShowDropList(HWND owner, RECT anchor, std::vector<std::wstring> items,
                  int sel, int visible, int listKind);
COLORREF HsvToColor(double h, double s, double v);
void ShowPalette(HWND owner, RECT anchor, int target);
int CALLBACK FontEnumProc(const LOGFONTW* lf, const TEXTMETRICW*, DWORD, LPARAM lp);
const std::vector<std::wstring>& SystemFonts();
bool AnyPopupOpen();
