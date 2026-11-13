// ============================================================================
//  p8b_app.cpp
//  Responsibility: application shell
//  Menus, UiRoot composition, window procedure and the message loop.
//
//  Shared contract: core.h + only the api_*.h headers this module references.
//  This translation unit contains no type or state definitions of its own (state.cpp does).
// ============================================================================
#include "core.h"
#include "api_p1_core.h"
#include "api_p2_doc.h"
#include "api_p3_formula.h"
#include "api_p4_image.h"
#include "api_p5_popup.h"
#include "api_p6a_grid_paint.h"
#include "api_p6b_grid_input.h"
#include "api_p7_chrome.h"
#include "api_p8a_commands.h"
#include "api_p8b_app.h"
// ---- src\p8b_app.inc ----
// ============================================================================
//  Excel - part 8b: menus, main window, entry point
// ============================================================================
void UpdateWindowTitle()
{
    if (!g_hwnd) return;
    Doc* d = ActiveDoc();
    std::wstring title;
    if (d) {
        title = (d->dirty ? L"* " : L"") + d->title;
        title += L"  —  ";
    }
    title += APP_NAME;
    SetWindowTextW(g_hwnd, title.c_str());
}


void InvalidateAll()
{
    // A redraw request is NOT a data change: cached display values stay valid.
    // Only Doc::Touch / Undo / Redo bump the content revision.
    if (g_hwnd) InvalidateRect(g_hwnd, nullptr, FALSE);
}



// ---------------------------------------------------------------------------
//  menu construction
// ---------------------------------------------------------------------------
MenuItem MItem(int id, const wchar_t* text, const wchar_t* shortcut , bool enabled , bool checked )
{
    MenuItem m;
    m.id = id;
    m.text = text;
    if (shortcut) m.shortcut = shortcut;
    m.kind = MK_NORMAL;
    m.enabled = enabled;
    m.checked = checked;
    return m;
}


MenuItem MSep()
{
    MenuItem m;
    m.kind = MK_SEP;
    return m;
}


MenuItem MSub(const wchar_t* text, std::vector<MenuItem> sub, bool enabled )
{
    MenuItem m;
    m.text = text;
    m.kind = MK_SUB;
    m.sub = std::move(sub);
    m.enabled = enabled;
    return m;
}


MenuItem MCheck(int id, const wchar_t* text, bool checked)
{
    MenuItem m;
    m.id = id;
    m.text = text;
    m.kind = MK_CHECK;
    m.checked = checked;
    return m;
}


std::vector<MenuItem> BuildImportMenu()
{
    return {
        MItem(ID_IMPORT_FILE, L"文件…"),
        MItem(ID_IMPORT_FOLDER, L"文件夹…"),
        MItem(ID_IMPORT_IMAGE, L"图像…"),
        MItem(ID_IMPORT_URL, L"网址…")
    };
}


std::vector<MenuItem> BuildInsertMenu()
{
    return {
        MItem(ID_INSERT_UP, L"上方插入行"),
        MItem(ID_INSERT_DOWN, L"下方插入行"),
        MItem(ID_INSERT_LEFT, L"左侧插入列"),
        MItem(ID_INSERT_RIGHT, L"右侧插入列")
    };
}


std::vector<MenuItem> BuildFileMenu()
{
    bool has = ActiveDoc() != nullptr;
    return {
        MItem(ID_FILE_NEW, L"新建", L"Ctrl+N"),
        MItem(ID_FILE_OPEN, L"打开…", L"Ctrl+O"),
        MSep(),
        MItem(ID_FILE_SAVE, L"保存", L"Ctrl+S", has),
        MItem(ID_FILE_SAVEAS, L"另存为…", L"Ctrl+Shift+S", has),
        MSep(),
        MItem(ID_FILE_EXIT, L"退出", L"Alt+F4")
    };
}


std::vector<MenuItem> BuildOpMenu()
{
    return {
        MItem(ID_OP_ADD, L"＋ 相加"),
        MItem(ID_OP_SUB, L"− 相减"),
        MItem(ID_OP_MUL, L"× 相乘"),
        MItem(ID_OP_DIV, L"÷ 相除")
    };
}


std::vector<MenuItem> BuildEditMenu()
{
    Doc* d = ActiveDoc();
    bool has = d != nullptr;
    return {
        MItem(ID_EDIT_CUT, L"剪切", L"Ctrl+X", has),
        MItem(ID_EDIT_COPY, L"复制", L"Ctrl+C", has),
        MItem(ID_EDIT_PASTE, L"粘贴", L"Ctrl+V", has),
        MSep(),
        MSub(L"导入", BuildImportMenu(), has),
        MItem(ID_EDIT_SUM, L"总和", L"Alt+=", has),
        MSub(L"运算", BuildOpMenu(), has)
    };
}


std::vector<MenuItem> BuildTopMenuItems(int index)
{
    if (index == 0) return BuildFileMenu();
    if (index == 1) return BuildEditMenu();
    return {};
}


std::vector<MenuItem> BuildContextMenu()
{
    return BuildEditMenu();
}


void OpenTopMenu(int index)
{
    if (index < 0 || index >= kMenuCount) return;
    if (index == 2) {
        CloseAllPopups();
        g_actions.Post(UiAction::OpenSettings);
        return;
    }
    if (index == 3) {
        CloseAllPopups();
        g_actions.Post(UiAction::OpenAbout);
        return;
    }
    RECT r = g_ui.menu.TopRect(index);
    POINT p = { r.left, r.bottom };
    ClientToScreen(g_hwnd, &p);
    CloseListPopup();
    ClosePalettePopup();
    ShowMenuAt(BuildTopMenuItems(index), p.x, p.y);
    g_ui.menuPopup.topIndex = index;
    g_ui.menu.hover = -1;
    InvalidateRect(g_hwnd, nullptr, FALSE);
}


// ---------------------------------------------------------------------------
//  logic-layer view state (computed before any paint)
// ---------------------------------------------------------------------------
void GridView::CommitPendingEdit()
{
    if (EditorActive()) CommitEditor(false, 0);
}


