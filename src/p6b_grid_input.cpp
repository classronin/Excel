// ============================================================================
//  p6b_grid_input.cpp
//  Responsibility: grid input
//  Editing, selection, mouse, keyboard, drag-move and viewport policy.
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
#include "api_p8b_app.h"
// ---- src\p6b_grid_input.inc ----
// ============================================================================
//  Excel - part 6b: editing, selection, mouse, keyboard, drag move
// ============================================================================


bool EditorActive()
{
    return g_editor && IsWindowVisible(g_editor);
}



bool EditorCoversCell(Doc* d, int r, int c)
{
    return g_editor && IsWindowVisible(g_editor) && g_editDoc == d && g_editRow == r && g_editCol == c;
}


void PositionEditor()
{
    if (!g_editDoc || !g_editor) return;
    Doc& d = *g_editDoc;
    int er = g_editRow, ec = g_editCol;
    RECT rc = CellRect(d, er, ec);
    if (const MergeRange* m = d.MergeAt(er, ec)) {
        RECT a = CellRect(d, m->r1, m->c1);
        RECT z = CellRect(d, m->r2, m->c2);
        rc = { a.left, a.top, z.right, z.bottom };
    }
    RECT b = GridBodyRect();
    RECT inter;
    if (!IntersectRect(&inter, &rc, &b)) {
        CancelEditor();
        return;
    }
    const Cell* cell = d.Find(er, ec);
    CellStyle def;
    const CellStyle& st = cell ? cell->st : def;
    HFONT font = CellFont(EffectiveFont(st), EffectiveSize(st), st.bold, st.italic);

    int w = std::max((int)(inter.right - inter.left), DpiScale(80));
    int x = inter.left;
    if (x + w > b.right) w = b.right - x;
    if (w < DpiScale(20)) w = DpiScale(20);
    int y = inter.top;
    int h = (int)(inter.bottom - inter.top);
    if (h < DpiScale(20)) h = DpiScale(20);
    SetWindowPos(g_editor, HWND_TOP, x, y, w, h, SWP_NOACTIVATE | SWP_SHOWWINDOW);

    // Feed the native EDIT control the SAME text rectangle the cell renderer
    // uses (CellTextRect measures with the grid font, not the UI font). This
    // is what keeps typing and committed text on one baseline: a native edit
    // always starts its first line at the top of its formatting rectangle.
    std::wstring et = EditorText();
    int lines = 1;
    for (wchar_t ch : et) if (ch == L'\r' || ch == L'\n') ++lines;
    int pad = DpiScale(4);
    RECT inner = { inter.left + pad, inter.top + 1, inter.right - pad, inter.bottom - 1 };
    int lineH = 0;
    HDC dc = GetDC(g_editor);
    RECT tr = CellTextRect(dc, inner, font, et.empty() ? L"Ag" : et,
                           EffectiveVAlign(st), &lineH);
    ReleaseDC(g_editor, dc);

    RECT fmt = { pad, tr.top - y, w - pad, 0 };
    if (fmt.top < 0) fmt.top = 0;
    int textH = std::max(lineH, lines * lineH) + DpiScale(2);
    fmt.bottom = (LONG)std::min(h, (int)fmt.top + textH);
    if (fmt.right <= fmt.left) fmt.right = fmt.left + 1;
    if (fmt.bottom <= fmt.top) fmt.bottom = (LONG)std::min(h, (int)fmt.top + lineH);
    SendMessageW(g_editor, EM_SETRECT, 0, (LPARAM)&fmt);
}


std::wstring EditorText()
{
    int n = GetWindowTextLengthW(g_editor);
    if (n <= 0) return L"";
    std::wstring s(n, L'\0');
    GetWindowTextW(g_editor, &s[0], n + 1);
    return s;
}


void CommitEditor(bool moveDown, int moveRight)
{
    if (!EditorActive() || !g_editDoc || g_committing) return;
    g_committing = true;
    Doc* d = g_editDoc;
    int r = g_editRow, c = g_editCol;
    std::wstring txt = EditorText();
    const Cell* oldCell = d->Find(r, c);
    std::wstring oldText = oldCell ? (!oldCell->formula.empty() ? oldCell->formula : oldCell->text) : L"";

    ShowWindow(g_editor, SW_HIDE);
    g_editDoc = nullptr;

    if (txt != oldText) {
        d->PushUndo();
        Cell& cell = d->Ensure(r, c);
        cell.kind = CK_TEXT;
        cell.path.clear();
        if (!txt.empty() && txt[0] == L'=') {
            cell.formula = txt;
            cell.text.clear();
        } else {
            cell.text = txt;
            cell.formula.clear();
        }
        d->EraseIfTrivial(r, c);
        d->Touch();
    }
    g_committing = false;
    SetFocus(g_hwnd);

    int nr = r, nc = c;
    if (moveDown) ++nr;
    if (moveRight != 0) nc += moveRight;
    if (nr >= d->rows) nr = d->rows - 1;
    if (nc < 0) nc = 0;
    if (nc >= d->cols) nc = d->cols - 1;
    if (nr != r || nc != c) {
        d->selAR = nr;
        d->selAC = nc;
        d->selFR = nr;
        d->selFC = nc;
        EnsureCellVisible(*d, nr, nc);
    }
    InvalidateRect(g_hwnd, nullptr, FALSE);
}


void CancelEditor()
{
    if (!EditorActive()) return;
    g_committing = true;
    ShowWindow(g_editor, SW_HIDE);
    g_editDoc = nullptr;
    g_committing = false;
    SetFocus(g_hwnd);
    InvalidateRect(g_hwnd, nullptr, FALSE);
}


void BeginEditCell(Doc& d, int r, int c, const std::wstring* initial)
{
    if (!g_editor) return;
    if (EditorActive()) CommitEditor(false, 0);
    if (r < 0 || c < 0 || r >= d.rows || c >= d.cols) return;
    if (const MergeRange* m = d.MergeAt(r, c)) { r = m->r1; c = m->c1; }
    g_editDoc = &d;
    g_editRow = r;
    g_editCol = c;

    const Cell* cell = d.Find(r, c);
    std::wstring txt = cell ? (!cell->formula.empty() ? cell->formula : cell->text) : L"";
    if (initial) txt = *initial;
    SetWindowTextW(g_editor, txt.c_str());

    const CellStyle* st = cell ? &cell->st : nullptr;
    CellStyle def;
    HFONT font = CellFont(EffectiveFont(st ? *st : def), EffectiveSize(st ? *st : def),
                          st ? st->bold : false, st ? st->italic : false);
    SendMessageW(g_editor, WM_SETFONT, (WPARAM)font, TRUE);

    PositionEditor();
    ShowWindow(g_editor, SW_SHOW);
    SetFocus(g_editor);
    int len = (int)txt.size();
    if (initial) SendMessageW(g_editor, EM_SETSEL, len, len);
    else SendMessageW(g_editor, EM_SETSEL, 0, len);
    // Text and font were assigned while the control was hidden, so it has no
    // dirty region of its own. Force its first frame explicitly.
    InvalidateRect(g_editor, nullptr, TRUE);
    UpdateWindow(g_editor);
    InvalidateRect(g_hwnd, nullptr, FALSE);
}


