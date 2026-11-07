// ============================================================================
//  api_p6b_grid_input.h - module contract: state + interfaces of p6b_grid_input.
//  Included only by translation units that reference this module.
// ============================================================================
#pragma once
#include "core.h"

inline HWND g_editor = nullptr;
inline Doc* g_editDoc = nullptr;
inline int g_editRow = -1, g_editCol = -1;
inline bool g_committing = false;
inline HWND g_bandWnd = nullptr;
inline bool g_bandLine = false;
std::vector<MenuItem> BuildContextMenu();
std::vector<MenuItem> BuildOpMenu();
bool EditorActive();
void CancelEditor();
std::wstring EditorText();
bool EditorCoversCell(Doc* d, int r, int c);
void PositionEditor();
std::wstring EditorText();
void CommitEditor(bool moveDown, int moveRight);
void CancelEditor();
void BeginEditCell(Doc& d, int r, int c, const std::wstring* initial);
LRESULT CALLBACK EditorSubclassProc(HWND h, UINT msg, WPARAM wp, LPARAM lp,                                            UINT_PTR, DWORD_PTR);
void CreateEditorControl(HWND parent);
void ClearSelectionContents(Doc& d);
void MoveFocus(Doc& d, int dr, int dc, bool extend);
void AutoScrollForPoint(Doc& d, POINT pt);
LRESULT CALLBACK GhostWndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp);
void RegisterGhostClass();
void DestroyGhost();
void CreateGhost(Doc& d, POINT clientPt);
std::vector<MergeRange> SelectedBlocks(Doc& d);
bool SelectionBorderHit(Doc& d, POINT pt);
bool FormulaRefWord(wchar_t ch);
std::wstring ShiftFormulaRefs(const std::wstring& f, int dr, int dc);
LRESULT CALLBACK BandWndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp);
void RegisterBandClass();
void HideBand();
void ShowBandRect(const RECT& screenRc, bool line);
void ShowBandClientRect(RECT clientRc, bool line);
void UpdateCursorForPoint(POINT pt);
void OpenCellTarget(Doc& d, int r, int c);
void ZoomGridAt(POINT pt, int step);