void GridView::PositionEditorControl()
{
    if (EditorActive()) PositionEditor();
}


void GridView::DropFiles(const std::vector<std::wstring>& paths, POINT pt)
{
    if (paths.empty()) return;
    if (!ActiveDoc()) AddBlankDoc();
    Doc* d = ActiveDoc();
    if (!d) return;
    int r = d->selFR, c = d->selFC;
    RECT body = GridBodyRect();
    if (PtInRect(&body, pt)) {          // the object decides what the point means
        r = RowAtY(*d, pt.y);
        c = ColAtX(*d, pt.x);
    }
    ImportPathsAt(*d, paths, r, c);
    if (g_hwnd) {
        SetForegroundWindow(g_hwnd);
        InvalidateAll();
    }
}


void GridView::SyncAnimation()
{
    Doc* d = ActiveDoc();
    AnimSig s;
    s.doc = d;
    s.rev = g_contentRev;
    s.sx = d ? d->scrollX : -1;
    s.sy = d ? d->scrollY : -1;
    s.zoom = g_gridZoom;
    s.uiZoom = g_uiZoom;
    s.dpi = g_dpi;
    s.rows = d ? d->rows : 0;
    s.cols = d ? d->cols : 0;
    s.active = g_active;
    s.l = rect.left; s.t = rect.top; s.r = rect.right; s.b = rect.bottom;
    if (s.Same(animSig)) return;        // O(1): nothing changed
    animSig = s;
    animCells.clear();
    if (!d) return;

    RECT b = GridBodyRect();
    if (b.right <= b.left || b.bottom <= b.top) return;
    d->RebuildLayout();
    int firstRow = RowAtY(*d, b.top);
    int lastRow = std::min(d->rows - 1, RowAtY(*d, b.bottom - 1) + 1);
    int firstCol = ColAtX(*d, b.left);
    int lastCol = std::min(d->cols - 1, ColAtX(*d, b.right - 1) + 1);

    auto animatedAt = [&](int r, int c, const RECT& rc) {
        const Cell* cell = d->Find(r, c);
        if (!cell || cell->kind != CK_IMAGE || cell->path.empty()) return false;
        int w = rc.right - rc.left, h = rc.bottom - rc.top;
        if (w < 8 || h < 8) return false;
        auto img = GetImage(d->AbsPathOf(cell->path), w - 4, h - 4);
        return img && img->Animated();
    };

    for (auto& m : d->merges) {
        RECT a = CellRect(*d, m.r1, m.c1);
        RECT z = CellRect(*d, m.r2, m.c2);
        RECT full = { a.left, a.top, z.right, z.bottom };
        RECT hit;
        if (!IntersectRect(&hit, &full, &b)) continue;
        if (animatedAt(m.r1, m.c1, full)) animCells.push_back({ d, m.r1, m.c1, full });
    }
    for (int r = firstRow; r <= lastRow; ++r) {
        for (int c = firstCol; c <= lastCol; ++c) {
            if (d->MergeAt(r, c)) continue;
            RECT rc = CellRect(*d, r, c);
            if (rc.right < b.left || rc.left > b.right || rc.bottom < b.top || rc.top > b.bottom) continue;
            if (animatedAt(r, c, rc)) animCells.push_back({ d, r, c, rc });
        }
    }
}


// ---------------------------------------------------------------------------
//  painting - there is no per-object Paint implementation any more. Every
//  object declares a spec in CommitFrame and UiObject::Paint executes it.
// ---------------------------------------------------------------------------
bool GridView::Cursor(POINT pt)
{
    UpdateCursorForPoint(pt);
    return true;
}


void GridView::EndPan(POINT pt)
{
    pan.End();
    // Capacity is reconciled in the frame commit, never on a gesture boundary.
    UpdateCursorForPoint(pt);
}


void GridView::ApplyViewRequest(Doc& d, const ViewRequest& req)
{
    // One entry for wheel / keyboard / middle-drag pan / scrollbar / zoom /
    // selection navigation. Devices only state where the view should be; the
    // VIEW LAYER settles the request in this fixed order:
    //   1. record the neutral intent
    //   2. declare the capacity that view needs (exact rows)
    //   3. pure clamp against the declared capacity
    //   4. commit the scroll with an incremental paint
    // No device sizes the sheet, and no device has its own capacity rule.
    RECT b = GridBodyRect();
    int vw = (int)(b.right - b.left);
    int vh = (int)(b.bottom - b.top);
    d.NoteViewRequest(req);
    d.ReconcileCapacity(vw, vh);
    POINT t = d.ResolveView(req, vw, vh);
    ScrollLive(d, t.x, t.y);
}


void GridView::ScrollLive(Doc& d, int nx, int ny)
{
    RECT b = GridBodyRect();
    int bodyW = (int)(b.right - b.left);
    int bodyH = (int)(b.bottom - b.top);
    int cx = std::max(0, std::min(d.MaxScrollX(bodyW), nx));
    int cy = std::max(0, std::min(d.MaxScrollY(bodyH), ny));
    // A request that runs past the current trailing edge is not discarded: the
    // view cannot go there yet, but the frame commit declares the capacity
    // that makes it reachable, so the frame still has to be produced.
    bool beyond = (nx > cx || ny > cy);
    if (cx == d.scrollX && cy == d.scrollY && !beyond) return;
    d.scrollX = cx;
    d.scrollY = cy;
    // Input commits state only. The frame loop re-declares every spec and
    // repaints the grid region from that one declaration. No device path owns
    // a ScrollWindowEx, a bitblt or a header repaint any more - which also
    // removes the old "frozen header / clipped digit" class of bugs.
    if (g_hwnd) InvalidateRect(g_hwnd, &rect, FALSE);
}