LRESULT CALLBACK EditorSubclassProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR)
{
    switch (msg) {
    case WM_KEYDOWN:
        if (wp == VK_RETURN) {
            if (GetKeyState(VK_CONTROL) & 0x8000) {
                SendMessageW(h, EM_REPLACESEL, TRUE, (LPARAM)L"\r\n");
                PositionEditor();
                return 0;
            }
            CommitEditor(true, 0);
            return 0;
        }
        if (wp == VK_TAB) { CommitEditor(false, (GetKeyState(VK_SHIFT) & 0x8000) ? -1 : 1); return 0; }
        if (wp == VK_ESCAPE) { CancelEditor(); return 0; }
        break;
    case WM_CHAR:
        if (wp == VK_TAB || wp == VK_RETURN || wp == L'\n' || wp == L'\r') return 0;
        break;
    case WM_KILLFOCUS:
        if (!g_committing && EditorActive()) CommitEditor(false, 0);
        break;
    }
    return DefSubclassProc(h, msg, wp, lp);
}


void CreateEditorControl(HWND parent)
{
    g_editor = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | ES_MULTILINE | ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_LEFT | WS_CLIPSIBLINGS,
                               0, 0, 10, 10, parent, (HMENU)1001, g_inst, nullptr);
    if (!g_editor) return;
    SendMessageW(g_editor, EM_SETLIMITTEXT, 1000000, 0);
    SetWindowSubclass(g_editor, EditorSubclassProc, 1, 0);
    ShowWindow(g_editor, SW_HIDE);
}


// ---------------------------------------------------------------------------
//  selection helpers
// ---------------------------------------------------------------------------
void ClearSelectionContents(Doc& d)
{
    int r1, c1, r2, c2;
    d.SelectionRect(r1, c1, r2, c2);
    std::vector<MergeRange> ranges;
    ranges.push_back({ r1, c1, r2, c2 });
    for (auto& e : d.selExtra) ranges.push_back(e);
    d.PushUndo();
    long long cleared = 0;
    for (auto& e : ranges) {
        for (int r = e.r1; r <= e.r2 && cleared < 200000; ++r) {
            for (int c = e.c1; c <= e.c2 && cleared < 200000; ++c, ++cleared) {
                Cell* cell = d.Find(r, c);
                if (!cell) continue;
                cell->text.clear();
                cell->formula.clear();
                cell->path.clear();
                cell->kind = CK_TEXT;
                d.EraseIfTrivial(r, c);
            }
        }
    }
    d.Touch();
}


void MoveFocus(Doc& d, int dr, int dc, bool extend)
{
    if (!extend) d.selExtra.clear();
    int r = d.selFR + dr;
    int c = d.selFC + dc;
    if (r < 0) r = 0;
    if (c < 0) c = 0;
    if (r >= d.rows) r = d.rows - 1;
    if (c >= d.cols) c = d.cols - 1;
    if (!extend) {
        d.selAR = r;
        d.selAC = c;
    }
    d.selFR = r;
    d.selFC = c;
    EnsureCellVisible(d, r, c);
    InvalidateRect(g_hwnd, nullptr, FALSE);
}


void AutoScrollForPoint(Doc& d, POINT pt)
{
    RECT b = GridBodyRect();
    int step = DpiScale(22);
    ViewRequest req{ d.scrollX, d.scrollY };
    bool moved = false;
    if (pt.y < b.top) { req.y -= step; moved = true; }
    else if (pt.y >= b.bottom) { req.y += step; moved = true; }
    if (pt.x < b.left) { req.x -= step; moved = true; }
    else if (pt.x >= b.right) { req.x += step; moved = true; }
    if (moved) g_ui.grid.ApplyViewRequest(d, req);
}


// ---------------------------------------------------------------------------
//  drag ghost
// ---------------------------------------------------------------------------

LRESULT CALLBACK GhostWndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        if (g_ui.grid.ghostBmp) {
            HDC mem = CreateCompatibleDC(dc);
            HGDIOBJ old = SelectObject(mem, g_ui.grid.ghostBmp);
            BitBlt(dc, 0, 0, g_ui.grid.ghostW, g_ui.grid.ghostH, mem, 0, 0, SRCCOPY);
            SelectObject(mem, old);
            DeleteDC(mem);
        }
        EndPaint(h, &ps);
        return 0;
    }
    }
    return DefWindowProcW(h, msg, wp, lp);
}


void RegisterGhostClass()
{
    static bool done = false;
    if (done) return;
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = GhostWndProc;
    wc.hInstance = g_inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = L"ExcelGhost";
    RegisterClassExW(&wc);
    done = true;
}


void DestroyGhost()
{
    if (g_ui.grid.ghostWnd) {
        DestroyWindow(g_ui.grid.ghostWnd);
        g_ui.grid.ghostWnd = nullptr;
    }
    if (g_ui.grid.ghostBmp) {
        DeleteObject(g_ui.grid.ghostBmp);
        g_ui.grid.ghostBmp = nullptr;
    }
    g_ui.grid.ghostW = g_ui.grid.ghostH = 0;
}


void CreateGhost(Doc& d, POINT clientPt)
{
    DestroyGhost();
    RegisterGhostClass();
    RECT sr = SelectionScreenRect(d);
    int w = sr.right - sr.left;
    int h = sr.bottom - sr.top;
    if (w <= 0 || h <= 0) return;
    if (w > DpiScale(1000)) w = DpiScale(1000);
    if (h > DpiScale(700)) h = DpiScale(700);

    HDC screen = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(screen);
    g_ui.grid.ghostBmp = CreateCompatibleBitmap(screen, w, h);
    HGDIOBJ old = SelectObject(mem, g_ui.grid.ghostBmp);
    RECT full = { 0, 0, w, h };
    FillRectC(mem, full, kGhostKey);

    // The ghost is just another consumer of the declaration the frame already
    // made. No cell is re-read, no formula evaluated, no text measured here.
    PaintSpec ghostSpec;
    for (const PaintPrim& p : g_ui.grid.spec.prims) {
        if (p.ownerR < 0 || p.ownerC < 0) continue;   // chrome / decorations
        if (p.op == PaintOp::PushClip || p.op == PaintOp::PopClip) continue;
        if (p.rc.right < sr.left || p.rc.left > sr.right ||
            p.rc.bottom < sr.top || p.rc.top > sr.bottom) continue;
        PaintPrim q = p;
        OffsetRect(&q.rc, -sr.left, -sr.top);
        if (q.rc2.right > q.rc2.left || q.rc2.bottom > q.rc2.top)
            OffsetRect(&q.rc2, -sr.left, -sr.top);
        q.p1 -= sr.left;
        q.p2 -= sr.top;
        ghostSpec.prims.push_back(std::move(q));
    }
    ExecutePaintSpec(mem, ghostSpec, &full);
    SelectObject(mem, old);
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);

    g_ui.grid.ghostW = w;
    g_ui.grid.ghostH = h;
    g_ui.grid.ghostOffset.x = clientPt.x - sr.left;
    g_ui.grid.ghostOffset.y = clientPt.y - sr.top;
    g_ui.grid.ghostTick = 0;
    POINT sp = clientPt;
    ClientToScreen(g_hwnd, &sp);
    g_ui.grid.ghostWnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE,
                                 L"ExcelGhost", L"", WS_POPUP,
                                 sp.x - g_ui.grid.ghostOffset.x, sp.y - g_ui.grid.ghostOffset.y, w, h,
                                 g_hwnd, nullptr, g_inst, nullptr);
    SetLayeredWindowAttributes(g_ui.grid.ghostWnd, kGhostKey, 210, LWA_COLORKEY | LWA_ALPHA);
    ShowWindow(g_ui.grid.ghostWnd, SW_SHOWNOACTIVATE);
}

std::vector<MergeRange> SelectedBlocks(Doc& d)
{
    std::vector<MergeRange> blocks;
    int r1, c1, r2, c2;
    d.SelectionRect(r1, c1, r2, c2);
    blocks.push_back({ r1, c1, r2, c2 });
    for (auto& e : d.selExtra) blocks.push_back(e);
    return blocks;
}


