// ============================================================================
//  core.h - layer 0: immutable contract.
//
//  Platform surface, constants, enum values, forward declarations and
//  every type definition. Mutable module state is NOT declared here;
//  see api_*.h. Class-inline members see only the small extern set below.
// ============================================================================
#pragma once

#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#define _UNICODE
#define _WIN32_WINNT 0x0A00
#define WINVER 0x0A00
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <wincodec.h>
#include <dwmapi.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <memory>
#include <functional>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cfloat>
#include <cstdarg>
#include <cwchar>
#include <cwctype>
#include "sqlite3.h"
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#define APP_NAME    L"Excel"
#define APP_VERSION L"1.0"
#define IDI_APP_ICON 101
#define IDR_ABOUT_IMAGE 200
#ifdef CLR_NONE
#undef CLR_NONE
#endif
#define CLR_NONE    0xFFFFFFFFu

struct UiMods;

// -- compile-time constants and enum values --
inline constexpr int kInitRows = 30;
inline constexpr int kInitCols = 20;
inline const int kDefaultRows = 3000;
inline const int kDefaultCols = 64;
inline const int kDefaultColW = 100;
inline const int kDefaultRowH = 30;
inline const int kPaletteRows = 6;
inline const int kPaletteCols = 12;
inline const COLORREF kGhostKey = RGB(255, 0, 255);
inline const COLORREF kBandKey = RGB(255, 0, 255);
inline constexpr int kMenuCount = 4;
inline const wchar_t* kMenuTitles[kMenuCount] = { L"文件", L"编辑", L"设置", L"关于" };
inline const wchar_t kModalOwnerProp[] = L"ExcelModalOwnerEnabled";
inline const COMDLG_FILTERSPEC kDbFilters[] = {
    { L"Excel 表格 (*.db)", L"*.db" },
    { L"所有文件 (*.*)", L"*.*" }
};
inline const COMDLG_FILTERSPEC kImageFilters[] = {
    { L"图像 (*.png;*.jpg;*.jpeg;*.gif;*.bmp;*.webp;*.tif;*.tiff)", L"*.png;*.jpg;*.jpeg;*.gif;*.bmp;*.webp;*.tif;*.tiff" },
    { L"所有文件 (*.*)", L"*.*" }
};
inline const COMDLG_FILTERSPEC kAllFilters[] = {
    { L"所有文件 (*.*)", L"*.*" }
};
enum class UiAction
{
    None,
    Command,        // param = command id
    RowHeight,
    ColWidth,
    OpenSettings,
    OpenAbout,
    OpenNumbering,
    OpenUnit,
    CloseTab,       // param = tab index
    CloseWindow,
    ClosePopups,    // dismiss every popup after the device event is done
    MenuPick,       // param = command id
    ListPick,       // param = list kind (0 font, 1 size), text = picked value
    PalettePick     // param = target, color = chosen colour
};

enum CellKind {
    CK_TEXT   = 0,
    CK_FILE   = 1,
    CK_FOLDER = 2,
    CK_IMAGE  = 3,
    CK_URL    = 4
};

enum MenuKind { MK_NORMAL, MK_SEP, MK_SUB, MK_CHECK };

enum class UiId {
    None, TitleBar, MenuBar, Toolbar, Grid, Tabs, Status,
    PopupMenu, PopupList, PopupPalette
};

enum class GestureKind
{
    None,
    Select,        // dragging a selection
    MovePending,   // pressed the selection border, threshold not passed
    MoveCells,     // dragging the selection to a new place
    ResizeCol,     // dragging a column boundary
    ResizeRow,     // dragging a row boundary
    ResizeGrid     // dragging the grid corner (rows x cols)
};

enum {
    ID_FILE_NEW = 1001,
    ID_FILE_OPEN,
    ID_FILE_SAVE,
    ID_FILE_SAVEAS,
    ID_FILE_EXIT,

    ID_EDIT_CUT = 1101,
    ID_EDIT_COPY,
    ID_EDIT_PASTE,
    ID_EDIT_UNDO,
    ID_EDIT_REDO,

    ID_IMPORT_FILE = 1201,
    ID_IMPORT_FOLDER,
    ID_IMPORT_IMAGE,
    ID_IMPORT_URL,

    ID_INSERT_UP = 1301,
    ID_INSERT_DOWN,
    ID_INSERT_LEFT,
    ID_INSERT_RIGHT,

    ID_EDIT_SUM = 1401,
    ID_OP_ADD, ID_OP_SUB, ID_OP_MUL, ID_OP_DIV,
    ID_TAB_LOCK = 1501
};

enum MsgMode { MSGB_OK = 0, MSGB_SAVE = 1, MSGB_YESNO = 2 };


// -- forward declarations (types defined below) --
struct ActionItem;
struct ActionBus;
struct Theme;
struct CellStyle;
struct Cell;
struct MergeRange;
struct Snapshot;
struct Doc;
struct MediaCopyResult;
struct FVal;
struct EvalActiveGuard;
struct FormulaEval;
struct ImageFrame;
struct ImageData;
struct MenuItem;
struct MenuPopup;
struct ListPopup;
struct PalettePopup;
struct UiMods;
struct UiObject;
struct PopupObject;
struct PopupInput;
struct AnimCell;
struct GridGesture;
struct PanGesture;
struct ScrollBar;
struct ToolLayout;
struct TabLayout;
struct GridView;
struct TitleBar;
struct MenuBar;
struct Toolbar;
struct TabBar;
struct HintSpec;
struct StatusSig;
struct StatusModel;
struct StatusBar;
struct UiRoot;
struct ModalFocusState;
struct MsgButton;
struct MsgState;
struct PromptState;
struct ClipBlock;
struct ImportItem;
struct PropState;
struct SettingsState;

// -- declarations class-inline members depend on --
std::wstring Format(const wchar_t* fmt, ...);
std::wstring ToLower(const std::wstring& s);
std::string WideToUtf8(const std::wstring& s);
std::wstring Utf8ToWide(const std::string& s);
bool ParseNum(const std::wstring& in, double& out);
std::wstring NumToStr(double v);
bool IsAbsPath(const std::wstring& p);
std::wstring PathDirName(const std::wstring& p);
std::wstring PathBaseName(const std::wstring& p);
bool IsUnderDir(const std::wstring& path, const std::wstring& dir);
std::wstring PathJoin(const std::wstring& a, const std::wstring& b);
std::wstring ExeDir();
bool FileExistsW(const std::wstring& p);
bool DirExistsW(const std::wstring& p);
void SaveSettings();
int DpiScale(int v);
int GridScale(int v);
HFONT UiFont(int pt, bool bold = false);
HFONT MonoFont(int pt, bool bold = false);
// Same fonts, but scaled by an explicit (dpi, zoom) pair instead of the live
// one. A window that has frozen its own scale must also freeze the size of the
// text it draws, or the text outgrows the box that holds it.
HFONT UiFontScaled(int pt, bool bold, int dpi, int zoom);
HFONT MonoFontScaled(int pt, bool bold, int dpi, int zoom);
void FillRectC(HDC dc, const RECT& rc, COLORREF c);
void FrameRectC(HDC dc, RECT rc, COLORREF c, int w = 1);
void HLine(HDC dc, int x1, int x2, int y, COLORREF c, int w = 1);
void TextIn(HDC dc, const std::wstring& s, RECT rc, COLORREF color, HFONT font, UINT flags);
SIZE MeasureText(HDC dc, const std::wstring& s, HFONT font);
void DrawCheckMark(HDC dc, int cx, int cy, COLORREF c);
void DrawChevron(HDC dc, int x, int y, int size, COLORREF c, bool right);
void UpdateWindowTitle();
void InvalidateAll();
std::wstring CellRefName(int r, int c);
long long CellKey(int r, int c);
int KeyRow(long long k);
int KeyCol(long long k);
Doc* ActiveDoc();
std::wstring CellDisplay(Doc& doc, int r, int c);
void ApplyColorToSelection(int target, COLORREF color);
void CloseListPopup();
void ClosePalettePopup();
void CloseMenuPopup();
const std::vector<COLORREF>& PaletteColors();

extern ActionBus g_actions;
extern int g_active;
extern bool      g_autoCopy;
extern COLORREF  g_btnCellBg;
extern COLORREF  g_btnFg;
extern COLORREF  g_btnOutline;
extern int       g_colW;
extern uint64_t g_contentRev;
extern bool      g_dark;
extern thread_local std::unordered_set<uint64_t> g_evalActive;
extern thread_local int g_evalBudget;
extern int       g_gridZoom;
extern HWND      g_hwnd;
extern HINSTANCE g_inst;
extern int       g_rowH;
extern Theme g_th;
extern int       g_uiZoom;

// -- type definitions --
// An action is neutral data. Nothing in an input handler executes a command;
// the bus is drained after the device event has finished.
struct ActionItem {
    UiAction type = UiAction::None;
    int param = 0;
    COLORREF color = 0;
    std::wstring text;
};

struct ActionBus
{
    std::vector<ActionItem> queue;

    // O(1): output an action, never execute it inside an object's input handler.
    void Post(UiAction type, int param = 0, COLORREF color = 0,
              const std::wstring& text = {})
    { queue.push_back({ type, param, color, text }); }

    void Drain();                              // after the device event is done
    bool HandleKey(WPARAM vk, const UiMods& m); // declarative shortcut table
};

struct Theme {
    COLORREF win;          // window background
    COLORREF cell;         // default cell background
    COLORREF panel;        // toolbar / tabs / status
    COLORREF panel2;       // inputs, editor
    COLORREF border;
    COLORREF borderStrong;
    COLORREF text;
    COLORREF dim;
    COLORREF faint;
    COLORREF accent;
    COLORREF accentSoft;
    COLORREF selFill;
    COLORREF header;
    COLORREF headerSel;
    COLORREF hover;
    COLORREF grid;
    COLORREF gridStrong;
    COLORREF danger;
};

struct CellStyle {
    std::wstring font;       // empty = default face
    int size = 0;            // 0 = default size
    int fg = -1;             // CLR_NONE
    int outline = -1;
    int cellBg = -1;
    int align = -1;          // -1 default, 0 left, 1 center, 2 right
    bool bold = false;
    bool italic = false;

    bool IsDefault() const
    {
        return font.empty() && size == 0 && fg == -1 && outline == -1 && cellBg == -1
            && align == -1 && !bold && !italic;
    }
};

struct Cell {
    std::wstring text;       // literal text
    std::wstring formula;    // starts with '='
    std::wstring path;       // stored path/url for CK_FILE/FOLDER/IMAGE/URL
    std::wstring unit;       // 单位 suffix, shown to the right of the value
    int kind = CK_TEXT;
    CellStyle st;

    // Logic layer writes the resolved display value once per data revision;
    // the render layer only reads it (no formula evaluation while painting).
    std::wstring display;
    uint64_t displayRev = 0;

    bool HasValue() const { return !text.empty() || !formula.empty(); }
    bool IsEmpty() const { return text.empty() && formula.empty() && path.empty() && unit.empty(); }
};

struct MergeRange {
    int r1 = 0, c1 = 0, r2 = 0, c2 = 0;
};

struct Snapshot {
    std::unordered_map<long long, Cell> cells;
    std::vector<MergeRange> merges;
    std::vector<int> colW, rowH;
    int rows = 0, cols = 0;
};

// The ONE neutral output of every input device. Devices state only where they
// want the view to be; viewport size, capacity, clamping and painting belong
// to the view layer. No device carries its own capacity rule.
struct ViewRequest {
    int x = 0, y = 0;      // requested scroll origin, deliberately unclamped
};

// O(1) guard for the declared capacity. The VIEW enters as quantised
// row/column indices, not as a raw scroll position: capacity must follow the
// view, or it degenerates into a constant and never follows the drag. What is
// deliberately absent is a scrollbar rectangle, a gesture, a device flag and
// any "reached the top / hit the end" condition.
struct CapacitySig {
    uint64_t rev = 0;
    int vw = 0, vh = 0;
    int rows = 0, cols = 0;
    int totalW = 0, totalH = 0;
    int selR = 0, selC = 0;
    int minR = 0, minC = 0;
    int viewR = 0, viewC = 0;   // quantised view range: the one dynamic input
    bool Same(const CapacitySig& o) const
    {
        return rev == o.rev && vw == o.vw && vh == o.vh
            && rows == o.rows && cols == o.cols
            && totalW == o.totalW && totalH == o.totalH
            && selR == o.selR && selC == o.selC
            && minR == o.minR && minC == o.minC
            && viewR == o.viewR && viewC == o.viewC;
    }
};

struct Doc {
    sqlite3* db = nullptr;
    std::wstring filePath;
    std::wstring sessionPath;   // 未命名标签的会话备份文件
    std::wstring title;
    std::wstring lastError;
    bool dirty = false;
    bool locked = false;

    std::unordered_map<long long, Cell> cells;
    std::vector<MergeRange> merges;
    std::vector<MergeRange> selExtra;
    std::vector<int> colW, rowH;
    int rows = 0, cols = 0;
    // Explicit capacity floor set by the user (grid-corner resize) or restored
    // from a saved document. The declared capacity never goes below this.
    int minRows = kInitRows, minCols = kInitCols;
    CapacitySig capacitySig;   // O(1) guard for declared capacity

    // Content extent: the last row/column that actually holds something. The
    // scan is O(content) and capacity is now reconciled on every drag frame, so
    // the result is cached against g_contentRev - the counter every cell and
    // merge edit bumps. A hit is a proof, not a guess.
    int extentRows = 0, extentCols = 0;
    uint64_t extentRev = ~0ull;

    int selAR = 0, selAC = 0, selFR = 0, selFC = 0;
    int scrollX = 0, scrollY = 0;

