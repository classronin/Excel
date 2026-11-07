// ============================================================================
//  api_p1_core.h - module contract: state + interfaces of p1_core.
//  Included only by translation units that reference this module.
// ============================================================================
#pragma once
#include "core.h"

inline HICON     g_iconLarge = nullptr;
inline HICON     g_iconSmall = nullptr;
inline int       g_dpi = 96;
inline int       g_maxRows    = 20000;
inline int       g_maxCols    = 256;
inline std::wstring g_defaultFont = L"微软雅黑";
inline int       g_defaultSize = 12;
inline int       g_winX = -1, g_winY = -1, g_winW = 1220, g_winH = 820;
inline bool      g_winMax = false;
inline std::wstring g_iniPath;
inline std::unordered_map<std::wstring, HFONT> g_fonts;
inline std::unordered_map<COLORREF, HBRUSH>    g_brushes;
inline std::unordered_map<long long, HPEN>     g_pens;
void ApplyThemeColors();
std::wstring Trim(const std::wstring& s);
std::wstring PathExtLower(const std::wstring& p);
std::wstring FullPathOf(const std::wstring& p);
bool EnsureDirectory(const std::wstring& p);
bool IsImageExt(const std::wstring& ext);
std::wstring ColorToHex(COLORREF c);
COLORREF HexToColor(const std::wstring& s);
std::wstring ReadTextUtf8(const std::wstring& path);
bool WriteTextUtf8(const std::wstring& path, const std::wstring& text);
std::wstring IniValue(const std::map<std::wstring, std::wstring>& kv,                              const wchar_t* key, const wchar_t* def);
int IniInt(const std::map<std::wstring, std::wstring>& kv, const wchar_t* key, int def);
void LoadSettings();
int PxFromPt(int pt);
int UiPxFromPt(int pt);
HFONT FontRaw(const std::wstring& face, int px, int weight, bool italic);
HFONT UiFontFace(const std::wstring& face, int pt, bool bold = false);
HFONT CellFont(const std::wstring& face, int pt, bool bold, bool italic);
HBRUSH Brush(COLORREF c);
HPEN Pen(COLORREF c, int w = 1);
void VLine(HDC dc, int x, int y1, int y2, COLORREF c, int w = 1);
COLORREF Blend(COLORREF a, COLORREF b, int pctA);
COLORREF Mix(COLORREF a, COLORREF b, int pctA);
void DrawCloseX(HDC dc, int cx, int cy, int r, COLORREF c);
void DrawLockGlyph(HDC dc, int cx, int cy, COLORREF c);
void DrawTriangleDown(HDC dc, int cx, int cy, int size, COLORREF c);
void DrawMark(HDC dc, int x, int y, int size, COLORREF bg, COLORREF fg);
RECT CellTextRect(HDC dc, const RECT& inner, HFONT font, const std::wstring& text, int vAlign, int* lineH = nullptr);