bool SelectionBorderHit(Doc& d, POINT pt)
{
    int t = DpiScale(4);
    for (auto& b : SelectedBlocks(d)) {
        RECT a = CellRect(d, b.r1, b.c1);
        RECT z = CellRect(d, b.r2, b.c2);
        RECT r = { a.left, a.top, z.right, z.bottom };
        if (PtInRect(&r, pt) &&
            (pt.x - r.left < t || r.right - pt.x <= t || pt.y - r.top < t || r.bottom - pt.y <= t))
            return true;
    }
    return false;
}


void GridGesture::BeginMove(Doc& d, POINT pt)
{
    kind = GestureKind::MovePending;
    press = pt;

    int r1, c1, r2, c2;
    d.SelectionRect(r1, c1, r2, c2);
    move.pressRow = RowAtY(d, pt.y);
    move.pressCol = ColAtX(d, pt.x);
    move.selR1 = r1;
    move.selC1 = c1;
    move.selH = r2 - r1 + 1;
    move.selW = c2 - c1 + 1;
    move.targetRow = r1;
    move.targetCol = c1;
    move.blocks = SelectedBlocks(d);
}


void GridGesture::UpdateMove(Doc& d, POINT pt)
{
    int r = RowAtY(d, pt.y);
    int c = ColAtX(d, pt.x);
    int dr = r - move.pressRow;
    int dc = c - move.pressCol;
    int minTop = move.selR1, minLeft = move.selC1;
    int maxBottom = move.selR1 + move.selH - 1, maxRight = move.selC1 + move.selW - 1;
    for (auto& b : move.blocks) {
        minTop = std::min(minTop, b.r1);
        minLeft = std::min(minLeft, b.c1);
        maxBottom = std::max(maxBottom, b.r2);
        maxRight = std::max(maxRight, b.c2);
    }
    if (dr < -minTop) dr = -minTop;
    if (dc < -minLeft) dc = -minLeft;
    if (dr > d.rows - 1 - maxBottom) dr = d.rows - 1 - maxBottom;
    if (dc > d.cols - 1 - maxRight) dc = d.cols - 1 - maxRight;
    move.targetRow = move.selR1 + dr;
    move.targetCol = move.selC1 + dc;

    if (g_ui.grid.ghostWnd) {
        ULONGLONG now = GetTickCount64();
        if (now - g_ui.grid.ghostTick >= 16) {
            g_ui.grid.ghostTick = now;
            POINT sp = pt;
            ClientToScreen(g_hwnd, &sp);
            SetWindowPos(g_ui.grid.ghostWnd, HWND_TOP, sp.x - g_ui.grid.ghostOffset.x, sp.y - g_ui.grid.ghostOffset.y,
                         g_ui.grid.ghostW, g_ui.grid.ghostH, SWP_NOACTIVATE | SWP_NOSIZE);
        }
    }
}


void GridGesture::MoveState::Clear()
{
    pressRow = pressCol = 0;
    selR1 = selC1 = 0;
    selH = selW = 1;
    targetRow = targetCol = -1;
    blocks.clear();
}


bool FormulaRefWord(wchar_t ch)
{
    return iswalnum(ch) || ch == L'_' || ch == L'$' || ch == L'.';
}


std::wstring ShiftFormulaRefs(const std::wstring& f, int dr, int dc)
{
    if (f.empty() || (dr == 0 && dc == 0)) return f;
    std::wstring out;
    out.reserve(f.size() + 8);
    bool quote = false;
    size_t i = 0;
    while (i < f.size()) {
        wchar_t ch = f[i];
        if (ch == L'"') { quote = !quote; out.push_back(ch); ++i; continue; }
        if (quote) { out.push_back(ch); ++i; continue; }

        size_t start = i;
        bool absC = false;
        if (f[i] == L'$') { absC = true; ++i; }
        int col = 0, letters = 0;
        while (i < f.size() && iswalpha(f[i]) && letters < 3) {
            col = col * 26 + (towupper(f[i]) - L'A' + 1);
            ++i; ++letters;
        }
        if (letters == 0) {
            if (i == start) {
                out.push_back(f[i]);
                ++i;
            } else {
                out.append(f, start, i - start);
            }
            continue;
        }
        bool absR = false;
        if (i < f.size() && f[i] == L'$') { absR = true; ++i; }
        int row = 0, digits = 0;
        while (i < f.size() && iswdigit(f[i]) && digits < 7) {
            row = row * 10 + (f[i] - L'0');
            ++i; ++digits;
        }
        if (digits == 0) {
            out.append(f, start, i - start);
            continue;
        }
        bool prevOk = (start == 0) || !FormulaRefWord(f[start - 1]);
        bool nextOk = (i >= f.size()) || !FormulaRefWord(f[i]);
        bool functionName = (i < f.size() && f[i] == L'(');
        if (!prevOk || !nextOk || functionName) {
            out.append(f, start, i - start);
            continue;
        }
        int nr = absR ? row : row + dr;
        int nc = absC ? col : col + dc;
        if (nr < 1 || nc < 1 || nr > 1048576 || nc > 16384) {
            out += L"#REF!";
            continue;
        }
        if (absC) out.push_back(L'$');
        wchar_t buf[8]; int n = 0;
        while (nc > 0) {
            int rem = (nc - 1) % 26;
            buf[n++] = (wchar_t)(L'A' + rem);
            nc = (nc - 1) / 26;
        }
        while (n > 0) out.push_back(buf[--n]);
        if (absR) out.push_back(L'$');
        out += std::to_wstring(nr);
    }
    return out;
}


bool GridGesture::ApplyMove(Doc& d)
{
    std::vector<MergeRange> blocks = move.blocks;
    if (blocks.empty()) blocks = SelectedBlocks(d);
    if (blocks.empty()) return false;

    int p1 = blocks[0].r1, q1 = blocks[0].c1;
    int tr = move.targetRow, tc = move.targetCol;
    int dr = tr - p1, dc = tc - q1;
    if (dr == 0 && dc == 0) return false;

    for (auto& b : blocks) {
        if (b.r1 + dr < 0 || b.c1 + dc < 0 ||
            b.r2 + dr >= d.rows || b.c2 + dc >= d.cols) return false;
    }

    d.PushUndo();
    struct MovedCell { int sr, sc; Cell cell; };
    std::vector<MovedCell> moved;
    std::unordered_set<long long> seen;
    for (auto& b : blocks) {
        for (int r = b.r1; r <= b.r2; ++r) {
            for (int c = b.c1; c <= b.c2; ++c) {
                long long k = CellKey(r, c);
                if (!seen.insert(k).second) continue;
                auto it = d.cells.find(k);
                if (it == d.cells.end()) continue;
                if (it->second.IsEmpty() && it->second.st.IsDefault()) continue;
                moved.push_back({ r, c, it->second });
            }
        }
    }
    for (auto& b : blocks) {
        for (int r = b.r1; r <= b.r2; ++r)
            for (int c = b.c1; c <= b.c2; ++c)
                d.cells.erase(CellKey(r, c));
    }
    for (auto& m : moved) {
        Cell cell = std::move(m.cell);
        if (!cell.formula.empty())
            cell.formula = ShiftFormulaRefs(cell.formula, dr, dc);
        d.cells[CellKey(m.sr + dr, m.sc + dc)] = std::move(cell);
    }

    std::vector<MergeRange> shiftedTargets;
    for (auto& b : blocks)
        shiftedTargets.push_back({ b.r1 + dr, b.c1 + dc, b.r2 + dr, b.c2 + dc });

    std::vector<MergeRange> keep;
    for (auto& m : d.merges) {
        bool fromSource = false, fullyInside = false;
        for (auto& b : blocks) {
            if (!(b.r2 < m.r1 || b.r1 > m.r2 || b.c2 < m.c1 || b.c1 > m.c2)) {
                fromSource = true;
                if (m.r1 >= b.r1 && m.c1 >= b.c1 && m.r2 <= b.r2 && m.c2 <= b.c2)
                    fullyInside = true;
            }
        }
        if (fromSource) {
            if (fullyInside) keep.push_back({ m.r1 + dr, m.c1 + dc, m.r2 + dr, m.c2 + dc });
            continue;
        }
        bool hitsTarget = false;
        for (auto& t : shiftedTargets) {
            if (!(t.r2 < m.r1 || t.r1 > m.r2 || t.c2 < m.c1 || t.c1 > m.c2)) {
                hitsTarget = true;
                break;
            }
        }
        if (!hitsTarget) keep.push_back(m);
    }
    d.merges.swap(keep);

    d.selAR = move.selR1 + dr;
    d.selAC = move.selC1 + dc;
    d.selFR = d.selAR + move.selH - 1;
    d.selFC = d.selAC + move.selW - 1;
    for (auto& e : d.selExtra) {
        e.r1 += dr; e.c1 += dc; e.r2 += dr; e.c2 += dc;
    }
    d.Touch();
    EnsureCellVisible(d, d.selFR, d.selFC);
    return true;
}


