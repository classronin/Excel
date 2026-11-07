// ============================================================================
//  p6a_grid_paint.cpp
//  Responsibility: grid geometry, the ONE paint-spec renderer, and the grid's
//  per-frame spec declaration.
//
//  No function in this file paints a document. GridView::BuildSpec() reads the
//  document during the frame commit and emits plain paint primitives; the
//  render layer (UiObject::Paint -> ExecutePaintSpec) only executes them.
// ============================================================================
#include "core.h"
#include "api_p1_core.h"
#include "api_p2_doc.h"
#include "api_p3_formula.h"
#include "api_p4_image.h"
#include "api_p6a_grid_paint.h"
#include "api_p6b_grid_input.h"
#include "api_p7_chrome.h"

// ---------------------------------------------------------------------------
//  window / grid metrics (layout data, shared with the input objects)
// ---------------------------------------------------------------------------
void SetStatus(const std::wstring& s)
{ g_ui.statusModel.Show(s); }

int MenuH()   { return DpiScale(30); }
int ToolH()   { return DpiScale(42); }
int TabH()    { return DpiScale(22); }
int StatusH() { return DpiScale(24); }
int HeaderW() { return DpiScale(50); }
int HeaderH() { return DpiScale(26); }
int ScrollW() { return DpiScale(13); }

RECT ClientRect()
{
    RECT r = { 0, 0, 0, 0 };
    if (g_hwnd) GetClientRect(g_hwnd, &r);
    return r;
}

RECT GridAreaRect()
{
    RECT c = ClientRect();
    int top = TitleH() + MenuH() + ToolH();
    int bottom = std::max(top, (int)c.bottom - StatusH() - TabH());
    return { 0, top, c.right, bottom };
}

RECT TabBarRect()
{
    RECT c = ClientRect();
    int barTop = (int)c.bottom - StatusH() - TabH();
    int minTop = TitleH() + MenuH() + ToolH() + TabH();
    if (barTop < minTop) barTop = minTop;
    return { 0, barTop, c.right, std::max(barTop, (int)c.bottom - StatusH()) };
}

RECT StatusRect()
{
    RECT c = ClientRect();
    int top = std::max(0, (int)c.bottom - StatusH());
    return { 0, top, c.right, c.bottom };
}

RECT GridBodyRect()
{
    RECT g = GridAreaRect();
    RECT b = { g.left + HeaderW(), g.top + HeaderH(), g.right - ScrollW(), g.bottom - ScrollW() };
    if (b.right < b.left) b.right = b.left;
    if (b.bottom < b.top) b.bottom = b.top;
    return b;
}

RECT GridGripRect() { return { 0, 0, 0, 0 }; }

int ContentLeft(Doc& d) { return GridBodyRect().left - d.scrollX; }
int ContentTop(Doc& d)  { return GridBodyRect().top - d.scrollY; }

int XOfCol(Doc& d, int c)
{ d.RebuildLayout(); if (d.cols <= 0) return ContentLeft(d); c = std::max(0, std::min(d.cols, c)); return ContentLeft(d) + d.colOff[c]; }

int YOfRow(Doc& d, int r)
{ d.RebuildLayout(); if (d.rows <= 0) return ContentTop(d); r = std::max(0, std::min(d.rows, r)); return ContentTop(d) + d.rowOff[r]; }

int FindIndexForPos(const std::vector<int>& off, int pos)
{
    if (off.size() < 2) return 0;
    auto it = std::upper_bound(off.begin(), off.end(), pos);
    int idx = (int)(it - off.begin()) - 1;
    if (idx < 0) idx = 0;
    if (idx >= (int)off.size() - 1) idx = (int)off.size() - 2;
    return idx;
}

int ColAtX(Doc& d, int x)
{
    d.RebuildLayout();
    return FindIndexForPos(d.colOff, x - ContentLeft(d));
}

int RowAtY(Doc& d, int y)
{
    d.RebuildLayout();
    return FindIndexForPos(d.rowOff, y - ContentTop(d));
}

