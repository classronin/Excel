// ============================================================================
//  p8a_commands.cpp
//  Responsibility: commands & dialogs
//  Commands, modal dialogs, import, insert, sum, properties.
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
// ---- src\p8a_commands.inc ----
// ============================================================================
//  Excel - part 8a: commands, dialogs, import, insert, sum, properties
// ============================================================================

// ---------------------------------------------------------------------------
//  modal plumbing
// ---------------------------------------------------------------------------


// Restore the owner *before* the modal disappears, so Windows never has to
// pick a temporary foreground window (which looked like the app flashing to
// another window and back). If the user switched to another app while the
// modal was open, leave that app alone instead of stealing focus.
LRESULT CALLBACK ModalFocusProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR ref)
{
    ModalFocusState* st = (ModalFocusState*)ref;
    if (msg == WM_DESTROY && st && st->restore) {
        HWND owner = st->owner;
        if (owner && IsWindow(owner)) {
            EnableWindow(owner, TRUE);
            st->enabled = true;
            // `foreground` was sampled before the closing message was
            // dispatched; after WM_DESTROY the modal may already have lost
            // foreground to a temporary window. Only SetForegroundWindow when
            // this app was actually foreground, otherwise a background close
            // must not steal focus from the user's current window.
            bool appWasForeground = st->foreground || GetForegroundWindow() == h;
            if (appWasForeground) {
                SetForegroundWindow(owner);
                SetActiveWindow(owner);
                SetFocus(owner);
            }
        }
    } else if (msg == WM_NCDESTROY) {
        RemoveWindowSubclass(h, ModalFocusProc, id);
    }
    return DefSubclassProc(h, msg, wp, lp);
}


void RunModalWindow(HWND h, HWND owner )
{
    if (!owner) owner = g_hwnd;
    BOOL ownerWasEnabled = IsWindowEnabled(owner);
    if (ownerWasEnabled) EnableWindow(owner, FALSE);

    ModalFocusState focus;
    focus.owner = owner;
    focus.restore = ownerWasEnabled != FALSE;
    bool subclassed = false;
    if (focus.restore && IsWindow(owner))
        subclassed = SetWindowSubclass(h, ModalFocusProc, 0x4D4F444C, (DWORD_PTR)&focus) != FALSE;
    if (ownerWasEnabled) SetPropW(h, kModalOwnerProp, (HANDLE)1);

    ShowWindow(h, SW_SHOW);
    BringWindowToTop(h);
    SetForegroundWindow(h);
    SetActiveWindow(h);
    if (HWND child = GetWindow(h, GW_CHILD)) SetFocus(child);
    else SetFocus(h);
    focus.foreground = (GetForegroundWindow() == h);
    MSG msg;
    while (IsWindow(h) && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        focus.foreground = (GetForegroundWindow() == h);
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
        g_actions.Drain();      // same one-way rule inside a modal frame host
    }
    if (subclassed && IsWindow(h))
        RemoveWindowSubclass(h, ModalFocusProc, 0x4D4F444C);
    if (ownerWasEnabled && IsWindow(owner)) {
        if (!focus.enabled) EnableWindow(owner, TRUE);
        if (focus.foreground || GetForegroundWindow() == owner) {
            SetForegroundWindow(owner);
            SetActiveWindow(owner);
            SetFocus(owner);
        }
        InvalidateRect(owner, nullptr, FALSE);
    }
}


// Close a modal window from inside its own procedure. Restore the owner while
// the modal is still the foreground window, then destroy it. Doing this before
// DestroyWindow prevents Windows from activating some other window for a
// moment (the visible "switch away and back" flash).
void CloseModalWindow(HWND h)
{
    if (!IsWindow(h)) return;
    if (GetPropW(h, kModalOwnerProp)) {
        HWND owner = GetWindow(h, GW_OWNER);
        if (owner && IsWindow(owner) &&
            (GetForegroundWindow() == h || GetActiveWindow() == h)) {
            EnableWindow(owner, TRUE);
            SetForegroundWindow(owner);
            SetActiveWindow(owner);
            SetFocus(owner);
        }
    }
    DestroyWindow(h);
}


// ---------------------------------------------------------------------------
//  Modal dialogs are frame hosts of their own. Each declares a complete
//  PaintSpec at the start of its WM_PAINT (the logic-layer commit) and then
//  executes it with the ONE renderer. No WndProc draws GDI directly.
// ---------------------------------------------------------------------------
static void EmitText(PaintSpec& s, const std::wstring& text, const RECT& rc,
                     COLORREF color, HFONT font, UINT flags)
{
    auto& p = s.Emit(PaintOp::Text, rc, color);
    p.text = text;
    p.font = font;
    p.flags = flags;
}

static void EmitHLine(PaintSpec& s, int x1, int x2, int y, COLORREF c, int w = 1)
{ s.Emit(PaintOp::HLine, RECT{ x1, y, x2, y }, c).w = w; }

static void EmitVLine(PaintSpec& s, int x, int y1, int y2, COLORREF c, int w = 1)
{ s.Emit(PaintOp::VLine, RECT{ x, y1, x, y2 }, c).w = w; }

static void EmitClose(PaintSpec& s, int cx, int cy, int size, COLORREF c)
{
    auto& p = s.Emit(PaintOp::CloseX, RECT{}, c);
    p.p1 = cx; p.p2 = cy; p.w = size;
}

static void EmitCheck(PaintSpec& s, int cx, int cy, COLORREF c, int mark = 0)
{
    auto& p = s.Emit(PaintOp::CheckMark, RECT{}, c);
    p.p1 = cx; p.p2 = cy; p.w = mark > 0 ? mark : DpiScale(6);
}

static void EmitTitleBar(PaintSpec& s, const std::wstring& title, int w, int h)
{
    s.Emit(PaintOp::FillRect, RECT{ 0, 0, w, h }, g_th.panel);
    s.Emit(PaintOp::FrameRect, RECT{ 0, 0, w, h }, g_th.borderStrong).w = 1;
    s.Emit(PaintOp::FillRect, RECT{ 0, 0, w, DpiScale(34) }, g_th.panel2);
    EmitHLine(s, 0, w, DpiScale(34), g_th.border);
    EmitText(s, title, RECT{ DpiScale(16), 0, w - DpiScale(40), DpiScale(34) },
             g_th.text, UiFont(10, true), DT_SINGLELINE | DT_VCENTER | DT_LEFT);
}

static void EmitButton(PaintSpec& s, RECT r, const std::wstring& text, bool primary, bool hot,
                       HFONT font = nullptr)
{
    COLORREF fill = primary ? (hot ? Blend(g_th.accent, g_th.panel, 82) : g_th.accent)
                            : (hot ? g_th.hover : g_th.panel2);
    COLORREF fg = primary ? (g_dark ? RGB(8, 20, 18) : RGB(255, 255, 255)) : g_th.text;
    s.Emit(PaintOp::FillRect, r, fill);
    s.Emit(PaintOp::FrameRect, r, primary ? g_th.accent : g_th.border).w = 1;
    EmitText(s, text, r, fg, font ? font : UiFont(9), DT_SINGLELINE | DT_CENTER | DT_VCENTER);
}


static void BuildMsgSpec(MsgState& s, HDC)
{
    PaintSpec& p = s.spec;
    p.Clear();
    int w = s.width, hh = s.height;
    EmitTitleBar(p, s.title, w, hh);
    RECT close = MsgCloseRect(&s);
    EmitClose(p, (close.left + close.right) / 2, (close.top + close.bottom) / 2,
              DpiScale(4), g_th.dim);

    int icon = DpiScale(20);
    RECT ic = { DpiScale(24), s.textRect.top + DpiScale(1),
                DpiScale(24) + icon, s.textRect.top + 1 + icon };
    bool danger = (s.mode == MSGB_SAVE);
    COLORREF accent = danger ? g_th.danger : g_th.accent;
    p.Emit(PaintOp::FillRect, ic, danger ? Blend(g_th.danger, g_th.panel, 22) : g_th.accentSoft);
    p.Emit(PaintOp::FrameRect, ic, accent).w = 1;
    EmitVLine(p, (ic.left + ic.right) / 2, ic.top + DpiScale(4), ic.top + DpiScale(11),
              accent, DpiScale(2));
    EmitHLine(p, (ic.left + ic.right) / 2 - 1, (ic.left + ic.right) / 2 + 1,
              ic.top + DpiScale(14), accent, DpiScale(2));
    EmitText(p, s.text, s.textRect, g_th.text, UiFont(10),
             DT_WORDBREAK | DT_LEFT | DT_NOPREFIX);
    for (auto& b : s.buttons)
        EmitButton(p, b.rect, b.label, b.primary, s.hover == b.result);
}