// ---------------------------------------------------------------------------
//  viewport policy (document-owned)
// ---------------------------------------------------------------------------
// Capacity is DECLARED once per frame as a pure function of the VIEW:
//
//   needRows = max(minRows, usedRows, viewRows)
//   rows     = needRows
//
// viewRows is the number of rows the view must be able to show. Its base is
// the COMMITTED origin (moving back therefore SHRINKS the sheet); it is raised
// by a ViewRequest only when that request reaches past the currently reachable
// origin (navigating forward therefore GROWS it). Both feed ONE assignment:
// no grow path, no trim path, no "reached the top / hit the end" test.
//
// Exact, not block-aligned: the scrollbar thumb is track * screen / sheet, so
// every row added is a pixel of thumb. A 20-row quantum would put a visible
// step in the bar on every growth. One row of demand, one row of sheet.
//
// Nothing here is read by any device, and no device has a capacity rule.
void Doc::ReconcileCapacity(int viewW, int viewH)
{
    RebuildLayout();
    int w = std::max(1, viewW), h = std::max(1, viewH);

    // Quantised view need. Real layout offsets keep custom row/column sizes
    // exact inside the sheet; extrapolation covers requests beyond capacity.
    int viewRows = std::min(rows, ViewRowsFor(scrollY + h));
    int viewCols = std::min(cols, ViewColsFor(scrollX + w));
    if (viewTargetY > MaxScrollY(h))
        viewRows = std::max(viewRows, ViewRowsFor(viewTargetY + h));
    if (viewTargetX > MaxScrollX(w))
        viewCols = std::max(viewCols, ViewColsFor(viewTargetX + w));

    CapacitySig sig;
    sig.rev = g_contentRev;
    sig.vw = w; sig.vh = h;
    sig.rows = rows; sig.cols = cols;
    sig.totalW = colOff[cols]; sig.totalH = rowOff[rows];
    sig.minR = minRows; sig.minC = minCols;
    sig.selR = std::max(selAR, selFR);
    sig.selC = std::max(selAC, selFC);
    for (auto& e : selExtra) {
        sig.selR = std::max(sig.selR, e.r2);
        sig.selC = std::max(sig.selC, e.c2);
    }
    sig.viewR = viewRows; sig.viewC = viewCols;
    if (capacitySig.Same(sig)) return;   // O(1): nothing that sizes the sheet changed
    capacitySig = sig;

    // Content extent is cached, not rescanned: this runs on every drag frame
    // while the pointer is outside the track, and the scan is O(cells).
    int usedRows = 0, usedCols = 0;
    ContentExtent(usedRows, usedCols);
    usedRows = std::max(usedRows, sig.selR + 1);
    usedCols = std::max(usedCols, sig.selC + 1);

    int nextRows = std::min(20000, std::max(minRows, std::max(usedRows, viewRows)));
    int nextCols = std::min(256,   std::max(minCols, std::max(usedCols, viewCols)));
    if (nextRows < 1) nextRows = 1;
    if (nextCols < 1) nextCols = 1;

    bool changed = false;
    if (nextRows != rows) {
        rows = nextRows;
        rowH.resize(rows, g_rowH);
        if (selAR >= rows) selAR = rows - 1;
        if (selFR >= rows) selFR = rows - 1;
        changed = true;
    }
    if (nextCols != cols) {
        cols = nextCols;
        colW.resize(cols, g_colW);
        if (selAC >= cols) selAC = cols - 1;
        if (selFC >= cols) selFC = cols - 1;
        changed = true;
    }
    if (changed) {
        // Tail-only change: every offset already measured is still exact, so
        // the layout is marked stale but NOT invalidated. Rebuilding costs one
        // row per added row instead of a pass over the whole sheet.
        layoutDirty = true;
        RebuildLayout();         // fold the new size into the guard, one scan only
        capacitySig.totalW = colOff[cols];
        capacitySig.totalH = rowOff[rows];
        capacitySig.rows = rows;
        capacitySig.cols = cols;
        MarkCapacityChanged();   // capacity is not data: g_contentRev stays put
    }
}



// ---------------------------------------------------------------------------
//  lightweight drag band (selection / target / resize preview)
// ---------------------------------------------------------------------------

LRESULT CALLBACK BandWndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT rc;
        GetClientRect(h, &rc);
        FillRectC(dc, rc, kBandKey);
        if (g_bandLine) FillRectC(dc, rc, g_th.accent);
        else FrameRectC(dc, rc, g_th.accent, DpiScale(2));
        EndPaint(h, &ps);
        return 0;
    }
    }
    return DefWindowProcW(h, msg, wp, lp);
}


void RegisterBandClass()
{
    static bool done = false;
    if (done) return;
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = BandWndProc;
    wc.hInstance = g_inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = L"ExcelBand";
    RegisterClassExW(&wc);
    done = true;
}


void HideBand()
{
    if (g_bandWnd) ShowWindow(g_bandWnd, SW_HIDE);
}


void ShowBandRect(const RECT& screenRc, bool line)
{
    int w = (int)(screenRc.right - screenRc.left);
    int h = (int)(screenRc.bottom - screenRc.top);
    if (w <= 0 || h <= 0) { HideBand(); return; }
    if (!g_bandWnd) {
        RegisterBandClass();
        g_bandWnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
                                    L"ExcelBand", L"", WS_POPUP, 0, 0, 1, 1,
                                    g_hwnd, nullptr, g_inst, nullptr);
        if (!g_bandWnd) return;
        SetLayeredWindowAttributes(g_bandWnd, kBandKey, 255, LWA_COLORKEY);
    }
    g_bandLine = line;
    SetWindowPos(g_bandWnd, HWND_TOP, screenRc.left, screenRc.top, w, h,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(g_bandWnd, nullptr, FALSE);
}


void ShowBandClientRect(RECT clientRc, bool line)
{
    POINT a = { clientRc.left, clientRc.top };
    POINT b = { clientRc.right, clientRc.bottom };
    ClientToScreen(g_hwnd, &a);
    ClientToScreen(g_hwnd, &b);
    RECT sr = { a.x, a.y, b.x, b.y };
    ShowBandRect(sr, line);
}