void GridView::ScrollBarDrag(ScrollBar& bar, Doc& d, POINT pt)
{
    // The bar reads the pointer against the scale it captured on press, so the
    // thumb sits under the hand for the whole gesture. The request is a plain
    // position; reaching past the trailing edge is how any device says "more
    // sheet", and the capacity rule, not this bar, decides what that means.
    int target = bar.DragTarget(pt);
    ViewRequest req;
    req.x = (bar.axis == ScrollBar::Vertical) ? d.scrollX : target;
    req.y = (bar.axis == ScrollBar::Vertical) ? target : d.scrollY;
    ApplyViewRequest(d, req);
}


bool GridView::ScrollBarUp()
{
    if (!vbar.dragging && !hbar.dragging) return false;
    vbar.EndDrag();
    hbar.EndDrag();
    // Releasing states nothing: every request was already handed over frame by
    // frame, and what is drawn is drawn from the view, never from the gesture.
    HideBand();
    InvalidateRect(g_hwnd, nullptr, FALSE);
    return true;
}


bool GridView::TickAnimation()
{
    if (!NeedsAnimation()) return false;
    if (!IsWindowVisible(g_hwnd) || IsIconic(g_hwnd)) return false;
    for (auto& a : animCells) InvalidateRect(g_hwnd, &a.rc, FALSE);
    return true;
}


void GridView::CaptureLost()
{
    pan.Reset();
    vbar.EndDrag();
    hbar.EndDrag();
    if (gesture.Is(GestureKind::MoveCells)) DestroyGhost();
    gesture.Reset();
    HideBand();
}


// ---------------------------------------------------------------------------
//  root object: layout, one-way paint, neutral input routing
// ---------------------------------------------------------------------------
void UiRoot::Layout()
{
    RECT c = ClientRect();
    title.Layout({ 0, 0, c.right, TitleH() });
    menu.Layout({ 0, TitleH(), c.right, TitleH() + MenuH() });
    toolbar.Layout({ 0, TitleH() + MenuH(), c.right, TitleH() + MenuH() + ToolH() });
    grid.Layout(GridAreaRect());
    tabs.Layout(TabBarRect());
    status.Layout(StatusRect());
}


bool UiRoot::PointerOnGridScrollBar(POINT pt)
{
    Layout();                       // the grid owns its track rectangles
    return grid.OnScrollBar(pt);
}


void UiRoot::BuildFrameSpec(HDC)
{
    RECT c = ClientRect();
    backSpec.Clear();
    frameSpec.Clear();
    backSpec.Emit(PaintOp::FillRect, c, g_th.win);
    frameSpec.Emit(PaintOp::FrameRect, c, g_th.border).w = 1;
}


void UiRoot::Present(HDC dc, const RECT& area)
{
    // Pure one-way output: execute already-declared values. No layout, no
    // theme lookup, no timers, no document, no state transitions.
    HDC s = surface.Begin(dc, area);
    if (!s) return;
    ExecutePaintSpec(s, backSpec, &area);
    for (UiObject* o : children)
        if (!o->Hosted()) ExecutePaintSpec(s, o->spec, &area);
    ExecutePaintSpec(s, frameSpec, &area);
    surface.End(dc);
}


// Logic-layer commit, executed once before painting. Everything that changes
// state lives here; it is also the ONE place where state becomes a PaintSpec.
void UiRoot::CommitFrame(HDC measure, const RECT* update)
{
    Layout();

    // Capacity and scroll validity are settled here, before any paint, from
    // the VIEW RANGE and the document. Capacity follows the view - one row of
    // demand is one row of sheet, and growth and shrink are the same
    // assignment. No device, gesture, scrollbar rectangle or endpoint
    // condition takes part in the rule.
    if (Doc* d = ActiveDoc()) {
        RECT body = GridBodyRect();
        int vw = (int)(body.right - body.left);
        int vh = (int)(body.bottom - body.top);
        d->ReconcileCapacity(vw, vh);
        d->ClampScroll(vw, vh);
        // Fold the neutral intent back to the committed origin: after the
        // frame, state matches what is actually drawn and a stale request
        // cannot keep capacity artificially high.
        d->viewTargetX = d->scrollX;
        d->viewTargetY = d->scrollY;
    }

    // Resolve all display values in the logic layer, before any paint. The
    // render path then only reads cached strings - no formula evaluation.
    if (Doc* d = ActiveDoc()) {
        for (auto& kv : d->cells) {
            if (kv.second.displayRev != g_contentRev)
                CellDisplay(*d, KeyRow(kv.first), KeyCol(kv.first));
        }
    }

    if (statusModel.Refresh(status.rect)) {
        status.SetSpec(statusModel.spec);
        bool covered = update
            && update->left <= status.rect.left && update->top <= status.rect.top
            && update->right >= status.rect.right && update->bottom >= status.rect.bottom;
        if (!covered && g_hwnd && status.rect.right > status.rect.left)
            InvalidateRect(g_hwnd, &status.rect, FALSE);
    }
    grid.SyncAnimation();
    UpdateAnimationTimer();

    // The single state -> spec conversion. Full paint, partial paint,
    // animation frames and the drag ghost all consume these same declarations;
    // no path may rebuild a spec from a device event or a paint message.
    BuildFrameSpec(measure);
    for (UiObject* o : children)
        if (!o->Hosted()) o->BuildSpec(measure);
}


void UiRoot::WindowClose()
{
    grid.CommitPendingEdit();
    SaveSessionState();
    if (g_hwnd) DestroyWindow(g_hwnd);
}


void UiRoot::AppActivate(bool active)
{
    if (!active) DismissPopups();
}


void UiRoot::DropFiles(const std::vector<std::wstring>& paths, POINT pt)
{
    // Neutral routing by object id: the grid owns import and hit-testing.
    if (UiObject* o = ById(UiId::Grid)) o->DropFiles(paths, pt);
}


void UiRoot::DismissPopups()
{
    CloseAllPopups();
}