RECT CellRect(Doc& d, int r, int c)
{
    if (d.rows <= 0 || d.cols <= 0) return { 0, 0, 0, 0 };
    r = std::max(0, std::min(d.rows - 1, r));
    c = std::max(0, std::min(d.cols - 1, c));
    RECT rc;
    rc.left = XOfCol(d, c);
    rc.top = YOfRow(d, r);
    rc.right = rc.left + d.ColPx(c);
    rc.bottom = rc.top + d.RowPx(r);
    return rc;
}

RECT SelectionScreenRect(Doc& d)
{
    int r1, c1, r2, c2;
    d.SelectionRect(r1, c1, r2, c2);
    RECT a = CellRect(d, r1, c1);
    RECT b = CellRect(d, r2, c2);
    return { a.left, a.top, b.right, b.bottom };
}

void EnsureCellVisible(Doc& d, int r, int c)
{
    if (d.rows <= 0 || d.cols <= 0) return;
    r = std::max(0, std::min(d.rows - 1, r));
    c = std::max(0, std::min(d.cols - 1, c));
    RECT b = GridBodyRect();
    int x1 = XOfCol(d, c), x2 = x1 + d.ColPx(c);
    int y1 = YOfRow(d, r), y2 = y1 + d.RowPx(r);
    // Selection navigation is a device too: it only states the view it needs.
    ViewRequest req{ d.scrollX, d.scrollY };
    bool moved = false;
    if (x1 < b.left) { req.x -= (b.left - x1); moved = true; }
    else if (x2 > b.right) { req.x += (x2 - b.right); moved = true; }
    if (y1 < b.top) { req.y -= (b.top - y1); moved = true; }
    else if (y2 > b.bottom) { req.y += (y2 - b.bottom); moved = true; }
    if (moved) g_ui.grid.ApplyViewRequest(d, req);
}

void SetSelection(Doc& d, int r1, int c1, int r2, int c2, bool)
{
    if (r1 < 0) r1 = 0;
    if (c1 < 0) c1 = 0;
    if (r2 >= d.rows) r2 = d.rows - 1;
    if (c2 >= d.cols) c2 = d.cols - 1;
    d.selAR = r1;
    d.selAC = c1;
    d.selFR = r2;
    d.selFC = c2;
}


// ---------------------------------------------------------------------------
//  the ONE renderer
// ---------------------------------------------------------------------------
// Every case below is a GDI call with parameters that were fully resolved by
// some object's BuildSpec(). There is no document, theme, clock, selection,
// font-cache or geometry lookup here - that is the entire point.
// Pure drawing cull. Bounds come only from already-resolved primitive values;
// no state is read and no decision is recomputed.
static RECT PrimBounds(const PaintPrim& p)
{
    switch (p.op) {
    case PaintOp::HLine:
        return { p.rc.left, p.rc.top, p.rc.right, p.rc.top + 1 };
    case PaintOp::VLine:
        return { p.rc.left, p.rc.top, p.rc.left + 1, p.rc.bottom };
    case PaintOp::CloseX:
    case PaintOp::Triangle:
    case PaintOp::Lock:
    case PaintOp::CheckMark:
    case PaintOp::Chevron: {
        int r = std::max(2, p.w * 2);
        return { p.p1 - r, p.p2 - r, p.p1 + r, p.p2 + r };
    }
    default:
        return p.rc;
    }
}