// ---------------------------------------------------------------------------
//  mouse
// ---------------------------------------------------------------------------
void UpdateCursorForPoint(POINT pt)
{
    Doc* dp = ActiveDoc();
    if (!dp) { SetCursor(LoadCursorW(nullptr, IDC_ARROW)); return; }
    Doc& d = *dp;
    if (PtInRect(&GridGripRect(), pt)) { SetCursor(LoadCursorW(nullptr, IDC_SIZENWSE)); return; }
    g_ui.grid.SyncBars(d);
    if (g_ui.grid.vbar.HitTrack(pt) || g_ui.grid.hbar.HitTrack(pt)) {
        SetCursor(LoadCursorW(nullptr, IDC_ARROW));
        return;
    }
    RECT g = GridAreaRect();
    RECT ch = { g.left + HeaderW(), g.top, g.right - ScrollW(), g.top + HeaderH() };
    if (PtInRect(&ch, pt)) {
        int c = ColAtX(d, pt.x);
        int left = XOfCol(d, c), right = left + d.ColPx(c);
        if (abs(pt.x - left) <= DpiScale(3) || abs(pt.x - right) <= DpiScale(3))
            SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
        else
            SetCursor(LoadCursorW(nullptr, IDC_ARROW));
        return;
    }
    RECT rh = { g.left, g.top + HeaderH(), g.left + HeaderW(), g.bottom - ScrollW() };
    if (PtInRect(&rh, pt)) {
        int r = RowAtY(d, pt.y);
        int top = YOfRow(d, r), bottom = top + d.RowPx(r);
        if (abs(pt.y - top) <= DpiScale(3) || abs(pt.y - bottom) <= DpiScale(3))
            SetCursor(LoadCursorW(nullptr, IDC_SIZENS));
        else
            SetCursor(LoadCursorW(nullptr, IDC_ARROW));
        return;
    }
    RECT b = GridBodyRect();
    if (PtInRect(&b, pt)) {
        RECT sr = SelectionScreenRect(d);
        int t = DpiScale(4);
        if (PtInRect(&sr, pt) &&
            (pt.x - sr.left < t || sr.right - pt.x <= t || pt.y - sr.top < t || sr.bottom - pt.y <= t)) {
            SetCursor(LoadCursorW(nullptr, IDC_SIZEALL));
            return;
        }
        SetCursor(LoadCursorW(nullptr, IDC_ARROW));
        return;
    }
    SetCursor(LoadCursorW(nullptr, IDC_ARROW));
}


void GridView::MouseDown(POINT pt, const UiMods& mods)
{
    Doc* dp = ActiveDoc();
    if (!dp) return;
    Doc& d = *dp;

    // The view, not the input layer, decides that the secondary button pans.
    if (mods.button == 3) {
        if (!PtInRect(&GridBodyRect(), pt)) return;
        CloseAllPopups();
        if (EditorActive()) CommitEditor(false, 0);
        SetFocus(g_hwnd);
        pan.Begin(&d, pt);
        SetCapture(g_hwnd);
        SetCursor(LoadCursorW(nullptr, IDC_SIZEALL));
        return;
    }
    if (mods.button != 1) return;

    bool shift = mods.shift;
    bool ctrl = mods.ctrl;
    if (EditorActive()) CommitEditor(false, 0);
    if (!ctrl) g_ui.grid.gesture.fromMerge = false;
    HideBand();
    RECT grip = GridGripRect();
    if (PtInRect(&grip, pt)) {
        g_ui.grid.gesture.kind = GestureKind::ResizeGrid;
        g_ui.grid.gesture.gridResizeStart = pt;
        g_ui.grid.gesture.gridStartRows = d.rows;
        g_ui.grid.gesture.gridStartCols = d.cols;
        g_ui.grid.gesture.gridTargetRows = d.rows;
        g_ui.grid.gesture.gridTargetCols = d.cols;
        SetCapture(g_hwnd);
        return;
    }

    SyncBars(d);
    if (vbar.HitTrack(pt)) {
        if (vbar.HitThumb(pt)) {
            vbar.BeginDrag(pt);
        } else {
            int target = vbar.PageOffset(pt);
            g_ui.grid.ApplyViewRequest(d, ViewRequest{ d.scrollX, target });
            SyncBars(d);              // the page lands first...
            vbar.BeginDrag(pt);   // ...then the gesture starts from there
        }
        SetCapture(g_hwnd);
        InvalidateRect(g_hwnd, nullptr, FALSE);
        return;
    }
    if (hbar.HitTrack(pt)) {
        if (hbar.HitThumb(pt)) {
            hbar.BeginDrag(pt);
        } else {
            int target = hbar.PageOffset(pt);
            g_ui.grid.ApplyViewRequest(d, ViewRequest{ target, d.scrollY });
            SyncBars(d);              // the page lands first...
            hbar.BeginDrag(pt);       // ...then the gesture starts from there
        }
        SetCapture(g_hwnd);
        InvalidateRect(g_hwnd, nullptr, FALSE);
        return;
    }

    RECT g = GridAreaRect();
    RECT ch = { g.left + HeaderW(), g.top, g.right - ScrollW(), g.top + HeaderH() };
    if (PtInRect(&ch, pt)) {
        int c = ColAtX(d, pt.x);
        int left = XOfCol(d, c);
        int right = left + d.ColPx(c);
        if (abs(pt.x - left) <= DpiScale(3) && c > 0) {
            g_ui.grid.gesture.kind = GestureKind::ResizeCol;
            g_ui.grid.gesture.resizeIndex = c - 1;
            g_ui.grid.gesture.resizeStartBase = d.colW[g_ui.grid.gesture.resizeIndex];
            g_ui.grid.gesture.resizeNewBase = g_ui.grid.gesture.resizeStartBase;
            g_ui.grid.gesture.resizeStartMouse = pt.x;
            SetCapture(g_hwnd);
            return;
        }
        if (abs(pt.x - right) <= DpiScale(3)) {
            g_ui.grid.gesture.kind = GestureKind::ResizeCol;
            g_ui.grid.gesture.resizeIndex = c;
            g_ui.grid.gesture.resizeStartBase = d.colW[c];
            g_ui.grid.gesture.resizeNewBase = g_ui.grid.gesture.resizeStartBase;
            g_ui.grid.gesture.resizeStartMouse = pt.x;
            SetCapture(g_hwnd);
            return;
        }
        d.selAR = 0;
        d.selFR = d.rows - 1;
        d.selAC = d.selFC = c;
        InvalidateRect(g_hwnd, nullptr, FALSE);
        return;
    }
    RECT rh = { g.left, g.top + HeaderH(), g.left + HeaderW(), g.bottom - ScrollW() };
    if (PtInRect(&rh, pt)) {
        int r = RowAtY(d, pt.y);
        int top = YOfRow(d, r);
        int bottom = top + d.RowPx(r);
        if (abs(pt.y - top) <= DpiScale(3) && r > 0) {
            g_ui.grid.gesture.kind = GestureKind::ResizeRow;
            g_ui.grid.gesture.resizeIndex = r - 1;
            g_ui.grid.gesture.resizeStartBase = d.rowH[g_ui.grid.gesture.resizeIndex];
            g_ui.grid.gesture.resizeNewBase = g_ui.grid.gesture.resizeStartBase;
            g_ui.grid.gesture.resizeStartMouse = pt.y;
            SetCapture(g_hwnd);
            return;
        }
        if (abs(pt.y - bottom) <= DpiScale(3)) {
            g_ui.grid.gesture.kind = GestureKind::ResizeRow;
            g_ui.grid.gesture.resizeIndex = r;
            g_ui.grid.gesture.resizeStartBase = d.rowH[r];
            g_ui.grid.gesture.resizeNewBase = g_ui.grid.gesture.resizeStartBase;
            g_ui.grid.gesture.resizeStartMouse = pt.y;
            SetCapture(g_hwnd);
            return;
        }
        d.selAC = 0;
        d.selFC = d.cols - 1;
        d.selAR = d.selFR = r;
        InvalidateRect(g_hwnd, nullptr, FALSE);
        return;
    }

    RECT b = GridBodyRect();
    if (!PtInRect(&b, pt)) return;
    int r = RowAtY(d, pt.y);
    int c = ColAtX(d, pt.x);

    if (ctrl) {
        MergeRange er = { r, c, r, c };
        if (MergeRange* m = d.MergeAt(r, c)) er = *m;
        auto it = std::find_if(d.selExtra.begin(), d.selExtra.end(), [&](const MergeRange& e) {
            return e.r1 == er.r1 && e.c1 == er.c1 && e.r2 == er.r2 && e.c2 == er.c2;
        });
        if (it != d.selExtra.end()) d.selExtra.erase(it);
        else d.selExtra.push_back(er);
        InvalidateRect(g_hwnd, nullptr, FALSE);
        return;
    }

    if (!shift && SelectionBorderHit(d, pt)) {
        g_ui.grid.gesture.BeginMove(d, pt);
        SetCapture(g_hwnd);
        return;
    }

    if (MergeRange* m = d.MergeAt(r, c)) {
        if (shift) {
            d.selFR = m->r2;
            d.selFC = m->c2;
            InvalidateRect(g_hwnd, nullptr, FALSE);
            return;
        }
        d.selExtra.clear();
        d.selAR = d.selFR = m->r1;
        d.selAC = d.selFC = m->c1;
        d.selFR = g_ui.grid.gesture.selTargetR = m->r2;
        d.selFC = g_ui.grid.gesture.selTargetC = m->c2;
        g_ui.grid.gesture.mergeRange = *m;
        g_ui.grid.gesture.fromMerge = true;
        g_ui.grid.gesture.kind = GestureKind::Select;
        SetCapture(g_hwnd);
        InvalidateRect(g_hwnd, nullptr, FALSE);
        return;
    }



    if (shift) {
        d.selFR = r;
        d.selFC = c;
    } else {
        d.selExtra.clear();
        d.selAR = r;
        d.selAC = c;
        d.selFR = r;
        d.selFC = c;
    }
    g_ui.grid.gesture.kind = GestureKind::Select;
    g_ui.grid.gesture.selTargetR = d.selFR;
    g_ui.grid.gesture.selTargetC = d.selFC;
    SetCapture(g_hwnd);
    InvalidateRect(g_hwnd, nullptr, FALSE);
}


