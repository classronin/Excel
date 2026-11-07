// ============================================================================
//  p5_popup.cpp
//  Responsibility: popup objects' logic layer.
//
//  A popup is a UiObject owned by UiRoot. Its HWND is only a presentation
//  host: the WndProc in core.h packages device data, this file turns that data
//  into object state, declares the frame's spec and posts neutral actions to
//  ActionBus. No document, theme, layout or command is read during WM_PAINT.
//
//  Shared contract: core.h + only the api_*.h headers this module references.
//  This translation unit contains no type or state definitions of its own.
// ============================================================================
#include "core.h"
#include "api_p5_popup.h"
#include "api_p6a_grid_paint.h"

// ---------------------------------------------------------------------------
//  menu popup: object state and actions
// ---------------------------------------------------------------------------
void MenuPopup::OpenSub(int idx)
{
    if (idx < 0 || idx >= (int)items.size()) return;
    if (items[idx].kind != MK_SUB || !items[idx].enabled) return;
    if (sub && subIndex == idx) return;
    CloseSub();

    RECT wr;
    GetWindowRect(host, &wr);
    // A submenu is a child of THIS object, not of the root. The root's table
    // only holds objects it presents; nesting stays inside the popup tree.
    sub = std::make_unique<MenuPopup>();
    sub->Create(host, items[idx].sub, level + 1);
    int y = wr.top + ItemTop(idx) - padY;
    int x = wr.right - DpiScale(2);
    MONITORINFO mi = { sizeof(mi) };
    GetMonitorInfoW(MonitorFromPoint({ x, y }, MONITOR_DEFAULTTONEAREST), &mi);
    if (x + sub->width > mi.rcWork.right)
        x = wr.left - sub->width + DpiScale(2);
    subIndex = idx;
    sub->Position(x, y);
    sub->Commit();
}


void MenuPopup::CloseSub()
{
    if (sub) {
        sub->DestroyAll();
        sub.reset();
    }
    subIndex = -1;
}


void MenuPopup::DestroyAll()
{
    CloseSub();
    DestroyHost();
    hover = -1;
    subIndex = -1;
    items.clear();
}


void MenuPopup::Close()
{
    DestroyAll();
    topIndex = -1;
    if (g_hwnd) InvalidateRect(g_hwnd, nullptr, FALSE);
}


void MenuPopup::Input(const PopupInput& in)
{
    if (in.kind != PopupInputKind::Move && in.kind != PopupInputKind::Press) return;
    int idx = ItemAt(in.pt.x, in.pt.y);
    if (idx != hover) {
        hover = idx;
        Commit();                            // state change -> one declaration
    }
    if (idx >= 0 && items[idx].kind == MK_SUB && items[idx].enabled) OpenSub(idx);
    else if (idx >= 0) CloseSub();

    if (in.kind != PopupInputKind::Press || idx < 0) return;
    const MenuItem& it = items[idx];
    if (!it.enabled) return;
    if (it.kind == MK_SUB) { OpenSub(idx); return; }
    // The object outputs data; closing the menu and running the command are
    // decided by the action bus after this device event has finished.
    if (it.id) g_actions.Post(UiAction::MenuPick, it.id);
    else       g_actions.Post(UiAction::ClosePopups);
}


void MenuPopup::KeyMove(int delta)
{
    MenuPopup* cur = this;
    while (cur->sub) cur = cur->sub.get();
    int n = (int)cur->items.size();
    if (n == 0) return;
    int idx = cur->hover;
    for (int step = 0; step < n; ++step) {
        idx += delta;
        if (idx < 0) idx = n - 1;
        if (idx >= n) idx = 0;
        const MenuItem& it = cur->items[idx];
        if (it.kind == MK_SEP || !it.enabled) continue;
        cur->hover = idx;
        cur->Commit();
        if (it.kind == MK_SUB) cur->OpenSub(idx);
        else cur->CloseSub();
        break;
    }
}


bool MenuPopup::KeyExecute()
{
    MenuPopup* cur = this;
    while (cur->sub) cur = cur->sub.get();
    if (cur->hover < 0 || cur->hover >= (int)cur->items.size()) return false;
    const MenuItem& it = cur->items[cur->hover];
    if (!it.enabled) return false;
    if (it.kind == MK_SUB) { cur->OpenSub(cur->hover); return true; }
    if (it.id) g_actions.Post(UiAction::MenuPick, it.id);
    else       g_actions.Post(UiAction::ClosePopups);
    return true;
}