    // Neutral view intent: the last origin requested by ANY device, unclamped.
    // It is view data, not device state. CommitFrame folds it back to the
    // settled origin after the frame, so state always matches what is drawn.
    int viewTargetX = 0, viewTargetY = 0;
    void NoteViewRequest(const ViewRequest& req)
    {
        viewTargetX = req.x;
        viewTargetY = req.y;
    }

    std::vector<int> colOff, rowOff;
    bool layoutDirty = true;

    std::vector<Snapshot> undos, redos;

    Doc() { CreateBlank(); }
    ~Doc()
    {
        if (db) { sqlite3_close(db); db = nullptr; }
    }

    // ------------------------------------------------------------------
    void CreateBlank()
    {
        if (db) { sqlite3_close(db); db = nullptr; }
        filePath.clear();
        sessionPath.clear();
        title = APP_NAME;
        dirty = false;
        locked = false;
        lastError.clear();
        cells.clear();
        merges.clear();
        selExtra.clear();
        rows = kInitRows;
        cols = kInitCols;
        minRows = kInitRows;
        minCols = kInitCols;
        colW.assign(cols, g_colW);
        rowH.assign(rows, g_rowH);
        selAR = selAC = selFR = selFC = 0;
        scrollX = scrollY = 0;
        viewTargetX = viewTargetY = 0;
        undos.clear();
        redos.clear();
        extentRev = ~0ull;
        InvalidateLayout();
    }

    Cell* Find(int r, int c)
    {
        auto it = cells.find(CellKey(r, c));
        return it == cells.end() ? nullptr : &it->second;
    }
    const Cell* Find(int r, int c) const
    {
        auto it = cells.find(CellKey(r, c));
        return it == cells.end() ? nullptr : &it->second;
    }
    MergeRange* MergeAt(int r, int c)
    {
        for (auto& m : merges)
            if (r >= m.r1 && r <= m.r2 && c >= m.c1 && c <= m.c2) return &m;
        return nullptr;
    }
    const MergeRange* MergeAt(int r, int c) const
    {
        for (auto& m : merges)
            if (r >= m.r1 && r <= m.r2 && c >= m.c1 && c <= m.c2) return &m;
        return nullptr;
    }
    bool MergeTopLeft(int r, int c) const
    {
        const MergeRange* m = MergeAt(r, c);
        return m && m->r1 == r && m->c1 == c;
    }

    Cell& Ensure(int r, int c)
    {
        auto& c2 = cells[CellKey(r, c)];
        return c2;
    }

    void EraseIfTrivial(int r, int c)
    {
        auto it = cells.find(CellKey(r, c));
        if (it == cells.end()) return;
        if (it->second.IsEmpty() && it->second.st.IsDefault())
            cells.erase(it);
    }

    void Touch()
    {
        dirty = true;
        ++g_contentRev;             // logic-layer data version for caches
        UpdateWindowTitle();
        InvalidateAll();
    }

    // Capacity changes are NOT data changes: no cell value changed, so the
    // CellDisplay cache keyed by g_contentRev stays valid. Bumping the content
    // revision here would also re-trigger capacity reconciliation every frame.
    void MarkCapacityChanged()
    {
        // Capacity is not stored in the file (only cells, column widths, row
        // heights and merges are), so growing or shrinking the sheet must not
        // mark the document dirty: dragging the scrollbar past the end would
        // otherwise turn an untouched sheet into "unsaved changes".
        UpdateWindowTitle();
        InvalidateAll();
    }

    void InvalidateLayout()
    {
        // A metric change (zoom, row/column size, a loaded document) makes every
        // existing offset wrong, so the arrays are dropped and measured again.
        // Capacity growth does NOT come through here - it only appends.
        rowOff.clear();
        colOff.clear();
        layoutDirty = true;
    }

    int ColPx(int c) const
    {
        int base = (c >= 0 && c < (int)colW.size()) ? colW[c] : g_colW;
        int v = GridScale(base);
        return v < 8 ? 8 : v;
    }
    int RowPx(int r) const
    {
        int base = (r >= 0 && r < (int)rowH.size()) ? rowH[r] : g_rowH;
        int v = GridScale(base);
        return v < 8 ? 8 : v;
    }

    void RebuildLayout()
    {
        if (!layoutDirty) return;
        // Appended, not recomputed. Capacity only ever grows or shrinks at the
        // tail, so the offsets already measured stay exact and the cost of one
        // more row is one more row. The array size IS the validity marker: a
        // separate counter could disagree with it, this cannot.
        int rr = (int)rowOff.size() - 1;
        if (rr < 0) rr = 0;
        if (rr > rows) rr = rows;               // shrunk: keep the valid prefix
        rowOff.resize(rows + 1);
        if (rr == 0) rowOff[0] = 0;
        int y = rowOff[rr];
        for (int r = rr; r < rows; ++r) { y += RowPx(r); rowOff[r + 1] = y; }

        int cc = (int)colOff.size() - 1;
        if (cc < 0) cc = 0;
        if (cc > cols) cc = cols;
        colOff.resize(cols + 1);
        if (cc == 0) colOff[0] = 0;
        int x = colOff[cc];
        for (int c = cc; c < cols; ++c) { x += ColPx(c); colOff[c + 1] = x; }
        layoutDirty = false;
    }

    int TotalWidth() { RebuildLayout(); return colOff[cols]; }
    int TotalHeight() { RebuildLayout(); return rowOff[rows]; }

    void ClampScroll(int viewW, int viewH)
    {
        RebuildLayout();
        int maxX = colOff[cols] - viewW;
        int maxY = rowOff[rows] - viewH;
        if (maxX < 0) maxX = 0;
        if (maxY < 0) maxY = 0;
        if (scrollX > maxX) scrollX = maxX;
        if (scrollY > maxY) scrollY = maxY;
        if (scrollX < 0) scrollX = 0;
        if (scrollY < 0) scrollY = 0;
    }

    // ---- declared capacity ------------------------------------------------
    // Capacity is a PURE FUNCTION OF THE VIEW: the row/column range the sheet
    // must be able to show, plus content, selection and the explicit floor.
    // It is evaluated once per frame in ReconcileCapacity, to the exact row.
    // No block quantum: the thumb's length is track * screen / sheet, so a
    // block-sized step in capacity would be a visible step in the thumb. One
    // row of demand is one row of sheet, and the thumb shrinks by one row's
    // worth - continuously, the way a scrollbar is meant to.
    // There is no grow path and no trim path - growth and shrink are the SAME
    // assignment. What is excluded is a scrollbar rectangle, a gesture, a
    // device flag and any "reached the top / hit the end" condition. The view
    // position itself is NOT excluded: without it capacity is a constant and
    // it never follows the drag.

    // Quantised view need: how many rows/cols the layout must have to reach a
    // given edge. Inside the current layout use real offsets (custom sizes are
    // exact); beyond it, extrapolate with the default metric. The caller must
    // have RebuildLayout() done, so rowOff/colOff are sized rows+1 / cols+1.
    int ViewRowsFor(int bottomY) const
    {
        if (bottomY <= rowOff[rows]) {
            int i = (int)(std::lower_bound(rowOff.begin(), rowOff.end(), bottomY) - rowOff.begin());
            return std::min(rows, std::max(0, i));
        }
        int px = std::max(1, GridScale(g_rowH));
        int extra = (bottomY - rowOff[rows] + px - 1) / px;
        return std::min(20000, rows + extra);
    }
    int ViewColsFor(int rightX) const
    {
        if (rightX <= colOff[cols]) {
            int i = (int)(std::lower_bound(colOff.begin(), colOff.end(), rightX) - colOff.begin());
            return std::min(cols, std::max(0, i));
        }
        int px = std::max(1, GridScale(g_colW));
        int extra = (rightX - colOff[cols] + px - 1) / px;
        return std::min(256, cols + extra);
    }

    void ContentExtent(int& ur, int& uc)
    {
        if (extentRev != g_contentRev) {
            extentRows = extentCols = 0;
            for (auto& kv : cells) {
                extentRows = std::max(extentRows, KeyRow(kv.first) + 1);
                extentCols = std::max(extentCols, KeyCol(kv.first) + 1);
            }
            for (auto& m : merges) {
                extentRows = std::max(extentRows, m.r2 + 1);
                extentCols = std::max(extentCols, m.c2 + 1);
            }
            extentRev = g_contentRev;
        }
        ur = extentRows;
        uc = extentCols;
    }

    void ReconcileCapacity(int viewW, int viewH);

    // ---- viewport actions -------------------------------------------------
    // The view supplies only its body size; the document never reads window
    // geometry. Resolving a request is a PURE CLAMP against the declared
    // capacity - every device goes through here, none of them sizes the sheet.
    int MaxScrollX(int viewW) { RebuildLayout(); return std::max(0, TotalWidth() - std::max(0, viewW)); }
    int MaxScrollY(int viewH) { RebuildLayout(); return std::max(0, TotalHeight() - std::max(0, viewH)); }
    POINT ResolveView(const ViewRequest& req, int viewW, int viewH)
    {
        int w = std::max(1, viewW), h = std::max(1, viewH);
        POINT t;
        t.x = std::max(0, std::min(MaxScrollX(w), req.x));
        t.y = std::max(0, std::min(MaxScrollY(h), req.y));
        return t;
    }

    void SelectionRect(int& r1, int& c1, int& r2, int& c2) const
    {
        r1 = std::min(selAR, selFR);
        c1 = std::min(selAC, selFC);
        r2 = std::max(selAR, selFR);
        c2 = std::max(selAC, selFC);
    }

    bool CellInSelection(int r, int c) const
    {
        int r1, c1, r2, c2;
        SelectionRect(r1, c1, r2, c2);
        if (r >= r1 && r <= r2 && c >= c1 && c <= c2) return true;
        for (auto& e : selExtra)
            if (r >= e.r1 && r <= e.r2 && c >= e.c1 && c <= e.c2) return true;
        return false;
    }

    // ------------------------------------------------------------------
    //  undo
    // ------------------------------------------------------------------
    Snapshot Capture() const
    {
        Snapshot s;
        s.cells = cells;
        s.merges = merges;
        s.colW = colW;
        s.rowH = rowH;
        s.rows = rows;
        s.cols = cols;
        return s;
    }

    void PushUndo()
    {
        undos.push_back(Capture());
        if (undos.size() > 40)
            undos.erase(undos.begin());
        redos.clear();
    }

    void ApplySnapshot(Snapshot&& s)
    {
        cells = std::move(s.cells);
        merges = std::move(s.merges);
        colW = std::move(s.colW);
        rowH = std::move(s.rowH);
        rows = s.rows;
        cols = s.cols;
        extentRev = ~0ull;
        InvalidateLayout();
        if (selFR >= rows) selFR = rows - 1;
        if (selAR >= rows) selAR = rows - 1;
        if (selFC >= cols) selFC = cols - 1;
        if (selAC >= cols) selAC = cols - 1;
    }

    void Undo()
    {
        if (undos.empty()) return;
        redos.push_back(Capture());
        ApplySnapshot(std::move(undos.back()));
        undos.pop_back();
        dirty = true;
        ++g_contentRev;             // snapshot replaced all cells
        UpdateWindowTitle();
        InvalidateAll();
    }

    void Redo()
    {
        if (redos.empty()) return;
        undos.push_back(Capture());
        ApplySnapshot(std::move(redos.back()));
        redos.pop_back();
        dirty = true;
        ++g_contentRev;             // snapshot replaced all cells
        UpdateWindowTitle();
        InvalidateAll();
    }

    // ------------------------------------------------------------------
    //  paths / media
    // ------------------------------------------------------------------
    std::wstring Dir() const
    {
        return filePath.empty() ? std::wstring() : PathDirName(filePath);
    }

    std::wstring MediaDirAbs() const
    {
        std::wstring exe = ExeDir();
        if (!exe.empty()) return PathJoin(exe, L"media");
        std::wstring d = Dir();
        return d.empty() ? std::wstring() : PathJoin(d, L"media");
    }

    std::wstring AbsPathOf(const std::wstring& stored) const
    {
        if (stored.empty()) return stored;
        if (IsAbsPath(stored)) return stored;
        std::wstring lower = ToLower(stored);
        if (lower.rfind(L"media\\", 0) == 0 || lower.rfind(L"media/", 0) == 0) {
            std::wstring exe = ExeDir();
            if (!exe.empty()) {
                std::wstring p = PathJoin(exe, stored);
                if (FileExistsW(p) || DirExistsW(p)) return p;
            }
        }
        std::wstring d = Dir();
        if (d.empty()) return stored;
        return PathJoin(d, stored);
    }

    std::wstring StoragePathOf(const std::wstring& abs) const
    {
        if (abs.empty()) return abs;
        if (!IsAbsPath(abs)) return abs;
        std::wstring media = PathJoin(ExeDir(), L"media");
        if (!media.empty() && IsUnderDir(abs, media)) {
            std::wstring rest = abs.substr(media.size());
            while (!rest.empty() && (rest[0] == L'\\' || rest[0] == L'/')) rest.erase(rest.begin());
            return PathJoin(L"media", rest);
        }
        std::wstring d = Dir();
        if (d.empty()) return abs;
        if (IsUnderDir(abs, d)) {
            std::wstring rest = abs.substr(d.size());
            while (!rest.empty() && (rest[0] == L'\\' || rest[0] == L'/')) rest.erase(rest.begin());
            return rest;
        }
        return abs;
    }