static void BuildPromptSpec(PromptState& s, HDC)
{
    PaintSpec& p = s.spec;
    p.Clear();
    int w = s.width, hh = s.height;
    EmitTitleBar(p, s.title, w, hh);
    RECT close = PromptCloseRect(&s);
    EmitClose(p, (close.left + close.right) / 2, (close.top + close.bottom) / 2,
              DpiScale(4), g_th.dim);
    EmitText(p, s.hint, RECT{ DpiScale(20), DpiScale(48), w - DpiScale(20), DpiScale(74) },
             g_th.dim, UiFont(9), DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    p.Emit(PaintOp::FrameRect,
           RECT{ DpiScale(19), DpiScale(85), w - DpiScale(19), DpiScale(119) },
           g_th.border).w = 1;
    EmitButton(p, PromptOkRect(&s), L"确定", true, s.hoverBtn == 1);
    EmitButton(p, PromptCancelRect(&s), L"取消", false, s.hoverBtn == 2);
}


static void BuildPropSpec(PropState& s, HDC)
{
    PaintSpec& p = s.spec;
    p.Clear();
    int w = s.width, hh = s.height;
    EmitTitleBar(p, L"属性", w, hh);
    RECT close = PropCloseRect(&s);
    EmitClose(p, (close.left + close.right) / 2, (close.top + close.bottom) / 2,
              DpiScale(4), g_th.dim);

    const wchar_t* kindName = L"文件";
    if (s.firstKind == CK_FOLDER) kindName = L"文件夹";
    else if (s.firstKind == CK_IMAGE) kindName = L"图像";
    else if (s.firstKind == CK_URL) kindName = L"网址";
    EmitText(p, Format(L"类型   %s", kindName),
             RECT{ DpiScale(24), DpiScale(48), w - DpiScale(24), DpiScale(70) },
             g_th.dim, UiFont(9), DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    EmitText(p, s.firstPath,
             RECT{ DpiScale(24), DpiScale(74), w - DpiScale(24), DpiScale(144) },
             g_th.text, UiFont(9), DT_WORDBREAK | DT_LEFT | DT_END_ELLIPSIS);
    if (s.cells.size() > 1)
        EmitText(p, Format(L"共 %d 项，上方为第一项路径", (int)s.cells.size()),
                 RECT{ DpiScale(24), DpiScale(146), w - DpiScale(24), DpiScale(168) },
                 g_th.faint, UiFont(8), DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    EmitText(p, L"复制到 exe 同级的 media 文件夹",
             RECT{ DpiScale(24), DpiScale(176), w - DpiScale(24), DpiScale(198) },
             g_th.accent, UiFont(9), DT_SINGLELINE | DT_VCENTER | DT_LEFT);

    RECT cb = PropCheckRect(&s);
    if (g_autoCopy) {
        p.Emit(PaintOp::FillRect, cb, g_th.accent);
        EmitCheck(p, (cb.left + cb.right) / 2, (cb.top + cb.bottom) / 2,
                  g_dark ? RGB(8, 20, 18) : RGB(255, 255, 255));
    } else {
        p.Emit(PaintOp::FillRect, cb, g_th.panel2);
        p.Emit(PaintOp::FrameRect, cb,
               s.hoverBtn == 3 ? g_th.borderStrong : g_th.border).w = 1;
    }
    EmitText(p, L"自动复制",
             RECT{ cb.right + DpiScale(10), cb.top - DpiScale(3),
                   cb.right + DpiScale(110), cb.bottom + DpiScale(3) },
             g_th.text, UiFont(9), DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    EmitText(p, L"导入或拖入文件时自动复制（关闭 exe 后保留）",
             RECT{ cb.right + DpiScale(110), cb.top - DpiScale(3),
                   w - DpiScale(24), cb.bottom + DpiScale(3) },
             g_th.faint, UiFont(8),
             DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);
    if (!s.message.empty())
        EmitText(p, s.message,
                 RECT{ DpiScale(24), DpiScale(238), w - DpiScale(24), DpiScale(260) },
                 g_th.danger, UiFont(8),
                 DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);
    EmitButton(p, PropCopyRect(&s), L"复制", true, s.hoverBtn == 1);
    EmitButton(p, PropNoCopyRect(&s), L"不复制", false, s.hoverBtn == 2);
}


// ---- the settings dialog's own scale ---------------------------------------
// This dialog contains the control that changes the app's zoom, and it is a
// borderless popup - the only thing able to paint itself. Changing its own size
// while it is on screen means there is a moment when its box and its pixels
// disagree, and the eye catches that frame as a flash or a tear. So the scale is
// captured when the window OPENS and never read from the live zoom again: box,
// every offset in the spec, and every font come from the same frozen pair.
// Reopening it at another zoom simply captures the new pair.
static int SettingsPx(const SettingsState& s, int v)
{
    return MulDiv(v, s.snapDpi * s.snapZoom, 96 * 100);
}
static int SettingsPx(const SettingsState* s, int v) { return SettingsPx(*s, v); }
static HFONT SettingsFont(const SettingsState& s, int pt, bool bold = false)
{
    return UiFontScaled(pt, bold, s.snapDpi, s.snapZoom);
}
static HFONT SettingsMonoFont(const SettingsState& s, int pt, bool bold = false)
{
    return MonoFontScaled(pt, bold, s.snapDpi, s.snapZoom);
}


static void BuildSettingsSpec(SettingsState& s, HDC)
{
    PaintSpec& p = s.spec;
    p.Clear();
    int w = s.width, hh = s.height;
    // Settings uses a 36px header rather than the shared 34px one.
    p.Emit(PaintOp::FillRect, RECT{ 0, 0, w, hh }, g_th.panel);
    p.Emit(PaintOp::FrameRect, RECT{ 0, 0, w, hh }, g_th.borderStrong).w = 1;
    p.Emit(PaintOp::FillRect, RECT{ 0, 0, w, SettingsPx(s, 36) }, g_th.panel2);
    EmitHLine(p, 0, w, SettingsPx(s, 36), g_th.border);
    EmitText(p, L"设置", RECT{ SettingsPx(s, 16), 0, w - SettingsPx(s, 40), SettingsPx(s, 36) },
             g_th.text, SettingsFont(s, 10, true), DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    EmitClose(p, w - SettingsPx(s, 17), SettingsPx(s, 18), SettingsPx(s, 4), g_th.dim);

    EmitText(p, L"主题", RECT{ SettingsPx(s, 24), SettingsPx(s, 52), SettingsPx(s, 140), SettingsPx(s, 82) },
             g_th.text, SettingsFont(s, 9), DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    for (int i = 0; i < 2; ++i) {
        bool on = (i == 0 && !s.dark) || (i == 1 && s.dark);
        RECT r = SetThemeRect(&s, i);
        bool hot = (s.hover == (i == 0 ? 1 : 2));
        p.Emit(PaintOp::FillRect, r, on ? g_th.accentSoft : (hot ? g_th.hover : g_th.panel2));
        p.Emit(PaintOp::FrameRect, r, on ? g_th.accent : g_th.border).w = 1;
        RECT sq = { r.left + SettingsPx(s, 10), r.top + SettingsPx(s, 9),
                    r.left + SettingsPx(s, 22), r.top + SettingsPx(s, 21) };
        if (on) p.Emit(PaintOp::FillRect, sq, g_th.accent);
        p.Emit(PaintOp::FrameRect, sq, on ? g_th.accent : g_th.dim).w = 1;
        EmitText(p, i == 0 ? L"浅色" : L"深色",
                 RECT{ sq.right + SettingsPx(s, 8), r.top, r.right - SettingsPx(s, 8), r.bottom },
                 on ? g_th.accent : g_th.text, SettingsFont(s, 9),
                 DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    }
    EmitHLine(p, SettingsPx(s, 24), w - SettingsPx(s, 24), SettingsPx(s, 88), g_th.border);

    auto emitZoomRow = [&](int top, const wchar_t* label, RECT m, RECT plus,
                           int hotMinus, int hotPlus, int value) {
        EmitText(p, label, RECT{ SettingsPx(s, 24), top, SettingsPx(s, 140), top + SettingsPx(s, 30) },
                 g_th.text, SettingsFont(s, 9), DT_SINGLELINE | DT_VCENTER | DT_LEFT);
        for (int i = 0; i < 2; ++i) {
            RECT r = i ? plus : m;
            bool hot = (s.hover == (i ? hotPlus : hotMinus));
            p.Emit(PaintOp::FillRect, r, hot ? g_th.hover : g_th.panel2);
            p.Emit(PaintOp::FrameRect, r, hot ? g_th.borderStrong : g_th.border).w = 1;
            EmitText(p, i ? L"+" : L"-", r, g_th.text, SettingsFont(s, 12, true),
                     DT_SINGLELINE | DT_CENTER | DT_VCENTER);
        }
        EmitText(p, Format(L"%d%%", value),
                 RECT{ SettingsPx(s, 188), top, SettingsPx(s, 280), top + SettingsPx(s, 30) },
                 g_th.accent, SettingsMonoFont(s, 11, true), DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    };
    emitZoomRow(SettingsPx(s, 100), L"界面缩放", SetUiMinus(&s), SetUiPlus(&s), 3, 4, s.uiZoom);
    emitZoomRow(SettingsPx(s, 142), L"表格缩放", SetGridMinus(&s), SetGridPlus(&s), 5, 6, s.gridZoom);
    EmitHLine(p, SettingsPx(s, 24), w - SettingsPx(s, 24), SettingsPx(s, 180), g_th.border);

    EmitText(p, L"行列数：默认窗口外多 20 行/列",
             RECT{ SettingsPx(s, 24), SettingsPx(s, 192), w - SettingsPx(s, 24), SettingsPx(s, 220) },
             g_th.text, SettingsFont(s, 9), DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    EmitText(p, L"拖右侧/底部滚动条到尽头继续拖，增加行/列；不是拖选区",
             RECT{ SettingsPx(s, 24), SettingsPx(s, 222), w - SettingsPx(s, 24), SettingsPx(s, 250) },
             g_th.faint, SettingsFont(s, 8),
             DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);
    EmitHLine(p, SettingsPx(s, 24), w - SettingsPx(s, 24), SettingsPx(s, 254), g_th.border);

    RECT cb = SetAutoRect(&s);
    if (s.autoCopy) {
        p.Emit(PaintOp::FillRect, cb, g_th.accent);
        EmitCheck(p, (cb.left + cb.right) / 2, (cb.top + cb.bottom) / 2,
                  g_dark ? RGB(8, 20, 18) : RGB(255, 255, 255), SettingsPx(s, 6));
    } else {
        p.Emit(PaintOp::FillRect, cb, g_th.panel2);
        p.Emit(PaintOp::FrameRect, cb,
               s.hover == 7 ? g_th.borderStrong : g_th.border).w = 1;
    }
    EmitText(p, L"自动复制",
             RECT{ cb.right + SettingsPx(s, 10), cb.top - SettingsPx(s, 3),
                   cb.right + SettingsPx(s, 100), cb.bottom + SettingsPx(s, 3) },
             g_th.text, SettingsFont(s, 9), DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    EmitText(p, L"导入或拖入文件时复制到 media/",
             RECT{ cb.right + SettingsPx(s, 100), cb.top - SettingsPx(s, 3),
                   w - SettingsPx(s, 24), cb.bottom + SettingsPx(s, 3) },
             g_th.faint, SettingsFont(s, 8),
             DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);

    RECT propRect = SetPropRect(&s);
    bool propsOk = false;
    if (Doc* d = ActiveDoc()) propsOk = CanShowProps(*d);
    bool propHot = (s.hover == 8);
    p.Emit(PaintOp::FillRect, propRect, propHot && propsOk ? g_th.hover : g_th.panel2);
    p.Emit(PaintOp::FrameRect, propRect,
           propsOk ? (propHot ? g_th.accent : g_th.borderStrong) : g_th.border).w = 1;
    EmitText(p, L"属性…", propRect, propsOk ? g_th.text : g_th.faint, SettingsFont(s, 9),
             DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    EmitText(p, L"仅对文件 / 文件夹 / 图像 / 网址启用",
             RECT{ propRect.right + SettingsPx(s, 12), propRect.top,
                   w - SettingsPx(s, 24), propRect.bottom },
             g_th.faint, SettingsFont(s, 8),
             DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);

    if (!s.message.empty())
        EmitText(p, s.message,
                 RECT{ SettingsPx(s, 24), s.height - SettingsPx(s, 92),
                       w - SettingsPx(s, 160), s.height - SettingsPx(s, 64) },
                 g_th.danger, SettingsFont(s, 8),
                 DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);
    EmitButton(p, SetApplyRect(&s), L"应用", false, s.hover == 11, SettingsFont(s, 9));
    EmitButton(p, SetCloseRect(&s), L"关闭", true, s.hover == 9, SettingsFont(s, 9));
}


// ---------------------------------------------------------------------------
//  themed message box (替代系统 MessageBox，避免浅色弹框破坏深色主题)
// ---------------------------------------------------------------------------



void MsgLayout(MsgState* s)
{
    int pad = DpiScale(24);
    s->width = DpiScale(560);
    int icon = DpiScale(20);
    int textX = pad + icon + DpiScale(14);
    int textW = s->width - textX - pad;
    HDC dc = GetDC(g_hwnd);
    HGDIOBJ old = SelectObject(dc, UiFont(10));
    RECT calc = { 0, 0, textW, 0 };
    DrawTextW(dc, s->text.c_str(), (int)s->text.size(), &calc,
              DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    SelectObject(dc, old);
    ReleaseDC(g_hwnd, dc);
    int textH = std::max(DpiScale(40), (int)(calc.bottom - calc.top));
    int top = DpiScale(34) + DpiScale(20);
    s->textRect = { textX, top, textX + textW, top + textH };

    int btnW = DpiScale(100), btnH = DpiScale(32), gap = DpiScale(8);
    int bottom = s->textRect.bottom + DpiScale(22) + btnH + DpiScale(20);
    s->height = std::max(DpiScale(190), bottom);
    int y = s->height - DpiScale(20) - btnH;
    int x = s->width - pad;

    s->buttons.clear();
    auto add = [&](const wchar_t* label, int result, bool primary) {
        x -= btnW;
        MsgButton b;
        b.rect = { x, y, x + btnW, y + btnH };
        b.label = label;
        b.result = result;
        b.primary = primary;
        s->buttons.push_back(b);
        x -= gap;
    };
    if (s->mode == MSGB_OK) {
        add(L"确定", IDOK, true);
    } else if (s->mode == MSGB_SAVE) {
        add(L"取消", IDCANCEL, false);
        add(L"不保存", IDNO, false);
        add(L"保存", IDYES, true);
    } else {
        add(L"否", IDNO, false);
        add(L"是", IDYES, true);
    }
}


RECT MsgCloseRect(MsgState* s)
{
    return { s->width - DpiScale(34), 0, s->width, DpiScale(34) };
}


LRESULT CALLBACK MsgProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    MsgState* s = (MsgState*)GetWindowLongPtrW(h, GWLP_USERDATA);
    switch (msg) {
    case WM_NCCREATE: {
        CREATESTRUCTW* cs = (CREATESTRUCTW*)lp;
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return TRUE;
    }
    case WM_CREATE:
        s = (MsgState*)GetWindowLongPtrW(h, GWLP_USERDATA);
        if (s) s->hwnd = h;
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        if (s) {
            BuildMsgSpec(*s, dc);                       // commit
            RECT c = {};
            GetClientRect(h, &c);
            PresentSpec(dc, s->surface, c, s->spec);    // pure render: one blit
        }
        EndPaint(h, &ps);
        return 0;
    }
    case WM_MOUSEMOVE:
        if (s) {
            POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            int hover = 0;
            for (auto& b : s->buttons) if (PtInRect(&b.rect, pt)) hover = b.result;
            if (hover != s->hover) {
                s->hover = hover;
                InvalidateRect(h, nullptr, FALSE);
            }
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, h, 0 };
            TrackMouseEvent(&tme);
        }
        return 0;
    case WM_MOUSELEAVE:
        if (s && s->hover) { s->hover = 0; InvalidateRect(h, nullptr, FALSE); }
        return 0;
    case WM_LBUTTONDOWN:
        if (!s) return 0;
        {
            POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            RECT close = MsgCloseRect(s);
            if (PtInRect(&close, pt)) {
                s->result = (s->mode == MSGB_SAVE) ? IDCANCEL : (s->mode == MSGB_YESNO ? IDNO : IDOK);
                CloseModalWindow(h);
                return 0;
            }
            for (auto& b : s->buttons) {
                if (PtInRect(&b.rect, pt)) {
                    s->result = b.result;
                    CloseModalWindow(h);
                    return 0;
                }
            }
        }
        return 0;
    case WM_KEYDOWN:
        if (!s) return 0;
        if (wp == VK_ESCAPE) {
            s->result = (s->mode == MSGB_SAVE) ? IDCANCEL : (s->mode == MSGB_YESNO ? IDNO : IDOK);
            CloseModalWindow(h);
            return 0;
        }
        if (wp == VK_RETURN) {
            s->result = (s->mode == MSGB_OK) ? IDOK : IDYES;
            CloseModalWindow(h);
            return 0;
        }
        return 0;
    case WM_CLOSE:
        if (s) {
            s->result = (s->mode == MSGB_SAVE) ? IDCANCEL : (s->mode == MSGB_YESNO ? IDNO : IDOK);
        }
        CloseModalWindow(h);
        return 0;
    case WM_DESTROY:
        if (s) s->hwnd = nullptr;
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}


void RegisterMsgClass()
{
    static bool done = false;
    if (done) return;
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.style = CS_DROPSHADOW;
    wc.lpfnWndProc = MsgProc;
    wc.hInstance = g_inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = L"ExcelMsg";
    RegisterClassExW(&wc);
    done = true;
}


int ShowMessageBox(HWND owner, const std::wstring& text, const std::wstring& title, int mode)
{
    if (!owner) owner = GetActiveWindow();
    if (!owner) owner = g_hwnd;
    MsgState st;
    st.mode = mode;
    st.title = title.empty() ? L"提示" : title;
    st.text = text;
    st.result = (mode == MSGB_SAVE || mode == MSGB_YESNO) ? IDCANCEL : IDOK;
    MsgLayout(&st);
    RegisterMsgClass();
    RECT o;
    GetWindowRect(owner, &o);
    int x = (o.left + o.right) / 2 - st.width / 2;
    int y = (o.top + o.bottom) / 2 - st.height / 2;
    HWND hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, L"ExcelMsg", L"", WS_POPUP | WS_CLIPCHILDREN,
                                x, y, st.width, st.height, owner, nullptr, g_inst, &st);
    if (!hwnd) return st.result;
    RunModalWindow(hwnd, owner);
    return st.result;
}

// ---------------------------------------------------------------------------
//  prompt window (网址输入)
// ---------------------------------------------------------------------------


RECT PromptOkRect(PromptState* s)
{
    int w = DpiScale(92), h = DpiScale(30);
    return { s->width - DpiScale(20) - w * 2 - DpiScale(8), s->height - DpiScale(20) - h,
             s->width - DpiScale(20) - w - DpiScale(8), s->height - DpiScale(20) };
}

RECT PromptCancelRect(PromptState* s)
{
    int w = DpiScale(92), h = DpiScale(30);
    return { s->width - DpiScale(20) - w, s->height - DpiScale(20) - h,
             s->width - DpiScale(20), s->height - DpiScale(20) };
}

RECT PromptCloseRect(PromptState* s)
{
    return { s->width - DpiScale(34), 0, s->width, DpiScale(34) };
}


LRESULT CALLBACK PromptEditProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR)
{
    if (msg == WM_KEYDOWN) {
        if (wp == VK_RETURN) { PostMessageW(GetParent(h), WM_APP + 1, 0, 0); return 0; }
        if (wp == VK_ESCAPE) { PostMessageW(GetParent(h), WM_APP + 2, 0, 0); return 0; }
    }
    return DefSubclassProc(h, msg, wp, lp);
}


LRESULT CALLBACK PromptProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    PromptState* s = (PromptState*)GetWindowLongPtrW(h, GWLP_USERDATA);
    switch (msg) {
    case WM_NCCREATE: {
        CREATESTRUCTW* cs = (CREATESTRUCTW*)lp;
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return TRUE;
    }
    case WM_CREATE: {
        s = (PromptState*)GetWindowLongPtrW(h, GWLP_USERDATA);
        if (!s) return -1;
        s->hwnd = h;
        s->edit = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                  DpiScale(20), DpiScale(86), s->width - DpiScale(40), DpiScale(32),
                                  h, (HMENU)2001, g_inst, nullptr);
        SendMessageW(s->edit, WM_SETFONT, (WPARAM)UiFont(11), TRUE);
        SendMessageW(s->edit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(DpiScale(6), DpiScale(6)));
        SetWindowSubclass(s->edit, PromptEditProc, 1, 0);
        SetWindowTextW(s->edit, s->result.c_str());
        SendMessageW(s->edit, EM_SETSEL, 0, -1);
        SetFocus(s->edit);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        if (s) {
            BuildPromptSpec(*s, dc);                    // commit
            RECT c = {};
            GetClientRect(h, &c);
            PresentSpec(dc, s->surface, c, s->spec);    // pure render: one blit
        }
        EndPaint(h, &ps);
        return 0;
    }
    case WM_MOUSEMOVE:
        if (s) {
            POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            int hb = 0;
            if (PtInRect(&PromptOkRect(s), pt)) hb = 1;
            else if (PtInRect(&PromptCancelRect(s), pt)) hb = 2;
            if (hb != s->hoverBtn) { s->hoverBtn = hb; InvalidateRect(h, nullptr, FALSE); }
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, h, 0 };
            TrackMouseEvent(&tme);
        }
        return 0;
    case WM_MOUSELEAVE:
        if (s && s->hoverBtn) { s->hoverBtn = 0; InvalidateRect(h, nullptr, FALSE); }
        return 0;

    case WM_LBUTTONDOWN: {
        if (!s) return 0;
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        RECT close = PromptCloseRect(s);
        if (PtInRect(&close, pt)) { CloseModalWindow(h); return 0; }
        if (pt.y < DpiScale(34)) {
            ReleaseCapture();
            SendMessageW(h, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            return 0;
        }
        if (PtInRect(&PromptOkRect(s), pt)) {
            wchar_t buf[4096];
            GetWindowTextW(s->edit, buf, 4096);
            s->result = buf;
            s->ok = true;
            CloseModalWindow(h);
            return 0;
        }
        if (PtInRect(&PromptCancelRect(s), pt)) { CloseModalWindow(h); return 0; }
        SetFocus(s->edit);
        return 0;
    }
    case WM_APP + 1: {
        if (!s) return 0;
        wchar_t buf[4096];
        GetWindowTextW(s->edit, buf, 4096);
        s->result = buf;
        s->ok = true;
        CloseModalWindow(h);
        return 0;
    }
    case WM_APP + 2:
        CloseModalWindow(h);
        return 0;
    case WM_CLOSE:
        CloseModalWindow(h);
        return 0;
    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)wp;
        SetTextColor(dc, g_th.text);
        SetBkColor(dc, g_th.panel2);
        return (LRESULT)Brush(g_th.panel2);
    }
    case WM_DESTROY:
        if (s) { s->hwnd = nullptr; s->edit = nullptr; }
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}


void RegisterPromptClass()
{
    static bool done = false;
    if (done) return;
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.style = CS_DROPSHADOW;
    wc.lpfnWndProc = PromptProc;
    wc.hInstance = g_inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = L"ExcelPrompt";
    RegisterClassExW(&wc);
    done = true;
}


std::wstring PromptText(const std::wstring& title, const std::wstring& hint, const std::wstring& initial)
{
    RegisterPromptClass();
    PromptState ps;
    ps.title = title;
    ps.hint = hint;
    ps.result = initial;
    ps.width = DpiScale(540);
    ps.height = DpiScale(196);
    RECT o;
    GetWindowRect(g_hwnd, &o);
    int x = (o.left + o.right) / 2 - ps.width / 2;
    int y = (o.top + o.bottom) / 2 - ps.height / 2;
    HWND hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, L"ExcelPrompt", L"", WS_POPUP | WS_CLIPCHILDREN,
                                x, y, ps.width, ps.height, g_hwnd, nullptr, g_inst, &ps);
    if (!hwnd) return L"";
    RunModalWindow(hwnd);
    return ps.ok ? ps.result : L"";
}


// ---------------------------------------------------------------------------
//  about window (关于)
// ---------------------------------------------------------------------------

static RECT AboutCloseRect(AboutState* s)
{
    return { s->width - DpiScale(34), 0, s->width, DpiScale(34) };
}

static RECT AboutTextRect(AboutState* s)
{
    int x = (s->width - DpiScale(200)) / 2;
    int top = DpiScale(34) + DpiScale(18);
    return { x, top, x + DpiScale(200), top + DpiScale(26) };
}

static RECT AboutImgRect(AboutState* s)
{
    int x = (s->width - DpiScale(200)) / 2;
    int top = AboutTextRect(s).bottom + DpiScale(12);
    return { x, top, x + DpiScale(200), top + DpiScale(200) };
}

static RECT AboutOkRect(AboutState* s)
{
    int w = DpiScale(92), h = DpiScale(32);
    int x = (s->width - w) / 2;
    int top = AboutImgRect(s).bottom + DpiScale(18);
    return { x, top, x + w, top + h };
}

static void BuildAboutSpec(AboutState& s, HDC)
{
    PaintSpec& p = s.spec;
    p.Clear();
    int w = s.width, hh = s.height;
    EmitTitleBar(p, L"关于", w, hh);
    RECT close = AboutCloseRect(&s);
    EmitClose(p, (close.left + close.right) / 2, (close.top + close.bottom) / 2,
              DpiScale(4), g_th.dim);
    EmitText(p, L"请我喝杯咖啡", AboutTextRect(&s), g_th.text, UiFont(10),
             DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
    if (s.img) {
        auto& bp = p.Emit(PaintOp::Bitmap, AboutImgRect(&s), 0);
        bp.bmp = s.img;
        bp.p1 = s.imgW;
        bp.p2 = s.imgH;
    }
    EmitButton(p, AboutOkRect(&s), L"好", true, s.hoverBtn == 1);
}

LRESULT CALLBACK AboutProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    AboutState* s = (AboutState*)GetWindowLongPtrW(h, GWLP_USERDATA);
    switch (msg) {
    case WM_NCCREATE: {
        CREATESTRUCTW* cs = (CREATESTRUCTW*)lp;
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return TRUE;
    }
    case WM_CREATE:
        s = (AboutState*)GetWindowLongPtrW(h, GWLP_USERDATA);
        if (s) s->hwnd = h;
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        if (s) {
            BuildAboutSpec(*s, dc);
            RECT c = {};
            GetClientRect(h, &c);
            PresentSpec(dc, s->surface, c, s->spec);
        }
        EndPaint(h, &ps);
        return 0;
    }
    case WM_MOUSEMOVE:
        if (s) {
            POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            int hb = PtInRect(&AboutOkRect(s), pt) ? 1 : 0;
            if (hb != s->hoverBtn) { s->hoverBtn = hb; InvalidateRect(h, nullptr, FALSE); }
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, h, 0 };
            TrackMouseEvent(&tme);
        }
        return 0;
    case WM_MOUSELEAVE:
        if (s && s->hoverBtn) { s->hoverBtn = 0; InvalidateRect(h, nullptr, FALSE); }
        return 0;
    case WM_LBUTTONDOWN: {
        if (!s) return 0;
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        RECT close = AboutCloseRect(s);
        if (PtInRect(&close, pt)) { CloseModalWindow(h); return 0; }
        if (pt.y < DpiScale(34)) {
            ReleaseCapture();
            SendMessageW(h, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            return 0;
        }
        if (PtInRect(&AboutOkRect(s), pt)) { CloseModalWindow(h); return 0; }
        return 0;
    }
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE || wp == VK_RETURN) { CloseModalWindow(h); return 0; }
        return 0;
    case WM_CLOSE:
        CloseModalWindow(h);
        return 0;
    case WM_DESTROY:
        if (s) {
            if (s->img) { DeleteObject(s->img); s->img = nullptr; }
            s->hwnd = nullptr;
        }
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

void RegisterAboutClass()
{
    static bool done = false;
    if (done) return;
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.style = CS_DROPSHADOW;
    wc.lpfnWndProc = AboutProc;
    wc.hInstance = g_inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = L"ExcelAbout";
    RegisterClassExW(&wc);
    done = true;
}

void ShowAbout()
{
    RegisterAboutClass();
    AboutState st;
    st.img = LoadEmbeddedImage(IDR_ABOUT_IMAGE, st.imgW, st.imgH);
    st.width = DpiScale(200) + DpiScale(24) * 2;
    st.height = AboutOkRect(&st).bottom + DpiScale(20);
    RECT o;
    GetWindowRect(g_hwnd, &o);
    int x = (o.left + o.right) / 2 - st.width / 2;
    int y = (o.top + o.bottom) / 2 - st.height / 2;
    HWND hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, L"ExcelAbout", L"", WS_POPUP | WS_CLIPCHILDREN,
                                x, y, st.width, st.height, g_hwnd, nullptr, g_inst, &st);
    if (!hwnd) { if (st.img) DeleteObject(st.img); return; }
    RunModalWindow(hwnd);
}


// ---------------------------------------------------------------------------
//  编号 / 单位 dialogs
// ---------------------------------------------------------------------------

static const wchar_t* kLabelStyle[] = { L"全角数字", L"中文小写" };

// Selected cells in pick order: the active block first, then every Ctrl+clicked
// block in the order added; merges normalise to their anchor, duplicates collapse.
static void OrderedSelectionCells(Doc& d, std::vector<std::pair<int, int>>& out)
{
    int r1, c1, r2, c2;
    d.SelectionRect(r1, c1, r2, c2);
    std::vector<MergeRange> ranges;
    ranges.push_back({ r1, c1, r2, c2 });
    for (auto& e : d.selExtra) ranges.push_back(e);
    std::unordered_set<long long> seen;
    out.clear();
    for (auto& e : ranges) {
        for (int r = e.r1; r <= e.r2; ++r) {
            for (int c = e.c1; c <= e.c2; ++c) {
                int ar = r, ac = c;
                if (const MergeRange* m = d.MergeAt(r, c)) { ar = m->r1; ac = m->c1; }
                if (!seen.insert(CellKey(ar, ac)).second) continue;
                out.push_back({ ar, ac });
            }
        }
    }
}

static std::wstring FullWidthNum(int n)
{
    std::wstring s = std::to_wstring(n), out;
    for (wchar_t ch : s)
        out += (ch >= L'0' && ch <= L'9') ? (wchar_t)(0xFF10 + (ch - L'0')) : ch;
    return out;
}

static std::wstring CnBelow10000(int n)
{
    static const wchar_t* d[] = { L"零", L"一", L"二", L"三", L"四", L"五", L"六", L"七", L"八", L"九" };
    static const wchar_t* u[] = { L"", L"十", L"百", L"千" };
    static const int div[] = { 1, 10, 100, 1000 };
    std::wstring res;
    bool started = false, zero = false;
    for (int pos = 3; pos >= 0; --pos) {
        int dig = (n / div[pos]) % 10;
        if (dig == 0) { if (started) zero = true; continue; }
        if (zero) { res += L"零"; zero = false; }
        res += d[dig];
        res += u[pos];
        started = true;
    }
    if (res.rfind(L"一十", 0) == 0) res = res.substr(1);
    return res;
}

static std::wstring ChineseLowerNum(int n)
{
    if (n <= 0) return L"零";
    if (n < 10000) return CnBelow10000(n);
    if (n < 100000000) {
        std::wstring s = CnBelow10000(n / 10000) + L"万";
        int rem = n % 10000;
        if (rem) { if (rem < 1000) s += L"零"; s += CnBelow10000(rem); }
        return s;
    }
    return FullWidthNum(n);
}

static std::wstring Numeral(int style, int n)
{
    return style == 1 ? ChineseLowerNum(n) : FullWidthNum(n);
}

static RECT LabelCloseRect(LabelState* s)
{
    return { s->width - DpiScale(34), 0, s->width, DpiScale(34) };
}

static RECT LabelStyleRect(LabelState* s)
{
    return { DpiScale(20), DpiScale(50), DpiScale(150), DpiScale(80) };
}

static RECT LabelStyleItemRect(LabelState* s, int i)
{
    RECT dr = LabelStyleRect(s);
    int h = DpiScale(28);
    return { dr.left, dr.bottom + i * h, dr.right, dr.bottom + (i + 1) * h };
}

static RECT LabelEditRect(LabelState* s)
{
    int left = s->numbering ? DpiScale(160) : DpiScale(20);
    return { left, DpiScale(50), s->width - DpiScale(20), DpiScale(80) };
}

static RECT LabelOkRect(LabelState* s)
{
    int w = DpiScale(92), h = DpiScale(30);
    return { s->width - DpiScale(20) - w * 2 - DpiScale(8), s->height - DpiScale(20) - h,
             s->width - DpiScale(20) - w - DpiScale(8), s->height - DpiScale(20) };
}

static RECT LabelCancelRect(LabelState* s)
{
    int w = DpiScale(92), h = DpiScale(30);
    return { s->width - DpiScale(20) - w, s->height - DpiScale(20) - h,
             s->width - DpiScale(20), s->height - DpiScale(20) };
}

static void BuildLabelSpec(LabelState& s, HDC)
{
    PaintSpec& p = s.spec;
    p.Clear();
    int w = s.width, hh = s.height;
    EmitTitleBar(p, s.numbering ? L"编号" : L"单位", w, hh);
    RECT close = LabelCloseRect(&s);
    EmitClose(p, (close.left + close.right) / 2, (close.top + close.bottom) / 2,
              DpiScale(4), g_th.dim);

    if (s.numbering) {
        RECT dr = LabelStyleRect(&s);
        bool hot = (s.hoverStyle == 1) || s.styleOpen;
        p.Emit(PaintOp::FillRect, dr, hot ? g_th.hover : g_th.panel2);
        p.Emit(PaintOp::FrameRect, dr, hot ? g_th.borderStrong : g_th.border).w = 1;
        RECT tr = { dr.left + DpiScale(9), dr.top, dr.right - DpiScale(20), dr.bottom };
        EmitText(p, kLabelStyle[s.style], tr, g_th.text, UiFont(9),
                 DT_SINGLELINE | DT_VCENTER | DT_LEFT);
        auto& a = p.Emit(PaintOp::Triangle, RECT{}, g_th.dim);
        a.p1 = dr.right - DpiScale(10);
        a.p2 = (dr.top + dr.bottom) / 2;
        a.w = DpiScale(4);
    }

    RECT er = LabelEditRect(&s);
    p.Emit(PaintOp::FrameRect, er, g_th.border).w = 1;

    if (s.numbering && s.styleOpen) {
        for (int i = 0; i < 2; ++i) {
            RECT ir = LabelStyleItemRect(&s, i);
            bool sel = (i == s.style);
            bool hot = (s.hoverStyle == 2 + i);
            p.Emit(PaintOp::FillRect, ir, hot ? g_th.hover : g_th.panel2);
            p.Emit(PaintOp::FrameRect, ir, g_th.border).w = 1;
            RECT tr = { ir.left + DpiScale(9), ir.top, ir.right - DpiScale(9), ir.bottom };
            EmitText(p, kLabelStyle[i], tr, sel ? g_th.accent : g_th.text, UiFont(9),
                     DT_SINGLELINE | DT_VCENTER | DT_LEFT);
        }
    }

    EmitButton(p, LabelOkRect(&s), L"确定", true, s.hoverBtn == 1);
    EmitButton(p, LabelCancelRect(&s), L"取消", false, s.hoverBtn == 2);
}

static LRESULT CALLBACK LabelEditProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR)
{
    if (msg == WM_KEYDOWN) {
        if (wp == VK_RETURN) { PostMessageW(GetParent(h), WM_APP + 1, 0, 0); return 0; }
        if (wp == VK_ESCAPE) { PostMessageW(GetParent(h), WM_APP + 2, 0, 0); return 0; }
    }
    return DefSubclassProc(h, msg, wp, lp);
}

static void LabelApply(LabelState& s)
{
    wchar_t buf[4096];
    GetWindowTextW(s.edit, buf, 4096);
    s.suffix = buf;
    Doc* dp = ActiveDoc();
    if (!dp) return;
    Doc& d = *dp;
    std::vector<std::pair<int, int>> cells;
    OrderedSelectionCells(d, cells);
    if (cells.empty()) { SetStatus(L"请先选中表格"); return; }
    d.PushUndo();
    for (size_t i = 0; i < cells.size(); ++i) {
        Cell& c = d.Ensure(cells[i].first, cells[i].second);
        if (s.numbering) {
            c.text = Numeral(s.style, (int)i + 1) + s.suffix;
            c.formula.clear();
            c.path.clear();
            c.kind = CK_TEXT;
        } else {
            // 单位 is a live suffix, not baked text: a later edit of the value
            // keeps the unit to its right (empty + 元, then type 200 -> 200元).
            c.unit = s.suffix;
        }
    }
    d.Touch();
    SetStatus(Format(s.numbering ? L"已编号 %d 格" : L"已加单位 %d 格", (int)cells.size()));
}

LRESULT CALLBACK LabelProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    LabelState* s = (LabelState*)GetWindowLongPtrW(h, GWLP_USERDATA);
    switch (msg) {
    case WM_NCCREATE: {
        CREATESTRUCTW* cs = (CREATESTRUCTW*)lp;
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return TRUE;
    }
    case WM_CREATE: {
        s = (LabelState*)GetWindowLongPtrW(h, GWLP_USERDATA);
        if (!s) return -1;
        s->hwnd = h;
        RECT er = LabelEditRect(s);
        s->edit = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                  er.left + 1, er.top + 1, er.right - er.left - 2, er.bottom - er.top - 2,
                                  h, (HMENU)2001, g_inst, nullptr);
        SendMessageW(s->edit, WM_SETFONT, (WPARAM)UiFont(11), TRUE);
        SendMessageW(s->edit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(DpiScale(6), DpiScale(6)));
        SetWindowSubclass(s->edit, LabelEditProc, 1, 0);
        SetWindowTextW(s->edit, s->suffix.c_str());
        SendMessageW(s->edit, EM_SETSEL, 0, -1);
        SetFocus(s->edit);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        if (s) {
            BuildLabelSpec(*s, dc);
            RECT c = {};
            GetClientRect(h, &c);
            PresentSpec(dc, s->surface, c, s->spec);
        }
        EndPaint(h, &ps);
        return 0;
    }
    case WM_MOUSEMOVE:
        if (s) {
            POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            int hb = 0, hs = 0;
            if (PtInRect(&LabelOkRect(s), pt)) hb = 1;
            else if (PtInRect(&LabelCancelRect(s), pt)) hb = 2;
            if (s->numbering) {
                if (s->styleOpen) {
                    for (int i = 0; i < 2; ++i)
                        if (PtInRect(&LabelStyleItemRect(s, i), pt)) hs = 2 + i;
                }
                if (!hs && PtInRect(&LabelStyleRect(s), pt)) hs = 1;
            }
            if (hb != s->hoverBtn || hs != s->hoverStyle) {
                s->hoverBtn = hb; s->hoverStyle = hs;
                InvalidateRect(h, nullptr, FALSE);
            }
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, h, 0 };
            TrackMouseEvent(&tme);
        }
        return 0;
    case WM_MOUSELEAVE:
        if (s && (s->hoverBtn || s->hoverStyle)) {
            s->hoverBtn = 0; s->hoverStyle = 0;
            InvalidateRect(h, nullptr, FALSE);
        }
        return 0;
    case WM_LBUTTONDOWN: {
        if (!s) return 0;
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        if (PtInRect(&LabelCloseRect(s), pt)) { CloseModalWindow(h); return 0; }
        if (pt.y < DpiScale(34)) {
            ReleaseCapture();
            SendMessageW(h, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            return 0;
        }
        if (s->numbering) {
            if (s->styleOpen) {
                for (int i = 0; i < 2; ++i) {
                    if (PtInRect(&LabelStyleItemRect(s, i), pt)) {
                        s->style = i; s->styleOpen = false; s->hoverStyle = 0;
                        InvalidateRect(h, nullptr, FALSE);
                        return 0;
                    }
                }
            }
            if (PtInRect(&LabelStyleRect(s), pt)) {
                s->styleOpen = !s->styleOpen;
                InvalidateRect(h, nullptr, FALSE);
                return 0;
            }
            if (s->styleOpen) { s->styleOpen = false; InvalidateRect(h, nullptr, FALSE); }
        }
        if (PtInRect(&LabelOkRect(s), pt)) { LabelApply(*s); CloseModalWindow(h); return 0; }
        if (PtInRect(&LabelCancelRect(s), pt)) { CloseModalWindow(h); return 0; }
        SetFocus(s->edit);
        return 0;
    }
    case WM_APP + 1:
        if (s) { LabelApply(*s); CloseModalWindow(h); }
        return 0;
    case WM_APP + 2:
        CloseModalWindow(h);
        return 0;
    case WM_KEYDOWN:
        if (wp == VK_RETURN) { if (s) { LabelApply(*s); CloseModalWindow(h); } return 0; }
        if (wp == VK_ESCAPE) { CloseModalWindow(h); return 0; }
        return 0;
    case WM_CLOSE:
        CloseModalWindow(h);
        return 0;
    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)wp;
        SetTextColor(dc, g_th.text);
        SetBkColor(dc, g_th.panel2);
        return (LRESULT)Brush(g_th.panel2);
    }
    case WM_DESTROY:
        if (s) { s->hwnd = nullptr; s->edit = nullptr; }
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