void ExecutePaintSpec(HDC dc, const PaintSpec& spec, const RECT* clip)
{
    int clipDepth = 0;
    if (clip) {
        SaveDC(dc);
        IntersectClipRect(dc, clip->left, clip->top, clip->right, clip->bottom);
        ++clipDepth;
    }
    for (const PaintPrim& p : spec.prims) {
        if (clip && p.op != PaintOp::PushClip && p.op != PaintOp::PopClip) {
            RECT bounds = PrimBounds(p);
            RECT hit;
            if (!IntersectRect(&hit, &bounds, clip)) continue;
        }
        switch (p.op) {
        case PaintOp::FillRect:
            FillRectC(dc, p.rc, p.color);
            break;
        case PaintOp::HLine:
            HLine(dc, p.rc.left, p.rc.right, p.rc.top, p.color, p.w);
            break;
        case PaintOp::VLine:
            VLine(dc, p.rc.left, p.rc.top, p.rc.bottom, p.color, p.w);
            break;
        case PaintOp::FrameRect:
            FrameRectC(dc, p.rc, p.color, p.w);
            break;
        case PaintOp::Text:
            TextIn(dc, p.text, p.rc, p.color, p.font, p.flags);
            break;
        case PaintOp::OutlinedText:
            DrawOutlinedText(dc, p.text, p.rc, p.color, p.color2, p.font, p.flags);
            break;
        case PaintOp::PushClip:
            SaveDC(dc);
            IntersectClipRect(dc, p.rc.left, p.rc.top, p.rc.right, p.rc.bottom);
            ++clipDepth;
            break;
        case PaintOp::PopClip:
            if (clipDepth > 0) { RestoreDC(dc, -1); --clipDepth; }
            break;
        case PaintOp::Icon:
            if (p.icon)
                DrawIconEx(dc, p.rc.left, p.rc.top, p.icon,
                           p.rc.right - p.rc.left, p.rc.bottom - p.rc.top,
                           0, nullptr, DI_NORMAL);
            break;
        case PaintOp::Image: {
            // Frame output only; animation lifetime belongs to SyncAnimation.
            bool animated = false;
            DrawImageCell(dc, p.text, p.rc, animated);
            break;
        }
        case PaintOp::CloseX:
            DrawCloseX(dc, p.p1, p.p2, p.w, p.color);
            break;
        case PaintOp::Triangle:
            DrawTriangleDown(dc, p.p1, p.p2, p.w, p.color);
            break;
        case PaintOp::Lock:
            DrawLockGlyph(dc, p.p1, p.p2, p.color);
            break;
        case PaintOp::CheckMark:
            DrawCheckMark(dc, p.p1, p.p2, p.color);
            break;
        case PaintOp::Chevron:
            DrawChevron(dc, p.p1, p.p2, p.w, p.color, p.p3 != 0);
            break;
        }
    }
    while (clipDepth > 0) { RestoreDC(dc, -1); --clipDepth; }
}


// ---------------------------------------------------------------------------
//  presentation surface
//
//  The render layer's only way to put a declaration on screen: compose the
//  whole area offscreen, then hand it over in one BitBlt. Executing primitives
//  directly onto a window DC exposes every intermediate state - a scrolled
//  view would show the background fill before the cells - which is exactly the
//  flash the user sees while moving the view.
// ---------------------------------------------------------------------------
HDC PaintSurface::Begin(HDC target, const RECT& a)
{
    int aw = (int)(a.right - a.left);
    int ah = (int)(a.bottom - a.top);
    if (aw <= 0 || ah <= 0) return nullptr;
    if (!dc) dc = CreateCompatibleDC(target);
    if (!dc) return nullptr;
    if (!bmp || w < aw || h < ah) {
        // Grow in fixed steps, never shrink: a hover band and a full-window
        // repaint alternate constantly and must not reallocate every frame.
        int nw = ((aw + 255) / 256) * 256;
        int nh = ((ah + 255) / 256) * 256;
        if (nw < w) nw = w;
        if (nh < h) nh = h;
        if (bmp) { SelectObject(dc, old); DeleteObject(bmp); bmp = nullptr; }
        bmp = CreateCompatibleBitmap(target, nw, nh);
        if (!bmp) return nullptr;
        old = SelectObject(dc, bmp);
        w = nw;
        h = nh;
    }
    area = a;
    // Composition runs in the target's own coordinate space; the surface is
    // clipped to the area so a larger reused bitmap never leaks stale pixels.
    SaveDC(dc);
    SetViewportOrgEx(dc, -a.left, -a.top, nullptr);
    IntersectClipRect(dc, a.left, a.top, a.right, a.bottom);
    return dc;
}