    void MakeStoredPathsRelative()
    {
        std::wstring media = PathJoin(ExeDir(), L"media");
        std::wstring d = Dir();
        for (auto& kv : cells) {
            Cell& cell = kv.second;
            if (cell.path.empty() || cell.kind == CK_URL) continue;
            if (!IsAbsPath(cell.path)) continue;
            if (!media.empty() && IsUnderDir(cell.path, media)) {
                std::wstring rest = cell.path.substr(media.size());
                while (!rest.empty() && (rest[0] == L'\\' || rest[0] == L'/')) rest.erase(rest.begin());
                cell.path = PathJoin(L"media", rest);
                continue;
            }
            if (d.empty()) continue;
            if (IsUnderDir(cell.path, d)) {
                std::wstring rest = cell.path.substr(d.size());
                while (!rest.empty() && (rest[0] == L'\\' || rest[0] == L'/')) rest.erase(rest.begin());
                cell.path = rest;
            }
        }
    }

    void AutoCopyPending();

    // ------------------------------------------------------------------
    //  SQLite
    // ------------------------------------------------------------------
    bool OpenDb(const std::wstring& path, std::wstring& err)
    {
        if (db) { sqlite3_close(db); db = nullptr; }
        sqlite3* nd = nullptr;
        std::string u8 = WideToUtf8(path);
        int rc = sqlite3_open_v2(u8.c_str(), &nd, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
        if (rc != SQLITE_OK) {
            err = nd ? Utf8ToWide(sqlite3_errmsg(nd)) : L"无法打开数据库";
            if (nd) sqlite3_close(nd);
            return false;
        }
        sqlite3_busy_timeout(nd, 4000);
        sqlite3_exec(nd, "PRAGMA journal_mode=DELETE;", nullptr, nullptr, nullptr);
        sqlite3_exec(nd, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);
        db = nd;
        return true;
    }

    bool EnsureSchema(std::wstring& err)
    {
        if (!db) { err = L"数据库未打开"; return false; }
        const char* sql =
            "CREATE TABLE IF NOT EXISTS meta (key TEXT PRIMARY KEY, value TEXT);"
            "CREATE TABLE IF NOT EXISTS cells ("
            "  row INTEGER NOT NULL, col INTEGER NOT NULL,"
            "  text TEXT, formula TEXT, kind INTEGER DEFAULT 0, path TEXT,"
            "  font TEXT, size INTEGER, fg INTEGER, textbg INTEGER, cellbg INTEGER,"
            "  align INTEGER, bold INTEGER, italic INTEGER, unit TEXT,"
            "  PRIMARY KEY(row, col));"
            "CREATE TABLE IF NOT EXISTS layout (type INTEGER NOT NULL, idx INTEGER NOT NULL, size INTEGER NOT NULL,"
            "  PRIMARY KEY(type, idx));"
            "CREATE TABLE IF NOT EXISTS merges (r1 INTEGER NOT NULL, c1 INTEGER NOT NULL, r2 INTEGER NOT NULL, c2 INTEGER NOT NULL,"
            "  PRIMARY KEY(r1, c1));";
        char* msg = nullptr;
        if (sqlite3_exec(db, sql, nullptr, nullptr, &msg) != SQLITE_OK) {
            err = msg ? Utf8ToWide(msg) : L"创建表失败";
            if (msg) sqlite3_free(msg);
            return false;
        }
        // 确认 cells 表是本程序的结构
        sqlite3_stmt* st = nullptr;
        bool okShape = false;
        if (sqlite3_prepare_v2(db, "PRAGMA table_info(cells);", -1, &st, nullptr) == SQLITE_OK) {
            while (sqlite3_step(st) == SQLITE_ROW) {
                const unsigned char* name = sqlite3_column_text(st, 1);
                if (name && std::string((const char*)name) == "formula") okShape = true;
            }
            sqlite3_finalize(st);
        } else {
            sqlite3_finalize(st);
        }
        if (!okShape) { err = L"该 .db 不是 Excel 表格结构"; return false; }
        // Older files predate the 单位 suffix column; add it in place (no-op if present).
        sqlite3_exec(db, "ALTER TABLE cells ADD COLUMN unit TEXT;", nullptr, nullptr, nullptr);
        sqlite3_exec(db,
            "INSERT OR IGNORE INTO meta(key,value) VALUES('app','Excel');"
            "INSERT OR IGNORE INTO meta(key,value) VALUES('version','1');",
            nullptr, nullptr, nullptr);
        return true;
    }

    bool LoadFile(const std::wstring& path, std::wstring& err)
    {
        if (!OpenDb(path, err)) return false;
        if (!EnsureSchema(err)) { sqlite3_close(db); db = nullptr; return false; }

        cells.clear();
        merges.clear();
        selExtra.clear();
        rows = kInitRows;
        cols = kInitCols;
        colW.assign(cols, g_colW);
        rowH.assign(rows, g_rowH);
        undos.clear();
        redos.clear();
        scrollX = scrollY = 0;
        viewTargetX = viewTargetY = 0;
        selAR = selAC = selFR = selFC = 0;

        int maxR = 0, maxC = 0, n = 0;
        sqlite3_stmt* st = nullptr;
        const char* sql =
            "SELECT row,col,text,formula,kind,path,font,size,fg,textbg,cellbg,align,bold,italic,unit FROM cells;";
        if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK) {
            err = Utf8ToWide(sqlite3_errmsg(db));
            return false;
        }
        while (sqlite3_step(st) == SQLITE_ROW) {
            int r = sqlite3_column_int(st, 0);
            int c = sqlite3_column_int(st, 1);
            if (r < 0 || c < 0 || r > 1000000 || c > 10000) continue;
            Cell cell;
            auto colText = [&](int idx) -> std::wstring {
                const unsigned char* t = sqlite3_column_text(st, idx);
                return t ? Utf8ToWide(std::string((const char*)t)) : std::wstring();
            };
            cell.text = colText(2);
            cell.formula = colText(3);
            cell.kind = sqlite3_column_int(st, 4);
            cell.path = colText(5);
            cell.st.font = colText(6);
            cell.st.size = sqlite3_column_int(st, 7);
            cell.st.fg = sqlite3_column_int(st, 8);
            cell.st.outline = sqlite3_column_int(st, 9);
            cell.st.cellBg = sqlite3_column_int(st, 10);
            cell.st.align = sqlite3_column_int(st, 11);
            cell.st.bold = sqlite3_column_int(st, 12) != 0;
            cell.st.italic = sqlite3_column_int(st, 13) != 0;
            cell.unit = colText(14);
            cells[CellKey(r, c)] = cell;
            if (r > maxR) maxR = r;
            if (c > maxC) maxC = c;
            ++n;
        }
        sqlite3_finalize(st);

        int savedRows = 0, savedCols = 0;
        if (sqlite3_prepare_v2(db, "SELECT key,value FROM meta WHERE key IN ('rows','cols');", -1, &st, nullptr) == SQLITE_OK) {
            while (sqlite3_step(st) == SQLITE_ROW) {
                const unsigned char* k = sqlite3_column_text(st, 0);
                const unsigned char* v = sqlite3_column_text(st, 1);
                if (!k || !v) continue;
                int n = atoi((const char*)v);
                if (std::string((const char*)k) == "rows") savedRows = n;
                else if (std::string((const char*)k) == "cols") savedCols = n;
            }
            sqlite3_finalize(st);
        }
        (void)savedRows;
        (void)savedCols;
        rows = std::max(kInitRows, maxR + 1);
        cols = std::max(kInitCols, maxC + 1);
        rows = std::max(1, std::min(rows, 20000));
        cols = std::max(1, std::min(cols, 256));
        minRows = rows;          // a saved sheet keeps its declared size
        minCols = cols;
        viewTargetX = viewTargetY = 0;
        colW.assign(cols, g_colW);
        rowH.assign(rows, g_rowH);

        if (sqlite3_prepare_v2(db, "SELECT type,idx,size FROM layout;", -1, &st, nullptr) == SQLITE_OK) {
            while (sqlite3_step(st) == SQLITE_ROW) {
                int type = sqlite3_column_int(st, 0);
                int idx = sqlite3_column_int(st, 1);
                int size = sqlite3_column_int(st, 2);
                if (type == 0 && idx >= 0 && idx < cols) colW[idx] = size;
                if (type == 1 && idx >= 0 && idx < rows) rowH[idx] = size;
            }
            sqlite3_finalize(st);
        }


        merges.clear();
        if (sqlite3_prepare_v2(db, "SELECT r1,c1,r2,c2 FROM merges;", -1, &st, nullptr) == SQLITE_OK) {
            while (sqlite3_step(st) == SQLITE_ROW) {
                int r1 = sqlite3_column_int(st, 0), c1 = sqlite3_column_int(st, 1);
                int r2 = sqlite3_column_int(st, 2), c2 = sqlite3_column_int(st, 3);
                if (r1 < 0 || c1 < 0 || r2 < r1 || c2 < c1) continue;
                if (r2 >= rows || c2 >= cols) continue;
                if (r1 == r2 && c1 == c2) continue;
                merges.push_back({ r1, c1, r2, c2 });
            }
            sqlite3_finalize(st);
        }
        filePath = path;
        sessionPath.clear();
        title = PathBaseName(path);
        dirty = false;
        lastError.clear();
        extentRev = ~0ull;
        InvalidateLayout();
        UpdateWindowTitle();
        return true;
    }