void UiRoot::UpdateAnimationTimer()
{
    // One job, one tempo: cell highlights fade on 100ms steps. A scrollbar has
    // no animation to run - it is drawn from the view - so nothing else can
    // hold this timer open. When nothing needs frames, the timer is gone
    // entirely: an idle window runs no timer at all.
    UINT want = grid.NeedsAnimation() ? 100 : 0;
    if (want == animInterval) return;
    animInterval = want;
    if (!g_hwnd) return;
    if (want == 0) KillTimer(g_hwnd, 1);
    else           SetTimer(g_hwnd, 1, want, nullptr);   // same id replaces, never stacks
}


void UiRoot::Tick(UINT_PTR id, ULONGLONG now)
{
    if (id == 2) {
        statusModel.Tick(now);      // logic layer owns the temp-message lifetime
        return;
    }
    if (id != 1) return;
    if (!grid.TickAnimation()) {
        if (g_hwnd) KillTimer(g_hwnd, 1);
        animInterval = 0;
        return;
    }
    UpdateAnimationTimer();   // 16ms -> 100ms -> off, decided by what is left to do
}


void UiRoot::EndWindowResize()
{
    if (resizeTargetValid) {
        RECT r = resizeTargetRect;
        SetWindowPos(g_hwnd, nullptr, r.left, r.top, r.right - r.left, r.bottom - r.top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }
    HideBand();
    resizeEdge = 0;
    resizeTargetValid = false;
    if (GetCapture() == g_hwnd) ReleaseCapture();
    InvalidateRect(g_hwnd, nullptr, FALSE);
}


void UiRoot::MouseMove(POINT pt, const UiMods& m)
{
    Layout();
    if (resizeEdge) {
        POINT cur;
        GetCursorPos(&cur);
        int dx = cur.x - resizeStartCursor.x;
        int dy = cur.y - resizeStartCursor.y;
        RECT r = resizeStartRect;
        int minW = DpiScale(780);
        int minH = DpiScale(480) + TitleH();
        if (resizeEdge & 1) {
            r.left += dx;
            if (r.right - r.left < minW) r.left = r.right - minW;
        } else if (resizeEdge & 2) {
            r.right += dx;
            if (r.right - r.left < minW) r.right = r.left + minW;
        }
        if (resizeEdge & 4) {
            r.top += dy;
            if (r.bottom - r.top < minH) r.top = r.bottom - minH;
        } else if (resizeEdge & 8) {
            r.bottom += dy;
            if (r.bottom - r.top < minH) r.bottom = r.top + minH;
        }
        resizeTargetRect = r;
        resizeTargetValid = true;
        ShowBandRect(r, false);
        return;
    }
    if (!captureChild && GetCapture() != g_hwnd && !PointerOnGridScrollBar(pt)) {
        int e = ResizeEdgeAt(g_hwnd, pt);
        if (e) {
            SetResizeCursor(e);
            return;
        }
    }
    if (captureChild) {
        captureChild->MouseMove(pt, m);
        return;
    }
    for (UiObject* o : children) {
        if (o->Hosted()) continue;        // presented by its own HWND
        if (o->HitTest(pt)) o->MouseMove(pt, m);
        else o->MouseLeave();
    }
}


void UiRoot::MouseDown(POINT pt, const UiMods& m)
{
    Layout();
    if (m.button != 1) {
        // Neutral secondary pointer: the object under the cursor gets the event
        // and decides for itself whether the button means anything.
        CloseAllPopups();
        for (UiObject* o : children) {
            if (o->Hosted() || !o->HitTest(pt)) continue;
            o->MouseDown(pt, m);
            if (GetCapture() == g_hwnd) captureChild = o;
            return;
        }
        return;
    }

    // The right edge of the client area IS the vertical scrollbar. Edge
    // resize must yield to the grid's own hit area, otherwise ~40% of the
    // bar starts a window resize instead of a scroll gesture.
    int edge = PointerOnGridScrollBar(pt) ? 0 : ResizeEdgeAt(g_hwnd, pt);
    if (edge) {
        CloseAllPopups();
        resizeEdge = edge;
        GetCursorPos(&resizeStartCursor);
        GetWindowRect(g_hwnd, &resizeStartRect);
        resizeTargetRect = resizeStartRect;
        resizeTargetValid = false;
        captureChild = nullptr;
        HideBand();
        SetCapture(g_hwnd);
        return;
    }

    if (title.HitTest(pt)) {
        captureChild = nullptr;
        title.MouseDown(pt, m);
        if (GetCapture() == g_hwnd) captureChild = &title;
        return;
    }

    if (AnyPopupOpen()) {
        // The parent only routes; the menu object owns its own action.
        if (menu.HitTest(pt)) menu.MouseDown(pt, m);
        else DismissPopups();
        return;
    }

    grid.CommitPendingEdit();

    if (menu.HitTest(pt)) { menu.MouseDown(pt, m); return; }
    if (toolbar.HitTest(pt)) { toolbar.MouseDown(pt, m); return; }
    if (tabs.HitTest(pt)) { tabs.MouseDown(pt, m); return; }
    if (grid.HitTest(pt)) {
        grid.MouseDown(pt, m);
        if (GetCapture() == g_hwnd) captureChild = &grid;
        return;
    }
}


void UiRoot::MouseUp(POINT pt, const UiMods& m)
{
    if (resizeEdge) {
        EndWindowResize();
        return;
    }
    if (!captureChild) return;
    UiObject* target = captureChild;
    captureChild = nullptr;
    target->MouseUp(pt, m);
}


void UiRoot::DblClick(POINT pt, const UiMods& m)
{
    Layout();
    if (title.HitTest(pt)) { title.DblClick(pt, m); return; }
    if (AnyPopupOpen()) return;
    grid.CommitPendingEdit();
    if (grid.HitTest(pt)) grid.DblClick(pt, m);
}


void UiRoot::ContextMenu(POINT pt, const UiMods& m)
{
    Layout();
    if (AnyPopupOpen()) { CloseAllPopups(); return; }
    if (tabs.HitTest(pt)) { tabs.ContextMenu(pt, m); return; }
    if (grid.HitTest(pt)) grid.ContextMenu(pt, m);
}


void UiRoot::Wheel(int delta, POINT pt, const UiMods& m)
{
    // pt arrives in screen coordinates. The root owns conversion and hit
    // order; the object under the pointer owns the action.
    // A hosted popup is an object, not a message target. The root converts
    // the screen point to host-local data and hands that data to the object;
    // no device sends a Windows message straight into another object's HWND.
    if (g_ui.listPopup.Open()) {
        RECT r;
        GetWindowRect(g_ui.listPopup.host, &r);
        if (PtInRect(&r, pt)) {
            POINT local = { pt.x - r.left, pt.y - r.top };
            g_ui.listPopup.Input({ PopupInputKind::Wheel, local, delta });
            return;
        }
    }
    if (AnyPopupOpen()) return;
    POINT client = pt;
    ScreenToClient(g_hwnd, &client);
    if (UiObject* o = HitChild(client)) {
        if (o->Wheel(delta, client, m)) return;   // the object consumed it
    }
    // Unconsumed wheel goes to the main view by id - neutral default route,
    // not a device-to-object binding.
    if (UiObject* view = ById(UiId::Grid)) view->Wheel(delta, client, m);
}


bool UiRoot::KeyDown(WPARAM vk, const UiMods& m)
{
    // The parent does not run commands or touch documents; the action bus
    // owns the declarative shortcut table.
    if (g_actions.HandleKey(vk, m)) return true;
    return grid.KeyDown(vk, m);
}


bool UiRoot::Char(wchar_t ch, const UiMods& m)
{
    if (AnyPopupOpen()) return true;
    if (m.ctrl) return true;
    return grid.Char(ch, m);
}


bool UiRoot::Cursor(POINT pt)
{
    int e = PointerOnGridScrollBar(pt) ? 0 : ResizeEdgeAt(g_hwnd, pt);
    if (e) {
        SetResizeCursor(e);
        return true;
    }
    if (UiObject* o = HitChild(pt)) {
        if (o->Cursor(pt)) return true;      // object decides its own cursor
    }
    SetCursor(LoadCursorW(nullptr, IDC_ARROW));
    return true;
}


void UiRoot::CaptureLost()
{
    for (UiObject* o : children)
        if (!o->Hosted()) o->CaptureLost();       // neutral broadcast, no child names
    resizeEdge = 0;
    resizeTargetValid = false;
    HideBand();
    captureChild = nullptr;
    InvalidateRect(g_hwnd, nullptr, FALSE);
}


// The in-cell editor is a native child window sharing our client area. A
// full-client BitBlt must never paint over it, and the child needs a frame of
// its own after every parent paint - otherwise text typed into the control is
// present (WM_GETTEXT sees it) but invisible until Enter commits the cell.
static RECT EditorClientRect()
{
    RECT empty = {};
    if (!g_editor || !IsWindowVisible(g_editor)) return empty;
    RECT w;
    if (!GetWindowRect(g_editor, &w)) return empty;
    POINT a = { w.left, w.top }, b = { w.right, w.bottom };
    ScreenToClient(g_hwnd, &a);
    ScreenToClient(g_hwnd, &b);
    return { a.x, a.y, b.x, b.y };
}


void PaintWindow()
{
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(g_hwnd, &ps);

    // The dirty rectangle is pure drawing data: it decides how much is
    // composed, never what is composed. State is committed once, for the
    // whole frame, and the presentation is a single blit - there is no direct
    // paint path that could show a half-drawn sheet while the view moves.
    RECT area = ps.rcPaint;
    RECT client = {};
    GetClientRect(g_hwnd, &client);
    if (area.right <= area.left || area.bottom <= area.top) area = client;

    RECT er = EditorClientRect();
    bool hasEditor = er.right > er.left && er.bottom > er.top;
    if (hasEditor) ExcludeClipRect(dc, er.left, er.top, er.right, er.bottom);

    g_ui.CommitFrame(dc, &area);
    g_ui.Present(dc, area);

    EndPaint(g_hwnd, &ps);
    if (hasEditor) {
        InvalidateRect(g_editor, nullptr, TRUE);
        UpdateWindow(g_editor);
    }
}


// ---------------------------------------------------------------------------
//  session: 关闭 exe 时自动保存所有标签，下次启动恢复
// ---------------------------------------------------------------------------
std::wstring SessionDirPath()
{
    std::wstring dir = PathJoin(ExeDir(), L"session");
    EnsureDirectory(dir);
    return dir;
}


std::wstring SessionListPath()
{
    return PathJoin(SessionDirPath(), L"session.txt");
}


void CleanupSessionFiles(const std::vector<std::wstring>& keep)
{
    std::wstring dir = SessionDirPath();
    if (dir.empty()) return;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(PathJoin(dir, L"tab-*.db").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        std::wstring full = PathJoin(dir, fd.cFileName);
        bool used = false;
        for (auto& k : keep) {
            if (_wcsicmp(k.c_str(), full.c_str()) == 0) { used = true; break; }
        }
        if (!used) DeleteFileW(full.c_str());
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}


void SaveSessionState()
{
    std::wstring listPath = SessionListPath();
    if (listPath.empty()) return;

    if (g_docs.empty()) {
        DeleteFileW(listPath.c_str());
        CleanupSessionFiles({});
        return;
    }

    std::wstring out = L"active=" + std::to_wstring(g_active) + L"\n";
    std::vector<std::wstring> used;
    std::wstring sessionDir = SessionDirPath();

    for (size_t i = 0; i < g_docs.size(); ++i) {
        Doc& d = *g_docs[i];
        if (EditorActive() && g_editDoc == &d) CommitEditor(false, 0);

        if (!d.filePath.empty() && FileExistsW(d.filePath)) {
            bool ok = true;
            std::wstring err;
            if (d.dirty) ok = d.SaveTo(d.filePath, err);
            if (ok) {
                d.sessionPath.clear();
                if (d.locked) out += L"locked\t1\n";
                out += L"file\t" + d.filePath + L"\n";
                continue;
            }
        }

        std::wstring sess = d.sessionPath;
        if (sess.empty() && !sessionDir.empty()) {
            for (int k = 0; k < 10000 && sess.empty(); ++k) {
                std::wstring cand = PathJoin(sessionDir, Format(L"tab-%d-%d.db", (int)i, k));
                if (FileExistsW(cand)) continue;
                bool taken = false;
                for (auto& other : g_docs) {
                    if (other.get() != &d && _wcsicmp(other->sessionPath.c_str(), cand.c_str()) == 0) {
                        taken = true;
                        break;
                    }
                }
                if (!taken) sess = cand;
            }
        }
        if (sess.empty()) continue;

        std::wstring savedTitle = d.title;
        std::wstring err;
        bool oldAuto = g_autoCopy;
        g_autoCopy = false;
        bool ok = d.SaveTo(sess, err);
        g_autoCopy = oldAuto;
        if (!ok) continue;

        d.filePath.clear();
        d.sessionPath = sess;
        d.title = savedTitle;
        d.dirty = false;
        if (d.locked) out += L"locked\t1\n";
        out += L"untitled\t" + sess + L"\t" + savedTitle + L"\n";
        used.push_back(sess);
    }

    WriteTextUtf8(listPath, out);
    CleanupSessionFiles(used);
}


bool RestoreSessionState()
{
    std::wstring listPath = SessionListPath();
    if (listPath.empty()) return false;
    std::wstring text = ReadTextUtf8(listPath);
    if (text.empty()) return false;

    struct Entry {
        bool untitled = false;
        bool locked = false;
        std::wstring path;
        std::wstring title;
    };
    std::vector<Entry> entries;
    int active = 0;
    bool pendingLocked = false;

    size_t pos = 0;
    while (pos < text.size()) {
        size_t e = text.find(L'\n', pos);
        std::wstring line = (e == std::wstring::npos) ? text.substr(pos) : text.substr(pos, e - pos);
        pos = (e == std::wstring::npos) ? text.size() : e + 1;
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        if (line.empty()) continue;
        if (line.rfind(L"active=", 0) == 0) {
            active = _wtoi(line.c_str() + 7);
            continue;
        }
        if (line.rfind(L"locked\t", 0) == 0) {
            pendingLocked = _wtoi(line.c_str() + 7) != 0;
            continue;
        }
        if (line.rfind(L"file\t", 0) == 0) {
            Entry en;
            en.locked = pendingLocked;
            pendingLocked = false;
            en.path = line.substr(5);
            if (!en.path.empty()) entries.push_back(en);
            continue;
        }
        if (line.rfind(L"untitled\t", 0) == 0) {
            Entry en;
            en.locked = pendingLocked;
            pendingLocked = false;
            size_t t1 = line.find(L'\t', 8);
            if (t1 == std::wstring::npos) continue;
            size_t t2 = line.find(L'\t', t1 + 1);
            en.untitled = true;
            en.path = line.substr(t1 + 1, (t2 == std::wstring::npos ? line.size() : t2) - t1 - 1);
            if (t2 != std::wstring::npos) en.title = line.substr(t2 + 1);
            if (!en.path.empty()) entries.push_back(en);
        }
    }

    for (auto& en : entries) {
        if (!FileExistsW(en.path)) continue;
        auto doc = std::make_unique<Doc>();
        std::wstring err;
        if (!doc->LoadFile(en.path, err)) continue;
        if (en.untitled) {
            doc->sessionPath = en.path;
            doc->filePath.clear();
            doc->title = en.title.empty() ? L"未命名" : en.title;
            doc->dirty = false;
        }
        doc->locked = en.locked;
        g_docs.push_back(std::move(doc));
    }

    if (g_docs.empty()) return false;
    for (auto& up : g_docs) {
        const std::wstring& t = up->title;
        if (t.rfind(L"未命名-", 0) == 0) {
            int n = _wtoi(t.c_str() + 4);
            if (n >= g_untitledSeq) g_untitledSeq = n + 1;
        }
    }
    if (active < 0) active = 0;
    if (active >= (int)g_docs.size()) active = (int)g_docs.size() - 1;
    g_active = active;
    RebuildAllLayouts();
    UpdateWindowTitle();
    InvalidateAll();
    return true;
}

// ---------------------------------------------------------------------------
//  window procedure
// ---------------------------------------------------------------------------
// Window drag/resize state lives in TitleBar / UiRoot; only the pure edge
// geometry and cursor helpers remain here as layout-level utilities.
int ResizeEdgeAt(HWND h, POINT pt)
{
    if (IsZoomed(h)) return 0;
    if (ActiveDoc() && PtInRect(&GridGripRect(), pt)) return 0;
    RECT c;
    GetClientRect(h, &c);
    int b = DpiScale(5);
    int e = 0;
    if (pt.x < b) e |= 1;
    else if (pt.x >= c.right - b) e |= 2;
    if (pt.y < b) e |= 4;
    else if (pt.y >= c.bottom - b) e |= 8;
    return e;
}

void SetResizeCursor(int e)
{
    if (e == 1 || e == 2) SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
    else if (e == 4 || e == 8) SetCursor(LoadCursorW(nullptr, IDC_SIZENS));
    else if (e == 5 || e == 10) SetCursor(LoadCursorW(nullptr, IDC_SIZENWSE));
    else SetCursor(LoadCursorW(nullptr, IDC_SIZENESW));
}


// Keyboard modifiers are device state, read once per message.
UiMods CurrentMods(int button )
{
    UiMods m;
    m.button = button;
    m.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    m.ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    m.alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
    return m;
}


LRESULT CALLBACK MainWndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE:
        g_hwnd = h;
        if (g_iconLarge) SendMessageW(h, WM_SETICON, ICON_BIG, (LPARAM)g_iconLarge);
        if (g_iconSmall) SendMessageW(h, WM_SETICON, ICON_SMALL, (LPARAM)g_iconSmall);
        CreateEditorControl(h);
        g_ui.Layout();
        DragAcceptFiles(h, TRUE);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_NCACTIVATE:
        return TRUE;

    case WM_PAINT:
        PaintWindow();
        return 0;

    case WM_SIZE:
        g_ui.Layout();
        g_ui.grid.PositionEditorControl();
        InvalidateRect(h, nullptr, FALSE);
        return 0;

    case WM_GETMINMAXINFO: {
        MINMAXINFO* mm = (MINMAXINFO*)lp;
        mm->ptMinTrackSize.x = DpiScale(780);
        mm->ptMinTrackSize.y = DpiScale(480) + TitleH();
        MONITORINFO mi = { sizeof(mi) };
        if (GetMonitorInfoW(MonitorFromWindow(h, MONITOR_DEFAULTTONEAREST), &mi)) {
            mm->ptMaxPosition.x = mi.rcWork.left - mi.rcMonitor.left;
            mm->ptMaxPosition.y = mi.rcWork.top - mi.rcMonitor.top;
            mm->ptMaxSize.x = mi.rcWork.right - mi.rcWork.left;
            mm->ptMaxSize.y = mi.rcWork.bottom - mi.rcWork.top;
        }
        return 0;
    }

    case WM_DPICHANGED: {
        g_dpi = LOWORD(wp);
        RECT* r = (RECT*)lp;
        SetWindowPos(h, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        RebuildAllLayouts();
        g_ui.Layout();
        g_ui.grid.PositionEditorControl();
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    }

    // The window procedure is a neutral adapter: it reads device state and
    // forwards it. No hit-testing, no UI actions, no state transitions here.
    case WM_MOUSEMOVE:
        g_ui.MouseMove({ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) }, CurrentMods());
        return 0;

    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT) {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(h, &pt);
            g_ui.Cursor(pt);
            return TRUE;
        }
        break;

    case WM_MBUTTONDOWN:
        SetFocus(h);
        g_ui.MouseDown({ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) }, CurrentMods(3));
        return 0;

    case WM_MBUTTONUP:
        g_ui.MouseUp({ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) }, CurrentMods(3));
        return 0;

    case WM_LBUTTONDOWN:
        SetFocus(h);
        g_ui.MouseDown({ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) }, CurrentMods(1));
        return 0;

    case WM_LBUTTONDBLCLK:
        g_ui.DblClick({ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) }, CurrentMods(1));
        return 0;

    case WM_LBUTTONUP:
        g_ui.MouseUp({ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) }, CurrentMods(1));
        return 0;

    case WM_RBUTTONUP:
        g_ui.ContextMenu({ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) }, CurrentMods());
        return 0;

    case WM_CAPTURECHANGED:
        g_ui.CaptureLost();
        return 0;

    case WM_MOUSEWHEEL:
        // Device data only (delta + screen point + modifiers). All routing,
        // hit-testing and coordinate conversion happens inside UiRoot.
        g_ui.Wheel(GET_WHEEL_DELTA_WPARAM(wp),
                   { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) }, CurrentMods());
        return 0;

    case WM_KEYDOWN:
        if (g_ui.KeyDown(wp, CurrentMods())) return 0;
        break;

    case WM_CHAR:
        if (g_ui.Char((wchar_t)wp, CurrentMods())) return 0;
        break;

    case WM_TIMER:
        g_ui.Tick(wp, GetTickCount64());
        return 0;

    case WM_ACTIVATEAPP:
        g_ui.AppActivate(wp != 0);
        return 0;

    case WM_DROPFILES: {
        // The adapter unpacks the device payload and stops. No hit-testing,
        // no import, no focus change, no state transitions here.
        HDROP drop = (HDROP)wp;
        POINT pt{};
        DragQueryPoint(drop, &pt);
        UINT n = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
        std::vector<std::wstring> paths;
        for (UINT i = 0; i < n; ++i) {
            wchar_t buf[MAX_PATH * 2];
            if (DragQueryFileW(drop, i, buf, (UINT)std::size(buf)) > 0)
                paths.push_back(buf);
        }
        DragFinish(drop);
        g_ui.DropFiles(paths, pt);
        return 0;
    }

    case WM_KEYUP:
        break;

    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)wp;
        SetTextColor(dc, g_th.text);
        SetBkColor(dc, g_th.panel2);
        return (LRESULT)Brush(g_th.panel2);
    }

    case WM_CLOSE:
        g_ui.WindowClose();
        return 0;

    case WM_DESTROY:
        if (g_bandWnd) { DestroyWindow(g_bandWnd); g_bandWnd = nullptr; }
        CloseAllPopups();
        KillTimer(h, 1);
        KillTimer(h, 2);
        SaveSettings();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}