// Hover paints exactly two things: one row header cell and one column header
// cell. So a hover change invalidates those strips and nothing else - a
// full-window repaint for a 26px highlight would rebuild every visible cell.
// Both rects are clipped to their own band, so an off-screen or scrolled-away
// row costs nothing.
static void InvalidateRowHeader(Doc& d, int r)
{
    if (!g_hwnd || r < 0 || r >= d.rows) return;
    RECT g = GridAreaRect();
    RECT band = { g.left, g.top + HeaderH(), g.left + HeaderW(), g.bottom - ScrollW() };
    int y = YOfRow(d, r);
    RECT rc = { band.left, y, band.right, y + d.RowPx(r) };
    if (rc.top < band.top) rc.top = band.top;
    if (rc.bottom > band.bottom) rc.bottom = band.bottom;
    if (rc.bottom <= rc.top) return;
    InvalidateRect(g_hwnd, &rc, FALSE);
}

static void InvalidateColHeader(Doc& d, int c)
{
    if (!g_hwnd || c < 0 || c >= d.cols) return;
    RECT g = GridAreaRect();
    RECT band = { g.left + HeaderW(), g.top, g.right - ScrollW(), g.top + HeaderH() };
    int x = XOfCol(d, c);
    RECT rc = { x, band.top, x + d.ColPx(c), band.bottom };
    if (rc.left < band.left) rc.left = band.left;
    if (rc.right > band.right) rc.right = band.right;
    if (rc.right <= rc.left) return;
    InvalidateRect(g_hwnd, &rc, FALSE);
}


