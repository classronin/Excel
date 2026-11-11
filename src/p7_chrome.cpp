// ============================================================================
//  p7_chrome.cpp
//  Responsibility: chrome controls
//  Title bar, menu bar, toolbar, tab bar, status bar controls.
//
//  Shared contract: core.h + only the api_*.h headers this module references.
//  This translation unit contains no type or state definitions of its own (state.cpp does).
// ============================================================================
#include "core.h"
#include "api_p1_core.h"
#include "api_p2_doc.h"
#include "api_p3_formula.h"
#include "api_p5_popup.h"
#include "api_p6a_grid_paint.h"
#include "api_p6b_grid_input.h"
#include "api_p7_chrome.h"
#include "api_p8a_commands.h"
#include "api_p8b_app.h"
// ---- src\p7_chrome.inc ----
// ============================================================================
//  Excel - part 7: menu bar, toolbar, tab bar, status bar
// ============================================================================

std::wstring CellRefName(int r, int c)
{
    std::wstring name;
    int n = c;
    do {
        name.insert(name.begin(), (wchar_t)(L'A' + (n % 26)));
        n = n / 26 - 1;
    } while (n >= 0);
    return name + std::to_wstring(r + 1);
}


// ---------------------------------------------------------------------------
//  custom title bar + menu bar
// ---------------------------------------------------------------------------
int TitleH()
{ return DpiScale(32); }


void TitleBar::Buttons(RECT& minR, RECT& maxR, RECT& closeR) const
{
    int bw = DpiScale(46);
    closeR = { rect.right - bw, rect.top, rect.right, rect.bottom };
    maxR = { rect.right - bw * 2, rect.top, rect.right - bw, rect.bottom };
    minR = { rect.right - bw * 3, rect.top, rect.right - bw * 2, rect.bottom };
}


void TitleBar::MouseLeave()
{
    if (hover == 0) return;
    hover = 0;
    if (g_hwnd) InvalidateRect(g_hwnd, &rect, FALSE);
}


void TitleBar::CaptureLost()
{
    dragging = false;
    moved = false;
}


void TitleBar::MouseDown(POINT pt, const UiMods& m)
{
    if (m.button != 1) return;
    RECT minR, maxR, closeR;
    Buttons(minR, maxR, closeR);
    if (PtInRect(&closeR, pt)) {
        CloseAllPopups();
        g_actions.Post(UiAction::CloseWindow);
        return;
    }
    if (PtInRect(&maxR, pt)) {
        CloseAllPopups();
        ShowWindow(g_hwnd, IsZoomed(g_hwnd) ? SW_RESTORE : SW_MAXIMIZE);
        return;
    }
    if (PtInRect(&minR, pt)) {
        CloseAllPopups();
        ShowWindow(g_hwnd, SW_MINIMIZE);
        return;
    }
    CloseAllPopups();
    if (!IsZoomed(g_hwnd)) {
        dragging = true;
        moved = false;
        GetCursorPos(&dragCursor);
        GetWindowRect(g_hwnd, &dragStart);
        SetCapture(g_hwnd);
    }
}