// ---------------------------------------------------------------------------
//  Action bus implementation - objects output actions, the bus executes them
//  after the device event is finished. No event bubbling to the window proc.
// ---------------------------------------------------------------------------
void ActionBus::Drain()
{
    if (queue.empty()) return;
    std::vector<ActionItem> local;
    local.swap(queue);               // modal loops may post more actions
    for (const ActionItem& a : local) {
        switch (a.type) {
        case UiAction::Command:
            ExecuteCommand(a.param);
            break;
        case UiAction::RowHeight: {
            std::wstring v = PromptText(L"全局行高", L"14 – 200 px", std::to_wstring(g_rowH));
            if (!v.empty()) ApplyGlobalRowH(_wtoi(v.c_str()));
            break;
        }
        case UiAction::ColWidth: {
            std::wstring v = PromptText(L"全局列宽", L"30 – 400 px", std::to_wstring(g_colW));
            if (!v.empty()) ApplyGlobalColW(_wtoi(v.c_str()));
            break;
        }
        case UiAction::OpenSettings:
            OpenSettingsWindow();
            break;
        case UiAction::OpenAbout:
            ShowAbout();
            break;
        case UiAction::OpenNumbering:
            ShowNumbering();
            break;
        case UiAction::OpenUnit:
            ShowUnit();
            break;
        case UiAction::CloseTab:
            CloseDocumentAt(a.param, false);
            break;
        case UiAction::CloseWindow:
            g_ui.WindowClose();
            break;
        case UiAction::ClosePopups:
            g_ui.DismissPopups();
            break;
        case UiAction::MenuPick:
            // Dismiss first, then run the command: the popup object is gone
            // before any modal loop or document mutation can be entered.
            g_ui.DismissPopups();
            if (a.param) ExecuteCommand(a.param);
            break;
        case UiAction::ListPick:
            g_ui.DismissPopups();
            if (a.param == 0) ApplyFontToSelection(a.text);
            else ApplySizeToSelection(_wtoi(a.text.c_str()));
            break;
        case UiAction::PalettePick: {
            g_ui.DismissPopups();
            if (a.param == 0) g_btnFg = a.color;
            else if (a.param == 1) g_btnOutline = a.color;
            else g_btnCellBg = a.color;
            SaveSettings();
            ApplyColorToSelection(a.param, a.color);
            InvalidateRect(g_hwnd, nullptr, FALSE);
            break;
        }
        case UiAction::None:
            break;
        }
    }
}