    bool SaveTo(const std::wstring& path, std::wstring& err)
    {
        if (path.empty()) { err = L"路径为空"; return false; }

        // 另存时先把旧的相对路径转成绝对路径，避免基准目录改变后失效
        if (!filePath.empty() && _wcsicmp(filePath.c_str(), path.c_str()) != 0) {
            for (auto& kv : cells) {
                Cell& cell = kv.second;
                if (cell.kind == CK_URL || cell.path.empty()) continue;
                if (!IsAbsPath(cell.path))
                    cell.path = AbsPathOf(cell.path);
            }
        }

        bool wasOpen = (db != nullptr);
        std::wstring oldPath = filePath;

        filePath = path;
        if (!wasOpen || !db) {
            if (!OpenDb(path, err)) { filePath = oldPath; return false; }
        } else if (_wcsicmp(oldPath.c_str(), path.c_str()) != 0) {
            if (!OpenDb(path, err)) { filePath = oldPath; return false; }
        }
        if (!EnsureSchema(err)) return false;

        if (g_autoCopy) AutoCopyPending();
        MakeStoredPathsRelative();

        char* msg = nullptr;
        if (sqlite3_exec(db, "BEGIN IMMEDIATE;", nullptr, nullptr, &msg) != SQLITE_OK) {
            err = msg ? Utf8ToWide(msg) : L"事务开始失败";
            if (msg) sqlite3_free(msg);
            return false;
        }
        sqlite3_exec(db, "DELETE FROM cells; DELETE FROM layout; DELETE FROM merges;", nullptr, nullptr, nullptr);

        sqlite3_stmt* st = nullptr;
        const char* ins =
            "INSERT INTO cells(row,col,text,formula,kind,path,font,size,fg,textbg,cellbg,align,bold,italic,unit)"
            " VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";
        if (sqlite3_prepare_v2(db, ins, -1, &st, nullptr) != SQLITE_OK) {
            err = Utf8ToWide(sqlite3_errmsg(db));
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            return false;
        }
        bool ok = true;
        for (auto& kv : cells) {
            const Cell& cell = kv.second;
            if (cell.IsEmpty() && cell.st.IsDefault()) continue;
            int r = KeyRow(kv.first);
            int c = KeyCol(kv.first);
            std::string t = WideToUtf8(cell.text);
            std::string f = WideToUtf8(cell.formula);
            std::string p = WideToUtf8(cell.path);
            std::string face = WideToUtf8(cell.st.font);
            std::string u = WideToUtf8(cell.unit);
            sqlite3_reset(st);
            sqlite3_bind_int(st, 1, r);
            sqlite3_bind_int(st, 2, c);
            sqlite3_bind_text(st, 3, t.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(st, 4, f.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(st, 5, cell.kind);
            sqlite3_bind_text(st, 6, p.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(st, 7, face.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(st, 8, cell.st.size);
            sqlite3_bind_int(st, 9, cell.st.fg);
            sqlite3_bind_int(st, 10, cell.st.outline);
            sqlite3_bind_int(st, 11, cell.st.cellBg);
            sqlite3_bind_int(st, 12, cell.st.align);
            sqlite3_bind_int(st, 13, cell.st.bold ? 1 : 0);
            sqlite3_bind_int(st, 14, cell.st.italic ? 1 : 0);
            sqlite3_bind_text(st, 15, u.c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(st) != SQLITE_DONE) {
                ok = false;
                err = Utf8ToWide(sqlite3_errmsg(db));
                break;
            }
        }
        sqlite3_finalize(st);
        if (!ok) {
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            return false;
        }

        if (sqlite3_prepare_v2(db, "INSERT INTO layout(type,idx,size) VALUES(?,?,?);", -1, &st, nullptr) == SQLITE_OK) {
            for (int c = 0; c < cols; ++c) {
                if (colW[c] == kDefaultColW) continue;
                sqlite3_reset(st);
                sqlite3_bind_int(st, 1, 0);
                sqlite3_bind_int(st, 2, c);
                sqlite3_bind_int(st, 3, colW[c]);
                sqlite3_step(st);
            }
            for (int r = 0; r < rows; ++r) {
                if (rowH[r] == kDefaultRowH) continue;
                sqlite3_reset(st);
                sqlite3_bind_int(st, 1, 1);
                sqlite3_bind_int(st, 2, r);
                sqlite3_bind_int(st, 3, rowH[r]);
                sqlite3_step(st);
            }
            sqlite3_finalize(st);
        }

        if (sqlite3_prepare_v2(db, "INSERT INTO merges(r1,c1,r2,c2) VALUES(?,?,?,?);", -1, &st, nullptr) == SQLITE_OK) {
            for (auto& m : merges) {
                sqlite3_reset(st);
                sqlite3_bind_int(st, 1, m.r1);
                sqlite3_bind_int(st, 2, m.c1);
                sqlite3_bind_int(st, 3, m.r2);
                sqlite3_bind_int(st, 4, m.c2);
                sqlite3_step(st);
            }
            sqlite3_finalize(st);
        }

        sqlite3_exec(db,
            "INSERT OR REPLACE INTO meta(key,value) VALUES('app','Excel');"
            "INSERT OR REPLACE INTO meta(key,value) VALUES('version','1');"
            "INSERT OR REPLACE INTO meta(key,value) VALUES('saved_at',datetime('now','localtime'));",
            nullptr, nullptr, nullptr);
        {
            std::string rowsSql = "INSERT OR REPLACE INTO meta(key,value) VALUES('rows','" +
                                  std::to_string(rows) + "');";
            std::string colsSql = "INSERT OR REPLACE INTO meta(key,value) VALUES('cols','" +
                                  std::to_string(cols) + "');";
            sqlite3_exec(db, rowsSql.c_str(), nullptr, nullptr, nullptr);
            sqlite3_exec(db, colsSql.c_str(), nullptr, nullptr, nullptr);
        }

        if (sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &msg) != SQLITE_OK) {
            err = msg ? Utf8ToWide(msg) : L"提交失败";
            if (msg) sqlite3_free(msg);
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            return false;
        }


        filePath = path;
        sessionPath.clear();
        title = PathBaseName(path);
        dirty = false;
        lastError.clear();
        UpdateWindowTitle();
        InvalidateAll();
        return true;
    }
};

struct MediaCopyResult {
    bool ok = false;
    bool existed = false;
    std::wstring rel;
    std::wstring abs;
    std::wstring err;
};

struct FVal {
    bool isStr = false;
    double n = 0.0;
    std::wstring s;

    static FVal Num(double v) { FVal r; r.n = v; return r; }
    static FVal Str(const std::wstring& v) { FVal r; r.isStr = true; r.s = v; return r; }
    std::wstring AsString() const { return isStr ? s : NumToStr(n); }
    double AsNumber() const
    {
        if (!isStr) return n;
        double v = 0;
        return ParseNum(s, v) ? v : 0.0;
    }
    bool Truthy() const
    {
        if (isStr) {
            if (_wcsicmp(s.c_str(), L"FALSE") == 0) return false;
            if (_wcsicmp(s.c_str(), L"TRUE") == 0) return true;
            return !s.empty();
        }
        return n != 0.0;
    }
};

struct EvalActiveGuard
{
    std::unordered_set<uint64_t>& set;
    uint64_t key;
    EvalActiveGuard(std::unordered_set<uint64_t>& s, uint64_t k) : set(s), key(k) {}
    ~EvalActiveGuard() { set.erase(key); }
};

struct FormulaEval {
    Doc& doc;
    int depth;
    std::wstring src;
    size_t pos = 0;
    bool error = false;

    FormulaEval(Doc& d, int dep) : doc(d), depth(dep) {}

    void SkipWs()
    {
        while (pos < src.size() && iswspace(src[pos])) ++pos;
    }
    wchar_t Peek() { SkipWs(); return pos < src.size() ? src[pos] : 0; }
    bool Match(wchar_t ch)
    {
        SkipWs();
        if (pos < src.size() && src[pos] == ch) { ++pos; return true; }
        return false;
    }
    bool Match2(const wchar_t* s)
    {
        SkipWs();
        size_t n = wcslen(s);
        if (src.compare(pos, n, s) == 0) { pos += n; return true; }
        return false;
    }

    static bool IsCellCharStart(wchar_t c) { return (c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z') || c == L'$'; }

    bool TryParseRef(int& r, int& c)
    {
        SkipWs();
        size_t save = pos;
        if (pos < src.size() && src[pos] == L'$') ++pos;
        int col = 0, letters = 0;
        while (pos < src.size() && ((src[pos] >= L'A' && src[pos] <= L'Z') || (src[pos] >= L'a' && src[pos] <= L'z'))) {
            wchar_t ch = towupper(src[pos]);
            col = col * 26 + (ch - L'A' + 1);
            ++letters;
            ++pos;
            if (letters > 3) break;
        }
        if (letters == 0) { pos = save; return false; }
        if (pos < src.size() && src[pos] == L'$') ++pos;
        int row = 0, digits = 0;
        while (pos < src.size() && src[pos] >= L'0' && src[pos] <= L'9') {
            row = row * 10 + (src[pos] - L'0');
            ++digits;
            ++pos;
            if (digits > 7) break;
        }
        if (digits == 0 || row <= 0) { pos = save; return false; }
        r = row - 1;
        c = col - 1;
        return true;
    }

    FVal CellValue(int r, int c)
    {
        if (--g_evalBudget <= 0) return FVal::Str(L"#CIRC!");
        if (r < 0 || c < 0 || r >= doc.rows || c >= doc.cols) return FVal::Num(0);
        const Cell* cell = doc.Find(r, c);
        if (!cell) return FVal::Num(0);
        if (!cell->formula.empty()) {
            if (depth > 64) return FVal::Str(L"#CIRC!");
            uint64_t key = ((uint64_t)(uintptr_t)&doc);
            key = key * 1315423911ull ^ ((uint64_t)(uint32_t)r << 32) ^ (uint32_t)c;
            if (g_evalActive.find(key) != g_evalActive.end())
                return FVal::Str(L"#CIRC!");
            g_evalActive.insert(key);
            EvalActiveGuard guard(g_evalActive, key);
            FormulaEval sub(doc, depth + 1);
            return sub.EvalString(cell->formula);
        }
        if (!cell->text.empty()) {
            double v;
            if (ParseNum(cell->text, v)) return FVal::Num(v);
            return FVal::Str(cell->text);
        }
        if (!cell->path.empty()) return FVal::Str(cell->path);
        return FVal::Num(0);
    }

    bool CellHasContent(int r, int c)
    {
        const Cell* cell = doc.Find(r, c);
        return cell && !cell->IsEmpty();
    }

    FVal EvalString(const std::wstring& in)
    {
        src = in;
        pos = 0;
        if (!src.empty() && src[0] == L'=') pos = 1;
        error = false;
        FVal v = ParseCmp();
        SkipWs();
        if (pos < src.size()) error = true;
        if (error) return FVal::Str(L"#ERR");
        return v;
    }

    FVal ParseCmp()
    {
        FVal a = ParseConcat();
        for (;;) {
            SkipWs();
            int op = 0; // 1 =  2 <>  3 <  4 >  5 <=  6 >=
            if (pos < src.size()) {
                if (src[pos] == L'=') { op = 1; ++pos; }
                else if (src[pos] == L'<') {
                    if (pos + 1 < src.size() && src[pos + 1] == L'>') { op = 2; pos += 2; }
                    else if (pos + 1 < src.size() && src[pos + 1] == L'=') { op = 5; pos += 2; }
                    else { op = 3; ++pos; }
                } else if (src[pos] == L'>') {
                    if (pos + 1 < src.size() && src[pos + 1] == L'=') { op = 6; pos += 2; }
                    else { op = 4; ++pos; }
                }
            }
            if (!op) break;
            FVal b = ParseConcat();
            bool res = false;
            if (!a.isStr && !b.isStr) {
                double x = a.n, y = b.n;
                switch (op) {
                case 1: res = x == y; break;
                case 2: res = x != y; break;
                case 3: res = x < y; break;
                case 4: res = x > y; break;
                case 5: res = x <= y; break;
                case 6: res = x >= y; break;
                }
            } else {
                std::wstring x = a.AsString(), y = b.AsString();
                int cmp = _wcsicmp(x.c_str(), y.c_str());
                switch (op) {
                case 1: res = cmp == 0; break;
                case 2: res = cmp != 0; break;
                case 3: res = cmp < 0; break;
                case 4: res = cmp > 0; break;
                case 5: res = cmp <= 0; break;
                case 6: res = cmp >= 0; break;
                }
            }
            a = FVal::Num(res ? 1.0 : 0.0);
        }
        return a;
    }

    FVal ParseConcat()
    {
        FVal a = ParseAdd();
        while (Peek() == L'&') {
            ++pos;
            FVal b = ParseAdd();
            a = FVal::Str(a.AsString() + b.AsString());
        }
        return a;
    }

    FVal ParseAdd()
    {
        FVal a = ParseMul();
        for (;;) {
            wchar_t ch = Peek();
            if (ch != L'+' && ch != L'-') break;
            ++pos;
            FVal b = ParseMul();
            if (ch == L'+') a = FVal::Num(a.AsNumber() + b.AsNumber());
            else a = FVal::Num(a.AsNumber() - b.AsNumber());
        }
        return a;
    }

    FVal ParseMul()
    {
        FVal a = ParseUnary();
        for (;;) {
            wchar_t ch = Peek();
            if (ch != L'*' && ch != L'/') break;
            ++pos;
            FVal b = ParseUnary();
            if (ch == L'*') {
                a = FVal::Num(a.AsNumber() * b.AsNumber());
            } else {
                double den = b.AsNumber();
                if (den == 0.0) return FVal::Str(L"#DIV/0!");
                a = FVal::Num(a.AsNumber() / den);
            }
        }
        return a;
    }

    FVal ParseUnary()
    {
        wchar_t ch = Peek();
        if (ch == L'+') { ++pos; return ParseUnary(); }
        if (ch == L'-') { ++pos; FVal v = ParseUnary(); return FVal::Num(-v.AsNumber()); }
        return ParsePrimary();
    }

    struct Arg {
        bool isRange = false;
        FVal val;
        int r1 = 0, c1 = 0, r2 = 0, c2 = 0;
    };

    Arg ParseArg()
    {
        Arg a;
        SkipWs();
        size_t save = pos;
        int r1, c1, r2, c2;
        if (TryParseRef(r1, c1)) {
            SkipWs();
            if (pos < src.size() && src[pos] == L':') {
                ++pos;
                if (TryParseRef(r2, c2)) {
                    a.isRange = true;
                    a.r1 = std::min(r1, r2);
                    a.c1 = std::min(c1, c2);
                    a.r2 = std::max(r1, r2);
                    a.c2 = std::max(c1, c2);
                    return a;
                }
            }
        }
        pos = save;
        a.val = ParseCmp();
        return a;
    }

    double RangeSum(const Arg& a)
    {
        if (!a.isRange) return a.val.AsNumber();
        double sum = 0;
        for (int r = a.r1; r <= a.r2; ++r) {
            for (int c = a.c1; c <= a.c2; ++c) {
                if (!CellHasContent(r, c)) continue;
                FVal v = CellValue(r, c);
                if (!v.isStr) sum += v.n;
            }
        }
        return sum;
    }

    int RangeCount(const Arg& a)
    {
        if (!a.isRange) {
            if (a.val.isStr) return 0;
            return 1;
        }
        int n = 0;
        for (int r = a.r1; r <= a.r2; ++r) {
            for (int c = a.c1; c <= a.c2; ++c) {
                if (!CellHasContent(r, c)) continue;
                FVal v = CellValue(r, c);
                if (!v.isStr) ++n;
            }
        }
        return n;
    }

    bool RangeMinMax(const Arg& a, double& out, bool wantMin, bool& found)
    {
        if (!a.isRange) {
            if (a.val.isStr) return false;
            out = a.val.n;
            found = true;
            return true;
        }
        for (int r = a.r1; r <= a.r2; ++r) {
            for (int c = a.c1; c <= a.c2; ++c) {
                if (!CellHasContent(r, c)) continue;
                FVal v = CellValue(r, c);
                if (v.isStr) continue;
                if (!found) { out = v.n; found = true; }
                else out = wantMin ? std::min(out, v.n) : std::max(out, v.n);
            }
        }
        return true;
    }

    FVal ParseFunction(const std::wstring& name)
    {
        if (!Match(L'(')) return FVal::Str(L"#ERR");
        std::wstring fn = name;
        for (auto& ch : fn) ch = (wchar_t)towupper(ch);

        if (fn == L"IF") {
            FVal cond = ParseCmp();
            if (!Match(L',')) return FVal::Str(L"#ERR");
            FVal a = ParseCmp();
            if (!Match(L',')) return cond.Truthy() ? a : FVal::Num(0);
            FVal b = ParseCmp();
            Match(L')');
            return cond.Truthy() ? a : b;
        }

        std::vector<Arg> args;
        SkipWs();
        if (!Match(L')')) {
            for (;;) {
                args.push_back(ParseArg());
                if (Match(L',')) continue;
                if (Match(L')')) break;
                return FVal::Str(L"#ERR");
            }
        }

        if (fn == L"SUM") {
            double sum = 0;
            for (auto& a : args) sum += RangeSum(a);
            return FVal::Num(sum);
        }
        if (fn == L"COUNT") {
            int n = 0;
            for (auto& a : args) n += RangeCount(a);
            return FVal::Num(n);
        }
        if (fn == L"AVERAGE" || fn == L"AVG") {
            double sum = 0;
            int n = 0;
            for (auto& a : args) {
                sum += RangeSum(a);
                n += RangeCount(a);
            }
            if (n == 0) return FVal::Str(L"#DIV/0!");
            return FVal::Num(sum / n);
        }
        if (fn == L"MIN" || fn == L"MAX") {
            double best = 0;
            bool found = false;
            for (auto& a : args) RangeMinMax(a, best, fn == L"MIN", found);
            return FVal::Num(found ? best : 0);
        }
        if (fn == L"ABS") {
            if (args.empty()) return FVal::Num(0);
            return FVal::Num(fabs(args[0].val.AsNumber()));
        }
        if (fn == L"ROUND") {
            if (args.empty()) return FVal::Num(0);
            int digits = args.size() > 1 ? (int)args[1].val.AsNumber() : 0;
            double mul = pow(10.0, digits);
            double v = args[0].val.AsNumber();
            return FVal::Num(floor(v * mul + 0.5) / mul);
        }
        if (fn == L"AND" || fn == L"OR") {
            bool first = true;
            bool acc = fn == L"AND";
            for (auto& a : args) {
                bool t = a.isRange ? (RangeCount(a) > 0) : a.val.Truthy();
                if (first) { acc = t; first = false; }
                else acc = (fn == L"AND") ? (acc && t) : (acc || t);
            }
            return FVal::Num(acc ? 1 : 0);
        }
        if (fn == L"NOT") {
            if (args.empty()) return FVal::Num(0);
            return FVal::Num(args[0].val.Truthy() ? 0 : 1);
        }
        return FVal::Str(L"#NAME?");
    }

    FVal ParsePrimary()
    {
        SkipWs();
        if (pos >= src.size()) { error = true; return FVal::Num(0); }
        wchar_t ch = src[pos];

        if (ch == L'(') {
            ++pos;
            FVal v = ParseCmp();
            Match(L')');
            return v;
        }
        if (ch == L'"') {
            ++pos;
            std::wstring out;
            while (pos < src.size()) {
                wchar_t c = src[pos++];
                if (c == L'"') {
                    if (pos < src.size() && src[pos] == L'"') { out.push_back(L'"'); ++pos; continue; }
                    break;
                }
                out.push_back(c);
            }
            return FVal::Str(out);
        }
        if ((ch >= L'0' && ch <= L'9') || ch == L'.') {
            wchar_t* end = nullptr;
            const wchar_t* start = src.c_str() + pos;
            double v = wcstod(start, &end);
            if (end == start) { error = true; return FVal::Num(0); }
            pos += (size_t)(end - start);
            return FVal::Num(v);
        }
        if (IsCellCharStart(ch)) {
            size_t save = pos;
            int r, c;
            if (TryParseRef(r, c)) {
                SkipWs();
                if (pos < src.size() && src[pos] == L'(') {
                    // shouldn't happen
                }
                return CellValue(r, c);
            }
            pos = save;
            // function name
            std::wstring name;
            while (pos < src.size() && (iswalnum(src[pos]) || src[pos] == L'_' || src[pos] == L'.')) {
                name.push_back(src[pos]);
                ++pos;
            }
            SkipWs();
            if (pos < src.size() && src[pos] == L'(')
                return ParseFunction(name);
            error = true;
            return FVal::Str(L"#NAME?");
        }
        error = true;
        return FVal::Str(L"#ERR");
    }
};

struct ImageFrame {
    HBITMAP bmp = nullptr;
    int w = 0, h = 0;
    int delay = 100;
};

struct ImageData {
    std::vector<ImageFrame> frames;
    int totalMs = 0;
    ULONGLONG startTick = 0;

    ~ImageData()
    {
        for (auto& f : frames) if (f.bmp) DeleteObject(f.bmp);
    }

    bool Animated() const { return frames.size() > 1; }

    int FrameIndex(ULONGLONG now) const
    {
        if (frames.size() <= 1 || totalMs <= 0) return 0;
        int t = (int)((now - startTick) % (ULONGLONG)totalMs);
        int acc = 0;
        for (size_t i = 0; i < frames.size(); ++i) {
            acc += frames[i].delay;
            if (t < acc) return (int)i;
        }
        return (int)frames.size() - 1;
    }
};

struct UiMods
{
    // Button is event data, not a binding: the object under the pointer decides
    // what (if anything) that button means.
    int button = 1;
    bool shift = false;
    bool ctrl = false;
    bool alt = false;
};

// ---------------------------------------------------------------------------
//  Declarative paint contract.
//
//  The render layer knows exactly one thing: how to execute a flat list of
//  paint primitives, and how to present the result without exposing a
//  half-drawn frame. Every object declares WHAT it wants drawn in BuildSpec()
//  during a frame commit. No declaration site can be reached from the render
//  path, so no drawing code can read a document, a theme, a clock, a font
//  cache or any other state. Adding a control, a cell type or a popup never
//  touches the renderer.
// ---------------------------------------------------------------------------
enum class PaintOp : unsigned char
{
    FillRect, HLine, VLine, FrameRect, Text, OutlinedText,
    PushClip, PopClip, Icon, Image, Bitmap, CloseX, Triangle, Lock, CheckMark, Chevron
};

// One primitive. Every geometric and stylistic decision has already been
// made by the owning object's BuildSpec(); this is a value, not a query.
struct PaintPrim
{
    PaintOp op = PaintOp::FillRect;
    RECT rc{};                  // primary geometry
    RECT rc2{};                 // secondary geometry (icon box, maximise boxes)
    COLORREF color = 0;         // primary colour
    COLORREF color2 = 0;        // outline / secondary colour
    std::wstring text;          // text, or image path for PaintOp::Image
    HFONT font = nullptr;
    HICON icon = nullptr;
    HBITMAP bmp = nullptr;      // PaintOp::Bitmap source (p1/p2 = source w/h)
    UINT flags = 0;
    int w = 1;                  // line width / glyph radius
    int p1 = 0, p2 = 0, p3 = 0; // op-specific scalars (centre x/y, direction)
    int ownerR = -1, ownerC = -1;   // cell identity for ghost / animation reuse
};

struct PaintSpec
{
    std::vector<PaintPrim> prims;
    void Clear() { prims.clear(); }
    // Builder helper, used only from BuildSpec() in the logic layer.
    PaintPrim& Emit(PaintOp op, const RECT& rc, COLORREF color)
    {
        prims.emplace_back();
        PaintPrim& p = prims.back();
        p.op = op;
        p.rc = rc;
        p.color = color;
        return p;
    }
};

// The single renderer. Its signature IS the whole logic/render contract.
// clip is an optional pure-drawing filter, never a source of state.
void ExecutePaintSpec(HDC dc, const PaintSpec& spec, const RECT* clip);

// ---------------------------------------------------------------------------
//  Presentation surface.
//
//  A declaration is never executed straight into a window DC. On a scrolling
//  view the primitive-by-primitive progress is visible: the background fill
//  lands first, the cells follow, so the user sees the sheet flash through a
//  state that never existed. Every frame is therefore composed into an
//  offscreen surface and presented with exactly one BitBlt - the same operand
//  for a 20x20 hover repaint and for a full-window repaint.
//
//  This is the render layer's own business: how a declaration becomes pixels.
//  It reads no state; the area is a pure-drawing rectangle, like clip.
// ---------------------------------------------------------------------------
struct PaintSurface
{
    HDC dc = nullptr;
    HBITMAP bmp = nullptr;
    HGDIOBJ old = nullptr;
    RECT area{};
    int w = 0, h = 0;

    ~PaintSurface() { Release(); }

    // Returns the composition DC with its origin at area.left/top and its clip
    // confined to area, or nullptr when there is nothing to present.
    HDC Begin(HDC target, const RECT& a);
    void End(HDC target);
    void Release();
};

// Compose one declaration into the surface, then present it with one blit.
void PresentSpec(HDC target, PaintSurface& surface, const RECT& area,
                 const PaintSpec& spec);

// Neutral input carried from a presentation host into its object. A WndProc
// only packages device data; it never decides what the data means.
enum class PopupInputKind { Move, Press, Wheel };
struct PopupInput
{
    PopupInputKind kind = PopupInputKind::Move;
    POINT pt{};
    int delta = 0;
};

struct UiObject
{
    UiId id = UiId::None;        // stable identity, assigned once by the root
    RECT rect{};                 // assigned by the parent's layout pass
    PaintSpec spec;              // declared in BuildSpec(), executed verbatim

    virtual ~UiObject() = default;
    // Nature of the object, never a runtime state: a hosted object presents
    // itself in its own HWND, so the root never lays it out, commits it,
    // paints it on the main DC or hit-tests it against main-window geometry.
    // This must not depend on whether the host currently exists - a closed
    // popup is still a hosted object, otherwise the root would render its
    // stale declaration into the window.
    virtual bool Hosted() const { return false; }
    virtual void Layout(const RECT& r) { rect = r; }
    // Logic layer: declare the complete drawing for this frame. Called once
    // from the frame commit, before any painting, with a measuring DC.
    virtual void BuildSpec(HDC) {}
    virtual bool HitTest(POINT pt) const { return PtInRect(&rect, pt) != 0; }
    virtual void MouseDown(POINT, const UiMods&) {}
    virtual void MouseMove(POINT, const UiMods&) {}
    virtual void MouseUp(POINT, const UiMods&) {}
    virtual void MouseLeave() {}
    virtual void DblClick(POINT, const UiMods&) {}
    virtual void ContextMenu(POINT, const UiMods&) {}
    virtual bool Wheel(int, POINT, const UiMods&) { return false; }
    virtual void DropFiles(const std::vector<std::wstring>&, POINT) {}
    virtual bool KeyDown(WPARAM, const UiMods&) { return false; }
    virtual bool Char(wchar_t, const UiMods&) { return false; }
    virtual bool Cursor(POINT) { return false; }
    virtual void CaptureLost() {}
};

// A popup is a UiObject whose HWND is only a presentation host. The object
// owns all state, declares its own spec and decides its own actions; the host
// receives nothing but packaged input and a request to execute the spec.
struct PopupObject : UiObject
{
    HWND host = nullptr;
    PaintSurface surface;        // its own presentation surface

    bool Hosted() const override { return true; }
    bool Open() const { return host != nullptr; }

    // The one state->spec conversion point for a hosted object. Called by the
    // logic layer when the popup opens or its state changes - never by WM_PAINT.
    void Commit()
    {
        if (!host) return;
        HDC dc = GetDC(host);
        BuildSpec(dc);
        ReleaseDC(host, dc);
        InvalidateRect(host, nullptr, FALSE);
    }

    // Pure execution of the declaration: compose offscreen, present once.
    // The target area is the host's own client rectangle - a presentation
    // fact of the window being painted, not a UI state.
    void PaintHost(HDC dc)
    {
        RECT c = {};
        if (host) GetClientRect(host, &c);
        PresentSpec(dc, surface, c, spec);
    }

    void DestroyHost()
    {
        if (host) {
            HWND h = host;
            host = nullptr;        // clear first: WM_NCDESTROY cannot re-enter
            DestroyWindow(h);
        }
        spec.Clear();
    }

    virtual void Input(const PopupInput&) {}
};

struct MenuItem {
    int id = 0;
    std::wstring text;
    std::wstring shortcut;
    int kind = MK_NORMAL;
    bool checked = false;
    bool enabled = true;
    std::vector<MenuItem> sub;
};

struct MenuPopup : PopupObject {
    std::vector<MenuItem> items;
    int hover = -1;
    int subIndex = -1;
    int level = 0;
    int topIndex = -1;          // which top-level menu is open (-1 = none)
    int itemH = 0, sepH = 0, padY = 0, padX = 0, width = 0, height = 0;
    int checkW = 0, arrowW = 0;
    std::unique_ptr<MenuPopup> sub;

    MenuPopup() { id = UiId::PopupMenu; }

    static void RegisterClass()
    {
        static bool done = false;
        if (done) return;
        WNDCLASSEXW wc = { sizeof(wc) };
        wc.style = CS_DROPSHADOW;
        wc.lpfnWndProc = MenuWndProc;
        wc.hInstance = g_inst;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"ExcelMenu";
        RegisterClassExW(&wc);
        done = true;
    }

    // Presentation host only. A WndProc packages device data and nothing else:
    // no hit decision, no command, no popup teardown, no document access.
    static LRESULT CALLBACK MenuWndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
    {
        MenuPopup* self = (MenuPopup*)GetWindowLongPtrW(h, GWLP_USERDATA);
        switch (msg) {
        case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
        case WM_ERASEBKGND:   return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            if (self) self->PaintHost(dc);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_MOUSEMOVE:
            if (self) self->Input({ PopupInputKind::Move,
                                    { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) }, 0 });
            return 0;
        case WM_LBUTTONDOWN:
            if (self) self->Input({ PopupInputKind::Press,
                                    { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) }, 0 });
            return 0;
        case WM_RBUTTONDOWN: return 0;
        case WM_NCDESTROY:
            SetWindowLongPtrW(h, GWLP_USERDATA, 0);
            return 0;
        }
        return DefWindowProcW(h, msg, wp, lp);
    }

    int ItemTop(int idx) const
    {
        int y = padY;
        for (int i = 0; i < idx; ++i)
            y += (items[i].kind == MK_SEP ? sepH : itemH);
        return y;
    }
    int ItemHeight(int idx) const { return items[idx].kind == MK_SEP ? sepH : itemH; }

    int ItemAt(int x, int y) const
    {
        if (x < 0 || x >= width) return -1;
        int cy = padY;
        for (size_t i = 0; i < items.size(); ++i) {
            int h = items[i].kind == MK_SEP ? sepH : itemH;
            if (y >= cy && y < cy + h) return items[i].kind == MK_SEP ? -1 : (int)i;
            cy += h;
        }
        return -1;
    }

    void Measure(HDC dc)
    {
        itemH = DpiScale(28);
        sepH = DpiScale(9);
        padY = DpiScale(6);
        padX = DpiScale(14);
        checkW = DpiScale(20);
        arrowW = DpiScale(22);
        HFONT font = UiFont(10);
        int maxText = 0;
        for (auto& it : items) {
            if (it.kind == MK_SEP) continue;
            SIZE a = MeasureText(dc, it.text, font);
            SIZE b = it.shortcut.empty() ? SIZE{ 0, 0 } : MeasureText(dc, it.shortcut, UiFont(9));
            int need = a.cx + (b.cx ? b.cx + DpiScale(36) : 0);
            maxText = std::max(maxText, need);
        }
        width = std::max(DpiScale(190), maxText + padX * 2 + checkW + arrowW);
        height = padY * 2;
        for (size_t i = 0; i < items.size(); ++i) height += ItemHeight((int)i);
    }

    void Create(HWND owner, const std::vector<MenuItem>& it, int lvl)
    {
        RegisterClass();
        items = it;
        level = lvl;
        HDC dc = GetDC(nullptr);
        Measure(dc);
        ReleaseDC(nullptr, dc);
        host = CreateWindowExW(WS_EX_TOOLWINDOW, L"ExcelMenu", L"", WS_POPUP,
                               0, 0, width, height, owner, nullptr, g_inst, nullptr);
        SetWindowLongPtrW(host, GWLP_USERDATA, (LONG_PTR)this);
    }

    void Position(int x, int y)
    {
        MONITORINFO mi = { sizeof(mi) };
        GetMonitorInfoW(MonitorFromPoint({ x, y }, MONITOR_DEFAULTTONEAREST), &mi);
        RECT wa = mi.rcWork;
        if (x + width > wa.right) x = wa.right - width;
        if (y + height > wa.bottom) y = wa.bottom - height;
        if (x < wa.left) x = wa.left;
        if (y < wa.top) y = wa.top;
        SetWindowPos(host, HWND_TOP, x, y, width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    void BuildSpec(HDC) override
    {
        PaintSpec& s = spec;
        s.Clear();
        RECT rc = { 0, 0, width, height };
        s.Emit(PaintOp::FillRect, rc, g_th.panel);
        s.Emit(PaintOp::FrameRect, rc, g_th.borderStrong).w = 1;

        HFONT font = UiFont(10);
        HFONT smallFont = UiFont(9);
        int cy = padY;
        for (size_t i = 0; i < items.size(); ++i) {
            const MenuItem& it = items[i];
            int h = ItemHeight((int)i);
            RECT row = { 1, cy, width - 1, cy + h };
            if (it.kind == MK_SEP) {
                RECT ln = { padX, cy + h / 2, width - padX, cy + h / 2 };
                s.Emit(PaintOp::HLine, ln, g_th.border).w = DpiScale(1);
                cy += h;
                continue;
            }
            if ((int)i == hover && it.enabled)
                s.Emit(PaintOp::FillRect, row, g_th.hover);
            if (it.kind == MK_CHECK && it.checked) {
                auto& p = s.Emit(PaintOp::CheckMark, RECT{}, g_th.accent);
                p.p1 = row.left + padX + DpiScale(6);
                p.p2 = row.top + h / 2;
            }
            RECT tr = { row.left + padX + checkW, row.top,
                        row.right - padX - arrowW, row.bottom };
            auto& tp = s.Emit(PaintOp::Text, tr, it.enabled ? g_th.text : g_th.faint);
            tp.text = it.text;
            tp.font = font;
            tp.flags = DT_SINGLELINE | DT_VCENTER | DT_LEFT;
            if (!it.shortcut.empty()) {
                RECT sr = { row.left + width - padX - arrowW - DpiScale(140), row.top,
                            row.right - padX - arrowW, row.bottom };
                auto& sp = s.Emit(PaintOp::Text, sr, it.enabled ? g_th.dim : g_th.faint);
                sp.text = it.shortcut;
                sp.font = smallFont;
                sp.flags = DT_SINGLELINE | DT_VCENTER | DT_RIGHT;
            }
            if (it.kind == MK_SUB) {
                auto& p = s.Emit(PaintOp::Chevron, RECT{}, it.enabled ? g_th.dim : g_th.faint);
                p.p1 = row.right - DpiScale(14);
                p.p2 = row.top + h / 2;
                p.w = DpiScale(4);
                p.p3 = 1;
            }
            cy += h;
        }
    }

    // Logic-layer operations. Defined in p5_popup.cpp, where the root object
    // (the single registry and the single action bus consumer) is complete.
    void OpenSub(int idx);
    void CloseSub();
    void DestroyAll();
    void Close();
    void KeyMove(int delta);
    bool KeyExecute();
    void Input(const PopupInput& in) override;
};

struct ListPopup : PopupObject {
    std::vector<std::wstring> items;
    int sel = -1, hover = -1, top = 0;
    int itemH = 0, visible = 14, width = 0, height = 0, pad = 0;
    int listKind = 0;           // neutral data: 0 = font, 1 = point size

    ListPopup() { id = UiId::PopupList; }

    static void RegisterClass()
    {
        static bool done = false;
        if (done) return;
        WNDCLASSEXW wc = { sizeof(wc) };
        wc.style = CS_DROPSHADOW;
        wc.lpfnWndProc = ListWndProc;
        wc.hInstance = g_inst;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"ExcelList";
        RegisterClassExW(&wc);
        done = true;
    }

    // The host packages input; the object decides. No callback is invoked here
    // and no window is destroyed from inside its own WndProc.
    static LRESULT CALLBACK ListWndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
    {
        ListPopup* self = (ListPopup*)GetWindowLongPtrW(h, GWLP_USERDATA);
        switch (msg) {
        case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
        case WM_ERASEBKGND:   return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            if (self) self->PaintHost(dc);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_MOUSEMOVE:
            if (self) self->Input({ PopupInputKind::Move,
                                    { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) }, 0 });
            return 0;
        case WM_LBUTTONDOWN:
            if (self) self->Input({ PopupInputKind::Press,
                                    { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) }, 0 });
            return 0;
        case WM_MOUSEWHEEL:
            if (self) self->Input({ PopupInputKind::Wheel,
                                    { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) },
                                    GET_WHEEL_DELTA_WPARAM(wp) });
            return 0;
        case WM_RBUTTONDOWN: return 0;
        case WM_NCDESTROY:
            SetWindowLongPtrW(h, GWLP_USERDATA, 0);
            return 0;
        }
        return DefWindowProcW(h, msg, wp, lp);
    }

    int Hit(int x, int y) const
    {
        if (x < 0 || x >= width) return -1;
        if (y < pad) return -1;
        int i = top + (y - pad) / itemH;
        if (i < top || i >= (int)items.size()) return -1;
        if (i >= top + visible) return -1;
        return i;
    }

    void BuildSpec(HDC) override
    {
        PaintSpec& s = spec;
        s.Clear();
        RECT rc = { 0, 0, width, height };
        s.Emit(PaintOp::FillRect, rc, g_th.panel);
        s.Emit(PaintOp::FrameRect, rc, g_th.borderStrong).w = 1;
        HFONT font = UiFont(10);
        for (int row = 0; row < visible; ++row) {
            int i = top + row;
            if (i >= (int)items.size()) break;
            RECT r = { 1, pad + row * itemH, width - 1, pad + (row + 1) * itemH };
            if (i == sel) s.Emit(PaintOp::FillRect, r, g_th.selFill);
            if (i == hover) s.Emit(PaintOp::FillRect, r, g_th.hover);
            RECT tr = { r.left + DpiScale(12), r.top, r.right - DpiScale(8), r.bottom };
            auto& p = s.Emit(PaintOp::Text, tr, i == sel ? g_th.accent : g_th.text);
            p.text = items[i];
            p.font = font;
            p.flags = DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS;
        }
        if ((int)items.size() > visible) {
            int trackH = height - pad * 2;
            int thumbH = std::max(DpiScale(24), trackH * visible / (int)items.size());
            int maxTop = (int)items.size() - visible;
            int thumbY = pad + (int)((trackH - thumbH) * (maxTop ? top / (double)maxTop : 0.0));
            int x = width - DpiScale(7);
            s.Emit(PaintOp::FillRect, RECT{ x, pad, x + DpiScale(3), height - pad }, g_th.panel2);
            s.Emit(PaintOp::FillRect,
                   RECT{ x, thumbY, x + DpiScale(3), thumbY + thumbH }, g_th.borderStrong);
        }
    }

    void Position(RECT anchor)
    {
        MONITORINFO mi = { sizeof(mi) };
        GetMonitorInfoW(MonitorFromRect(&anchor, MONITOR_DEFAULTTONEAREST), &mi);
        RECT wa = mi.rcWork;
        int x = anchor.left;
        int y = anchor.bottom + DpiScale(2);
        if (y + height > wa.bottom) y = anchor.top - height - DpiScale(2);
        if (x + width > wa.right) x = wa.right - width;
        if (x < wa.left) x = wa.left;
        if (y < wa.top) y = wa.top;
        SetWindowPos(host, HWND_TOP, x, y, width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    void Input(const PopupInput& in) override;   // p5_popup.cpp
};

struct PalettePopup : PopupObject {
    int target = 0;           // 0 fg, 1 text bg, 2 cell bg
    int hover = -2;           // -1 = special row, >=0 = swatch
    int pad = 0, sw = 0, gap = 0, specialH = 0, width = 0, height = 0;

    PalettePopup() { id = UiId::PopupPalette; }

    static void RegisterClass()
    {
        static bool done = false;
        if (done) return;
        WNDCLASSEXW wc = { sizeof(wc) };
        wc.style = CS_DROPSHADOW;
        wc.lpfnWndProc = PaletteWndProc;
        wc.hInstance = g_inst;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"ExcelPalette";
        RegisterClassExW(&wc);
        done = true;
    }

    static LRESULT CALLBACK PaletteWndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
    {
        PalettePopup* self = (PalettePopup*)GetWindowLongPtrW(h, GWLP_USERDATA);
        switch (msg) {
        case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
        case WM_ERASEBKGND:   return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            if (self) self->PaintHost(dc);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_MOUSEMOVE:
            if (self) self->Input({ PopupInputKind::Move,
                                    { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) }, 0 });
            return 0;
        case WM_LBUTTONDOWN:
            if (self) self->Input({ PopupInputKind::Press,
                                    { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) }, 0 });
            return 0;
        case WM_NCDESTROY:
            SetWindowLongPtrW(h, GWLP_USERDATA, 0);
            return 0;
        }
        return DefWindowProcW(h, msg, wp, lp);
    }

    RECT SpecialRect() const { return { pad, pad, width - pad, pad + specialH }; }
    RECT SwatchRect(int idx) const
    {
        int row = idx / kPaletteCols;
        int col = idx % kPaletteCols;
        int x = pad + col * (sw + gap);
        int y = pad + specialH + gap + row * (sw + gap);
        return { x, y, x + sw, y + sw };
    }

    int Hit(int x, int y) const
    {
        RECT sp = SpecialRect();
        if (x >= sp.left && x < sp.right && y >= sp.top && y < sp.bottom) return -1;
        for (int i = 0; i < kPaletteRows * kPaletteCols; ++i) {
            RECT r = SwatchRect(i);
            if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) return i;
        }
        return -2;
    }

    void BuildSpec(HDC) override
    {
        PaintSpec& s = spec;
        s.Clear();
        RECT rc = { 0, 0, width, height };
        s.Emit(PaintOp::FillRect, rc, g_th.panel);
        s.Emit(PaintOp::FrameRect, rc, g_th.borderStrong).w = 1;

        RECT sp = SpecialRect();
        if (hover == -1) s.Emit(PaintOp::FillRect, sp, g_th.hover);
        RECT swBox = { sp.left, sp.top, sp.left + sw, sp.bottom };
        s.Emit(PaintOp::FillRect, swBox, g_th.panel2);
        s.Emit(PaintOp::FrameRect, swBox, g_th.border).w = 1;
        RECT labelRc = { sp.left + sw + DpiScale(10), sp.top,
                         sp.right - DpiScale(6), sp.bottom };
        auto& lp = s.Emit(PaintOp::Text, labelRc, g_th.text);
        lp.text = (target == 0) ? L"自动" : (target == 1 ? L"无描边" : L"无填充");
        lp.font = UiFont(9);
        lp.flags = DT_SINGLELINE | DT_VCENTER | DT_LEFT;

        for (int i = 0; i < kPaletteRows * kPaletteCols; ++i) {
            RECT r = SwatchRect(i);
            s.Emit(PaintOp::FillRect, r, PaletteColors()[i]);
            s.Emit(PaintOp::FrameRect, r, g_th.border).w = 1;
            if (i == hover) s.Emit(PaintOp::FrameRect, r, g_th.accent).w = DpiScale(2);
        }
        COLORREF cur = (target == 0) ? g_btnFg
                     : (target == 1) ? g_btnOutline : g_btnCellBg;
        if (cur == CLR_NONE) {
            RECT ln = { sp.left, sp.top + sw / 2, sp.left + sw, sp.top + sw / 2 };
            s.Emit(PaintOp::HLine, ln, g_th.danger).w = DpiScale(1);
        }
    }

    void Position(RECT anchor)
    {
        MONITORINFO mi = { sizeof(mi) };
        GetMonitorInfoW(MonitorFromRect(&anchor, MONITOR_DEFAULTTONEAREST), &mi);
        RECT wa = mi.rcWork;
        int x = anchor.left;
        int y = anchor.bottom + DpiScale(2);
        if (y + height > wa.bottom) y = anchor.top - height - DpiScale(2);
        if (x + width > wa.right) x = wa.right - width;
        if (x < wa.left) x = wa.left;
        if (y < wa.top) y = wa.top;
        SetWindowPos(host, HWND_TOP, x, y, width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    void Input(const PopupInput& in) override;   // p5_popup.cpp
};

struct AnimCell { Doc* doc; int r, c; RECT rc; };

struct GridGesture
{
    GestureKind kind = GestureKind::None;

    POINT press{};           // press point, client coordinates

    // selection
    bool fromMerge = false;
    MergeRange mergeRange{};
    int selTargetR = 0, selTargetC = 0;

    // column / row resize
    int resizeIndex = -1;
    int resizeStartBase = 0;
    int resizeNewBase = 0;
    int resizeStartMouse = 0;

    // whole grid resize
    POINT gridResizeStart{};
    int gridStartRows = 0, gridStartCols = 0;
    int gridTargetRows = 0, gridTargetCols = 0;

    // drag-move: everything the move needs is captured at press time, so the
    // update and commit steps never read loose globals.
    struct MoveState
    {
        int pressRow = 0, pressCol = 0;
        int selR1 = 0, selC1 = 0, selH = 1, selW = 1;
        int targetRow = -1, targetCol = -1;
        std::vector<MergeRange> blocks;

        void Begin(Doc& d, POINT pt);
        void Update(Doc& d, POINT pt);
        bool Apply(Doc& d);
        void Clear();
    };
    MoveState move;

    bool Active() const { return kind != GestureKind::None; }
    bool Is(GestureKind k) const { return kind == k; }

    void BeginMove(Doc& d, POINT pt);
    void UpdateMove(Doc& d, POINT pt);
    bool ApplyMove(Doc& d);

    void Reset()
    {
        kind = GestureKind::None;
        fromMerge = false;
        resizeIndex = -1;
        move.Clear();
    }
};

struct PanGesture
{
    bool active = false;
    POINT startClient{};
    int startScrollX = 0, startScrollY = 0;

    void Begin(Doc* d, POINT pt)
    {
        active = true;
        startClient = pt;
        startScrollX = d ? d->scrollX : 0;
        startScrollY = d ? d->scrollY : 0;
    }
    // Pure read: the view owns applying the requested viewport position.
    void Target(int& x, int& y, POINT pt) const
    {
        x = startScrollX - (pt.x - startClient.x);
        y = startScrollY - (pt.y - startClient.y);
    }
    void End() { active = false; }
    void Reset() { active = false; }
};

struct ScrollBar
{
    enum Axis { Vertical, Horizontal };

    // ---- an ordinary scrollbar ----------------------------------------------
    // Length is how much of the sheet a screen shows; place is where the view
    // is: the thumb touches the top of the track at the first row and the
    // bottom at the last one, and while the hand drags it stays under the
    // pointer. Nothing flexes, nothing eases, nothing remembers a gesture after
    // the button is up - a bar that moves on its own is harder to aim than one
    // that simply follows.
    //
    // The only thing the bar ever says is WHERE in the sheet the view should
    // be. How long the sheet is remains the view's business.
    static constexpr int kMinThumb = 50;   // still grabbable when the sheet is huge

    Axis axis = Vertical;
    RECT track{}, thumb{};
    int contentPx = 1, bodyPx = 1, offset = 0;

    bool dragging = false;
    bool synced = false; // a bar only accepts thumb hits after the logic layer fed it

    // Gesture state. The scale is captured on press and held for the whole
    // gesture: drawing uses travel / max, reading the pointer back uses its
    // reciprocal, and both must be the same pair or the thumb drifts away from
    // the hand. Values are captured rather than read live because the sheet may
    // still be growing under the gesture.
    int pressPos = 0;      // pointer position when the button went down
    int pressOffset = 0;   // view origin at that moment
    int pressMax = 0;      // scrollable length at that moment
    int pressTravel = 0;   // thumb travel at that moment

    // A scrollbar says where in the sheet the view should be; it does not
    // decide how big the sheet is. Its request is a plain one, the same kind
    // the wheel and the keys emit.

    void SetTrack(const RECT& r)
    {
        track = r;
        // Layout() runs on every pointer move, so the track is re-set often.
        // Clobbering the thumb here used to make ThumbLen() == TrackLen();
        // re-project the existing scroll state onto the new track instead.
        if (synced) Sync(contentPx, bodyPx, offset);
        else        thumb = r;
    }
    bool HitTrack(POINT pt) const { return PtInRect(&track, pt) != 0; }
    bool HitThumb(POINT pt) const { return synced && PtInRect(&thumb, pt) != 0; }

    int MaxOffset() const { return std::max(0, contentPx - bodyPx); }
    int TrackLen() const { return axis == Vertical ? track.bottom - track.top : track.right - track.left; }
    int ThumbLen() const { return axis == Vertical ? thumb.bottom - thumb.top : thumb.right - thumb.left; }
    int TrackStart() const { return axis == Vertical ? track.top : track.left; }
    int TrackEnd() const { return axis == Vertical ? track.bottom : track.right; }
    int PosOf(POINT pt) const { return axis == Vertical ? pt.y : pt.x; }

    // The free room the thumb has, from the length it currently shows: track
    // minus thumb. One scale, used both ways - drawing the thumb from the
    // origin, and reading the origin back from the pointer.
    int Travel() const { return std::max(0, TrackLen() - ThumbLen()); }

    // The only display rule, and it is the ordinary one:
    //   length = track * screen / sheet     (floored so it stays grabbable)
    //   place  = start + travel * origin / scrollable length
    // Both vary with the sheet, which is what a scrollbar is for: the eye reads
    // "how much of this is left", and one glance at the thumb end says where
    // the view is.
    void Sync(int content, int body, int off)
    {
        contentPx = std::max(1, content);
        bodyPx = std::max(1, body);
        offset = off;
        synced = true;
        int trackLen = std::max(1, TrackLen());
        int start = TrackStart();

        int len;
        if (MaxOffset() <= 0) {
            len = trackLen;                    // the sheet fits: the bar is the page
        } else {
            len = (int)((long long)trackLen * bodyPx / contentPx);
            int floorPx = DpiScale(kMinThumb);
            if (len < floorPx) len = floorPx;
            if (len > trackLen) len = trackLen;
        }

        int travel = trackLen - len;
        int pos = start;
        if (MaxOffset() > 0 && travel > 0)
            pos = start + (int)((long long)travel * offset / MaxOffset());
        if (pos < start) pos = start;
        if (pos > start + travel) pos = start + travel;

        if (axis == Vertical)
            thumb = { track.left + 2, pos, track.right - 2, pos + len };
        else
            thumb = { pos, track.top + 2, pos + len, track.bottom - 2 };
    }

    int ThumbPos() const { return axis == Vertical ? thumb.top : thumb.left; }

    void BeginDrag(POINT pt)
    {
        dragging = true;
        pressPos = PosOf(pt);
        pressOffset = offset;
        pressMax = MaxOffset();
        pressTravel = Travel();
    }

    void EndDrag() { dragging = false; }

    // Pointer -> view origin: the inverse of Sync, on the scale captured at
    // press. Inside the sheet this puts the thumb exactly under the hand.
    // Past the end it keeps asking instead of freezing - a request beyond the
    // trailing edge is the same statement the wheel and the keys make, and the
    // capacity rule answers all of them alike. The thumb is DRAWN clamped to the
    // track, so nothing about a growing sheet can move it off the ends.
    int DragTarget(POINT pt) const
    {
        if (!dragging) return offset;
        if (pressMax <= 0 || pressTravel <= 0) return offset;
        long long d = (long long)PosOf(pt) - pressPos;
        long long t = (long long)pressOffset + d * pressMax / pressTravel;
        if (t < 0) t = 0;
        if (t > 0x3FFFFFFF) t = 0x3FFFFFFF;
        return (int)t;
    }

    int PageDirection(POINT pt) const { return PosOf(pt) < ThumbPos() ? -1 : 1; }

    // A page click jumps one screenful. It is a jump inside the sheet, so it
    // stops at the trailing edge like any other ordinary scrollbar.
    int PageOffset(POINT pt) const
    {
        int t = offset + PageDirection(pt) * std::max(1, bodyPx);
        if (t < 0) t = 0;
        if (t > MaxOffset()) t = MaxOffset();
        return t;
    }

    // The bar holds geometry only. GridView::BuildSpec() declares its paint;
    // there is deliberately no Paint method on a projection of state.
};

struct ToolLayout {
    RECT font = {}, size = {}, row = {}, col = {};
    RECT mode = {};
    RECT merge = {};
    RECT num = {}, unit = {};
    RECT align[3] = {};
    RECT valign[3] = {};
    RECT fg = {}, outline = {}, cellbg = {};
    RECT colorArrow[3] = {};
    RECT sum = {};
    RECT op = {};
};

struct TabLayout {
    std::vector<RECT> tab, close;
    RECT plus = {};
};

struct GridView : UiObject
{
    GridGesture gesture;
    PanGesture pan;
    ScrollBar vbar, hbar;
    int hoverRow = -1, hoverCol = -1;
    std::vector<AnimCell> animCells;

    // O(1) view signature; animation state is rebuilt only when it changes.
    struct AnimSig
    {
        const void* doc = nullptr;
        uint64_t rev = 0;
        int sx = -1, sy = -1, zoom = 0, uiZoom = 0, dpi = 0;
        int rows = 0, cols = 0, active = -1;
        int l = 0, t = 0, r = 0, b = 0;

        bool Same(const AnimSig& o) const
        {
            return doc == o.doc && rev == o.rev && sx == o.sx && sy == o.sy
                && zoom == o.zoom && uiZoom == o.uiZoom && dpi == o.dpi
                && rows == o.rows && cols == o.cols && active == o.active
                && l == o.l && t == o.t && r == o.r && b == o.b;
        }
    } animSig;

    // drag-move ghost window (owned by this view)
    HWND ghostWnd = nullptr;
    HBITMAP ghostBmp = nullptr;
    int ghostW = 0, ghostH = 0;
    POINT ghostOffset{};
    ULONGLONG ghostTick = 0;

    GridView()
    {
        vbar.axis = ScrollBar::Vertical;
        hbar.axis = ScrollBar::Horizontal;
    }

    void Layout(const RECT& r) override;
    void BuildSpec(HDC dc) override;    // the only place that reads the document
    bool NeedsAnimation() const { return !animCells.empty(); }
    bool TickAnimation();
    void SyncAnimation();          // logic layer owns animation state
    void CommitPendingEdit();      // the editor belongs to this view
    void PositionEditorControl();  // the object owns its own child control
    void DropFiles(const std::vector<std::wstring>& paths, POINT pt) override;
    void SyncBars(Doc& d);
    bool OnScrollBar(POINT pt) const;   // the view declares its own hit area
    void ScrollBarDrag(ScrollBar& bar, Doc& d, POINT pt);
    bool ScrollBarUp();
    void ApplyViewRequest(Doc& d, const ViewRequest& req);  // single entry for every device
    void ScrollLive(Doc& d, int nx, int ny);
    void BeginPan(POINT pt) { pan.Begin(ActiveDoc(), pt); }
    void EndPan(POINT pt);
    void MouseDown(POINT pt, const UiMods& m) override;
    void MouseMove(POINT pt, const UiMods& m) override;
    void MouseUp(POINT pt, const UiMods& m) override;
    void MouseLeave() override;
    void DblClick(POINT pt, const UiMods& m) override;
    void ContextMenu(POINT pt, const UiMods& m) override;
    bool Wheel(int delta, POINT pt, const UiMods& m) override;
    bool KeyDown(WPARAM vk, const UiMods& m) override;
    bool Char(wchar_t ch, const UiMods& m) override;
    bool Cursor(POINT pt) override;
    void CaptureLost() override;
};

struct TitleBar : UiObject
{
    int hover = 0;               // 1 min, 2 max, 3 close
    bool dragging = false;
    bool moved = false;
    POINT dragCursor{};
    RECT dragStart{};

    void BuildSpec(HDC dc) override;
    void Buttons(RECT& minR, RECT& maxR, RECT& closeR) const;
    void MouseDown(POINT pt, const UiMods& m) override;
    void MouseMove(POINT pt, const UiMods& m) override;
    void MouseUp(POINT pt, const UiMods& m) override;
    void DblClick(POINT pt, const UiMods& m) override;
    void MouseLeave() override;
    void CaptureLost() override;
};

struct MenuBar : UiObject
{
    int hover = -1;

    void BuildSpec(HDC dc) override;
    void TopRects(HDC dc, RECT out[kMenuCount]) const;  // one geometry source
    RECT TopRect(int index) const;
    int IndexAt(POINT pt) const;
    void MouseDown(POINT pt, const UiMods& m) override;
    void MouseMove(POINT pt, const UiMods& m) override;
    void MouseLeave() override;
};

struct Toolbar : UiObject
{
    int hoverId = 0;

    void BuildSpec(HDC dc) override;
    ToolLayout Buttons() const;
    int Hit(POINT pt, bool& arrow) const;
    void MouseDown(POINT pt, const UiMods& m) override;
    void MouseMove(POINT pt, const UiMods& m) override;
    void MouseLeave() override;
    void Click(POINT pt);
};

struct TabBar : UiObject
{
    int hover = -1;
    bool hoverClose = false;
    int menuIndex = -1;

    // The object owns its own parameters; outsiders only ask for a value.
    int TakeMenuIndex() { int i = menuIndex; menuIndex = -1; return i; }

    void BuildSpec(HDC dc) override;
    TabLayout Tabs() const;
    void MouseDown(POINT pt, const UiMods& m) override;
    void MouseMove(POINT pt, const UiMods& m) override;
    void MouseLeave() override;
    void ContextMenu(POINT pt, const UiMods& m) override;
};

struct HintSpec
{
    RECT rect{};
    RECT leftBox{}, rightBox{};
    std::wstring left, right;
    HFONT leftFont = nullptr, rightFont = nullptr;
    COLORREF bg = 0, border = 0, leftColor = 0, rightColor = 0;
    UINT leftFlags = 0, rightFlags = 0;
};

struct StatusSig
{
    const void* doc = nullptr;
    uint64_t rev = 0, tempRev = 0;
    int selAR = -1, selAC = -1, selFR = -1, selFC = -1;
    uint64_t extraHash = 0;
    size_t cellCount = 0;
    int active = -1, zoom = 0, uiZoom = 0, dark = 0, tempActive = 0;
    int l = 0, t = 0, r = 0, b = 0;

    bool Same(const StatusSig& o) const
    {
        return doc == o.doc && rev == o.rev && tempRev == o.tempRev
            && selAR == o.selAR && selAC == o.selAC && selFR == o.selFR && selFC == o.selFC
            && extraHash == o.extraHash && cellCount == o.cellCount && active == o.active
            && zoom == o.zoom && uiZoom == o.uiZoom && dark == o.dark
            && tempActive == o.tempActive && l == o.l && t == o.t && r == o.r && b == o.b;
    }
};

struct StatusModel
{
    std::wstring temp;
    ULONGLONG tempUntil = 0;
    uint64_t tempRev = 0;
    bool timerOn = false;
    HintSpec spec;
    StatusSig sig;
    bool hasSpec = false;

    void StopTimer()
    {
        if (timerOn && g_hwnd) KillTimer(g_hwnd, 2);
        timerOn = false;
    }

    void InvalidateHint()
    {
        if (!g_hwnd) return;
        if (hasSpec && spec.rect.right > spec.rect.left)
            InvalidateRect(g_hwnd, &spec.rect, FALSE);
        else
            InvalidateRect(g_hwnd, nullptr, FALSE);
    }

    void Show(const std::wstring& s)
    {
        temp = s;
        tempUntil = GetTickCount64() + 5000;
        ++tempRev;
        timerOn = true;
        if (g_hwnd) SetTimer(g_hwnd, 2, 5000, nullptr);
        InvalidateHint();           // request a frame; CommitFrame rebuilds spec
    }

    void Tick(ULONGLONG now)
    {
        if (temp.empty()) { StopTimer(); return; }
        if (now < tempUntil) {
            // Decide first, then re-arm for the remaining time: never leave a
            // dead timer together with a live temporary message.
            if (g_hwnd) SetTimer(g_hwnd, 2, (UINT)std::min<ULONGLONG>(tempUntil - now, 5000), nullptr);
            timerOn = true;
            return;
        }
        temp.clear();
        tempUntil = 0;
        ++tempRev;
        StopTimer();
        InvalidateHint();
    }

    // O(1) signature check. Heavy text is built only when state changes.
    bool Refresh(const RECT& statusRect)
    {
        ULONGLONG now = GetTickCount64();
        if (!temp.empty() && now >= tempUntil) {
            temp.clear();
            tempUntil = 0;
            ++tempRev;
            StopTimer();
        }

        Doc* dp = ActiveDoc();
        StatusSig s;
        s.doc = dp;
        s.rev = g_contentRev;
        s.tempRev = tempRev;
        s.tempActive = (!temp.empty() && now < tempUntil) ? 1 : 0;
        if (dp) {
            s.selAR = dp->selAR; s.selAC = dp->selAC;
            s.selFR = dp->selFR; s.selFC = dp->selFC;
            // O(1) comparison: fold the extra selection blocks into one word.
            uint64_t h = 0;
            for (auto& e : dp->selExtra) {
                h = h * 1315423911ull
                    ^ ((uint64_t)(uint32_t)e.r1 << 48)
                    ^ ((uint64_t)(uint32_t)e.c1 << 32)
                    ^ ((uint64_t)(uint32_t)e.r2 << 16)
                    ^ (uint32_t)e.c2;
            }
            s.extraHash = h ^ (uint64_t)dp->selExtra.size();
            s.cellCount = dp->cells.size();
        }
        s.active = g_active;
        s.zoom = g_gridZoom;
        s.uiZoom = g_uiZoom;
        s.dark = g_dark ? 1 : 0;
        s.l = statusRect.left; s.t = statusRect.top;
        s.r = statusRect.right; s.b = statusRect.bottom;

        if (hasSpec && s.Same(sig)) return false;
        sig = s;
        hasSpec = true;
        BuildHint(dp, statusRect);
        return true;
    }

private:
    void BuildHint(Doc* dp, const RECT& r)
    {
        HintSpec h;
        h.rect = r;
        h.bg = g_th.panel;
        h.border = g_th.border;
        h.leftFont = UiFont(9);
        h.rightFont = MonoFont(8);
        h.leftFlags = DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS;
        h.rightFlags = DT_SINGLELINE | DT_VCENTER | DT_RIGHT;
        h.leftColor = sig.tempActive ? g_th.accent : g_th.dim;
        h.rightColor = g_th.faint;
        h.leftBox = { r.left + DpiScale(12), r.top, r.right - DpiScale(360), r.bottom };
        h.rightBox = { r.right - DpiScale(340), r.top, r.right - DpiScale(12), r.bottom };

        if (sig.tempActive) {
            h.left = temp;
        } else if (dp) {
            int r1, c1, r2, c2;
            dp->SelectionRect(r1, c1, r2, c2);
            std::wstring ref = CellRefName(r1, c1);
            if (r1 != r2 || c1 != c2) ref += L":" + CellRefName(r2, c2);
            std::vector<MergeRange> ranges;
            ranges.push_back({ r1, c1, r2, c2 });
            for (auto& e : dp->selExtra) ranges.push_back(e);
            long long cells = 0;
            for (auto& e : ranges)
                cells += (long long)(e.r2 - e.r1 + 1) * (e.c2 - e.c1 + 1);
            int nonEmpty = 0, numeric = 0, scanned = 0;
            double sum = 0;
            std::unordered_set<long long> seen;
            for (auto& e : ranges) {
                for (int rr = e.r1; rr <= e.r2 && scanned < 6000; ++rr) {
                    for (int cc = e.c1; cc <= e.c2 && scanned < 6000; ++cc) {
                        long long k = CellKey(rr, cc);
                        if (!seen.insert(k).second) continue;
                        ++scanned;
                        const Cell* cell = dp->Find(rr, cc);
                        if (!cell || cell->IsEmpty()) continue;
                        ++nonEmpty;
                        double v;
                        if (cell->kind == CK_TEXT && ParseNum(CellDisplay(*dp, rr, cc), v)) {
                            ++numeric;
                            sum += v;
                        }
                    }
                }
            }
            h.left = ref + L"    " + std::to_wstring(cells) + L" 格";
            if (dp->selExtra.size()) h.left += L"   +" + std::to_wstring(dp->selExtra.size()) + L" 处";
            if (nonEmpty) h.left += L"   非空 " + std::to_wstring(nonEmpty);
            if (numeric) h.left += L"   求和 " + NumToStr(sum) + L"   均值 " + NumToStr(sum / numeric);
        } else {
            h.left = L"没有打开的表格";
        }
        h.right = Format(L"缩放 %d%%", g_gridZoom);
        spec = std::move(h);
    }
};

struct StatusBar : UiObject
{
    HintSpec hint;                                  // declared by StatusModel

    void SetSpec(const HintSpec& s) { hint = s; }   // zero processing
    void BuildSpec(HDC dc) override;                // HintSpec -> primitives
};

struct UiRoot
{
    TitleBar title;
    MenuBar menu;
    Toolbar toolbar;
    GridView grid;
    TabBar tabs;
    StatusBar status;
    StatusModel statusModel;      // logic layer: produces hint data

    // Popups are objects too: the root owns their state, their HWND only
    // presents it. No scattered global pointers, no second ownership model.
    MenuPopup menuPopup;
    ListPopup listPopup;
    PalettePopup palettePopup;

    // The complete object table. Submenus are nested inside MenuPopup, so
    // nothing is ever inserted or erased from this table at runtime: every
    // iteration over it is stable. A vector (not a fixed array) means adding
    // an object is one line in the constructor, never a layout rewrite.
    std::vector<UiObject*> children;
    PaintSpec backSpec;          // root background, executed before children
    PaintSpec frameSpec;         // root border, executed after children
    PaintSurface surface;        // the root's presentation surface

    int resizeEdge = 0;
    POINT resizeStartCursor{};
    RECT resizeStartRect{};
    RECT resizeTargetRect{};
    bool resizeTargetValid = false;

    UiObject* hoverChild = nullptr;
    UiObject* captureChild = nullptr;
    UINT animInterval = 0;       // 0 = no animation timer running

    UiRoot()
    {
        title.id = UiId::TitleBar;
        menu.id = UiId::MenuBar;
        toolbar.id = UiId::Toolbar;
        grid.id = UiId::Grid;
        tabs.id = UiId::Tabs;
        status.id = UiId::Status;
        children = { &title, &menu, &toolbar, &grid, &tabs, &status,
                     &menuPopup, &listPopup, &palettePopup };
    }

    UiObject* ById(UiId id)
    {
        for (UiObject* o : children) if (o->id == id) return o;
        return nullptr;
    }
    // A hosted object lives in its own HWND; main-window pointer geometry
    // never applies to it, so it is skipped by the parent's hit test.
    UiObject* HitChild(POINT pt)
    {
        for (UiObject* o : children)
            if (!o->Hosted() && o->HitTest(pt)) return o;
        return nullptr;
    }

    void Layout();
    void BuildFrameSpec(HDC dc);            // root's own declaration
    void Present(HDC dc, const RECT& area); // compose all declarations, blit once
    bool PointerOnGridScrollBar(POINT pt);  // edge-resize must yield to the bars
    void CommitFrame(HDC measure, const RECT* update);  // one state->spec commit
    void WindowClose();
    void AppActivate(bool active);
    void DropFiles(const std::vector<std::wstring>& paths, POINT pt);
    void DismissPopups();
    void MouseMove(POINT pt, const UiMods& m);
    void MouseDown(POINT pt, const UiMods& m);
    void MouseUp(POINT pt, const UiMods& m);
    void DblClick(POINT pt, const UiMods& m);
    void ContextMenu(POINT pt, const UiMods& m);
    void Wheel(int delta, POINT pt, const UiMods& m);
    bool KeyDown(WPARAM vk, const UiMods& m);
    bool Char(wchar_t ch, const UiMods& m);
    bool Cursor(POINT pt);
    void CaptureLost();
    void Tick(UINT_PTR id, ULONGLONG now);
    void UpdateAnimationTimer();
    void EndWindowResize();
};

struct ModalFocusState {
    HWND owner = nullptr;
    bool restore = false;
    bool enabled = false;
    bool foreground = false;
};

struct MsgButton {
    RECT rect = {};
    std::wstring label;
    int result = 0;
    bool primary = false;
};

// Modal utility windows are frame hosts of their own. Each holds a declared
// PaintSpec; its WndProc commits then executes, exactly like the main window.
struct MsgState {
    HWND hwnd = nullptr;
    PaintSpec spec;
    PaintSurface surface;
    std::wstring title;
    std::wstring text;
    int mode = MSGB_OK;
    int result = IDOK;
    int hover = 0;
    int width = 0, height = 0;
    RECT textRect = {};
    std::vector<MsgButton> buttons;
};

struct PromptState {
    HWND hwnd = nullptr;
    PaintSpec spec;
    PaintSurface surface;
    HWND edit = nullptr;
    std::wstring title;
    std::wstring hint;
    std::wstring result;
    bool ok = false;
    int hoverBtn = 0;
    int width = 0, height = 0;
};

// About dialog: one line of text, one embedded 200x200 image, one 好 button.
struct AboutState {
    HWND hwnd = nullptr;
    PaintSpec spec;
    PaintSurface surface;
    HBITMAP img = nullptr;
    int imgW = 0, imgH = 0;
    int hoverBtn = 0;
    int width = 0, height = 0;
};

// 编号 / 单位 dialog: optional style dropdown (编号) + suffix edit + 确定/取消.
struct LabelState {
    HWND hwnd = nullptr;
    HWND edit = nullptr;
    PaintSpec spec;
    PaintSurface surface;
    bool numbering = false;      // true = 编号 (dropdown + sequence), false = 单位 (append)
    int style = 0;               // 0 全角数字, 1 中文小写
    bool styleOpen = false;
    std::wstring suffix;
    int hoverBtn = 0;            // 1 ok, 2 cancel
    int hoverStyle = 0;          // 1 dropdown, 2..3 list items
    int width = 0, height = 0;
};

struct ClipBlock {
    bool valid = false;
    int w = 0, h = 0;
    std::vector<Cell> cells;
};

struct ImportItem {
    int kind = CK_FILE;
    std::wstring text;
    std::wstring path;
};

struct PropState {
    HWND hwnd = nullptr;
    PaintSpec spec;
    PaintSurface surface;
    Doc* doc = nullptr;
    std::vector<std::pair<int, int>> cells;
    std::wstring firstPath;
    int firstKind = CK_FILE;
    std::wstring message;
    int hoverBtn = 0;     // 1 copy, 2 nocopy, 3 checkbox
    int width = 0, height = 0;
};

struct SettingsState {
    HWND hwnd = nullptr;
    PaintSpec spec;
    PaintSurface surface;
    HWND colEdit = nullptr;
    HWND rowEdit = nullptr;
    int width = 0, height = 0;
    // The scale this window was OPENED at. Not the live one: this dialog owns
    // the control that changes the app's zoom, and a window that resizes while
    // it is the only thing able to paint itself shows a torn frame. So it
    // freezes the scale at birth and lives by it; the next open picks up
    // whatever the zoom is then.
    int snapDpi = 96;
    int snapZoom = 100;
    int hover = 0;
    bool dark = true;
    bool autoCopy = false;
    int uiZoom = 100;
    int gridZoom = 100;
    int maxRows = 15;
    int maxCols = 50;
    std::wstring message;
};