void RegisterLabelClass()
{
    static bool done = false;
    if (done) return;
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.style = CS_DROPSHADOW;
    wc.lpfnWndProc = LabelProc;
    wc.hInstance = g_inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = L"ExcelLabel";
    RegisterClassExW(&wc);
    done = true;
}

static void RunLabelDialog(bool numbering)
{
    RegisterLabelClass();
    LabelState st;
    st.numbering = numbering;
    st.suffix = numbering ? L"" : L"元";
    st.width = DpiScale(420);
    st.height = DpiScale(150);
    RECT o;
    GetWindowRect(g_hwnd, &o);
    int x = (o.left + o.right) / 2 - st.width / 2;
    int y = (o.top + o.bottom) / 2 - st.height / 2;
    HWND hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, L"ExcelLabel", L"", WS_POPUP | WS_CLIPCHILDREN,
                                x, y, st.width, st.height, g_hwnd, nullptr, g_inst, &st);
    if (!hwnd) return;
    RunModalWindow(hwnd);
}

void ShowNumbering() { RunLabelDialog(true); }
void ShowUnit() { RunLabelDialog(false); }


// ---------------------------------------------------------------------------
//  file dialogs
// ---------------------------------------------------------------------------

std::vector<std::wstring> PickFiles(const wchar_t* title, const COMDLG_FILTERSPEC* filters, UINT filterCount, bool multi)
{
    std::vector<std::wstring> result;
    IFileOpenDialog* dlg = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&dlg));
    if (FAILED(hr) || !dlg) return result;
    DWORD opts = 0;
    dlg->GetOptions(&opts);
    opts |= FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST;
    if (multi) opts |= FOS_ALLOWMULTISELECT;
    dlg->SetOptions(opts);
    if (title) dlg->SetTitle(title);
    if (filters && filterCount) dlg->SetFileTypes(filterCount, filters);
    if (SUCCEEDED(dlg->Show(g_hwnd))) {
        IShellItemArray* arr = nullptr;
        if (SUCCEEDED(dlg->GetResults(&arr)) && arr) {
            DWORD n = 0;
            arr->GetCount(&n);
            for (DWORD i = 0; i < n; ++i) {
                IShellItem* item = nullptr;
                if (FAILED(arr->GetItemAt(i, &item)) || !item) continue;
                PWSTR p = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &p)) && p) {
                    result.push_back(p);
                    CoTaskMemFree(p);
                }
                item->Release();
            }
            arr->Release();
        }
    }
    dlg->Release();
    return result;
}


