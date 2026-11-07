// ============================================================================
//  api_p6a_grid_paint.h - module contract: state + interfaces of p6a_grid_paint.
//  Included only by translation units that reference this module.
// ============================================================================
#pragma once
#include "core.h"

inline UiRoot g_ui;
int ResizeEdgeAt(HWND h, POINT pt);
void SetResizeCursor(int edge);
void SetStatus(const std::wstring& s);
int TitleH();
int MenuH();
int ToolH();
int TabH();
int StatusH();
int HeaderW();
int HeaderH();
int ScrollW();
RECT ClientRect();
RECT GridAreaRect();
RECT TabBarRect();
RECT StatusRect();
RECT GridBodyRect();
RECT GridGripRect();
int ContentLeft(Doc& d);
int ContentTop(Doc& d);
int XOfCol(Doc& d, int c);
int YOfRow(Doc& d, int r);
int FindIndexForPos(const std::vector<int>& off, int pos);
int ColAtX(Doc& d, int x);
int RowAtY(Doc& d, int y);
RECT CellRect(Doc& d, int r, int c);
RECT SelectionScreenRect(Doc& d);
void EnsureCellVisible(Doc& d, int r, int c);
void SetSelection(Doc& d, int r1, int c1, int r2, int c2, bool keepAnchor = false);
const wchar_t* CellTag(int kind);
void DrawOutlinedText(HDC dc, const std::wstring& s, RECT rc, COLORREF fg, COLORREF outline, HFONT font, UINT flags);