void GridView::MouseMove(POINT pt, const UiMods& mods)
{
    (void)mods;
    Doc* dp = ActiveDoc();
    if (!dp) return;
    Doc& d = *dp;
    if (pan.active) {
        int x = 0, y = 0;
        pan.Target(x, y, pt);                       // pure read: a view request
        g_ui.grid.ApplyViewRequest(d, ViewRequest{ x, y });
        SetCursor(LoadCursorW(nullptr, IDC_SIZEALL));
        return;
    }
    bool captured = GetCapture() == g_hwnd;

    if (g_ui.grid.gesture.Is(GestureKind::ResizeGrid)) {
        int rh = std::max(1, GridScale(g_rowH));
        int cw = std::max(1, GridScale(g_colW));
        int dr = (pt.y - g_ui.grid.gesture.gridResizeStart.y) / rh;
        int dc = (pt.x - g_ui.grid.gesture.gridResizeStart.x) / cw;
        // Cached content extent, not a rescan of `cells`: this runs on every
        // pointer move for the whole length of the corner drag.
        int usedR = 0, usedC = 0;
        d.ContentExtent(usedR, usedC);
        int maxR = std::max(0, usedR - 1);
        int maxC = std::max(0, usedC - 1);
        int nr = g_ui.grid.gesture.gridStartRows + dr;
        int nc = g_ui.grid.gesture.gridStartCols + dc;
        if (nr < maxR + 1) nr = maxR + 1;
        if (nc < maxC + 1) nc = maxC + 1;
        if (nr < 1) nr = 1;
        if (nc < 1) nc = 1;
        if (nr > 20000) nr = 20000;
        if (nc > 256) nc = 256;
        g_ui.grid.gesture.gridTargetRows = nr;
        g_ui.grid.gesture.gridTargetCols = nc;
        SetStatus(Format(L"行列：%d × %d", nr, nc));
        return;
    }

    if (vbar.dragging) { ScrollBarDrag(vbar, d, pt); return; }
    if (hbar.dragging) { ScrollBarDrag(hbar, d, pt); return; }

    if (g_ui.grid.gesture.Is(GestureKind::ResizeCol) && g_ui.grid.gesture.resizeIndex >= 0) {
        int delta = pt.x - g_ui.grid.gesture.resizeStartMouse;
        int px = GridScale(g_ui.grid.gesture.resizeStartBase) + delta;
        int minPx = DpiScale(24);
        if (px < minPx) px = minPx;
        int base = MulDiv(px, 9600, g_dpi * g_gridZoom);
        if (base < 20) base = 20;
        if (base > 2000) base = 2000;
        g_ui.grid.gesture.resizeNewBase = base;
        int x = XOfCol(d, g_ui.grid.gesture.resizeIndex) + GridScale(base);
        RECT b = GridBodyRect();
        RECT line = { x, b.top, x + DpiScale(2), b.bottom - ScrollW() };
        ShowBandClientRect(line, true);
        return;
    }
    if (g_ui.grid.gesture.Is(GestureKind::ResizeRow) && g_ui.grid.gesture.resizeIndex >= 0) {
        int delta = pt.y - g_ui.grid.gesture.resizeStartMouse;
        int px = GridScale(g_ui.grid.gesture.resizeStartBase) + delta;
        int minPx = DpiScale(16);
        if (px < minPx) px = minPx;
        int base = MulDiv(px, 9600, g_dpi * g_gridZoom);
        if (base < 12) base = 12;
        if (base > 1000) base = 1000;
        g_ui.grid.gesture.resizeNewBase = base;
        int y = YOfRow(d, g_ui.grid.gesture.resizeIndex) + GridScale(base);
        RECT b = GridBodyRect();
        RECT line = { b.left, y, b.right - ScrollW(), y + DpiScale(2) };
        ShowBandClientRect(line, true);
        return;
    }

    if (g_ui.grid.gesture.Is(GestureKind::Select) && captured) {
        RECT b = GridBodyRect();
        POINT q = pt;
        if (q.y < b.top + DpiScale(4)) q.y = b.top + DpiScale(4);
        if (q.y >= b.bottom) q.y = b.bottom - 1;
        if (q.x < b.left + DpiScale(4)) q.x = b.left + DpiScale(4);
        if (q.x >= b.right) q.x = b.right - 1;
        int oldX = d.scrollX, oldY = d.scrollY;
        AutoScrollForPoint(d, pt);
        if (oldX != d.scrollX || oldY != d.scrollY) InvalidateAll();

        int r = RowAtY(d, q.y);
        int c = ColAtX(d, q.x);
        g_ui.grid.gesture.selTargetR = r;
        g_ui.grid.gesture.selTargetC = c;
        RECT u;
        if (g_ui.grid.gesture.fromMerge) {
            int ar1 = std::min(g_ui.grid.gesture.mergeRange.r1, r);
            int ac1 = std::min(g_ui.grid.gesture.mergeRange.c1, c);
            int ar2 = std::max(g_ui.grid.gesture.mergeRange.r2, r);
            int ac2 = std::max(g_ui.grid.gesture.mergeRange.c2, c);
            RECT a = CellRect(d, ar1, ac1);
            RECT z = CellRect(d, ar2, ac2);
            u = { a.left, a.top, z.right, z.bottom };
        } else {
            RECT ar = CellRect(d, d.selAR, d.selAC);
            RECT cr = CellRect(d, r, c);
            u = { std::min(ar.left, cr.left), std::min(ar.top, cr.top),
                  std::max(ar.right, cr.right), std::max(ar.bottom, cr.bottom) };
        }
        ShowBandClientRect(u, false);
        return;
    }

    if (g_ui.grid.gesture.Is(GestureKind::MovePending) && captured) {
        if (abs(pt.x - g_ui.grid.gesture.press.x) + abs(pt.y - g_ui.grid.gesture.press.y) > DpiScale(5)) {
            g_ui.grid.gesture.kind = GestureKind::MoveCells;
            CreateGhost(d, pt);
        }
    }
    if (g_ui.grid.gesture.Is(GestureKind::MoveCells) && captured) {
        RECT b = GridBodyRect();
        AutoScrollForPoint(d, pt);
        POINT q = pt;
        if (q.y < b.top) q.y = b.top;
        if (q.y >= b.bottom) q.y = b.bottom - 1;
        if (q.x < b.left) q.x = b.left;
        if (q.x >= b.right) q.x = b.right - 1;
        g_ui.grid.gesture.UpdateMove(d, q);
        return;
    }

    RECT b = GridBodyRect();
    int r = -1, c = -1;
    if (PtInRect(&b, pt)) {
        r = RowAtY(d, pt.y);
        c = ColAtX(d, pt.x);
    }
    // Only a CHANGE repaints, and only the two header cells involved: the old
    // one has to be repainted back to normal, the new one to highlight.
    int oldR = g_ui.grid.hoverRow, oldC = g_ui.grid.hoverCol;
    if (r != oldR) { InvalidateRowHeader(d, oldR); InvalidateRowHeader(d, r); }
    if (c != oldC) { InvalidateColHeader(d, oldC); InvalidateColHeader(d, c); }
    g_ui.grid.hoverRow = r;
    g_ui.grid.hoverCol = c;
    UpdateCursorForPoint(pt);
}


void GridView::MouseLeave()
{
    if (hoverRow == -1 && hoverCol == -1) return;
    hoverRow = -1;
    hoverCol = -1;
    if (g_hwnd) InvalidateRect(g_hwnd, &rect, FALSE);
}


void GridView::MouseUp(POINT pt, const UiMods& mods)
{
    Doc* d = ActiveDoc();
    if (mods.button == 3) {
        if (pan.active) EndPan(pt);
        if (GetCapture() == g_hwnd) ReleaseCapture();
        return;
    }
    if (mods.button != 1) return;

    if (vbar.dragging || hbar.dragging) {
        ScrollBarUp();
        if (GetCapture() == g_hwnd) ReleaseCapture();
        return;
    }

    GestureKind kind = g_ui.grid.gesture.kind;
    if (kind == GestureKind::None) return;

    switch (kind) {
    case GestureKind::ResizeGrid:
        if (d && g_ui.grid.gesture.gridTargetRows > 0 && g_ui.grid.gesture.gridTargetCols > 0) {
            d->rows = g_ui.grid.gesture.gridTargetRows;
            d->cols = g_ui.grid.gesture.gridTargetCols;
            d->minRows = d->rows;   // explicit user size becomes the floor
            d->minCols = d->cols;
            d->rowH.resize(d->rows, g_rowH);
            d->colW.resize(d->cols, g_colW);
            if (d->selAR >= d->rows) d->selAR = d->rows - 1;
            if (d->selFR >= d->rows) d->selFR = d->rows - 1;
            if (d->selAC >= d->cols) d->selAC = d->cols - 1;
            if (d->selFC >= d->cols) d->selFC = d->cols - 1;
            d->InvalidateLayout();
            d->Touch();
        }
        HideBand();
        InvalidateAll();
        break;

    case GestureKind::ResizeCol:
    case GestureKind::ResizeRow:
        if (d && g_ui.grid.gesture.resizeIndex >= 0) {
            if (kind == GestureKind::ResizeCol && g_ui.grid.gesture.resizeIndex < (int)d->colW.size())
                d->colW[g_ui.grid.gesture.resizeIndex] = g_ui.grid.gesture.resizeNewBase;
            if (kind == GestureKind::ResizeRow && g_ui.grid.gesture.resizeIndex < (int)d->rowH.size())
                d->rowH[g_ui.grid.gesture.resizeIndex] = g_ui.grid.gesture.resizeNewBase;
            d->InvalidateLayout();
            d->Touch();
        }
        HideBand();
        InvalidateRect(g_hwnd, nullptr, FALSE);
        break;

    case GestureKind::MoveCells:
        HideBand();
        DestroyGhost();
        if (d) g_ui.grid.gesture.ApplyMove(*d);
        InvalidateRect(g_hwnd, nullptr, FALSE);
        break;

    case GestureKind::MovePending:
        break;

    case GestureKind::Select:
        if (d) {
            if (g_ui.grid.gesture.fromMerge) {
                d->selAR = std::min(g_ui.grid.gesture.mergeRange.r1, g_ui.grid.gesture.selTargetR);
                d->selAC = std::min(g_ui.grid.gesture.mergeRange.c1, g_ui.grid.gesture.selTargetC);
                d->selFR = std::max(g_ui.grid.gesture.mergeRange.r2, g_ui.grid.gesture.selTargetR);
                d->selFC = std::max(g_ui.grid.gesture.mergeRange.c2, g_ui.grid.gesture.selTargetC);
            } else {
                d->selFR = g_ui.grid.gesture.selTargetR;
                d->selFC = g_ui.grid.gesture.selTargetC;
            }
        }
        HideBand();
        InvalidateRect(g_hwnd, nullptr, FALSE);
        break;

    case GestureKind::None:
        break;
    }

    g_ui.grid.gesture.Reset();
    if (GetCapture() == g_hwnd) ReleaseCapture();
}