// ---------------------------------------------------------------------------
//  dropdown list: object state and actions
// ---------------------------------------------------------------------------
void ListPopup::Input(const PopupInput& in)
{
    switch (in.kind) {
    case PopupInputKind::Move: {
        int idx = Hit(in.pt.x, in.pt.y);
        if (idx != hover) {
            hover = idx;
            Commit();
        }
        break;
    }
    case PopupInputKind::Press: {
        int idx = Hit(in.pt.x, in.pt.y);
        if (idx < 0 || idx >= (int)items.size()) return;
        // Output the picked VALUE, not an index into somebody else's array and
        // not a callback that would destroy this object inside its WndProc.
        g_actions.Post(UiAction::ListPick, listKind, 0, items[idx]);
        break;
    }
    case PopupInputKind::Wheel: {
        int maxTop = (int)items.size() - visible;
        if (maxTop <= 0) return;
        int next = top - in.delta / WHEEL_DELTA;
        next = std::max(0, std::min(maxTop, next));
        if (next == top) return;
        top = next;
        Commit();
        break;
    }
    }
}


// ---------------------------------------------------------------------------
//  colour palette: object state and actions
// ---------------------------------------------------------------------------
void PalettePopup::Input(const PopupInput& in)
{
    if (in.kind == PopupInputKind::Move) {
        int idx = Hit(in.pt.x, in.pt.y);
        if (idx != hover) {
            hover = idx;
            Commit();
        }
        return;
    }
    if (in.kind != PopupInputKind::Press) return;
    int idx = Hit(in.pt.x, in.pt.y);
    if (idx == -2) return;
    COLORREF color = (idx == -1) ? CLR_NONE : PaletteColors()[idx];
    g_actions.Post(UiAction::PalettePick, target, color);
}


// ---------------------------------------------------------------------------
//  popup lifecycle: the root owns the objects, these functions only open and
//  close them. No global unique_ptr, no per-popup ownership scattered around.
// ---------------------------------------------------------------------------
void CloseMenuPopup()
{
    g_ui.menuPopup.Close();
}


bool MenuPopupIsOpen()
{
    return g_ui.menuPopup.Open();
}


void ShowMenuAt(const std::vector<MenuItem>& items, int x, int y)
{
    CloseMenuPopup();
    MenuPopup& m = g_ui.menuPopup;
    m.Create(g_hwnd, items, 0);
    m.Position(x, y);
    m.Commit();
}


void MenuKeyMove(int delta)
{
    g_ui.menuPopup.KeyMove(delta);
}


bool MenuKeyExecute()
{
    return g_ui.menuPopup.KeyExecute();
}


void CloseListPopup()
{
    ListPopup& lp = g_ui.listPopup;
    if (!lp.Open()) return;
    lp.DestroyHost();
    lp.items.clear();
    lp.sel = lp.hover = -1;
    lp.top = 0;
}


void ShowDropList(HWND owner, RECT anchor, std::vector<std::wstring> items,
                  int sel, int visible, int listKind)
{
    CloseListPopup();
    if (items.empty()) return;
    ListPopup& lp = g_ui.listPopup;
    lp.items = std::move(items);
    lp.sel = sel;
    lp.hover = -1;
    lp.listKind = listKind;
    lp.visible = std::min<int>(visible, (int)lp.items.size());
    lp.itemH = DpiScale(26);
    lp.pad = DpiScale(5);

    HDC dc = GetDC(nullptr);
    int maxw = 0;
    for (auto& s : lp.items) maxw = std::max(maxw, (int)MeasureText(dc, s, UiFont(10)).cx);
    ReleaseDC(nullptr, dc);
    lp.width = std::max((int)(anchor.right - anchor.left), maxw + DpiScale(34));
    lp.width = std::min(lp.width, DpiScale(420));
    lp.height = lp.pad * 2 + lp.itemH * lp.visible;
    lp.top = 0;
    if (sel >= 0) {
        lp.top = std::max(0, sel - lp.visible / 2);
        lp.top = std::min(lp.top, (int)lp.items.size() - lp.visible);
    }

    ListPopup::RegisterClass();
    lp.host = CreateWindowExW(WS_EX_TOOLWINDOW, L"ExcelList", L"", WS_POPUP,
                              0, 0, lp.width, lp.height, owner, nullptr, g_inst, nullptr);
    SetWindowLongPtrW(lp.host, GWLP_USERDATA, (LONG_PTR)&lp);
    lp.Position(anchor);
    lp.Commit();
}


void ClosePalettePopup()
{
    PalettePopup& pp = g_ui.palettePopup;
    if (!pp.Open()) return;
    pp.DestroyHost();
    pp.hover = -2;
}