bool ActionBus::HandleKey(WPARAM vk, const UiMods& m)
{
    if (AnyPopupOpen()) {
        if (vk == VK_ESCAPE) { g_ui.DismissPopups(); return true; }
        if (g_ui.menuPopup.topIndex >= 0) {
            if (vk == VK_UP) { MenuKeyMove(-1); return true; }
            if (vk == VK_DOWN) { MenuKeyMove(1); return true; }
            if (vk == VK_LEFT) { OpenTopMenu((g_ui.menuPopup.topIndex + kMenuCount - 1) % kMenuCount); return true; }
            if (vk == VK_RIGHT) { OpenTopMenu((g_ui.menuPopup.topIndex + 1) % kMenuCount); return true; }
            if (vk == VK_RETURN) { MenuKeyExecute(); return true; }
        }
        return true;
    }

    if (vk == VK_F10) { OpenTopMenu(0); return true; }
    if (m.ctrl && vk == 'Z') { ExecuteCommand(m.shift ? ID_EDIT_REDO : ID_EDIT_UNDO); return true; }
    if (m.ctrl && vk == 'Y') { ExecuteCommand(ID_EDIT_REDO); return true; }

    // Declarative shortcut table: a key only selects a command.
    struct Shortcut { UINT vk; bool ctrl, shift, alt; int cmd; };
    static const Shortcut kShortcuts[] = {
        { 'N', true, false, false, ID_FILE_NEW },
        { 'O', true, false, false, ID_FILE_OPEN },
        { 'S', true, false, false, ID_FILE_SAVE },
        { 'S', true, true,  false, ID_FILE_SAVEAS },
        { 'X', true, false, false, ID_EDIT_CUT },
        { 'C', true, false, false, ID_EDIT_COPY },
        { 'V', true, false, false, ID_EDIT_PASTE },
    };
    for (const Shortcut& s : kShortcuts) {
        if (s.vk == vk && s.ctrl == m.ctrl && s.shift == m.shift && s.alt == m.alt) {
            ExecuteCommand(s.cmd);
            return true;
        }
    }
    if (m.alt && (vk == VK_OEM_PLUS || vk == VK_ADD)) {
        ExecuteCommand(ID_EDIT_SUM);
        return true;
    }
    return false;
}