std::wstring PickFolderPath(const wchar_t* title)
{
    std::wstring result;
    IFileOpenDialog* dlg = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&dlg));
    if (FAILED(hr) || !dlg) return result;
    DWORD opts = 0;
    dlg->GetOptions(&opts);
    opts |= FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_PICKFOLDERS;
    dlg->SetOptions(opts);
    if (title) dlg->SetTitle(title);
    if (SUCCEEDED(dlg->Show(g_hwnd))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item)) && item) {
            PWSTR p = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &p)) && p) {
                result = p;
                CoTaskMemFree(p);
            }
            item->Release();
        }
    }
    dlg->Release();
    return result;
}


std::wstring PickSaveDbPath(const std::wstring& suggested)
{
    std::wstring result;
    IFileSaveDialog* dlg = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&dlg));
    if (FAILED(hr) || !dlg) return result;
    dlg->SetFileTypes(2, kDbFilters);
    dlg->SetDefaultExtension(L"db");
    dlg->SetTitle(L"保存 Excel 表格");
    dlg->SetFileName(suggested.c_str());
    DWORD opts = 0;
    dlg->GetOptions(&opts);
    opts |= FOS_OVERWRITEPROMPT | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST;
    dlg->SetOptions(opts);
    if (SUCCEEDED(dlg->Show(g_hwnd))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item)) && item) {
            PWSTR p = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &p)) && p) {
                result = p;
                CoTaskMemFree(p);
            }
            item->Release();
        }
    }
    dlg->Release();
    if (!result.empty() && PathExtLower(result) != L"db")
        result += L".db";
    return result;
}