void TitleBar::MouseMove(POINT pt, const UiMods&)
{
    if (dragging) {
        POINT cur;
        GetCursorPos(&cur);
        int dx = cur.x - dragCursor.x;
        int dy = cur.y - dragCursor.y;
        if (abs(dx) + abs(dy) > 2) moved = true;
        if (moved) {
            SetWindowPos(g_hwnd, nullptr, dragStart.left + dx, dragStart.top + dy,
                         0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
        return;
    }
    int th = 0;
    RECT minR, maxR, closeR;
    Buttons(minR, maxR, closeR);
    if (PtInRect(&closeR, pt)) th = 3;
    else if (PtInRect(&maxR, pt)) th = 2;
    else if (PtInRect(&minR, pt)) th = 1;
    if (th != hover) {
        hover = th;
        if (g_hwnd) InvalidateRect(g_hwnd, &rect, FALSE);
    }
}


void TitleBar::MouseUp(POINT, const UiMods&)
{
    if (!dragging) return;
    dragging = false;
    if (GetCapture() == g_hwnd) ReleaseCapture();
}


void TitleBar::DblClick(POINT pt, const UiMods&)
{
    RECT minR, maxR, closeR;
    Buttons(minR, maxR, closeR);
    if (PtInRect(&minR, pt) || PtInRect(&maxR, pt) || PtInRect(&closeR, pt)) return;
    if (dragging) {
        dragging = false;
        if (GetCapture() == g_hwnd) ReleaseCapture();
    }
    ShowWindow(g_hwnd, IsZoomed(g_hwnd) ? SW_RESTORE : SW_MAXIMIZE);
}


void TitleBar::BuildSpec(HDC)
{
    PaintSpec& s = spec;
    s.Clear();
    RECT c = ClientRect();
    int th = TitleH();
    RECT r = { 0, 0, c.right, th };
    s.Emit(PaintOp::FillRect, r, g_th.panel);
    RECT bottomLine = { 0, th - 1, c.right, th - 1 };
    s.Emit(PaintOp::HLine, bottomLine, g_th.border).w = 1;

    RECT ic = { DpiScale(12), (th - DpiScale(16)) / 2, DpiScale(28), (th + DpiScale(16)) / 2 };
    if (g_iconSmall) {
        s.Emit(PaintOp::Icon, ic, 0).icon = g_iconSmall;
    } else {
        s.Emit(PaintOp::FillRect, ic, g_th.accentSoft);
        s.Emit(PaintOp::FrameRect, ic, g_th.accent).w = 1;
        RECT l1 = { ic.left + DpiScale(3), ic.top + DpiScale(5), ic.right - DpiScale(3), ic.top + DpiScale(5) };
        s.Emit(PaintOp::HLine, l1, g_th.accent).w = 1;
        RECT l2 = { ic.left + DpiScale(3), ic.top + DpiScale(10), ic.right - DpiScale(3), ic.top + DpiScale(10) };
        s.Emit(PaintOp::HLine, l2, g_th.accent).w = 1;
        RECT v1 = { ic.left + DpiScale(5), ic.top + DpiScale(3), ic.left + DpiScale(5), ic.bottom - DpiScale(3) };
        s.Emit(PaintOp::VLine, v1, g_th.accent).w = 1;
    }

    RECT minR, maxR, closeR;
    Buttons(minR, maxR, closeR);

    wchar_t title[512] = { 0 };
    GetWindowTextW(g_hwnd, title, 512);
    RECT tr = { ic.right + DpiScale(10), 0, minR.left - DpiScale(10), th };
    auto& tp = s.Emit(PaintOp::Text, tr, g_th.text);
    tp.text = title;
    tp.font = UiFont(9, true);
    tp.flags = DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS;

    auto button = [&](const RECT& br, int id, bool isClose) {
        bool hot = (hover == id);
        if (hot) s.Emit(PaintOp::FillRect, br, isClose ? g_th.danger : g_th.hover);
        COLORREF col = hot ? (isClose ? RGB(255, 255, 255) : g_th.text) : g_th.dim;
        int cx = (br.left + br.right) / 2;
        int cy = (br.top + br.bottom) / 2;
        if (id == 1) {
            RECT ln = { cx - DpiScale(5), cy, cx + DpiScale(5) + 1, cy };
            s.Emit(PaintOp::HLine, ln, col).w = 1;
        } else if (id == 2) {
            if (IsZoomed(g_hwnd)) {
                RECT b1 = { cx - DpiScale(6), cy - DpiScale(3), cx + DpiScale(2), cy + DpiScale(5) };
                RECT b2 = { cx - DpiScale(2), cy - DpiScale(6), cx + DpiScale(6), cy + DpiScale(2) };
                s.Emit(PaintOp::FrameRect, b1, col).w = 1;
                s.Emit(PaintOp::FrameRect, b2, col).w = 1;
            } else {
                RECT b = { cx - DpiScale(5), cy - DpiScale(5), cx + DpiScale(5), cy + DpiScale(5) };
                s.Emit(PaintOp::FrameRect, b, col).w = 1;
            }
        } else {
            auto& p = s.Emit(PaintOp::CloseX, RECT{}, col);
            p.p1 = cx;
            p.p2 = cy;
            p.w = DpiScale(4);
        }
    };
    button(minR, 1, false);
    button(maxR, 2, false);
    button(closeR, 3, true);
}



// One geometry source for both the frame declaration and input hit-testing.
void MenuBar::TopRects(HDC dc, RECT out[3]) const
{
    int pad = DpiScale(14);
    int x = rect.left + DpiScale(8);
    HFONT font = UiFont(10);
    for (int i = 0; i < 3; ++i) {
        SIZE sz = MeasureText(dc, kMenuTitles[i], font);
        int w = sz.cx + pad * 2;
        out[i] = { x, rect.top, x + w, rect.bottom };
        x += w;
    }
}


RECT MenuBar::TopRect(int index) const
{
    if (index < 0 || index > 2) return { 0, 0, 0, 0 };
    RECT out[3];
    HDC dc = GetDC(g_hwnd);
    TopRects(dc, out);
    ReleaseDC(g_hwnd, dc);
    return out[index];
}


int MenuBar::IndexAt(POINT pt) const
{
    if (!PtInRect(&rect, pt)) return -1;
    RECT out[3];
    HDC dc = GetDC(g_hwnd);
    TopRects(dc, out);
    ReleaseDC(g_hwnd, dc);
    for (int i = 0; i < 3; ++i)
        if (pt.x >= out[i].left && pt.x < out[i].right) return i;
    return -1;
}


void MenuBar::MouseDown(POINT pt, const UiMods& m)
{
    if (m.button != 1) return;
    int mi = IndexAt(pt);
    if (mi >= 0) OpenTopMenu(mi);
}


void MenuBar::MouseMove(POINT pt, const UiMods&)
{
    int bar = IndexAt(pt);
    if (bar != hover) {
        hover = bar;
        if (g_hwnd) InvalidateRect(g_hwnd, nullptr, FALSE);
    }
    if (g_ui.menuPopup.topIndex >= 0 && bar >= 0 && bar < 2 && bar != g_ui.menuPopup.topIndex)
        OpenTopMenu(bar);
}


void MenuBar::MouseLeave()
{
    if (hover == -1) return;
    hover = -1;
    if (g_hwnd) InvalidateRect(g_hwnd, &rect, FALSE);
}


void MenuBar::BuildSpec(HDC dc)
{
    PaintSpec& s = spec;
    s.Clear();
    RECT r = rect;
    s.Emit(PaintOp::FillRect, r, g_th.panel);
    RECT bottomLine = { 0, r.bottom - 1, r.right, r.bottom - 1 };
    s.Emit(PaintOp::HLine, bottomLine, g_th.border).w = 1;

    RECT mr[3];
    TopRects(dc, mr);
    for (int i = 0; i < 3; ++i) {
        bool active = (g_ui.menuPopup.topIndex == i);
        bool hot = (hover == i);
        if (active) {
            s.Emit(PaintOp::FillRect, mr[i], g_th.hover);
            RECT accent = { mr[i].left + DpiScale(8), mr[i].bottom - 2,
                            mr[i].right - DpiScale(8), mr[i].bottom - 2 };
            s.Emit(PaintOp::HLine, accent, g_th.accent).w = DpiScale(2);
        } else if (hot) {
            s.Emit(PaintOp::FillRect, mr[i], g_th.hover);
        }
        RECT tr = { mr[i].left + DpiScale(14), mr[i].top,
                    mr[i].right - DpiScale(14), mr[i].bottom };
        auto& tp = s.Emit(PaintOp::Text, tr, active ? g_th.text : g_th.dim);
        tp.text = kMenuTitles[i];
        tp.font = UiFont(10);
        tp.flags = DT_SINGLELINE | DT_VCENTER | DT_CENTER;
    }
    RECT br = { r.right - DpiScale(240), r.top, r.right - DpiScale(12), r.bottom };
    auto& bp = s.Emit(PaintOp::Text, br, g_th.faint);
    bp.text = L"一流浪人·ClassRonin";
    bp.font = UiFont(8);
    bp.flags = DT_SINGLELINE | DT_VCENTER | DT_RIGHT;
}


// ---------------------------------------------------------------------------
//  toolbar layout
// ---------------------------------------------------------------------------

ToolLayout Toolbar::Buttons() const
{
    ToolLayout t;
    int pad = DpiScale(10);
    int h = DpiScale(28);
    int y = rect.top + ((rect.bottom - rect.top) - h) / 2;
    int gap = DpiScale(6);
    int x = rect.left + pad;

    t.font = { x, y, x + DpiScale(196), y + h };
    x = t.font.right + gap;
    t.size = { x, y, x + DpiScale(66), y + h };
    x = t.size.right + gap;
    t.row = { x, y, x + DpiScale(32), y + h };
    x = t.row.right + gap;
    t.col = { x, y, x + DpiScale(32), y + h };
    x = t.col.right + gap;
    // Display precision switch: 原值 -> 小数 -> 整数, initialised to 小数.
    t.mode = { x, y, x + DpiScale(56), y + h };
    x = t.mode.right + gap;
    t.merge = { x, y, x + DpiScale(56), y + h };
    x = t.merge.right + DpiScale(16);

    int aw = DpiScale(36);
    for (int i = 0; i < 3; ++i) {
        t.align[i] = { x, y, x + aw, y + h };
        x += aw;
    }
    x += DpiScale(6);
    int vw = DpiScale(30);
    for (int i = 0; i < 3; ++i) {
        t.valign[i] = { x, y, x + vw, y + h };
        x += vw;
    }
    x += DpiScale(16);

    int cw = DpiScale(42);
    t.fg = { x, y, x + cw, y + h };
    x += cw + DpiScale(4);
    t.outline = { x, y, x + cw, y + h };
    x += cw + DpiScale(4);
    t.cellbg = { x, y, x + cw, y + h };
    x += cw + DpiScale(16);
    t.sum = { x, y, x + DpiScale(40), y + h };
    t.op = { t.sum.right + DpiScale(8), y, t.sum.right + DpiScale(8) + DpiScale(44), y + h };

    t.colorArrow[0] = { t.fg.right - DpiScale(13), t.fg.top, t.fg.right, t.fg.bottom };
    t.colorArrow[1] = { t.outline.right - DpiScale(13), t.outline.top, t.outline.right, t.outline.bottom };
    t.colorArrow[2] = { t.cellbg.right - DpiScale(13), t.cellbg.top, t.cellbg.right, t.cellbg.bottom };
    return t;
}


int Toolbar::Hit(POINT pt, bool& arrow) const
{
    arrow = false;
    ToolLayout t = Buttons();
    if (PtInRect(&t.font, pt)) return 1;
    if (PtInRect(&t.size, pt)) return 2;
    if (PtInRect(&t.row, pt)) return 10;
    if (PtInRect(&t.col, pt)) return 11;
    if (PtInRect(&t.mode, pt)) return 17;
    if (PtInRect(&t.merge, pt)) return 16;
    for (int i = 0; i < 3; ++i)
        if (PtInRect(&t.align[i], pt)) return 6 + i;
    for (int i = 0; i < 3; ++i)
        if (PtInRect(&t.valign[i], pt)) return 13 + i;
    if (PtInRect(&t.fg, pt)) { arrow = PtInRect(&t.colorArrow[0], pt) != 0; return 3; }
    if (PtInRect(&t.outline, pt)) { arrow = PtInRect(&t.colorArrow[1], pt) != 0; return 4; }
    if (PtInRect(&t.cellbg, pt)) { arrow = PtInRect(&t.colorArrow[2], pt) != 0; return 5; }
    if (PtInRect(&t.sum, pt)) return 9;
    if (PtInRect(&t.op, pt)) return 12;
    return 0;
}


// Spec emitters: these are logic-layer declarations, not drawing functions.
// Every colour, font, rectangle and glyph position is resolved before paint.
void EmitBox(PaintSpec& s, RECT rc, const std::wstring& text, COLORREF textColor,
             bool hot, bool showArrow)
{
    s.Emit(PaintOp::FillRect, rc, hot ? g_th.hover : g_th.panel2);
    s.Emit(PaintOp::FrameRect, rc, hot ? g_th.borderStrong : g_th.border).w = 1;
    RECT tr = { rc.left + DpiScale(9), rc.top,
                rc.right - (showArrow ? DpiScale(24) : DpiScale(8)), rc.bottom };
    auto& p = s.Emit(PaintOp::Text, tr, textColor);
    p.text = text;
    p.font = UiFont(9);
    p.flags = DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS;
    if (showArrow) {
        auto& a = s.Emit(PaintOp::Triangle, RECT{}, g_th.dim);
        a.p1 = rc.right - DpiScale(9);
        a.p2 = (rc.top + rc.bottom) / 2;
        a.w = DpiScale(4);
    }
}


void EmitColorButton(PaintSpec& s, RECT rc, COLORREF c, int id, bool arrowArea, bool hot)
{
    s.Emit(PaintOp::FillRect, rc, hot ? g_th.hover : g_th.panel2);
    s.Emit(PaintOp::FrameRect, rc, hot ? g_th.borderStrong : g_th.border).w = 1;
    RECT sw = { rc.left + DpiScale(6), rc.top + (rc.bottom - rc.top - DpiScale(18)) / 2,
                rc.left + DpiScale(6) + DpiScale(18),
                rc.top + (rc.bottom - rc.top - DpiScale(18)) / 2 + DpiScale(18) };
    s.Emit(PaintOp::FillRect, sw, g_th.panel2);
    if (c == CLR_NONE) {
        if (id == 3) {
            auto& p = s.Emit(PaintOp::Text, sw, g_th.faint);
            p.text = L"A";
            p.font = UiFont(10, true);
            p.flags = DT_SINGLELINE | DT_CENTER | DT_VCENTER;
        } else if (id == 4) {
            s.Emit(PaintOp::FrameRect, sw, g_th.faint).w = DpiScale(2);
        } else {
            s.Emit(PaintOp::FrameRect, sw, g_th.border).w = 1;
            RECT bar = { sw.left, (sw.top + sw.bottom) / 2, sw.right, (sw.top + sw.bottom) / 2 };
            s.Emit(PaintOp::HLine, bar, g_th.danger).w = 1;
        }
    } else if (id == 4) {
        s.Emit(PaintOp::FrameRect, sw, c).w = DpiScale(2);
    } else {
        s.Emit(PaintOp::FillRect, sw, c);
        s.Emit(PaintOp::FrameRect, sw, g_th.borderStrong).w = 1;
        if (id == 3) {
            int lum = (GetRValue(c) + GetGValue(c) + GetBValue(c)) / 3;
            auto& p = s.Emit(PaintOp::Text, sw,
                             lum > 140 ? RGB(10, 12, 15) : RGB(244, 248, 252));
            p.text = L"A";
            p.font = UiFont(10, true);
            p.flags = DT_SINGLELINE | DT_CENTER | DT_VCENTER;
        }
    }
    RECT ar = { rc.right - DpiScale(14), rc.top, rc.right, rc.bottom };
    if (hot && arrowArea) s.Emit(PaintOp::FillRect, ar, g_th.hover);
    RECT vl = { ar.left, ar.top + DpiScale(4), ar.left, ar.bottom - DpiScale(4) };
    s.Emit(PaintOp::VLine, vl, g_th.border).w = 1;
    auto& a = s.Emit(PaintOp::Triangle, RECT{}, g_th.dim);
    a.p1 = ar.left + DpiScale(7);
    a.p2 = (ar.top + ar.bottom) / 2;
    a.w = DpiScale(3);
}


int SelectionAlign(Doc& d)
{
    int r1, c1, r2, c2;
    d.SelectionRect(r1, c1, r2, c2);
    int value = -2;
    for (int r = r1; r <= r2; ++r) {
        for (int c = c1; c <= c2; ++c) {
            const Cell* cell = d.Find(r, c);
            int a = cell ? EffectiveHAlign(cell->st) : 0;
            if (value == -2) value = a;
            else if (value != a) return -1;
        }
    }
    return value == -2 ? 0 : value;
}


int SelectionVAlign(Doc& d)
{
    int r1, c1, r2, c2;
    d.SelectionRect(r1, c1, r2, c2);
    int value = -2;
    for (int r = r1; r <= r2; ++r) {
        for (int c = c1; c <= c2; ++c) {
            const Cell* cell = d.Find(r, c);
            int a = cell ? EffectiveVAlign(cell->st) : 0;
            if (value == -2) value = a;
            else if (value != a) return -1;
        }
    }
    return value == -2 ? 0 : value;
}


bool SelectionIntersectsMerge(Doc& d)
{
    std::vector<MergeRange> ranges;
    int r1, c1, r2, c2;
    d.SelectionRect(r1, c1, r2, c2);
    ranges.push_back({ r1, c1, r2, c2 });
    for (auto& e : d.selExtra) ranges.push_back(e);
    for (auto& m : d.merges) {
        for (auto& q : ranges)
            if (!(q.r2 < m.r1 || q.r1 > m.r2 || q.c2 < m.c1 || q.c1 > m.c2)) return true;
    }
    return false;
}


void ToggleMergeSelection()
{
    Doc* doc = ActiveDoc();
    if (!doc) return;
    Doc& d = *doc;
    int r1, c1, r2, c2;
    d.SelectionRect(r1, c1, r2, c2);
    if (r1 < 0 || c1 < 0 || r2 >= d.rows || c2 >= d.cols) return;
    d.PushUndo();
    bool wasMerged = SelectionIntersectsMerge(d);
    std::vector<MergeRange> ranges;
    ranges.push_back({ r1, c1, r2, c2 });
    for (auto& e : d.selExtra) ranges.push_back(e);
    std::vector<MergeRange> keep;
    for (auto& m : d.merges) {
        bool hit = false;
        for (auto& q : ranges) {
            if (!(q.r2 < m.r1 || q.r1 > m.r2 || q.c2 < m.c1 || q.c1 > m.c2)) { hit = true; break; }
        }
        if (!hit) keep.push_back(m);
    }
    d.merges.swap(keep);
    if (!wasMerged) {
        if (r1 == r2 && c1 == c2) {
            SetStatus(L"请选择至少两个单元格再合并");
            d.undos.pop_back();
            return;
        }
        d.merges.push_back({ r1, c1, r2, c2 });
        Cell& top = d.Ensure(r1, c1);
        top.st.align = 1 | (1 << 2) | 16;
    }
    d.Touch();
    InvalidateAll();
    SetStatus(wasMerged ? L"已取消合并" : L"已合并并居中");
}


void EmitAlignGlyph(PaintSpec& s, RECT rc, int align, COLORREF color)
{
    int cx = (rc.left + rc.right) / 2;
    int cy = (rc.top + rc.bottom) / 2;
    int w = DpiScale(7);
    int gap = DpiScale(4);
    int left = cx - w;
    for (int i = 0; i < 4; ++i) {
        int len = w * 2 - (i % 2 ? DpiScale(7) : 0);
        int x1 = left, x2 = left + len;
        if (align == 1) {
            x1 = cx - len / 2;
            x2 = x1 + len;
        } else if (align == 2) {
            x2 = left + w * 2;
            x1 = x2 - len;
        }
        int y = cy - gap + i * DpiScale(3);
        s.Emit(PaintOp::FillRect, RECT{ x1, y, x2, y + 1 }, color);
    }
}


void EmitVAlignGlyph(PaintSpec& s, RECT rc, int valign, COLORREF color)
{
    int cx = (rc.left + rc.right) / 2;
    int y = rc.top + DpiScale(7);
    if (valign == 1) y = (rc.top + rc.bottom) / 2 - DpiScale(4);
    else if (valign == 2) y = rc.bottom - DpiScale(11);
    for (int i = 0; i < 3; ++i) {
        int yy = y + i * DpiScale(3);
        s.Emit(PaintOp::FillRect,
               RECT{ cx - DpiScale(8), yy, cx + DpiScale(8), yy + 1 }, color);
    }
}


void Toolbar::BuildSpec(HDC)
{
    PaintSpec& s = spec;
    s.Clear();
    RECT bar = rect;
    s.Emit(PaintOp::FillRect, bar, g_th.panel);
    RECT bottomLine = { bar.left, bar.bottom - 1, bar.right, bar.bottom - 1 };
    s.Emit(PaintOp::HLine, bottomLine, g_th.border).w = 1;

    ToolLayout t = Buttons();
    // All state is read here, in the commit, never during paint: document
    // lookups, three selection scans and every theme colour become values.
    Doc* dp = ActiveDoc();
    std::wstring fontName = g_defaultFont;
    int fontSize = g_defaultSize;
    if (dp) {
        const Cell* cell = dp->Find(dp->selFR, dp->selFC);
        if (cell) {
            fontName = EffectiveFont(cell->st);
            fontSize = EffectiveSize(cell->st);
        }
    }
    EmitBox(s, t.font, fontName, g_th.text, hoverId == 1, true);
    EmitBox(s, t.size, std::to_wstring(fontSize), g_th.text, hoverId == 2, true);
    EmitBox(s, t.row, L"行", g_th.text, hoverId == 10, false);
    EmitBox(s, t.col, L"列", g_th.text, hoverId == 11, false);
    static const wchar_t* kNumModeName[] = { L"原值", L"小数", L"整数" };
    bool modeHot = (hoverId == 17);
    s.Emit(PaintOp::FillRect, t.mode, modeHot ? g_th.hover : g_th.panel2);
    s.Emit(PaintOp::FrameRect, t.mode, modeHot ? g_th.borderStrong : g_th.border).w = 1;
    auto& mp0 = s.Emit(PaintOp::Text, t.mode, modeHot ? g_th.text : g_th.dim);
    mp0.text = kNumModeName[g_numMode];
    mp0.font = UiFont(9);
    mp0.flags = DT_SINGLELINE | DT_CENTER | DT_VCENTER;

    bool merged = dp && SelectionIntersectsMerge(*dp);
    bool mergeHot = (hoverId == 16);
    s.Emit(PaintOp::FillRect, t.merge,
           merged ? g_th.accentSoft : (mergeHot ? g_th.hover : g_th.panel2));
    s.Emit(PaintOp::FrameRect, t.merge,
           merged ? g_th.accent : (mergeHot ? g_th.borderStrong : g_th.border)).w = 1;
    auto& mp = s.Emit(PaintOp::Text, t.merge,
                      merged ? g_th.accent : (mergeHot ? g_th.text : g_th.dim));
    mp.text = merged ? L"取消" : L"合并";
    mp.font = UiFont(9);
    mp.flags = DT_SINGLELINE | DT_CENTER | DT_VCENTER;

    int alg = dp ? SelectionAlign(*dp) : 0;
    for (int i = 0; i < 3; ++i) {
        RECT r = t.align[i];
        bool active = (alg == i);
        bool hot = (hoverId == 6 + i);
        s.Emit(PaintOp::FillRect, r, active ? g_th.accentSoft : (hot ? g_th.hover : g_th.panel2));
        s.Emit(PaintOp::FrameRect, r, active ? g_th.accent : g_th.border).w = 1;
        EmitAlignGlyph(s, r, i, active ? g_th.accent : (hot ? g_th.text : g_th.dim));
    }

    int valg = dp ? SelectionVAlign(*dp) : 0;
    for (int i = 0; i < 3; ++i) {
        RECT r = t.valign[i];
        bool active = (valg == i);
        bool hot = (hoverId == 13 + i);
        s.Emit(PaintOp::FillRect, r, active ? g_th.accentSoft : (hot ? g_th.hover : g_th.panel2));
        s.Emit(PaintOp::FrameRect, r, active ? g_th.accent : g_th.border).w = 1;
        EmitVAlignGlyph(s, r, i, active ? g_th.accent : (hot ? g_th.text : g_th.dim));
    }

    EmitColorButton(s, t.fg, g_btnFg, 3, false, hoverId == 3);
    EmitColorButton(s, t.outline, g_btnOutline, 4, false, hoverId == 4);
    EmitColorButton(s, t.cellbg, g_btnCellBg, 5, false, hoverId == 5);

    bool sumHot = (hoverId == 9);
    s.Emit(PaintOp::FillRect, t.sum, sumHot ? g_th.hover : g_th.panel2);
    s.Emit(PaintOp::FrameRect, t.sum, sumHot ? g_th.accent : g_th.border).w = 1;
    auto& sp = s.Emit(PaintOp::Text, t.sum, sumHot ? g_th.accent : g_th.dim);
    sp.text = L"Σ";
    sp.font = UiFont(12, true);
    sp.flags = DT_SINGLELINE | DT_CENTER | DT_VCENTER;

    bool opHot = (hoverId == 12);
    s.Emit(PaintOp::FillRect, t.op, opHot ? g_th.hover : g_th.panel2);
    s.Emit(PaintOp::FrameRect, t.op, opHot ? g_th.accent : g_th.border).w = 1;
    auto& op = s.Emit(PaintOp::Text, t.op, opHot ? g_th.accent : g_th.dim);
    op.text = L"±";
    op.font = UiFont(12, true);
    op.flags = DT_SINGLELINE | DT_CENTER | DT_VCENTER;
}

// ---------------------------------------------------------------------------
//  tab bar
// ---------------------------------------------------------------------------

TabLayout TabBar::Tabs() const
{
    TabLayout t;
    RECT bar = rect;
    int pad = DpiScale(8);
    int n = (int)g_docs.size();
    int plusW = DpiScale(30);
    int gap = DpiScale(3);
    int avail = (int)(bar.right - bar.left) - pad * 2 - plusW - gap;
    int each = 0;
    if (n > 0) {
        each = (avail - gap * (n - 1)) / n;
        if (each > DpiScale(230)) each = DpiScale(230);
        if (each < DpiScale(80)) each = DpiScale(80);
    }
    int x = pad;
    for (int i = 0; i < n; ++i) {
        RECT r = { x, bar.top, x + each, bar.bottom };
        t.tab.push_back(r);
        RECT cr = { r.right - DpiScale(22), r.top + DpiScale(7), r.right - DpiScale(6), r.bottom - DpiScale(7) };
        t.close.push_back(cr);
        x += each + gap;
    }
    t.plus = { x, bar.top + DpiScale(6), x + plusW, bar.bottom - DpiScale(6) };
    return t;
}


void TabBar::BuildSpec(HDC)
{
    PaintSpec& s = spec;
    s.Clear();
    RECT bar = rect;
    s.Emit(PaintOp::FillRect, bar, g_th.panel);
    RECT topLine = { bar.left, bar.top, bar.right, bar.top };
    s.Emit(PaintOp::HLine, topLine, g_th.border).w = 1;

    TabLayout t = Tabs();       // layout division belongs to the commit
    for (int i = 0; i < (int)g_docs.size() && i < (int)t.tab.size(); ++i) {
        Doc& d = *g_docs[i];
        RECT r = t.tab[i];
        bool active = (i == g_active);
        bool hot = (i == hover);
        if (active) s.Emit(PaintOp::FillRect, r, g_th.win);
        else if (hot) s.Emit(PaintOp::FillRect, r, g_th.hover);
        if (active) {
            RECT accent = { r.left, r.top, r.right, r.top };
            s.Emit(PaintOp::HLine, accent, g_th.accent).w = DpiScale(2);
        }
        RECT vl = { r.right - 1, r.top, r.right - 1, r.bottom };
        s.Emit(PaintOp::VLine, vl, g_th.border).w = 1;

        int tx = r.left + DpiScale(12);
        if (d.dirty) {
            int mid = r.top + (int)((r.bottom - r.top) / 2);
            s.Emit(PaintOp::FillRect,
                   RECT{ tx, mid - DpiScale(2), tx + DpiScale(4), mid + DpiScale(2) },
                   g_th.accent);
            tx += DpiScale(9);
        }
        RECT tr = { tx, r.top, r.right - DpiScale(26), r.bottom };
        auto& tp = s.Emit(PaintOp::Text, tr, active ? g_th.text : g_th.dim);
        tp.text = d.title;
        tp.font = UiFont(9, active);
        tp.flags = DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS;

        RECT cr = t.close[i];
        bool closeHot = (hover == i && hoverClose);
        COLORREF glyph = closeHot ? g_th.text : g_th.faint;
        if (closeHot && !d.locked) s.Emit(PaintOp::FillRect, cr, g_th.hover);
        auto& p = s.Emit(d.locked ? PaintOp::Lock : PaintOp::CloseX, RECT{}, glyph);
        p.p1 = (cr.left + cr.right) / 2;
        p.p2 = (cr.top + cr.bottom) / 2;
        p.w = DpiScale(3);
    }

    RECT pr = t.plus;
    bool plusHot = (hover == -2);
    COLORREF col = plusHot ? g_th.text : g_th.dim;
    if (plusHot) s.Emit(PaintOp::FillRect, pr, g_th.hover);
    s.Emit(PaintOp::FrameRect, pr, g_th.border).w = 1;
    int cx = (pr.left + pr.right) / 2;
    int cy = (pr.top + pr.bottom) / 2;
    RECT hl = { cx - DpiScale(4), cy, cx + DpiScale(4) + 1, cy };
    s.Emit(PaintOp::HLine, hl, col).w = 1;
    RECT vl = { cx, cy - DpiScale(4), cx, cy + DpiScale(4) + 1 };
    s.Emit(PaintOp::VLine, vl, col).w = 1;
}


// ---------------------------------------------------------------------------
//  status bar
// ---------------------------------------------------------------------------
void StatusBar::BuildSpec(HDC)
{
    // HintSpec -> primitives. The renderer then has no parameters left to
    // resolve; no document, clock, theme lookup or computation at paint time.
    PaintSpec& s = spec;
    s.Clear();
    const HintSpec& h = hint;
    s.Emit(PaintOp::FillRect, h.rect, h.bg);
    RECT topLine = { h.rect.left, h.rect.top, h.rect.right, h.rect.top };
    s.Emit(PaintOp::HLine, topLine, h.border).w = 1;
    auto& lp = s.Emit(PaintOp::Text, h.leftBox, h.leftColor);
    lp.text = h.left;
    lp.font = h.leftFont;
    lp.flags = h.leftFlags;
    auto& rp = s.Emit(PaintOp::Text, h.rightBox, h.rightColor);
    rp.text = h.right;
    rp.font = h.rightFont;
    rp.flags = h.rightFlags;
}


// ---------------------------------------------------------------------------
//  chrome interaction
// ---------------------------------------------------------------------------
void ApplyGlobalRowH(int px)
{
    if (px < 14) px = 14;
    if (px > 200) px = 200;
    g_rowH = px;
    for (auto& up : g_docs) {
        Doc& d = *up;
        d.rowH.assign(d.rows, px);
        d.Touch();
        d.InvalidateLayout();
    }
    SaveSettings();
    SetStatus(Format(L"全局行高 %d", g_rowH));
    InvalidateAll();
}


void ApplyGlobalColW(int px)
{
    if (px < 30) px = 30;
    if (px > 400) px = 400;
    g_colW = px;
    for (auto& up : g_docs) {
        Doc& d = *up;
        d.colW.assign(d.cols, px);
        d.Touch();
        d.InvalidateLayout();
    }
    SaveSettings();
    SetStatus(Format(L"全局列宽 %d", g_colW));
    InvalidateAll();
}


void Toolbar::MouseMove(POINT pt, const UiMods&)
{
    bool arrow = false;
    int id = Hit(pt, arrow);
    if (id != hoverId) {
        hoverId = id;
        if (g_hwnd) InvalidateRect(g_hwnd, nullptr, FALSE);
    }
}


void Toolbar::MouseLeave()
{
    if (hoverId == 0) return;
    hoverId = 0;
    if (g_hwnd) InvalidateRect(g_hwnd, &rect, FALSE);
}


void Toolbar::MouseDown(POINT pt, const UiMods& m)
{
    if (m.button != 1) return;
    Click(pt);
}


void Toolbar::Click(POINT pt)
{
    bool arrow = false;
    int id = Hit(pt, arrow);
    if (!id) return;
    if (id == 1) {
        RECT r = Buttons().font;
        MapWindowPoints(g_hwnd, nullptr, (POINT*)&r, 2);
        std::vector<std::wstring> fonts = SystemFonts();
        int sel = -1;
        std::wstring cur = g_defaultFont;
        if (Doc* d = ActiveDoc()) {
            const Cell* cell = d->Find(d->selFR, d->selFC);
            if (cell) cur = EffectiveFont(cell->st);
        }
        for (int i = 0; i < (int)fonts.size(); ++i) if (fonts[i] == cur) { sel = i; break; }
        ShowDropList(g_hwnd, r, fonts, sel, 15, 0);   // 0 = font face
    } else if (id == 2) {
        RECT r = Buttons().size;
        MapWindowPoints(g_hwnd, nullptr, (POINT*)&r, 2);
        std::vector<std::wstring> sizes;
        int stdSizes[] = { 8, 9, 10, 11, 12, 14, 16, 18, 20, 22, 24, 26, 28, 32, 36, 48, 72 };
        int cur = g_defaultSize, sel = -1;
        if (Doc* d = ActiveDoc()) {
            const Cell* cell = d->Find(d->selFR, d->selFC);
            if (cell) cur = EffectiveSize(cell->st);
        }
        for (int i = 0; i < (int)(sizeof(stdSizes) / sizeof(stdSizes[0])); ++i) {
            sizes.push_back(std::to_wstring(stdSizes[i]));
            if (stdSizes[i] == cur) sel = i;
        }
        ShowDropList(g_hwnd, r, sizes, sel, 12, 1);   // 1 = point size
    } else if (id >= 3 && id <= 5) {
        int target = id - 3;
        COLORREF cur = (target == 0) ? g_btnFg : (target == 1) ? g_btnOutline : g_btnCellBg;
        if (arrow || cur == CLR_NONE) {
            ToolLayout t = Buttons();
            RECT r = (target == 0) ? t.fg : (target == 1) ? t.outline : t.cellbg;
            MapWindowPoints(g_hwnd, nullptr, (POINT*)&r, 2);
            ShowPalette(g_hwnd, r, target);
        } else {
            ApplyColorToSelection(target, cur);
        }
    } else if (id >= 6 && id <= 8) {
        ApplyAlignToSelection(id - 6);
    } else if (id >= 13 && id <= 15) {
        ApplyVAlignToSelection(id - 13);
    } else if (id == 16) {
        ToggleMergeSelection();
    } else if (id == 17) {
        // Display-only cycling: the stored text/formula keeps full precision,
        // so returning to 原值 shows the original value again.
        g_numMode = (g_numMode + 1) % 3;
        ++g_contentRev;                    // re-resolve cached CellDisplay
        if (g_hwnd) InvalidateRect(g_hwnd, nullptr, FALSE);
    } else if (id == 9) {
        CmdSum();
    } else if (id == 10) {
        g_actions.Post(UiAction::RowHeight);
    } else if (id == 11) {
        g_actions.Post(UiAction::ColWidth);
    } else if (id == 12) {
        RECT r = Buttons().op;
        MapWindowPoints(g_hwnd, nullptr, (POINT*)&r, 2);
        ShowMenuAt(BuildOpMenu(), r.left, r.bottom + DpiScale(2));
    }
}


void TabBar::MouseDown(POINT pt, const UiMods& m)
{
    if (m.button != 1) return;
    TabLayout t = Tabs();
    for (int i = 0; i < (int)t.tab.size(); ++i) {
        if (PtInRect(&t.tab[i], pt)) {
            if (PtInRect(&t.close[i], pt)) {
                if (g_docs[i]->locked) SetStatus(L"标签页已锁定");
                else g_actions.Post(UiAction::CloseTab, i);
            } else if (i != g_active) {
                if (EditorActive()) CommitEditor(false, 0);
                CloseAllPopups();
                g_active = i;
                RebuildAllLayouts();
                UpdateWindowTitle();
                InvalidateAll();
            }
            return;
        }
    }
    if (PtInRect(&t.plus, pt)) {
        AddBlankDoc();
    }
}


void TabBar::MouseMove(POINT pt, const UiMods&)
{
    int tab = -1;
    bool tabClose = false;
    TabLayout t = Tabs();
    for (int i = 0; i < (int)t.tab.size(); ++i) {
        if (PtInRect(&t.tab[i], pt)) {
            tab = i;
            tabClose = PtInRect(&t.close[i], pt) != 0;
            break;
        }
    }
    if (tab < 0 && PtInRect(&t.plus, pt)) tab = -2;
    if (tab != hover || tabClose != hoverClose) {
        hover = tab;
        hoverClose = tabClose;
        if (g_hwnd) InvalidateRect(g_hwnd, nullptr, FALSE);
    }
}


void TabBar::MouseLeave()
{
    if (hover == -1 && !hoverClose) return;
    hover = -1;
    hoverClose = false;
    if (g_hwnd) InvalidateRect(g_hwnd, &rect, FALSE);
}


void TabBar::ContextMenu(POINT pt, const UiMods&)
{
    TabLayout t = Tabs();
    for (int i = 0; i < (int)t.tab.size(); ++i) {
        if (!PtInRect(&t.tab[i], pt)) continue;
        menuIndex = i;
        bool locked = g_docs[i]->locked;
        MenuItem lockItem;
        lockItem.id = 1501;
        lockItem.text = locked ? L"解锁标签页" : L"锁定标签页";
        std::vector<MenuItem> items = { lockItem };
        POINT sp = pt;
        ClientToScreen(g_hwnd, &sp);
        ShowMenuAt(items, sp.x, sp.y);
        return;
    }
}