void ShowPalette(HWND owner, RECT anchor, int target)
{
    ClosePalettePopup();
    PalettePopup& pp = g_ui.palettePopup;
    pp.target = target;
    pp.hover = -2;
    pp.pad = DpiScale(8);
    pp.sw = DpiScale(20);
    pp.gap = DpiScale(4);
    pp.specialH = pp.sw;
    pp.width = pp.pad * 2 + kPaletteCols * (pp.sw + pp.gap) - pp.gap;
    pp.height = pp.pad * 2 + pp.specialH + pp.gap + kPaletteRows * (pp.sw + pp.gap) - pp.gap;

    PalettePopup::RegisterClass();
    pp.host = CreateWindowExW(WS_EX_TOOLWINDOW, L"ExcelPalette", L"", WS_POPUP,
                              0, 0, pp.width, pp.height, owner, nullptr, g_inst, nullptr);
    SetWindowLongPtrW(pp.host, GWLP_USERDATA, (LONG_PTR)&pp);
    pp.Position(anchor);
    pp.Commit();
}


void CloseAllPopups()
{
    CloseMenuPopup();
    CloseListPopup();
    ClosePalettePopup();
    if (g_hwnd) InvalidateRect(g_hwnd, nullptr, FALSE);
}


bool AnyPopupOpen()
{
    return g_ui.menuPopup.Open() || g_ui.listPopup.Open() || g_ui.palettePopup.Open();
}


// ---------------------------------------------------------------------------
//  palette colour table and installed fonts (pure data helpers)
// ---------------------------------------------------------------------------
COLORREF HsvToColor(double h, double s, double v)
{
    h = fmod(h, 360.0);
    if (h < 0) h += 360.0;
    double c = v * s;
    double x = c * (1.0 - fabs(fmod(h / 60.0, 2.0) - 1.0));
    double m = v - c;
    double r = 0, g = 0, b = 0;
    int seg = (int)(h / 60.0) % 6;
    if (seg == 0) { r = c; g = x; }
    else if (seg == 1) { r = x; g = c; }
    else if (seg == 2) { g = c; b = x; }
    else if (seg == 3) { g = x; b = c; }
    else if (seg == 4) { r = x; b = c; }
    else { r = c; b = x; }
    return RGB((int)((r + m) * 255.0 + 0.5), (int)((g + m) * 255.0 + 0.5),
               (int)((b + m) * 255.0 + 0.5));
}


const std::vector<COLORREF>& PaletteColors()
{
    static const std::vector<COLORREF> colors = [] {
        std::vector<COLORREF> v;
        v.reserve(72);
        const int grays[12] = { 0, 23, 46, 70, 93, 117, 140, 164, 187, 211, 234, 255 };
        for (int g : grays) v.push_back(RGB(g, g, g));
        const double sat[5] = { 0.72, 0.80, 0.95, 0.45, 0.18 };
        const double val[5] = { 0.45, 0.82, 0.98, 1.00, 1.00 };
        for (int row = 0; row < 5; ++row)
            for (int i = 0; i < 12; ++i)
                v.push_back(HsvToColor(i * 30.0, sat[row], val[row]));
        return v;
    }();
    return colors;
}


int CALLBACK FontEnumProc(const LOGFONTW* lf, const TEXTMETRICW*, DWORD, LPARAM lp)
{
    auto* set = (std::unordered_map<std::wstring, int>*)lp;
    std::wstring name = lf->lfFaceName;
    if (!name.empty() && name[0] != L'@')
        (*set)[name] = 1;
    return 1;
}


const std::vector<std::wstring>& SystemFonts()
{
    if (!g_systemFonts.empty()) return g_systemFonts;
    std::unordered_map<std::wstring, int> seen;
    HDC dc = GetDC(nullptr);
    LOGFONTW lf = {};
    lf.lfCharSet = DEFAULT_CHARSET;
    EnumFontFamiliesExW(dc, &lf, FontEnumProc, (LPARAM)&seen, 0);
    ReleaseDC(nullptr, dc);
    g_systemFonts.reserve(seen.size());
    for (auto& kv : seen) g_systemFonts.push_back(kv.first);
    std::sort(g_systemFonts.begin(), g_systemFonts.end(),
              [](const std::wstring& a, const std::wstring& b) { return _wcsicmp(a.c_str(), b.c_str()) < 0; });
    if (g_systemFonts.empty()) g_systemFonts.push_back(L"微软雅黑");
    return g_systemFonts;
}