// ---------------------------------------------------------------------------
//  open / save / close
// ---------------------------------------------------------------------------
bool SaveDocument(Doc& d, bool saveAs)
{
    if (EditorActive() && g_editDoc == &d) CommitEditor(false, 0);
    std::wstring path = d.filePath;
    if (saveAs || path.empty()) {
        std::wstring suggested = PathBaseName(path);
        if (suggested.empty()) suggested = d.title;
        if (PathExtLower(suggested) != L"db") suggested += L".db";
        path = PickSaveDbPath(suggested);
        if (path.empty()) return false;
    }
    std::wstring err;
    std::wstring oldSession = d.sessionPath;
    if (!d.SaveTo(path, err)) {
        ShowMessageBox(GetActiveWindow(), L"保存失败：\n" + err, L"保存失败", MSGB_OK);
        return false;
    }
    SetStatus(L"已保存 " + d.filePath);
    if (!oldSession.empty() && _wcsicmp(oldSession.c_str(), d.filePath.c_str()) != 0)
        DeleteFileW(oldSession.c_str());
    d.sessionPath.clear();
    for (auto& doc : g_docs) doc->InvalidateLayout();
    UpdateWindowTitle();
    InvalidateAll();
    return true;
}


void OpenDocumentFile(const std::wstring& path)
{
    if (path.empty()) return;
    for (int i = 0; i < (int)g_docs.size(); ++i) {
        if (!g_docs[i]->filePath.empty() && _wcsicmp(g_docs[i]->filePath.c_str(), path.c_str()) == 0) {
            g_active = i;
            RebuildAllLayouts();
            UpdateWindowTitle();
            InvalidateAll();
            return;
        }
    }
    auto doc = std::make_unique<Doc>();
    std::wstring err;
    if (!doc->LoadFile(path, err)) {
        ShowMessageBox(GetActiveWindow(), L"打开失败：\n" + err, L"打开失败", MSGB_OK);
        return;
    }
    g_docs.push_back(std::move(doc));
    g_active = (int)g_docs.size() - 1;
    RebuildAllLayouts();
    UpdateWindowTitle();
    InvalidateAll();
    SetStatus(L"已打开 " + path);
}


void CmdOpen()
{
    auto files = PickFiles(L"打开表格", kDbFilters, 2, false);
    if (!files.empty()) OpenDocumentFile(files[0]);
}


void CmdNew()
{
    if (EditorActive()) CommitEditor(false, 0);
    AddBlankDoc();
}


bool CloseDocumentAt(int index, bool fromExit )
{
    if (index < 0 || index >= (int)g_docs.size()) return false;
    Doc* d = g_docs[index].get();
    if (d->locked && !fromExit) {
        SetStatus(L"标签页已锁定，先右键解锁");
        return false;
    }
    if (EditorActive() && g_editDoc == d) CommitEditor(false, 0);
    if (d->dirty) {
        std::wstring msg = L"是否保存对“" + d->title + L"”的更改？";
        int res = ShowMessageBox(g_hwnd, msg, L"关闭标签页", MSGB_SAVE);
        if (res == IDCANCEL) return false;
        if (res == IDYES) {
            if (!SaveDocument(*d, false)) return false;
        }
    }
    std::wstring closedSession = d->sessionPath;
    d->sessionPath.clear();
    g_docs.erase(g_docs.begin() + index);
    if (!closedSession.empty()) DeleteFileW(closedSession.c_str());
    if (g_active > index) --g_active;
    else if (g_active == index) g_active = std::min(index, (int)g_docs.size() - 1);
    if (g_docs.empty()) {
        g_active = -1;
        if (!fromExit) g_actions.Post(UiAction::CloseWindow);
    }
    UpdateWindowTitle();
    InvalidateAll();
    return true;
}


bool CloseAllForExit()
{
    while (!g_docs.empty()) {
        if (!CloseDocumentAt((int)g_docs.size() - 1, true)) return false;
    }
    return true;
}


// ---------------------------------------------------------------------------
//  clipboard
// ---------------------------------------------------------------------------
bool PutClipboardText(const std::wstring& text)
{
    if (!OpenClipboard(g_hwnd)) return false;
    EmptyClipboard();
    size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (h) {
        void* p = GlobalLock(h);
        if (p) {
            memcpy(p, text.c_str(), bytes);
            GlobalUnlock(h);
            if (!SetClipboardData(CF_UNICODETEXT, h)) GlobalFree(h);
        } else {
            GlobalFree(h);
        }
    }
    CloseClipboard();
    return h != nullptr;
}


std::wstring GetClipboardText()
{
    std::wstring result;
    if (!OpenClipboard(g_hwnd)) return result;
    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    if (h) {
        const wchar_t* p = (const wchar_t*)GlobalLock(h);
        if (p) {
            result = p;
            GlobalUnlock(h);
        }
    }
    CloseClipboard();
    return result;
}


std::wstring CellClipText(Doc& d, int r, int c)
{
    const Cell* cell = d.Find(r, c);
    if (!cell) return L"";
    if (!cell->formula.empty()) return cell->formula;
    if (!cell->text.empty()) return cell->text;
    return cell->path;
}



void CopySelection(Doc& d, bool cut)
{
    int r1, c1, r2, c2;
    d.SelectionRect(r1, c1, r2, c2);
    std::wstring out;
    g_clip.cells.clear();
    g_clip.w = c2 - c1 + 1;
    g_clip.h = r2 - r1 + 1;
    g_clip.valid = true;
    for (int r = r1; r <= r2; ++r) {
        for (int c = c1; c <= c2; ++c) {
            if (c > c1) out += L'\t';
            out += CellClipText(d, r, c);
            const Cell* cell = d.Find(r, c);
            if (cell) {
                Cell copy = *cell;
                if (!copy.path.empty() && copy.kind != CK_URL)
                    copy.path = d.AbsPathOf(copy.path);
                g_clip.cells.push_back(copy);
            } else {
                g_clip.cells.push_back(Cell());
            }
        }
        out += L"\r\n";
    }
    PutClipboardText(out);
    g_clipSeq = GetClipboardSequenceNumber();
    if (cut) {
        ClearSelectionContents(d);
        SetStatus(L"已剪切");
    } else {
        SetStatus(L"已复制");
    }
}


void SplitLines(const std::wstring& text, std::vector<std::wstring>& lines)
{
    lines.clear();
    std::wstring cur;
    for (size_t i = 0; i < text.size(); ++i) {
        wchar_t ch = text[i];
        if (ch == L'\r') {
            if (i + 1 < text.size() && text[i + 1] == L'\n') ++i;
            lines.push_back(cur);
            cur.clear();
        } else if (ch == L'\n') {
            lines.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(ch);
        }
    }
    lines.push_back(cur);
    while (!lines.empty() && lines.back().empty()) lines.pop_back();
}


void SplitTabs(const std::wstring& line, std::vector<std::wstring>& cells)
{
    cells.clear();
    std::wstring cur;
    for (wchar_t ch : line) {
        if (ch == L'\t') { cells.push_back(cur); cur.clear(); }
        else cur.push_back(ch);
    }
    cells.push_back(cur);
}



int KindForPath(const std::wstring& path)
{
    DWORD a = GetFileAttributesW(path.c_str());
    if (a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY)) return CK_FOLDER;
    if (IsImageExt(PathExtLower(path))) return CK_IMAGE;
    return CK_FILE;
}


std::wstring PrepareImportedPath(Doc& d, const std::wstring& abs, int kind)
{
    if (!g_autoCopy) return abs;
    if (IsUnderDir(abs, d.MediaDirAbs()))
        return d.StoragePathOf(abs);
    MediaCopyResult r = CopyIntoMedia(d.MediaDirAbs(), abs, kind == CK_FOLDER, true);
    if (r.ok) return d.StoragePathOf(r.abs);
    return abs;
}


void ImportItemsAt(Doc& d, std::vector<ImportItem> items, int startR, int startC)
{
    if (items.empty()) return;
    if (EditorActive()) CommitEditor(false, 0);
    int r1, c1, r2, c2;
    d.SelectionRect(r1, c1, r2, c2);
    bool horizontal = (c2 > c1 && r2 == r1);

    d.PushUndo();
    int r = startR, c = startC;
    int lastR = r, lastC = c;
    for (auto& it : items) {
        if (r >= d.rows || c >= d.cols) break;
        Cell& cell = d.Ensure(r, c);
        cell.kind = it.kind;
        cell.text = it.text;
        cell.path = it.path;
        cell.formula.clear();
        lastR = r;
        lastC = c;
        if (horizontal) {
            ++c;
            if (c >= d.cols) { c = startC; ++r; }
        } else {
            ++r;
            if (r >= d.rows) { r = startR; ++c; }
        }
    }
    d.selAR = startR;
    d.selAC = startC;
    d.selFR = lastR;
    d.selFC = lastC;
    d.Touch();
    EnsureCellVisible(d, lastR, lastC);
}


void ImportPathsAt(Doc& d, const std::vector<std::wstring>& paths, int startR, int startC)
{
    std::vector<ImportItem> items;
    for (auto& p : paths) {
        int kind = KindForPath(p);
        std::wstring stored = PrepareImportedPath(d, p, kind);
        ImportItem it;
        it.kind = kind;
        it.text = PathBaseName(p);
        it.path = stored;
        items.push_back(it);
    }
    int imported = (int)items.size();
    ImportItemsAt(d, std::move(items), startR, startC);
    SetStatus(Format(L"已导入 %d 项", imported));
}