void PaintSurface::End(HDC target)
{
    if (!dc) return;
    RestoreDC(dc, -1);
    SetViewportOrgEx(dc, 0, 0, nullptr);
    int aw = (int)(area.right - area.left);
    int ah = (int)(area.bottom - area.top);
    if (aw > 0 && ah > 0) BitBlt(target, area.left, area.top, aw, ah, dc, 0, 0, SRCCOPY);
}


void PaintSurface::Release()
{
    if (dc) {
        if (bmp) SelectObject(dc, old);
        DeleteDC(dc);
        dc = nullptr;
    }
    if (bmp) { DeleteObject(bmp); bmp = nullptr; }
    old = nullptr;
    w = h = 0;
}


void PresentSpec(HDC target, PaintSurface& surface, const RECT& area, const PaintSpec& spec)
{
    HDC dc = surface.Begin(target, area);
    if (!dc) return;
    ExecutePaintSpec(dc, spec, &area);
    surface.End(target);
}


const wchar_t* CellTag(int kind)
{
    switch (kind) {
    case CK_FILE:   return L"FILE";
    case CK_FOLDER: return L"DIR";
    case CK_IMAGE:  return L"IMG";
    case CK_URL:    return L"URL";
    default:        return L"";
    }
}


void DrawOutlinedText(HDC dc, const std::wstring& s, RECT rc, COLORREF fg, COLORREF outline, HFONT font, UINT flags)
{
    if (s.empty()) return;
    if (outline == CLR_NONE) {
        TextIn(dc, s, rc, fg, font, flags);
        return;
    }
    SaveDC(dc);
    const int halo = 1;
    IntersectClipRect(dc, rc.left - halo, rc.top - halo, rc.right + halo, rc.bottom + halo);
    SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    static const int dirs[8][2] = { {-1,0}, {1,0}, {0,-1}, {0,1}, {-1,-1}, {1,-1}, {-1,1}, {1,1} };
    int radius = 1;
    for (int r = 1; r <= radius; ++r) {
        SetTextColor(dc, outline);
        for (auto& dir : dirs) {
            RECT rr = rc;
            OffsetRect(&rr, dir[0] * r, dir[1] * r);
            DrawTextW(dc, s.c_str(), (int)s.size(), &rr, flags | DT_NOPREFIX);
        }
    }
    SetTextColor(dc, fg);
    DrawTextW(dc, s.c_str(), (int)s.size(), &rc, flags | DT_NOPREFIX);
    RestoreDC(dc, -1);
}


// ---------------------------------------------------------------------------
//  grid layout & scrollbar projection (logic layer, no painting)
// ---------------------------------------------------------------------------
void GridView::Layout(const RECT& r)
{
    rect = r;
    vbar.SetTrack({ r.right - ScrollW(), r.top + HeaderH(), r.right, r.bottom - ScrollW() });
    hbar.SetTrack({ r.left + HeaderW(), r.bottom - ScrollW(), r.right - ScrollW(), r.bottom });
}

void GridView::SyncBars(Doc& d)
{
    d.RebuildLayout();
    RECT b = GridBodyRect();
    vbar.Sync(d.TotalHeight(), (int)(b.bottom - b.top), d.scrollY);
    hbar.Sync(d.TotalWidth(), (int)(b.right - b.left), d.scrollX);
}

bool GridView::OnScrollBar(POINT pt) const
{
    return vbar.HitTrack(pt) || hbar.HitTrack(pt);
}

static bool SelectionIsSingleMerge(Doc& d)
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