// ---------------------------------------------------------------------------
//  entry point
// ---------------------------------------------------------------------------
int APIENTRY wWinMain(HINSTANCE inst, HINSTANCE, LPWSTR, int show)
{
    g_inst = inst;

    typedef BOOL(WINAPI* SetDpiProc)(DPI_AWARENESS_CONTEXT);
    if (HMODULE user32 = GetModuleHandleW(L"user32.dll")) {
        auto setDpi = (SetDpiProc)GetProcAddress(user32, "SetProcessDpiAwarenessContext");
        if (setDpi) setDpi(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    }

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES };
    InitCommonControlsEx(&icc);
    LoadSettings();
    ApplyThemeColors();
    g_dpi = GetDpiForSystem();
    g_iconLarge = (HICON)LoadImageW(inst, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON,
                                    GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
    g_iconSmall = (HICON)LoadImageW(inst, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON,
                                    GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS | CS_DROPSHADOW;
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = g_inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = g_iconLarge ? g_iconLarge : LoadIconW(nullptr, IDI_APPLICATION);
    wc.hIconSm = g_iconSmall ? g_iconSmall : LoadIconW(nullptr, IDI_APPLICATION);
    wc.lpszClassName = L"ExcelMain";
    if (!RegisterClassExW(&wc)) return 0;

    int w = std::max(DpiScale(780), g_winW);
    int h = std::max(DpiScale(480), g_winH);
    int x = g_winX, y = g_winY;
    if (x < -10000 || y < -10000) { x = -1; y = -1; }
    if (x < 0 || y < 0) {
        x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
        y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;
    }

    DWORD mainStyle = WS_POPUP | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU | WS_CLIPCHILDREN;
    HWND hwnd = CreateWindowExW(WS_EX_APPWINDOW, L"ExcelMain", APP_NAME, mainStyle,
                                x, y, w, h, nullptr, nullptr, g_inst, nullptr);
    if (!hwnd) return 0;
    g_hwnd = hwnd;
    g_dpi = GetDpiForWindow(hwnd);

    ShowWindow(hwnd, g_winMax ? SW_SHOWMAXIMIZED : show);

    bool restored = RestoreSessionState();
    bool opened = false;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv && argc > 1) {
        std::wstring path = argv[1];
        if (FileExistsW(path) && PathExtLower(path) == L"db") {
            OpenDocumentFile(path);
            opened = (ActiveDoc() != nullptr);
        }
    }
    if (argv) LocalFree(argv);
    if (!restored && !opened) AddBlankDoc();

    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
        g_actions.Drain();          // execute actions after the device event
    }

    SaveSettings();
    if (g_wic) { g_wic->Release(); g_wic = nullptr; }
    CoUninitialize();
    return (int)msg.wParam;
}