void CmdImportFiles(bool imagesOnly)
{
    Doc* dp = ActiveDoc();
    if (!dp) return;
    auto files = PickFiles(imagesOnly ? L"导入图像" : L"导入文件",
                           imagesOnly ? kImageFilters : kAllFilters,
                           imagesOnly ? 2 : 1, true);
    if (files.empty()) return;
    ImportPathsAt(*dp, files, dp->selFR, dp->selFC);
}


void CmdImportFolder()
{
    Doc* dp = ActiveDoc();
    if (!dp) return;
    std::wstring folder = PickFolderPath(L"导入文件夹");
    if (folder.empty()) return;
    std::vector<std::wstring> one = { folder };
    ImportPathsAt(*dp, one, dp->selFR, dp->selFC);
}


void CmdImportUrl()
{
    Doc* dp = ActiveDoc();
    if (!dp) return;
    std::wstring url = PromptText(L"导入网址", L"输入网址，双击单元格用默认浏览器打开。", L"https://");
    url = Trim(url);
    if (url.empty() || url == L"https://" || url == L"http://") {
        SetStatus(L"未输入网址");
        return;
    }
    if (url.find(L"://") == std::wstring::npos && url.find(L'.') != std::wstring::npos)
        url = L"https://" + url;
    ImportItem it;
    it.kind = CK_URL;
    it.text = url;
    it.path = url;
    std::vector<ImportItem> items = { it };
    ImportItemsAt(*dp, std::move(items), dp->selFR, dp->selFC);
    SetStatus(L"已导入网址");
}


void PasteFromClipboard(Doc& d)
{
    if (EditorActive()) CommitEditor(false, 0);

    if (g_clip.valid && g_clip.w > 0 && g_clip.h > 0 &&
        GetClipboardSequenceNumber() == g_clipSeq &&
        (int)g_clip.cells.size() == g_clip.w * g_clip.h) {
        int r0 = d.selFR, c0 = d.selFC;
        d.PushUndo();
        int maxR = r0, maxC = c0;
        for (int i = 0; i < g_clip.h; ++i) {
            for (int j = 0; j < g_clip.w; ++j) {
                int r = r0 + i, c = c0 + j;
                if (r >= d.rows || c >= d.cols) continue;
                Cell cell = g_clip.cells[(size_t)i * g_clip.w + j];
                if (!cell.path.empty() && cell.kind != CK_URL)
                    cell.path = d.StoragePathOf(cell.path);
                if (cell.IsEmpty() && cell.st.IsDefault())
                    d.cells.erase(CellKey(r, c));
                else
                    d.cells[CellKey(r, c)] = cell;
                maxR = std::max(maxR, r);
                maxC = std::max(maxC, c);
            }
        }
        d.selAR = r0;
        d.selAC = c0;
        d.selFR = maxR;
        d.selFC = maxC;
        d.Touch();
        EnsureCellVisible(d, maxR, maxC);
        SetStatus(L"已粘贴");
        return;
    }

    std::wstring text = GetClipboardText();
    if (!text.empty()) {
        std::vector<std::wstring> lines;
        SplitLines(text, lines);
        if (lines.empty()) return;
        int r0 = d.selFR, c0 = d.selFC;
        d.PushUndo();
        int maxR = r0, maxC = c0;
        for (size_t i = 0; i < lines.size(); ++i) {
            std::vector<std::wstring> cells;
            SplitTabs(lines[i], cells);
            for (size_t j = 0; j < cells.size(); ++j) {
                int r = r0 + (int)i;
                int c = c0 + (int)j;
                if (r >= d.rows || c >= d.cols) continue;
                Cell& cell = d.Ensure(r, c);
                cell.kind = CK_TEXT;
                cell.path.clear();
                if (!cells[j].empty() && cells[j][0] == L'=') {
                    cell.formula = cells[j];
                    cell.text.clear();
                } else {
                    cell.text = cells[j];
                    cell.formula.clear();
                }
                d.EraseIfTrivial(r, c);
                maxR = std::max(maxR, r);
                maxC = std::max(maxC, c);
            }
        }
        d.selAR = r0;
        d.selAC = c0;
        d.selFR = maxR;
        d.selFC = maxC;
        d.Touch();
        EnsureCellVisible(d, maxR, maxC);
        SetStatus(L"已粘贴");
        return;
    }

    // 文件粘贴
    std::vector<std::wstring> paths;
    if (OpenClipboard(g_hwnd)) {
        HANDLE h = GetClipboardData(CF_HDROP);
        if (h) {
            HDROP drop = (HDROP)h;
            UINT n = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
            for (UINT i = 0; i < n; ++i) {
                wchar_t buf[MAX_PATH * 2];
                if (DragQueryFileW(drop, i, buf, (UINT)std::size(buf)) > 0)
                    paths.push_back(buf);
            }
        }
        CloseClipboard();
    }
    if (!paths.empty()) ImportPathsAt(d, paths, d.selFR, d.selFC);
}


// ---------------------------------------------------------------------------
//  insert rows / columns
// ---------------------------------------------------------------------------
void InsertRows(Doc& d, int at, int count)
{
    if (count <= 0) return;
    if (d.rows + count > g_maxRows) { SetStatus(Format(L"行数上限 %d", g_maxRows)); return; }
    d.PushUndo();
    std::unordered_map<long long, Cell> nc;
    nc.reserve(d.cells.size());
    for (auto& kv : d.cells) {
        int r = KeyRow(kv.first), c = KeyCol(kv.first);
        if (r >= at) r += count;
        if (r >= d.rows + count) continue;
        nc[CellKey(r, c)] = kv.second;
    }
    d.cells.swap(nc);
    AdjustFormulasOnInsert(d, true, at, count);
    for (int i = 0; i < count; ++i) d.rowH.insert(d.rowH.begin() + std::min<int>(at + i, (int)d.rowH.size()), g_rowH);
    d.rows += count;
    if (d.selAR >= at) d.selAR += count;
    if (d.selFR >= at) d.selFR += count;
    d.InvalidateLayout();
    d.Touch();
}


void InsertCols(Doc& d, int at, int count)
{
    if (count <= 0) return;
    if (d.cols + count > g_maxCols) { SetStatus(Format(L"列数上限 %d", g_maxCols)); return; }
    d.PushUndo();
    std::unordered_map<long long, Cell> nc;
    nc.reserve(d.cells.size());
    for (auto& kv : d.cells) {
        int r = KeyRow(kv.first), c = KeyCol(kv.first);
        if (c >= at) c += count;
        if (c >= d.cols + count) continue;
        nc[CellKey(r, c)] = kv.second;
    }
    d.cells.swap(nc);
    AdjustFormulasOnInsert(d, false, at, count);
    for (int i = 0; i < count; ++i) d.colW.insert(d.colW.begin() + std::min<int>(at + i, (int)d.colW.size()), g_colW);
    d.cols += count;
    if (d.selAC >= at) d.selAC += count;
    if (d.selFC >= at) d.selFC += count;
    d.InvalidateLayout();
    d.Touch();
}


void CmdInsert(int id)
{
    Doc* dp = ActiveDoc();
    if (!dp) return;
    Doc& d = *dp;
    int r1, c1, r2, c2;
    d.SelectionRect(r1, c1, r2, c2);
    int h = r2 - r1 + 1, w = c2 - c1 + 1;
    switch (id) {
    case ID_INSERT_UP: InsertRows(d, r1, h); break;
    case ID_INSERT_DOWN: InsertRows(d, r2 + 1, h); break;
    case ID_INSERT_LEFT: InsertCols(d, c1, w); break;
    case ID_INSERT_RIGHT: InsertCols(d, c2 + 1, w); break;
    }
    EnsureCellVisible(d, d.selFR, d.selFC);
}


// ---------------------------------------------------------------------------
//  sum
// ---------------------------------------------------------------------------

// Ctrl+click keeps the active block and appends to d.selExtra in click order,
// so a scattered selection reads as "the block that was picked first, then
// every Ctrl+clicked block in the order it was added". Merge ranges normalise
// to their anchor and duplicate hits collapse. Non-commutative operations use
// this order directly: pick A2, then A1, then an empty cell and A2 op A1 wins.
static bool PickScatteredCells(Doc& d, std::vector<std::pair<int, int>>& inputs,
                               int& outR, int& outC, std::wstring& error)
{
    int r1, c1, r2, c2;
    d.SelectionRect(r1, c1, r2, c2);
    std::vector<MergeRange> ranges;
    ranges.push_back({ r1, c1, r2, c2 });
    for (auto& e : d.selExtra) ranges.push_back(e);

    std::vector<std::pair<int, int>> cells;
    std::unordered_set<long long> seen;
    for (auto& e : ranges) {
        for (int r = e.r1; r <= e.r2; ++r) {
            for (int c = e.c1; c <= e.c2; ++c) {
                int ar = r, ac = c;
                if (const MergeRange* m = d.MergeAt(r, c)) { ar = m->r1; ac = m->c1; }
                if (!seen.insert(CellKey(ar, ac)).second) continue;
                cells.push_back({ ar, ac });
            }
        }
    }

    std::vector<std::pair<int, int>> empties;
    inputs.clear();
    for (auto& rc : cells) {
        const Cell* cell = d.Find(rc.first, rc.second);
        if (!cell || cell->IsEmpty()) empties.push_back(rc);
        else inputs.push_back(rc);
    }
    if (empties.size() != 1) {
        error = empties.empty()
            ? L"请再选中一个空白格作为输出格"
            : L"选中了多个空白格，无法确定输出格";
        return false;
    }
    if (inputs.size() < 2) {
        error = L"至少需要 2 个有内容的格";
        return false;
    }
    outR = empties[0].first;
    outC = empties[0].second;
    return true;
}


static void WriteScatteredResult(Doc& d, int outR, int outC, const std::wstring& formula)
{
    d.PushUndo();
    Cell& out = d.Ensure(outR, outC);
    out.formula = formula;
    out.text.clear();
    out.path.clear();
    out.kind = CK_TEXT;
    d.selExtra.clear();
    d.selAR = d.selFR = outR;
    d.selAC = d.selFC = outC;
    d.Touch();
    EnsureCellVisible(d, outR, outC);
}


void CmdSum()
{
    Doc* dp = ActiveDoc();
    if (!dp) return;
    Doc& d = *dp;

    if (!d.selExtra.empty()) {
        std::vector<std::pair<int, int>> inputs;
        int outR = -1, outC = -1;
        std::wstring error;
        if (!PickScatteredCells(d, inputs, outR, outC, error)) {
            SetStatus(L"总和：" + error);
            return;
        }
        std::wstring args;
        for (size_t i = 0; i < inputs.size(); ++i) {
            if (i) args += L",";
            args += CellRefName(inputs[i].first, inputs[i].second);
        }
        std::wstring formula = L"=IF(COUNT(" + args + L")>0,SUM(" + args + L"),\"\")";
        WriteScatteredResult(d, outR, outC, formula);
        SetStatus(L"已写入 " + formula);
        return;
    }

    int r1, c1, r2, c2;
    d.SelectionRect(r1, c1, r2, c2);
    bool oneRow = (r1 == r2);
    bool oneCol = (c1 == c2);
    if (!oneRow && !oneCol) {
        SetStatus(L"总和：请选择一行或一列（末尾为输出格）");
        return;
    }
    int n = oneRow ? (c2 - c1 + 1) : (r2 - r1 + 1);
    if (n < 2) {
        SetStatus(L"总和：至少选择 2 格，最后一格为输出格");
        return;
    }

    const Cell* lastCell = d.Find(r2, c2);
    bool lastIsData = lastCell && lastCell->formula.empty() &&
                      (!lastCell->text.empty() || lastCell->kind != CK_TEXT);

    int outR = r2, outC = c2;
    int rr1 = r1, cc1 = c1, rr2 = r2, cc2 = c2;
    if (lastIsData) {
        if (oneRow) { outC = c2 + 1; }
        else { outR = r2 + 1; }
        if (outR >= d.rows || outC >= d.cols) {
            SetStatus(L"总和：没有可用的输出格");
            return;
        }
        rr1 = r1; cc1 = c1; rr2 = r2; cc2 = c2;
    } else {
        if (oneRow) { rr2 = c2 - 1; cc2 = c1; }
        else { rr2 = r2 - 1; cc2 = c2; }
    }

    std::wstring range = CellRefName(rr1, cc1) + L":" + CellRefName(rr2, cc2);
    int count = (rr2 - rr1 + 1) * (cc2 - cc1 + 1);
    if (count <= 0) {
        SetStatus(L"总和：区域为空");
        return;
    }
    std::wstring formula = L"=IF(COUNT(" + range + L")>0,SUM(" + range + L"),\"\")";
    d.PushUndo();
    Cell& out = d.Ensure(outR, outC);
    out.formula = formula;
    out.text.clear();
    out.path.clear();
    out.kind = CK_TEXT;
    d.selAR = r1;
    d.selAC = c1;
    d.selFR = outR;
    d.selFC = outC;
    d.Touch();
    EnsureCellVisible(d, outR, outC);
    SetStatus(L"已写入 " + formula);
}