// ---------------------------------------------------------------------------
//  GridView::BuildSpec - the ONLY place the grid reads a document.
//
//  Called once per frame from UiRoot::CommitFrame with a measuring DC. Every
//  visible rectangle, string, font, colour, alignment and flag is resolved
//  here and appended as a value. Full paint, partial paint, animation frames
//  and the drag ghost all consume this same declaration.
// ---------------------------------------------------------------------------
void GridView::BuildSpec(HDC dc)
{
    PaintSpec& s = spec;
    s.Clear();

    RECT g = GridAreaRect();
    RECT b = GridBodyRect();
    s.Emit(PaintOp::FillRect, g, g_th.win);

    Doc* d = ActiveDoc();
    if (!d) {
        auto& t = s.Emit(PaintOp::Text, g, g_th.faint);
        t.text = L"按 Ctrl+N 新建表格，或按 Ctrl+O 打开 .db 文件";
        t.font = UiFont(11);
        t.flags = DT_SINGLELINE | DT_CENTER | DT_VCENTER;
        return;
    }
    d->RebuildLayout();

    int firstRow = RowAtY(*d, b.top);
    int lastRow  = std::min(d->rows - 1, RowAtY(*d, b.bottom - 1) + 1);
    int firstCol = ColAtX(*d, b.left);
    int lastCol  = std::min(d->cols - 1, ColAtX(*d, b.right - 1) + 1);
    int r1, c1, r2, c2;
    d->SelectionRect(r1, c1, r2, c2);

    auto emit = [&](PaintOp op, const RECT& rc, COLORREF color, int r, int c) -> PaintPrim& {
        PaintPrim& p = s.Emit(op, rc, color);
        p.ownerR = r;
        p.ownerC = c;
        return p;
    };

    // All content geometry is resolved here, including text measurement.
    auto emitCell = [&](int r, int c, const RECT& rc) {
        const Cell* cell = d->Find(r, c);
        COLORREF base = (cell && cell->st.cellBg != -1) ? (COLORREF)cell->st.cellBg : g_th.cell;
        emit(PaintOp::FillRect, rc, base, r, c);
        RECT hl = { rc.left, rc.bottom - 1, rc.right, rc.bottom - 1 };
        emit(PaintOp::HLine, hl, g_th.grid, r, c).w = 1;
        RECT vl = { rc.right - 1, rc.top, rc.right - 1, rc.bottom };
        emit(PaintOp::VLine, vl, g_th.grid, r, c).w = 1;

        int pad = DpiScale(4);
        RECT inner = { rc.left + pad, rc.top + 1, rc.right - pad, rc.bottom - 1 };
        if (inner.right <= inner.left || EditorCoversCell(d, r, c) || !cell) return;

        int kind = cell->kind;
        if (kind == CK_IMAGE && !cell->path.empty()) {
            RECT ir = { rc.left + 2, rc.top + 2, rc.right - 2, rc.bottom - 2 };
            auto& p = emit(PaintOp::Image, ir, 0, r, c);
            p.text = d->AbsPathOf(cell->path);
        } else if (kind == CK_FILE || kind == CK_FOLDER || kind == CK_URL) {
            const wchar_t* tag = CellTag(kind);
            int tagH = std::min((int)(inner.bottom - inner.top) - DpiScale(4), UiPxFromPt(7) + DpiScale(6));
            if (tagH < DpiScale(12)) tagH = DpiScale(12);
            int tagW = (int)MeasureText(dc, tag, UiFont(7, true)).cx + DpiScale(8);
            int ty = inner.top + ((inner.bottom - inner.top) - tagH) / 2;
            RECT tr = { inner.left, ty, inner.left + tagW, ty + tagH };
            emit(PaintOp::FillRect, tr, g_th.accentSoft, r, c);
            emit(PaintOp::FrameRect, tr, g_th.accent, r, c).w = 1;
            auto& tp = emit(PaintOp::Text, tr, g_th.accent, r, c);
            tp.text = tag;
            tp.font = UiFont(7, true);
            tp.flags = DT_SINGLELINE | DT_CENTER | DT_VCENTER;

            RECT tx = { tr.right + DpiScale(6), inner.top, inner.right, inner.bottom };
            std::wstring disp = cell->text.empty() ? PathBaseName(cell->path) : cell->text;
            HFONT dfont = CellFont(EffectiveFont(cell->st), EffectiveSize(cell->st),
                                   cell->st.bold, cell->st.italic);
            COLORREF outline = cell->st.outline != -1 ? (COLORREF)cell->st.outline : CLR_NONE;
            auto& dp = emit(PaintOp::OutlinedText, tx, g_th.text, r, c);
            dp.text = disp;
            dp.font = dfont;
            dp.color2 = outline;
            dp.flags = DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS;
            if (kind == CK_URL) {
                LONG ux = std::min<LONG>(tx.right, tx.left + (LONG)MeasureText(dc, disp, dfont).cx);
                RECT ul = { tx.left, inner.bottom - 2, ux, inner.bottom - 2 };
                emit(PaintOp::HLine, ul, g_th.accent, r, c).w = 1;
            }
        } else {
            std::wstring disp = CellDisplay(*d, r, c);
            if (!disp.empty()) {
                HFONT font = CellFont(EffectiveFont(cell->st), EffectiveSize(cell->st),
                                      cell->st.bold, cell->st.italic);
                COLORREF fg = cell->st.fg != -1 ? (COLORREF)cell->st.fg : g_th.text;
                int align = EffectiveHAlign(cell->st);
                UINT af = DT_LEFT;
                if (align == 1) af = DT_CENTER;
                else if (align == 2) af = DT_RIGHT;
                COLORREF outline = cell->st.outline != -1 ? (COLORREF)cell->st.outline : CLR_NONE;
                bool multi = disp.find_first_of(L"\r\n") != std::wstring::npos;
                // One geometry source for painting and for the native editor.
                RECT tr = CellTextRect(dc, inner, font, disp, EffectiveVAlign(cell->st), nullptr);
                UINT flags = af | DT_TOP;
                if (multi) flags |= DT_WORDBREAK | DT_EDITCONTROL;
                else       flags |= DT_SINGLELINE | DT_END_ELLIPSIS;
                auto& tp = emit(PaintOp::OutlinedText, tr, fg, r, c);
                tp.text = disp;
                tp.font = font;
                tp.color2 = outline;
                tp.flags = flags;
            }
            if (!cell->formula.empty()) {
                RECT fm = { rc.right - DpiScale(7), rc.top + DpiScale(2),
                            rc.right - DpiScale(2), rc.top + DpiScale(7) };
                emit(PaintOp::FillRect, fm, g_th.accent, r, c);
            }
        }
    };

    // ---- cells, clipped to the body ----
    s.Emit(PaintOp::PushClip, b, 0);
    // A merged range can start above or left of the viewport; declare those
    // first so the visible part is never left as window background.
    for (auto& m : d->merges) {
        RECT a = CellRect(*d, m.r1, m.c1);
        RECT z = CellRect(*d, m.r2, m.c2);
        RECT full = { a.left, a.top, z.right, z.bottom };
        RECT hit;
        if (!IntersectRect(&hit, &full, &b)) continue;
        emitCell(m.r1, m.c1, full);
    }
    for (int r = firstRow; r <= lastRow; ++r) {
        for (int c = firstCol; c <= lastCol; ++c) {
            if (d->MergeAt(r, c)) continue;
            RECT rc = CellRect(*d, r, c);
            if (rc.right < b.left || rc.left > b.right ||
                rc.bottom < b.top || rc.top > b.bottom) continue;
            emitCell(r, c, rc);
        }
    }
    int endY = YOfRow(*d, d->rows);
    if (endY < b.bottom) {
        RECT el = { b.left, endY, b.right, endY };
        s.Emit(PaintOp::HLine, el, g_th.grid).w = 1;
    }
    s.Emit(PaintOp::PopClip, RECT{}, 0);

    // ---- selection decorations ----
    s.Emit(PaintOp::PushClip, b, 0);
    RECT selectionRect = SelectionScreenRect(*d);
    RECT inter;
    if (IntersectRect(&inter, &selectionRect, &b)) {
        s.Emit(PaintOp::FrameRect, selectionRect, g_th.accent).w = DpiScale(1);
        if (!SelectionIsSingleMerge(*d)) {
            RECT fr = CellRect(*d, d->selFR, d->selFC);
            s.Emit(PaintOp::FrameRect, fr, g_th.accent).w = DpiScale(1);
        }
        if (gesture.Is(GestureKind::MoveCells) &&
            gesture.move.targetRow >= 0 && gesture.move.targetCol >= 0) {
            RECT dst = CellRect(*d, gesture.move.targetRow, gesture.move.targetCol);
            dst.right = dst.left + (selectionRect.right - selectionRect.left);
            dst.bottom = dst.top + (selectionRect.bottom - selectionRect.top);
            s.Emit(PaintOp::FrameRect, dst, g_th.accent).w = DpiScale(1);
            RECT inner = { dst.left + DpiScale(2), dst.top + DpiScale(2),
                           dst.right - DpiScale(2), dst.bottom - DpiScale(2) };
            if (inner.right > inner.left && inner.bottom > inner.top)
                s.Emit(PaintOp::FrameRect, inner, g_th.borderStrong).w = DpiScale(1);
        }
    }
    for (auto& e : d->selExtra) {
        RECT a = CellRect(*d, e.r1, e.c1);
        RECT z = CellRect(*d, e.r2, e.c2);
        RECT rr = { a.left, a.top, z.right, z.bottom };
        s.Emit(PaintOp::FrameRect, rr, g_th.accent).w = DpiScale(1);
    }
    s.Emit(PaintOp::PopClip, RECT{}, 0);

    // ---- headers (their own clip: partly visible captions use the slice) ----
    RECT colHdr = { g.left + HeaderW(), g.top, g.right - ScrollW(), g.top + HeaderH() };
    RECT rowHdr = { g.left, g.top + HeaderH(), g.left + HeaderW(), g.bottom - ScrollW() };
    RECT corner = { g.left, g.top, g.left + HeaderW(), g.top + HeaderH() };

    s.Emit(PaintOp::FillRect, colHdr, g_th.header);
    s.Emit(PaintOp::PushClip, colHdr, 0);
    for (int c = firstCol; c <= lastCol; ++c) {
        int x = XOfCol(*d, c);
        int w = d->ColPx(c);
        RECT rc = { x, colHdr.top, x + w, colHdr.bottom };
        if (rc.right < colHdr.left || rc.left > colHdr.right) continue;
        RECT vis = rc;
        if (vis.left < colHdr.left) vis.left = colHdr.left;
        if (vis.right > colHdr.right) vis.right = colHdr.right;
        bool sel = (c >= c1 && c <= c2);
        bool hot = (c == hoverCol);
        if (sel) s.Emit(PaintOp::FillRect, vis, g_th.headerSel);
        else if (hot) s.Emit(PaintOp::FillRect, vis, g_th.hover);
        RECT vl = { rc.right - 1, rc.top, rc.right - 1, rc.bottom };
        s.Emit(PaintOp::VLine, vl, g_th.grid).w = 1;
        std::wstring name;
        int n = c;
        do { name.insert(name.begin(), (wchar_t)(L'A' + (n % 26))); n = n / 26 - 1; } while (n >= 0);
        auto& t = s.Emit(PaintOp::Text, vis, sel ? g_th.accent : (hot ? g_th.text : g_th.dim));
        t.text = name;
        t.font = UiFont(9);
        t.flags = DT_SINGLELINE | DT_CENTER | DT_VCENTER;
    }
    s.Emit(PaintOp::PopClip, RECT{}, 0);
    RECT chl = { colHdr.left, colHdr.bottom - 1, colHdr.right, colHdr.bottom - 1 };
    s.Emit(PaintOp::HLine, chl, g_th.border).w = 1;
    RECT cvl = { colHdr.left - 1, colHdr.top, colHdr.left - 1, colHdr.bottom };
    s.Emit(PaintOp::VLine, cvl, g_th.border).w = 1;

    s.Emit(PaintOp::FillRect, rowHdr, g_th.header);
    s.Emit(PaintOp::PushClip, rowHdr, 0);
    for (int r = firstRow; r <= lastRow; ++r) {
        int y = YOfRow(*d, r);
        int h = d->RowPx(r);
        RECT rc = { rowHdr.left, y, rowHdr.right, y + h };
        if (rc.bottom < rowHdr.top || rc.top > rowHdr.bottom) continue;
        RECT vis = rc;
        if (vis.top < rowHdr.top) vis.top = rowHdr.top;
        if (vis.bottom > rowHdr.bottom) vis.bottom = rowHdr.bottom;
        bool sel = (r >= r1 && r <= r2);
        bool hot = (r == hoverRow);
        if (sel) s.Emit(PaintOp::FillRect, vis, g_th.headerSel);
        else if (hot) s.Emit(PaintOp::FillRect, vis, g_th.hover);
        RECT hl = { rc.left, rc.bottom - 1, rc.right, rc.bottom - 1 };
        s.Emit(PaintOp::HLine, hl, g_th.grid).w = 1;
        RECT tr = { vis.left + DpiScale(4), vis.top, vis.right - DpiScale(9), vis.bottom };
        auto& t = s.Emit(PaintOp::Text, tr, sel ? g_th.accent : (hot ? g_th.text : g_th.dim));
        t.text = std::to_wstring(r + 1);
        t.font = MonoFont(9);
        t.flags = DT_SINGLELINE | DT_RIGHT | DT_VCENTER;
    }
    s.Emit(PaintOp::PopClip, RECT{}, 0);
    RECT rhl = { rowHdr.left, rowHdr.bottom - 1, rowHdr.right, rowHdr.bottom - 1 };
    s.Emit(PaintOp::HLine, rhl, g_th.border).w = 1;
    RECT rvl = { rowHdr.right - 1, rowHdr.top, rowHdr.right - 1, rowHdr.bottom };
    s.Emit(PaintOp::VLine, rvl, g_th.border).w = 1;

    s.Emit(PaintOp::FillRect, corner, g_th.header);
    s.Emit(PaintOp::FrameRect, corner, g_th.border).w = 1;
    int cx = (corner.left + corner.right) / 2;
    int cy = (corner.top + corner.bottom) / 2;
    RECT c1l = { cx - DpiScale(6), cy - DpiScale(2), cx + DpiScale(6), cy - DpiScale(2) };
    s.Emit(PaintOp::HLine, c1l, g_th.dim).w = 1;
    RECT c2l = { cx - DpiScale(2), cy - DpiScale(6), cx - DpiScale(2), cy + DpiScale(6) };
    s.Emit(PaintOp::VLine, c2l, g_th.dim).w = 1;

    // ---- scrollbars are a projection; Sync is a logic-layer operation ----
    SyncBars(*d);
    auto emitBar = [&](const ScrollBar& bar) {
        s.Emit(PaintOp::FillRect, bar.track, g_th.panel);
        if (bar.MaxOffset() > 0) {
            s.Emit(PaintOp::FillRect, bar.thumb, bar.dragging ? g_th.accent : g_th.borderStrong);
        } else if (bar.axis == ScrollBar::Vertical) {
            RECT tip = { bar.track.left + 2, bar.track.top,
                         bar.track.right - 2, bar.track.top + DpiScale(3) };
            s.Emit(PaintOp::FillRect, tip, g_th.border);
        } else {
            RECT tip = { bar.track.left, bar.track.top + 2,
                         bar.track.left + DpiScale(3), bar.track.bottom - 2 };
            s.Emit(PaintOp::FillRect, tip, g_th.border);
        }
    };
    emitBar(vbar);
    emitBar(hbar);
    RECT barCorner = { rect.right - ScrollW(), rect.bottom - ScrollW(), rect.right, rect.bottom };
    s.Emit(PaintOp::FillRect, barCorner, g_th.panel);
    s.Emit(PaintOp::FrameRect, barCorner, g_th.border).w = 1;
}
