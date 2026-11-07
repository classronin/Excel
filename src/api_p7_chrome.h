// ============================================================================
//  api_p7_chrome.h - module contract: state + interfaces of p7_chrome.
//  Included only by translation units that reference this module.
// ============================================================================
#pragma once
#include "core.h"

void OpenTopMenu(int index);
bool CloseDocumentAt(int index, bool fromExit);
void CommitEditor(bool moveDown, int moveRight);
void CmdSum();
void CmdOp(wchar_t op);
std::wstring PromptText(const std::wstring& title, const std::wstring& hint, const std::wstring& initial);
int TitleH();
void EmitBox(PaintSpec& s, RECT rc, const std::wstring& text, COLORREF textColor, bool hot, bool showArrow);
void EmitColorButton(PaintSpec& s, RECT rc, COLORREF c, int id, bool arrowArea, bool hot);
int SelectionAlign(Doc& d);
int SelectionVAlign(Doc& d);
bool SelectionIntersectsMerge(Doc& d);
void ToggleMergeSelection();
void EmitAlignGlyph(PaintSpec& s, RECT rc, int align, COLORREF color);
void EmitVAlignGlyph(PaintSpec& s, RECT rc, int valign, COLORREF color);
void ApplyGlobalRowH(int px);
void ApplyGlobalColW(int px);