void CmdOp(wchar_t op)
{
    Doc* dp = ActiveDoc();
    if (!dp) return;
    Doc& d = *dp;

    if (!d.selExtra.empty()) {
        std::vector<std::pair<int, int>> inputs;
        int outR = -1, outC = -1;
        std::wstring error;
        if (!PickScatteredCells(d, inputs, outR, outC, error)) {
            SetStatus(L"运算：" + error);
            return;
        }
        std::wstring args, expr;
        for (size_t i = 0; i < inputs.size(); ++i) {
            std::wstring ref = CellRefName(inputs[i].first, inputs[i].second);
            if (i) { args += L","; expr.push_back(op); }
            args += ref;
            expr += ref;
        }
        std::wstring formula;
        if (op == L'+')
            formula = L"=IF(COUNT(" + args + L")>0,SUM(" + args + L"),\"\")";
        else
            formula = L"=IF(COUNT(" + args + L")=" + std::to_wstring(inputs.size()) +
                      L"," + expr + L",\"\")";
        WriteScatteredResult(d, outR, outC, formula);
        const wchar_t* name = (op == L'+') ? L"相加" : (op == L'-') ? L"相减" :
                              (op == L'*') ? L"相乘" : L"相除";
        SetStatus(std::wstring(L"已计算：") + name);
        return;
    }

    int r1, c1, r2, c2;
    d.SelectionRect(r1, c1, r2, c2);
    bool oneRow = (r1 == r2);
    bool oneCol = (c1 == c2);
    if (!oneRow && !oneCol) {
        SetStatus(L"运算：请选择一行或一列（最后一格为输出格）");
        return;
    }
    int n = oneRow ? (c2 - c1 + 1) : (r2 - r1 + 1);
    if (n < 2) {
        SetStatus(L"运算：至少选择 2 格");
        return;
    }

    const Cell* lastCell = d.Find(r2, c2);
    bool lastIsData = lastCell && lastCell->formula.empty() &&
                      (!lastCell->text.empty() || lastCell->kind != CK_TEXT);
    int outR = r2, outC = c2;
    int rr1 = r1, cc1 = c1, rr2 = r2, cc2 = c2;
    if (lastIsData) {
        if (oneRow) outC = c2 + 1;
        else outR = r2 + 1;
        if (outR >= d.rows || outC >= d.cols) {
            SetStatus(L"运算：没有可用的输出格");
            return;
        }
    } else {
        if (oneRow) { rr2 = c2 - 1; cc2 = c1; }
        else { rr2 = r2 - 1; cc2 = c2; }
    }

    int count = (rr2 - rr1 + 1) * (cc2 - cc1 + 1);
    if (count < 1) {
        SetStatus(L"运算：区域为空");
        return;
    }
    std::wstring range = CellRefName(rr1, cc1) + L":" + CellRefName(rr2, cc2);
    std::wstring expr;
    if (oneRow) {
        for (int c = cc1; c <= cc2; ++c) {
            if (!expr.empty()) expr.push_back(op);
            expr += CellRefName(rr1, c);
        }
    } else {
        for (int r = rr1; r <= rr2; ++r) {
            if (!expr.empty()) expr.push_back(op);
            expr += CellRefName(r, cc1);
        }
    }
    std::wstring formula;
    if (op == L'+')
        formula = L"=IF(COUNT(" + range + L")>0,SUM(" + range + L"),\"\")";
    else
        formula = L"=IF(COUNT(" + range + L")=" + std::to_wstring(count) +
                  L"," + expr + L",\"\")";

    d.PushUndo();
    Cell& out = d.Ensure(outR, outC);
    out.formula = formula;
    out.text.clear();
    out.path.clear();
    out.kind = CK_TEXT;
    d.selAR = r1;
    d.selAC = c1;
    d.selFR = outR;
    d.selFC = outC;
    d.Touch();
    EnsureCellVisible(d, outR, outC);
    const wchar_t* name = (op == L'+') ? L"相加" : (op == L'-') ? L"相减" :
                          (op == L'*') ? L"相乘" : L"相除";
    SetStatus(std::wstring(L"已计算：") + name);
}

// ---------------------------------------------------------------------------
//  properties
// ---------------------------------------------------------------------------
bool CanShowProps(Doc& d)
{
    int r1, c1, r2, c2;
    d.SelectionRect(r1, c1, r2, c2);
    bool any = false;
    for (int r = r1; r <= r2; ++r) {
        for (int c = c1; c <= c2; ++c) {
            const Cell* cell = d.Find(r, c);
            if (!cell || cell->IsEmpty()) continue;
            if (cell->kind == CK_TEXT) return false;
            any = true;
        }
    }
    return any;
}




RECT PropCheckRect(PropState* s)
{
    (void)s;
    return { DpiScale(24), DpiScale(214), DpiScale(40), DpiScale(230) };
}

RECT PropCopyRect(PropState* s)
{
    int w = DpiScale(104), h = DpiScale(32);
    return { s->width - DpiScale(24) - w * 2 - DpiScale(10), s->height - DpiScale(24) - h,
             s->width - DpiScale(24) - w - DpiScale(10), s->height - DpiScale(24) };
}

RECT PropNoCopyRect(PropState* s)
{
    int w = DpiScale(104), h = DpiScale(32);
    return { s->width - DpiScale(24) - w, s->height - DpiScale(24) - h,
             s->width - DpiScale(24), s->height - DpiScale(24) };
}

RECT PropCloseRect(PropState* s)
{
    return { s->width - DpiScale(34), 0, s->width, DpiScale(34) };
}


void PropDoCopy(PropState& s)
{
    Doc* d = s.doc;
    if (!d) return;

    std::wstring media = d->MediaDirAbs();
    int copied = 0, existed = 0;
    std::wstring firstErr;
    for (auto& rc : s.cells) {
        Cell* cell = d->Find(rc.first, rc.second);
        if (!cell || cell->path.empty() || cell->kind == CK_URL) continue;
        std::wstring abs = d->AbsPathOf(cell->path);
        if (GetFileAttributesW(abs.c_str()) == INVALID_FILE_ATTRIBUTES) {
            if (firstErr.empty()) firstErr = L"路径不存在：" + abs;
            continue;
        }
        if (!media.empty() && _wcsnicmp(abs.c_str(), media.c_str(), media.size()) == 0)
            continue;
        MediaCopyResult r = CopyIntoMedia(media, abs, cell->kind == CK_FOLDER, false);
        if (r.existed) {
            ++existed;
            if (firstErr.empty()) firstErr = L"已经存在：" + PathBaseName(abs);
            continue;
        }
        if (!r.ok) {
            if (firstErr.empty()) firstErr = r.err;
            continue;
        }
        cell->path = d->StoragePathOf(r.abs);
        ++copied;
    }
    if (copied > 0) d->Touch();
    if (!firstErr.empty()) {
        s.message = firstErr;
        ShowMessageBox(s.hwnd, firstErr, L"属性", MSGB_OK);
        InvalidateRect(s.hwnd, nullptr, FALSE);
    }
    if (copied > 0) {
        SetStatus(Format(L"已复制 %d 项到 media", copied));
        CloseModalWindow(s.hwnd);
    } else if (existed > 0) {
        SetStatus(L"media 中已经存在，未覆盖");
    }
}


LRESULT CALLBACK PropProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    PropState* s = (PropState*)GetWindowLongPtrW(h, GWLP_USERDATA);
    switch (msg) {
    case WM_NCCREATE: {
        CREATESTRUCTW* cs = (CREATESTRUCTW*)lp;
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return TRUE;
    }
    case WM_CREATE:
        s = (PropState*)GetWindowLongPtrW(h, GWLP_USERDATA);
        if (s) s->hwnd = h;
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        if (s) {
            BuildPropSpec(*s, dc);                      // commit
            RECT c = {};
            GetClientRect(h, &c);
            PresentSpec(dc, s->surface, c, s->spec);    // pure render: one blit
        }
        EndPaint(h, &ps);
        return 0;
    }
    case WM_MOUSEMOVE:
        if (s) {
            POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            int hb = 0;
            if (PtInRect(&PropCopyRect(s), pt)) hb = 1;
            else if (PtInRect(&PropNoCopyRect(s), pt)) hb = 2;
            else if (PtInRect(&PropCheckRect(s), pt)) hb = 3;
            if (hb != s->hoverBtn) { s->hoverBtn = hb; InvalidateRect(h, nullptr, FALSE); }
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, h, 0 };
            TrackMouseEvent(&tme);
        }
        return 0;
    case WM_MOUSELEAVE:
        if (s && s->hoverBtn) { s->hoverBtn = 0; InvalidateRect(h, nullptr, FALSE); }
        return 0;
    case WM_LBUTTONDOWN:
        if (!s) return 0;
        {
            POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            RECT close = PropCloseRect(s);
            if (PtInRect(&close, pt)) { CloseModalWindow(h); return 0; }
            if (pt.y < DpiScale(34)) {
                ReleaseCapture();
                SendMessageW(h, WM_NCLBUTTONDOWN, HTCAPTION, 0);
                return 0;
            }
            if (PtInRect(&PropCopyRect(s), pt)) { PropDoCopy(*s); return 0; }
            if (PtInRect(&PropNoCopyRect(s), pt)) { CloseModalWindow(h); return 0; }
            if (PtInRect(&PropCheckRect(s), pt)) {
                g_autoCopy = !g_autoCopy;
                SaveSettings();
                InvalidateRect(h, nullptr, FALSE);
                return 0;
            }
        }
        return 0;
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) { CloseModalWindow(h); return 0; }
        return 0;
    case WM_CLOSE:
        CloseModalWindow(h);
        return 0;
    case WM_DESTROY:
        if (s) s->hwnd = nullptr;
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}


void RegisterPropClass()
{
    static bool done = false;
    if (done) return;
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.style = CS_DROPSHADOW;
    wc.lpfnWndProc = PropProc;
    wc.hInstance = g_inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = L"ExcelProps";
    RegisterClassExW(&wc);
    done = true;
}


void CmdProperties(HWND ownerOverride )
{
    Doc* dp = ActiveDoc();
    if (!dp) return;
    Doc& d = *dp;
    if (!CanShowProps(d)) {
        SetStatus(L"属性仅对文件、文件夹、图像、网址有效");
        return;
    }
    int r1, c1, r2, c2;
    d.SelectionRect(r1, c1, r2, c2);
    PropState ps;
    ps.doc = &d;
    ps.width = DpiScale(660);
    ps.height = DpiScale(300);
    for (int r = r1; r <= r2; ++r) {
        for (int c = c1; c <= c2; ++c) {
            const Cell* cell = d.Find(r, c);
            if (!cell || cell->IsEmpty()) continue;
            ps.cells.push_back({ r, c });
            if (ps.firstPath.empty()) {
                ps.firstPath = d.AbsPathOf(cell->path);
                ps.firstKind = cell->kind;
            }
        }
    }
    if (ps.cells.empty()) return;
    RegisterPropClass();
    HWND owner = ownerOverride ? ownerOverride : g_hwnd;
    RECT o;
    GetWindowRect(owner, &o);
    int x = (o.left + o.right) / 2 - ps.width / 2;
    int y = (o.top + o.bottom) / 2 - ps.height / 2;
    HWND hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, L"ExcelProps", L"", WS_POPUP | WS_CLIPCHILDREN,
                                x, y, ps.width, ps.height, owner, nullptr, g_inst, &ps);
    if (!hwnd) return;
    RunModalWindow(hwnd, owner);
}


// ---------------------------------------------------------------------------
//  settings window (自绘，非下拉菜单)
// ---------------------------------------------------------------------------


RECT SetThemeRect(SettingsState* s, int index)
{
    
    int x = SettingsPx(s, 150) + index * (SettingsPx(s, 96) + SettingsPx(s, 8));
    return { x, SettingsPx(s, 52), x + SettingsPx(s, 96), SettingsPx(s, 82) };
}

RECT SetUiMinus(SettingsState* s)
{ return { SettingsPx(s, 150), SettingsPx(s, 100), SettingsPx(s, 182), SettingsPx(s, 130) }; }

RECT SetUiPlus(SettingsState* s)
{ return { SettingsPx(s, 284), SettingsPx(s, 100), SettingsPx(s, 316), SettingsPx(s, 130) }; }

RECT SetGridMinus(SettingsState* s)
{ return { SettingsPx(s, 150), SettingsPx(s, 142), SettingsPx(s, 182), SettingsPx(s, 172) }; }

RECT SetGridPlus(SettingsState* s)
{ return { SettingsPx(s, 284), SettingsPx(s, 142), SettingsPx(s, 316), SettingsPx(s, 172) }; }