void OpenCellTarget(Doc& d, int r, int c)
{
    const Cell* cell = d.Find(r, c);
    if (!cell || cell->path.empty()) return;
    if (cell->kind == CK_URL) {
        ShellExecuteW(g_hwnd, L"open", cell->path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    } else if (cell->kind == CK_FOLDER) {
        std::wstring p = d.AbsPathOf(cell->path);
        ShellExecuteW(g_hwnd, L"open", p.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    } else {
        std::wstring p = d.AbsPathOf(cell->path);
        ShellExecuteW(g_hwnd, L"open", p.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
}


void GridView::DblClick(POINT pt, const UiMods& mods)
{
    (void)mods;
    Doc* dp = ActiveDoc();
    if (!dp) return;
    Doc& d = *dp;
    RECT b = GridBodyRect();
    if (!PtInRect(&b, pt)) return;
    int r = RowAtY(d, pt.y);
    int c = ColAtX(d, pt.x);
    const Cell* cell = d.Find(r, c);
    if (cell && cell->kind != CK_TEXT && !cell->path.empty()) {
        OpenCellTarget(d, r, c);
        return;
    }
    BeginEditCell(d, r, c, nullptr);
}


void GridView::ContextMenu(POINT pt, const UiMods& mods)
{
    (void)mods;
    Doc* dp = ActiveDoc();
    if (!dp) return;
    if (EditorActive()) CommitEditor(false, 0);
    Doc& d = *dp;
    RECT b = GridBodyRect();
    if (!PtInRect(&b, pt)) return;
    int r = RowAtY(d, pt.y);
    int c = ColAtX(d, pt.x);
    if (!d.CellInSelection(r, c)) {
        d.selAR = r;
        d.selAC = c;
        d.selFR = r;
        d.selFC = c;
        InvalidateRect(g_hwnd, nullptr, FALSE);
    }
    CloseListPopup();
    ClosePalettePopup();
    POINT sp = pt;
    ClientToScreen(g_hwnd, &sp);
    ShowMenuAt(BuildContextMenu(), sp.x, sp.y);
    g_ui.menuPopup.topIndex = -1;
}


// ---------------------------------------------------------------------------
//  wheel / zoom
// ---------------------------------------------------------------------------
void ZoomGridAt(POINT pt, int step)
{
    Doc* dp = ActiveDoc();
    if (!dp) return;
    Doc& d = *dp;
    int old = g_gridZoom;
    int z = old + step;
    if (z < 50) z = 50;
    if (z > 250) z = 250;
    if (z == old) return;

    int r = RowAtY(d, pt.y);
    int c = ColAtX(d, pt.x);
    RECT cr = CellRect(d, r, c);
    int offX = pt.x - cr.left;
    int offY = pt.y - cr.top;

    g_gridZoom = z;
    RebuildAllLayouts();
    d.RebuildLayout();
    RECT b = GridBodyRect();
    int tx = (int)(b.left + d.colOff[c] - (pt.x - offX));
    int ty = (int)(b.top + d.rowOff[r] - (pt.y - offY));
    // Zooming out exposes more cells; the request goes through the same view
    // layer as wheel / pan / scrollbar, so capacity follows the view rectangle.
    g_ui.grid.ApplyViewRequest(d, ViewRequest{ tx, ty });
    SaveSettings();
    InvalidateRect(g_hwnd, nullptr, FALSE);
}


bool GridView::Wheel(int delta, POINT pt, const UiMods& mods)
{
    Doc* dp = ActiveDoc();
    if (!dp) return true;
    Doc& d = *dp;
    if (mods.ctrl) {
        ZoomGridAt(pt, delta > 0 ? 10 : -10);
        if (EditorActive()) PositionEditor();   // editor belongs to this view
        return true;
    }
    int clicks = delta / WHEEL_DELTA;
    if (clicks == 0) clicks = delta > 0 ? 1 : -1;
    ViewRequest req{ d.scrollX, d.scrollY };
    if (mods.shift) {
        req.x -= clicks * DpiScale(70);
    } else {
        int amount = std::max(DpiScale(30), d.RowPx(d.selFR) * 3);
        req.y -= clicks * amount;
    }
    g_ui.grid.ApplyViewRequest(d, req);
    if (EditorActive()) PositionEditor();       // editor belongs to this view
    return true;
}


// ---------------------------------------------------------------------------
//  keyboard
// ---------------------------------------------------------------------------
bool GridView::KeyDown(WPARAM vk, const UiMods& mods)
{
    Doc* dp = ActiveDoc();
    if (!dp) return false;
    Doc& d = *dp;
    bool shift = mods.shift;
    bool ctrl = mods.ctrl;

    switch (vk) {
    case VK_UP:    MoveFocus(d, -1, 0, shift); return true;
    case VK_DOWN:  MoveFocus(d, 1, 0, shift); return true;
    case VK_LEFT:  MoveFocus(d, 0, -1, shift); return true;
    case VK_RIGHT: MoveFocus(d, 0, 1, shift); return true;
    case VK_TAB:   MoveFocus(d, 0, shift ? -1 : 1, false); return true;
    case VK_RETURN: MoveFocus(d, shift ? -1 : 1, 0, false); return true;
    case VK_F2: BeginEditCell(d, d.selFR, d.selFC, nullptr); return true;
    case VK_DELETE: ClearSelectionContents(d); return true;
    case VK_BACK: {
        std::wstring empty;
        BeginEditCell(d, d.selFR, d.selFC, &empty);
        return true;
    }
    case VK_HOME:
        if (ctrl) {
            d.selAR = 0; d.selAC = 0;
            d.selFR = 0; d.selFC = 0;
        } else {
            d.selAC = 0;
            d.selFC = 0;
        }
        EnsureCellVisible(d, d.selFR, 0);
        InvalidateRect(g_hwnd, nullptr, FALSE);
        return true;
    case VK_END:
        if (ctrl) {
            int maxR = 0, maxC = 0;
            for (auto& kv : d.cells) {
                if (kv.second.IsEmpty()) continue;
                maxR = std::max(maxR, KeyRow(kv.first));
                maxC = std::max(maxC, KeyCol(kv.first));
            }
            d.selAR = maxR; d.selAC = maxC;
            d.selFR = maxR; d.selFC = maxC;
            EnsureCellVisible(d, maxR, maxC);
        } else {
            d.selFC = d.cols - 1;
            EnsureCellVisible(d, d.selFR, d.selFC);
        }
        InvalidateRect(g_hwnd, nullptr, FALSE);
        return true;
    case VK_PRIOR:
    case VK_NEXT: {
        RECT b = GridBodyRect();
        int rowsPerPage = std::max(1, (int)((b.bottom - b.top) / std::max(1, d.RowPx(d.selFR))));
        int dr = (vk == VK_PRIOR ? -rowsPerPage : rowsPerPage);
        g_ui.grid.ApplyViewRequest(d, ViewRequest{ d.scrollX, d.scrollY + dr * d.RowPx(d.selFR) });
        int nr = d.selFR + dr;
        if (nr < 0) nr = 0;
        if (nr >= d.rows) nr = d.rows - 1;
        d.selFR = nr;
        if (!shift) d.selAR = nr;
        InvalidateRect(g_hwnd, nullptr, FALSE);
        return true;
    }
    case VK_ESCAPE:
        CloseAllPopups();
        return true;
    }
    return false;
}


bool GridView::Char(wchar_t ch, const UiMods& mods)
{
    if (ch < 32) return false;
    if (mods.ctrl) return false;
    Doc* dp = ActiveDoc();
    if (!dp) return false;
    if (AnyPopupOpen()) return false;
    std::wstring s(1, ch);
    BeginEditCell(*dp, dp->selFR, dp->selFC, &s);
    return true;
}