RECT SetRowFrame(SettingsState* s)
{ return { SettingsPx(s, 124), SettingsPx(s, 192), SettingsPx(s, 224), SettingsPx(s, 222) }; }

RECT SetColFrame(SettingsState* s)
{ return { SettingsPx(s, 340), SettingsPx(s, 192), SettingsPx(s, 440), SettingsPx(s, 222) }; }

RECT SetAutoRect(SettingsState* s)
{ return { SettingsPx(s, 24), SettingsPx(s, 262), SettingsPx(s, 40), SettingsPx(s, 278) }; }

RECT SetPropRect(SettingsState* s)
{ return { SettingsPx(s, 24), SettingsPx(s, 306), SettingsPx(s, 144), SettingsPx(s, 338) }; }

RECT SetApplyRect(SettingsState* s)
{ return { s->width - SettingsPx(s, 244), s->height - SettingsPx(s, 56), s->width - SettingsPx(s, 132), s->height - SettingsPx(s, 24) }; }

RECT SetCloseRect(SettingsState* s)
{ return { s->width - SettingsPx(s, 124), s->height - SettingsPx(s, 56), s->width - SettingsPx(s, 24), s->height - SettingsPx(s, 24) }; }

RECT SetCloseX(SettingsState* s)
{ return { s->width - SettingsPx(s, 34), 0, s->width, SettingsPx(s, 36) }; }


void SettingsLayoutEdits(SettingsState* s)
{
    if (!s || !s->hwnd) return;
    RECT c = SetColFrame(s);
    RECT r = SetRowFrame(s);
    MoveWindow(s->colEdit, c.left + 1, c.top + 1, c.right - c.left - 2, c.bottom - c.top - 2, TRUE);
    MoveWindow(s->rowEdit, r.left + 1, r.top + 1, r.right - r.left - 2, r.bottom - r.top - 2, TRUE);
}


LRESULT CALLBACK SettingsEditProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR)
{
    if (msg == WM_KEYDOWN) {
        if (wp == VK_RETURN) { PostMessageW(GetParent(h), WM_APP + 1, 0, 0); return 0; }
        if (wp == VK_ESCAPE) { PostMessageW(GetParent(h), WM_APP + 2, 0, 0); return 0; }
    }
    if (msg == WM_CHAR && (wp == VK_RETURN || wp == VK_ESCAPE)) return 0;
    return DefSubclassProc(h, msg, wp, lp);
}


void SettingsSetTheme(SettingsState* s, bool dark)
{
    if (!s) return;
    s->dark = dark;
    if (g_dark != dark) {
        g_dark = dark;
        ApplyThemeColors();
        SaveSettings();
        if (s->colEdit) InvalidateRect(s->colEdit, nullptr, TRUE);
        if (s->rowEdit) InvalidateRect(s->rowEdit, nullptr, TRUE);
        RedrawWindow(s->hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
        InvalidateAll();
    }
}


// The window box follows the dialog's OWN frozen scale, exactly like every
// offset drawn inside it. One measurer, one reading - so the box can never be
// cut at a scale other than the one the spec is built at.
static const int kSettingsW = 600;
static const int kSettingsH = 470;

static void SettingsMeasureSize(SettingsState& s)
{
    s.width = SettingsPx(s, kSettingsW);
    s.height = SettingsPx(s, kSettingsH);
}


// Values only. The window itself is NOT touched: the dialog's geometry was
// frozen when it opened, so applying a new zoom changes the app behind it and
// the two percentages printed inside, and nothing else. That is exactly why
// "apply" is now one clean change - there is no resize to hide, so there is no
// frame in between, so there is nothing that can flash.
static void SettingsCommit(SettingsState* s)
{
    g_uiZoom = s->uiZoom;
    g_gridZoom = s->gridZoom;
    g_autoCopy = s->autoCopy;
    RebuildAllLayouts();
    SaveSettings();
    InvalidateAll();
}


bool SettingsApply(SettingsState* s)
{
    if (!s || !s->hwnd) return false;
    SettingsCommit(s);
    InvalidateRect(s->hwnd, nullptr, FALSE);
    return true;
}


void SettingsClose(SettingsState* s)
{
    if (!s || !s->hwnd) return;
    SettingsCommit(s);
    CloseModalWindow(s->hwnd);
}


LRESULT CALLBACK SettingsProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    SettingsState* s = (SettingsState*)GetWindowLongPtrW(h, GWLP_USERDATA);
    switch (msg) {
    case WM_NCCREATE: {
        CREATESTRUCTW* cs = (CREATESTRUCTW*)lp;
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return TRUE;
    }
    case WM_CREATE: {
        s = (SettingsState*)GetWindowLongPtrW(h, GWLP_USERDATA);
        if (!s) return -1;
        s->hwnd = h;
        SettingsMeasureSize(*s);

        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC outDC = BeginPaint(h, &ps);
        if (!s) { EndPaint(h, &ps); return 0; }
        BuildSettingsSpec(*s, outDC);                  // commit
        RECT c = {};
        GetClientRect(h, &c);
        PresentSpec(outDC, s->surface, c, s->spec);     // pure render: one blit
        EndPaint(h, &ps);
        return 0;
    }
    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)wp;
        SetTextColor(dc, g_th.text);
        SetBkColor(dc, g_th.panel2);
        return (LRESULT)Brush(g_th.panel2);
    }
    case WM_MOUSEMOVE:
        if (s) {
            POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            int hv = 0;
            for (int i = 0; i < 2; ++i) if (PtInRect(&SetThemeRect(s, i), pt)) hv = i + 1;
            if (PtInRect(&SetUiMinus(s), pt)) hv = 3;
            else if (PtInRect(&SetUiPlus(s), pt)) hv = 4;
            else if (PtInRect(&SetGridMinus(s), pt)) hv = 5;
            else if (PtInRect(&SetGridPlus(s), pt)) hv = 6;
            else if (PtInRect(&SetAutoRect(s), pt)) hv = 7;
            else if (PtInRect(&SetPropRect(s), pt)) hv = 8;
            else if (PtInRect(&SetApplyRect(s), pt)) hv = 11;
            else if (PtInRect(&SetCloseRect(s), pt)) hv = 9;
            if (PtInRect(&SetCloseX(s), pt)) hv = 10;
            if (hv != s->hover) { s->hover = hv; InvalidateRect(h, nullptr, FALSE); }
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, h, 0 };
            TrackMouseEvent(&tme);
        }
        return 0;
    case WM_MOUSELEAVE:
        if (s && s->hover) { s->hover = 0; InvalidateRect(h, nullptr, FALSE); }
        return 0;
    case WM_LBUTTONDOWN:
        if (!s) return 0;
        {
            POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            if (PtInRect(&SetCloseX(s), pt)) {
                SettingsClose(s);
                return 0;
            }
            for (int i = 0; i < 2; ++i) {
                if (PtInRect(&SetThemeRect(s, i), pt)) { SettingsSetTheme(s, i == 1); return 0; }
            }
            if (PtInRect(&SetUiMinus(s), pt)) { s->uiZoom = std::max(80, s->uiZoom - 10); InvalidateRect(h, nullptr, FALSE); return 0; }
            if (PtInRect(&SetUiPlus(s), pt)) { s->uiZoom = std::min(150, s->uiZoom + 10); InvalidateRect(h, nullptr, FALSE); return 0; }
            if (PtInRect(&SetGridMinus(s), pt)) { s->gridZoom = std::max(50, s->gridZoom - 10); InvalidateRect(h, nullptr, FALSE); return 0; }
            if (PtInRect(&SetGridPlus(s), pt)) { s->gridZoom = std::min(250, s->gridZoom + 10); InvalidateRect(h, nullptr, FALSE); return 0; }
            if (PtInRect(&SetAutoRect(s), pt)) {
                s->autoCopy = !s->autoCopy;
                g_autoCopy = s->autoCopy;
                SaveSettings();
                InvalidateRect(h, nullptr, FALSE);
                return 0;
            }
            if (PtInRect(&SetPropRect(s), pt)) {
                Doc* d = ActiveDoc();
                if (d && CanShowProps(*d)) PostMessageW(h, WM_APP + 4, 0, 0);
                else {
                    s->message = L"请先选中文件 / 文件夹 / 图像 / 网址单元格";
                    InvalidateRect(h, nullptr, FALSE);
                }
                return 0;
            }
            if (PtInRect(&SetApplyRect(s), pt)) { SettingsApply(s); return 0; }
            if (PtInRect(&SetCloseRect(s), pt)) { SettingsClose(s); return 0; }
        }
        return 0;
    case WM_APP + 1:
        SettingsClose(s);
        return 0;
    case WM_APP + 2:
        SettingsClose(s);
        return 0;
    case WM_APP + 4:
        CmdProperties(h);
        return 0;
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) { SettingsClose(s); return 0; }
        if (wp == VK_RETURN) { SettingsClose(s); return 0; }
        return 0;
    case WM_CLOSE:
        SettingsClose(s);
        return 0;
    case WM_DESTROY:
        if (s) s->hwnd = nullptr;
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}


void RegisterSettingsClass()
{
    static bool done = false;
    if (done) return;
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.style = CS_DROPSHADOW;
    wc.lpfnWndProc = SettingsProc;
    wc.hInstance = g_inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = L"ExcelSettings";
    RegisterClassExW(&wc);
    done = true;
}


void OpenSettingsWindow()
{
    if (EditorActive()) CommitEditor(false, 0);
    CloseAllPopups();
    SettingsState st;
    st.dark = g_dark;
    st.autoCopy = g_autoCopy;
    st.uiZoom = g_uiZoom;
    st.gridZoom = g_gridZoom;
    st.maxRows = g_maxRows;
    st.maxCols = g_maxCols;
    // Freeze the scale here, at the one moment this window is allowed to have
    // one: before it exists. Everything after - box, offsets, fonts - reads this
    // pair, so the dialog cannot be rescaled by a control inside itself.
    st.snapDpi = g_dpi;
    st.snapZoom = g_uiZoom;
    SettingsMeasureSize(st);
    RECT o;
    GetWindowRect(g_hwnd, &o);
    int x = (o.left + o.right) / 2 - st.width / 2;
    int y = (o.top + o.bottom) / 2 - st.height / 2;
    RegisterSettingsClass();
    HWND hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, L"ExcelSettings", L"", WS_POPUP | WS_CLIPCHILDREN,
                                x, y, st.width, st.height, g_hwnd, nullptr, g_inst, &st);
    if (!hwnd) return;
    g_settings = &st;
    RunModalWindow(hwnd, g_hwnd);
    g_settings = nullptr;
}

// ---------------------------------------------------------------------------
//  command dispatch
// ---------------------------------------------------------------------------
void ExecuteCommand(int id)
{
    if (EditorActive()) CommitEditor(false, 0);
    Doc* dp = ActiveDoc();

    switch (id) {
    case ID_TAB_LOCK: {
        int mi = g_ui.tabs.TakeMenuIndex();
        if (mi >= 0 && mi < (int)g_docs.size()) {
            Doc* t = g_docs[mi].get();
            t->locked = !t->locked;
            SetStatus(t->locked ? L"标签页已锁定" : L"标签页已解锁");
            InvalidateAll();
        }
        break;
    }
    case ID_FILE_NEW: CmdNew(); break;
    case ID_FILE_OPEN: CmdOpen(); break;
    case ID_FILE_SAVE:
        if (dp) SaveDocument(*dp, false);
        break;
    case ID_FILE_SAVEAS:
        if (dp) SaveDocument(*dp, true);
        break;
    case ID_FILE_EXIT:
        g_actions.Post(UiAction::CloseWindow);
        break;

    case ID_EDIT_CUT:
        if (dp) CopySelection(*dp, true);
        break;
    case ID_EDIT_COPY:
        if (dp) CopySelection(*dp, false);
        break;
    case ID_EDIT_PASTE:
        if (dp) PasteFromClipboard(*dp);
        break;
    case ID_EDIT_UNDO:
        if (dp) dp->Undo();
        break;
    case ID_EDIT_REDO:
        if (dp) dp->Redo();
        break;

    case ID_IMPORT_FILE: CmdImportFiles(false); break;
    case ID_IMPORT_FOLDER: CmdImportFolder(); break;
    case ID_IMPORT_IMAGE: CmdImportFiles(true); break;
    case ID_IMPORT_URL: CmdImportUrl(); break;

    case ID_INSERT_UP:
    case ID_INSERT_DOWN:
    case ID_INSERT_LEFT:
    case ID_INSERT_RIGHT: CmdInsert(id); break;

    case ID_EDIT_SUM: CmdSum(); break;
    case ID_OP_ADD: CmdOp(L'+'); break;
    case ID_OP_SUB: CmdOp(L'-'); break;
    case ID_OP_MUL: CmdOp(L'*'); break;
    case ID_OP_DIV: CmdOp(L'/'); break;
    }
    InvalidateAll();
}


