// ============================================================================
//  p1_core.cpp
//  Responsibility: core / platform
//  Process setup, settings, theme, DPI, fonts and GDI primitives.
//
//  Shared contract: core.h + only the api_*.h headers this module references.
//  This translation unit contains no type or state definitions of its own (state.cpp does).
// ============================================================================
#include "core.h"
#include "api_p1_core.h"
// ---- src\p1_core.inc ----
// ============================================================================
//  Excel - part 1: core, theme, settings, GDI helpers
// ============================================================================




// ---------------------------------------------------------------------------
//  Action bus - logic layer.
//  Objects OUTPUT actions as data. There is no event bubbling through the
//  window procedure and no parent reaching into a child's parameters.
// ---------------------------------------------------------------------------




// Logic-layer content revision. Bumped only when document data changes
// (Doc::Touch / Undo / Redo), never by a plain redraw. Display values,
// animation state and hint specs key off it, so painting only reads.


// ---------------------------------------------------------------------------
//  global user settings
// ---------------------------------------------------------------------------
// Initial grid: fixed logic-layer constants (no runtime computation).
// A new sheet is always 30 rows x 20 columns.

// ---------------------------------------------------------------------------
//  theme
// ---------------------------------------------------------------------------


void ApplyThemeColors()
{
    if (g_dark) {
        g_th.win         = RGB(11, 13, 16);
        g_th.cell        = RGB(16, 19, 23);
        g_th.panel       = RGB(20, 23, 28);
        g_th.panel2      = RGB(26, 30, 36);
        g_th.border      = RGB(40, 46, 55);
        g_th.borderStrong= RGB(62, 71, 84);
        g_th.text        = RGB(223, 229, 236);
        g_th.dim         = RGB(139, 150, 163);
        g_th.faint       = RGB(88, 98, 111);
        g_th.accent      = RGB(56, 211, 184);
        g_th.accentSoft  = RGB(26, 74, 70);
        g_th.selFill     = RGB(28, 58, 62);
        g_th.header      = RGB(23, 27, 33);
        g_th.headerSel   = RGB(35, 66, 70);
        g_th.hover       = RGB(33, 39, 47);
        g_th.grid        = RGB(35, 41, 49);
        g_th.gridStrong  = RGB(55, 64, 76);
        g_th.danger      = RGB(255, 99, 99);
    } else {
        g_th.win         = RGB(244, 246, 248);
        g_th.cell        = RGB(255, 255, 255);
        g_th.panel       = RGB(250, 251, 252);
        g_th.panel2      = RGB(238, 241, 244);
        g_th.border      = RGB(207, 213, 221);
        g_th.borderStrong= RGB(160, 170, 182);
        g_th.text        = RGB(22, 26, 32);
        g_th.dim         = RGB(92, 102, 115);
        g_th.faint       = RGB(140, 150, 162);
        g_th.accent      = RGB(0, 122, 138);
        g_th.accentSoft  = RGB(205, 238, 235);
        g_th.selFill     = RGB(214, 240, 237);
        g_th.header      = RGB(236, 240, 244);
        g_th.headerSel   = RGB(206, 232, 229);
        g_th.hover       = RGB(228, 234, 240);
        g_th.grid        = RGB(219, 224, 231);
        g_th.gridStrong  = RGB(170, 180, 192);
        g_th.danger      = RGB(200, 40, 40);
    }
}


// ---------------------------------------------------------------------------
//  small string helpers
// ---------------------------------------------------------------------------
std::wstring Format(const wchar_t* fmt, ...)
{
    wchar_t buf[2048];
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf_s(buf, _TRUNCATE, fmt, ap);
    va_end(ap);
    return std::wstring(buf);
}


std::wstring Trim(const std::wstring& s)
{
    size_t a = 0, b = s.size();
    while (a < b && iswspace(s[a])) ++a;
    while (b > a && iswspace(s[b - 1])) --b;
    return s.substr(a, b - a);
}


std::wstring ToLower(const std::wstring& s)
{
    std::wstring r = s;
    for (auto& c : r) c = (wchar_t)towlower(c);
    return r;
}


std::string WideToUtf8(const std::wstring& s)
{
    if (s.empty()) return std::string();
    int n = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0, nullptr, nullptr);
    std::string r(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), &r[0], n, nullptr, nullptr);
    return r;
}


std::wstring Utf8ToWide(const std::string& s)
{
    if (s.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring r(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &r[0], n);
    return r;
}


bool ParseNum(const std::wstring& in, double& out)
{
    std::wstring s = Trim(in);
    if (s.empty()) return false;
    wchar_t* end = nullptr;
    double v = wcstod(s.c_str(), &end);
    if (end == s.c_str()) return false;
    while (end && *end && iswspace(*end)) ++end;
    if (end && *end != 0) return false;
    if (!_finite(v)) return false;
    out = v;
    return true;
}


std::wstring NumToStr(double v)
{
    if (!_finite(v)) return L"#NUM!";
    if (fabs(v) < 1e-12) v = 0.0;
    if (fabs(v - floor(v + 0.5)) < 1e-9 && fabs(v) < 1e15) {
        wchar_t buf[64];
        swprintf_s(buf, L"%.0f", v);
        return buf;
    }
    wchar_t buf[96];
    swprintf_s(buf, L"%.10f", v);
    std::wstring s = buf;
    while (!s.empty() && s.back() == L'0') s.pop_back();
    if (!s.empty() && s.back() == L'.') s.pop_back();
    if (s == L"-0") s = L"0";
    return s;
}


// ---------------------------------------------------------------------------
//  path helpers
// ---------------------------------------------------------------------------
bool IsAbsPath(const std::wstring& p)
{
    if (p.size() >= 2 && p[1] == L':') return true;
    if (p.size() >= 2 && (p[0] == L'\\' || p[0] == L'/')) return true;
    return false;
}


std::wstring PathDirName(const std::wstring& p)
{
    size_t i = p.find_last_of(L"\\/");
    if (i == std::wstring::npos) return L"";
    if (i == 2 && p.size() > 2 && p[1] == L':') return p.substr(0, 3);
    return p.substr(0, i);
}


std::wstring PathBaseName(const std::wstring& p)
{
    size_t i = p.find_last_of(L"\\/");
    if (i == std::wstring::npos) return p;
    return p.substr(i + 1);
}


std::wstring PathExtLower(const std::wstring& p)
{
    std::wstring base = PathBaseName(p);
    size_t i = base.find_last_of(L'.');
    if (i == std::wstring::npos) return L"";
    return ToLower(base.substr(i + 1));
}


bool IsUnderDir(const std::wstring& path, const std::wstring& dir)
{
    if (path.empty() || dir.empty()) return false;
    size_t n = dir.size();
    if (path.size() < n) return false;
    if (_wcsnicmp(path.c_str(), dir.c_str(), n) != 0) return false;
    if (path.size() == n) return true;
    wchar_t ch = path[n];
    return ch == L'\\' || ch == L'/';
}

std::wstring PathJoin(const std::wstring& a, const std::wstring& b)
{
    if (a.empty()) return b;
    if (b.empty()) return a;
    if (IsAbsPath(b)) return b;
    wchar_t last = a.back();
    if (last == L'\\' || last == L'/') return a + b;
    return a + L"\\" + b;
}


std::wstring ExeDir()
{
    static std::wstring cached;
    if (!cached.empty()) return cached;
    wchar_t buf[MAX_PATH * 2];
    DWORD n = GetModuleFileNameW(nullptr, buf, (DWORD)std::size(buf));
    if (n == 0 || n >= std::size(buf)) return L"";
    cached = PathDirName(std::wstring(buf, n));
    return cached;
}


bool FileExistsW(const std::wstring& p)
{
    DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}


bool DirExistsW(const std::wstring& p)
{
    DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}


std::wstring FullPathOf(const std::wstring& p)
{
    wchar_t buf[MAX_PATH * 2];
    DWORD n = GetFullPathNameW(p.c_str(), (DWORD)std::size(buf), buf, nullptr);
    if (n == 0 || n >= std::size(buf)) return p;
    return std::wstring(buf, n);
}


bool EnsureDirectory(const std::wstring& p)
{
    if (p.empty()) return false;
    int r = SHCreateDirectoryExW(nullptr, p.c_str(), nullptr);
    return r == ERROR_SUCCESS || r == ERROR_ALREADY_EXISTS || r == ERROR_FILE_EXISTS;
}


bool IsImageExt(const std::wstring& ext)
{
    static const wchar_t* k[] = { L"png", L"jpg", L"jpeg", L"gif", L"bmp", L"webp", L"tif", L"tiff",
                                  L"ico", L"jfif", L"heic", L"heif", L"avif" };
    for (auto e : k) if (ext == e) return true;
    return false;
}


std::wstring ColorToHex(COLORREF c)
{
    if (c == CLR_NONE) return L"-1";
    return Format(L"%02X%02X%02X", GetRValue(c), GetGValue(c), GetBValue(c));
}


COLORREF HexToColor(const std::wstring& s)
{
    std::wstring t = Trim(s);
    if (t.empty() || t == L"-1") return CLR_NONE;
    wchar_t* end = nullptr;
    unsigned long v = wcstoul(t.c_str(), &end, 16);
    if (end == t.c_str()) return CLR_NONE;
    return RGB((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF);
}


// ---------------------------------------------------------------------------
//  settings (ini next to the exe, portable)
// ---------------------------------------------------------------------------
std::wstring ReadTextUtf8(const std::wstring& path)
{
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return L"";
    LARGE_INTEGER sz = {};
    if (!GetFileSizeEx(h, &sz) || sz.QuadPart <= 0 || sz.QuadPart > 4 * 1024 * 1024) {
        CloseHandle(h);
        return L"";
    }
    std::string data((size_t)sz.QuadPart, '\0');
    DWORD read = 0;
    BOOL ok = ReadFile(h, &data[0], (DWORD)data.size(), &read, nullptr);
    CloseHandle(h);
    if (!ok) return L"";
    data.resize(read);
    if (data.size() >= 3 && (unsigned char)data[0] == 0xEF && (unsigned char)data[1] == 0xBB &&
        (unsigned char)data[2] == 0xBF)
        data.erase(0, 3);
    return Utf8ToWide(data);
}


bool WriteTextUtf8(const std::wstring& path, const std::wstring& text)
{
    std::string data = WideToUtf8(text);
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    BOOL ok = WriteFile(h, data.data(), (DWORD)data.size(), &written, nullptr);
    CloseHandle(h);
    return ok != FALSE;
}


std::wstring IniValue(const std::map<std::wstring, std::wstring>& kv, const wchar_t* key, const wchar_t* def)
{
    auto it = kv.find(key);
    return it == kv.end() ? std::wstring(def) : it->second;
}


int IniInt(const std::map<std::wstring, std::wstring>& kv, const wchar_t* key, int def)
{
    auto it = kv.find(key);
    if (it == kv.end()) return def;
    wchar_t* end = nullptr;
    long v = wcstol(it->second.c_str(), &end, 10);
    if (end == it->second.c_str()) return def;
    return (int)v;
}


void LoadSettings()
{
    std::wstring dir = ExeDir();
    if (dir.empty()) dir = L".";
    g_iniPath = PathJoin(dir, L"settings.ini");

    // One-time portable migration: if the new exe-level file is absent, copy
    // the old per-user settings so existing users keep their theme/geometry.
    if (!FileExistsW(g_iniPath)) {
        wchar_t appdata[MAX_PATH * 2] = { 0 };
        DWORD n = GetEnvironmentVariableW(L"APPDATA", appdata, (DWORD)std::size(appdata));
        if (n > 0 && n < std::size(appdata)) {
            std::wstring oldPath = PathJoin(PathJoin(std::wstring(appdata), L"Excel"), L"settings.ini");
            if (FileExistsW(oldPath)) {
                std::wstring oldText = ReadTextUtf8(oldPath);
                if (!oldText.empty()) WriteTextUtf8(g_iniPath, oldText);
            }
        }
    }

    std::map<std::wstring, std::wstring> kv;
    std::wstring text = ReadTextUtf8(g_iniPath);
    size_t pos = 0;
    while (pos < text.size()) {
        size_t e = text.find(L'\n', pos);
        std::wstring line = (e == std::wstring::npos) ? text.substr(pos) : text.substr(pos, e - pos);
        pos = (e == std::wstring::npos) ? text.size() : e + 1;
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        line = Trim(line);
        if (line.empty() || line[0] == L'[') continue;
        size_t eq = line.find(L'=');
        if (eq == std::wstring::npos) continue;
        std::wstring key = Trim(line.substr(0, eq));
        std::wstring val = Trim(line.substr(eq + 1));
        if (!key.empty()) kv[key] = val;
    }

    std::wstring theme = IniValue(kv, L"theme", L"dark");
    g_dark = (theme != L"light");
    g_uiZoom = IniInt(kv, L"ui_zoom", 100);
    g_gridZoom = IniInt(kv, L"grid_zoom", 100);
    g_autoCopy = IniInt(kv, L"auto_copy", 0) != 0;
    g_rowH = IniInt(kv, L"row_h", 30);
    g_colW = IniInt(kv, L"col_w", 100);
    g_defaultFont = IniValue(kv, L"font", L"微软雅黑");
    g_defaultSize = IniInt(kv, L"size", 12);
    g_btnFg = HexToColor(IniValue(kv, L"color_fg", L"-1"));
    g_btnOutline = HexToColor(IniValue(kv, L"color_outline", IniValue(kv, L"color_textbg", L"-1").c_str()));
    g_btnCellBg = HexToColor(IniValue(kv, L"color_cellbg", L"-1"));
    g_winX = IniInt(kv, L"win_x", -1);
    g_winY = IniInt(kv, L"win_y", -1);
    g_winW = IniInt(kv, L"win_w", 1220);
    g_winH = IniInt(kv, L"win_h", 820);
    g_winMax = IniInt(kv, L"win_max", 0) != 0;

    if (g_uiZoom < 80) g_uiZoom = 80;
    if (g_uiZoom > 150) g_uiZoom = 150;
    if (g_gridZoom < 50) g_gridZoom = 50;
    if (g_gridZoom > 250) g_gridZoom = 250;
    if (g_defaultSize < 6) g_defaultSize = 12;
    if (g_defaultSize > 200) g_defaultSize = 200;
    if (g_rowH < 14) g_rowH = 14;
    if (g_rowH > 200) g_rowH = 200;
    if (g_colW < 30) g_colW = 30;
    if (g_colW > 400) g_colW = 400;
    if (g_defaultFont.empty()) g_defaultFont = L"微软雅黑";
    ApplyThemeColors();
}


void SaveSettings()
{
    if (g_iniPath.empty()) return;

    WINDOWPLACEMENT wp = { sizeof(wp) };
    if (g_hwnd && GetWindowPlacement(g_hwnd, &wp)) {
        g_winMax = (wp.showCmd == SW_SHOWMAXIMIZED);
        RECT r = wp.rcNormalPosition;
        g_winX = r.left;
        g_winY = r.top;
        g_winW = r.right - r.left;
        g_winH = r.bottom - r.top;
    }

    std::wstring out;
    out += L"[geek]\n";
    out += L"theme=" + std::wstring(g_dark ? L"dark" : L"light") + L"\n";
    out += L"ui_zoom=" + std::to_wstring(g_uiZoom) + L"\n";
    out += L"grid_zoom=" + std::to_wstring(g_gridZoom) + L"\n";
    out += L"auto_copy=" + std::wstring(g_autoCopy ? L"1" : L"0") + L"\n";
    out += L"row_h=" + std::to_wstring(g_rowH) + L"\n";
    out += L"col_w=" + std::to_wstring(g_colW) + L"\n";
    out += L"font=" + g_defaultFont + L"\n";
    out += L"size=" + std::to_wstring(g_defaultSize) + L"\n";
    out += L"color_fg=" + ColorToHex(g_btnFg) + L"\n";
    out += L"color_outline=" + ColorToHex(g_btnOutline) + L"\n";
    out += L"color_cellbg=" + ColorToHex(g_btnCellBg) + L"\n";
    out += L"win_x=" + std::to_wstring(g_winX) + L"\n";
    out += L"win_y=" + std::to_wstring(g_winY) + L"\n";
    out += L"win_w=" + std::to_wstring(g_winW) + L"\n";
    out += L"win_h=" + std::to_wstring(g_winH) + L"\n";
    out += L"win_max=" + std::wstring(g_winMax ? L"1" : L"0") + L"\n";
    WriteTextUtf8(g_iniPath, out);
}

// ---------------------------------------------------------------------------
//  scaling / fonts / GDI caches
// ---------------------------------------------------------------------------
int DpiScale(int v)
{
    return MulDiv(v, g_dpi * g_uiZoom, 96 * 100);
}


int GridScale(int v)
{
    return MulDiv(v, g_dpi * g_gridZoom, 96 * 100);
}


int PxFromPt(int pt)
{
    int px = MulDiv(pt, g_dpi * g_gridZoom, 72 * 100);
    return px < 6 ? 6 : px;
}


int UiPxFromPt(int pt)
{
    int px = MulDiv(pt, g_dpi * g_uiZoom, 72 * 100);
    return px < 6 ? 6 : px;
}



HFONT FontRaw(const std::wstring& face, int px, int weight, bool italic)
{
    if (px < 1) px = 1;
    std::wstring key = face + L"|" + std::to_wstring(px) + L"|" + std::to_wstring(weight) + (italic ? L"i" : L"n");
    auto it = g_fonts.find(key);
    if (it != g_fonts.end()) return it->second;
    LOGFONTW lf = {};
    lf.lfHeight = -px;
    lf.lfWeight = weight;
    lf.lfItalic = italic ? TRUE : FALSE;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_TT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = CLEARTYPE_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    wcsncpy_s(lf.lfFaceName, face.c_str(), _TRUNCATE);
    HFONT f = CreateFontIndirectW(&lf);
    g_fonts[key] = f;
    return f;
}


HFONT UiFont(int pt, bool bold )
{
    return FontRaw(L"微软雅黑", UiPxFromPt(pt), bold ? FW_SEMIBOLD : FW_NORMAL, false);
}


HFONT UiFontFace(const std::wstring& face, int pt, bool bold )
{
    return FontRaw(face, UiPxFromPt(pt), bold ? FW_SEMIBOLD : FW_NORMAL, false);
}


HFONT MonoFont(int pt, bool bold )
{
    return FontRaw(L"Consolas", UiPxFromPt(pt), bold ? FW_SEMIBOLD : FW_NORMAL, false);
}


HFONT UiFontScaled(int pt, bool bold, int dpi, int zoom)
{
    int px = MulDiv(pt, dpi * zoom, 72 * 100);
    return FontRaw(L"微软雅黑", px < 6 ? 6 : px, bold ? FW_SEMIBOLD : FW_NORMAL, false);
}


HFONT MonoFontScaled(int pt, bool bold, int dpi, int zoom)
{
    int px = MulDiv(pt, dpi * zoom, 72 * 100);
    return FontRaw(L"Consolas", px < 6 ? 6 : px, bold ? FW_SEMIBOLD : FW_NORMAL, false);
}


HFONT CellFont(const std::wstring& face, int pt, bool bold, bool italic)
{
    return FontRaw(face, PxFromPt(pt), bold ? FW_SEMIBOLD : FW_NORMAL, italic);
}


HBRUSH Brush(COLORREF c)
{
    auto it = g_brushes.find(c);
    if (it != g_brushes.end()) return it->second;
    HBRUSH b = CreateSolidBrush(c);
    g_brushes[c] = b;
    return b;
}


HPEN Pen(COLORREF c, int w )
{
    long long key = ((long long)c << 8) | (w & 0xFF);
    auto it = g_pens.find(key);
    if (it != g_pens.end()) return it->second;
    HPEN p = CreatePen(PS_SOLID, w, c);
    g_pens[key] = p;
    return p;
}


void FillRectC(HDC dc, const RECT& rc, COLORREF c)
{
    ::FillRect(dc, &rc, Brush(c));
}


void FrameRectC(HDC dc, RECT rc, COLORREF c, int w )
{
    if (rc.right <= rc.left || rc.bottom <= rc.top) return;
    if (w <= 0) w = 1;
    FillRectC(dc, { rc.left, rc.top, rc.right, rc.top + w }, c);
    FillRectC(dc, { rc.left, rc.bottom - w, rc.right, rc.bottom }, c);
    FillRectC(dc, { rc.left, rc.top, rc.left + w, rc.bottom }, c);
    FillRectC(dc, { rc.right - w, rc.top, rc.right, rc.bottom }, c);
}


void HLine(HDC dc, int x1, int x2, int y, COLORREF c, int w )
{
    FillRectC(dc, { x1, y, x2, y + w }, c);
}


void VLine(HDC dc, int x, int y1, int y2, COLORREF c, int w )
{
    FillRectC(dc, { x, y1, x + w, y2 }, c);
}


void TextIn(HDC dc, const std::wstring& s, RECT rc, COLORREF color, HFONT font, UINT flags)
{
    if (s.empty()) return;
    HGDIOBJ old = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, s.c_str(), (int)s.size(), &rc, flags | DT_NOPREFIX);
    SelectObject(dc, old);
}


SIZE MeasureText(HDC dc, const std::wstring& s, HFONT font)
{
    SIZE sz = { 0, 0 };
    HGDIOBJ old = SelectObject(dc, font);
    GetTextExtentPoint32W(dc, s.c_str(), (int)s.size(), &sz);
    SelectObject(dc, old);
    return sz;
}


// One text geometry for both the renderer and the in-cell editor. DrawText's
// DT_VCENTER centres on the FONT line box (ascent+descent), which puts CJK
// glyphs visibly high. Positioning the rectangle from the visible glyph box
// (line height minus internal leading) makes top / middle / bottom consistent,
// and lets the native EDIT control start its first line at exactly the same y.
RECT CellTextRect(HDC dc, const RECT& inner, HFONT font,
                  const std::wstring& text, int vAlign, int* lineH)
{
    RECT r = inner;
    if (r.right <= r.left || r.bottom <= r.top || text.empty()) return r;

    HGDIOBJ old = SelectObject(dc, font);
    TEXTMETRICW tm = {};
    GetTextMetricsW(dc, &tm);
    int one = tm.tmHeight > 0 ? tm.tmHeight : 1;
    if (lineH) *lineH = one;

    bool multi = text.find_first_of(L"\r\n") != std::wstring::npos;
    UINT measure = DT_CALCRECT | DT_NOPREFIX
                 | (multi ? (DT_WORDBREAK | DT_EDITCONTROL) : DT_SINGLELINE);
    RECT calc = inner;
    DrawTextW(dc, text.c_str(), (int)text.size(), &calc, measure);
    int th = calc.bottom - calc.top;
    if (th < one) th = one;
    SelectObject(dc, old);

    int ih = (int)(inner.bottom - inner.top);
    int lead = tm.tmInternalLeading > 0 ? tm.tmInternalLeading : 0;
    int visH = std::max(1, th - lead);
    int top = inner.top;
    if (vAlign == 1)      top = inner.top + (ih - visH) / 2 - lead;
    else if (vAlign == 2) top = inner.bottom - th;
    if (top < inner.top) top = inner.top;

    r.top = top;
    r.bottom = top + th;
    if (r.bottom > inner.bottom) r.bottom = inner.bottom;
    return r;
}


COLORREF Blend(COLORREF a, COLORREF b, int pctA)
{
    int r = (GetRValue(a) * pctA + GetRValue(b) * (100 - pctA)) / 100;
    int g = (GetGValue(a) * pctA + GetGValue(b) * (100 - pctA)) / 100;
    int bl = (GetBValue(a) * pctA + GetBValue(b) * (100 - pctA)) / 100;
    return RGB(r, g, bl);
}


COLORREF Mix(COLORREF a, COLORREF b, int pctA)
{
    return Blend(a, b, pctA);
}


// menu helpers ---------------------------------------------------------------
void DrawCheckMark(HDC dc, int cx, int cy, COLORREF c)
{
    HPEN p = Pen(c, DpiScale(2));
    HGDIOBJ op = SelectObject(dc, p);
    MoveToEx(dc, cx - DpiScale(4), cy, nullptr);
    LineTo(dc, cx - DpiScale(1), cy + DpiScale(3));
    LineTo(dc, cx + DpiScale(4), cy - DpiScale(4));
    SelectObject(dc, op);
}


void DrawChevron(HDC dc, int x, int y, int size, COLORREF c, bool right)
{
    HPEN p = Pen(c, DpiScale(1));
    HGDIOBJ op = SelectObject(dc, p);
    if (right) {
        MoveToEx(dc, x, y - size, nullptr);
        LineTo(dc, x + size, y);
        LineTo(dc, x, y + size);
    } else {
        MoveToEx(dc, x - size, y, nullptr);
        LineTo(dc, x, y + size);
        LineTo(dc, x + size, y);
    }
    SelectObject(dc, op);
}


void DrawCloseX(HDC dc, int cx, int cy, int r, COLORREF c)
{
    HPEN p = Pen(c, DpiScale(1));
    HGDIOBJ op = SelectObject(dc, p);
    MoveToEx(dc, cx - r, cy - r, nullptr);
    LineTo(dc, cx + r, cy + r);
    MoveToEx(dc, cx + r, cy - r, nullptr);
    LineTo(dc, cx - r, cy + r);
    SelectObject(dc, op);
}


void DrawLockGlyph(HDC dc, int cx, int cy, COLORREF c)
{
    RECT body = { cx - DpiScale(4), cy - DpiScale(1), cx + DpiScale(4), cy + DpiScale(5) };
    FrameRectC(dc, body, c, 1);
    HLine(dc, cx - DpiScale(3), cx + DpiScale(3), cy - DpiScale(3), c, 1);
    VLine(dc, cx - DpiScale(3), cy - DpiScale(3), cy - DpiScale(1), c, 1);
    VLine(dc, cx + DpiScale(3), cy - DpiScale(3), cy - DpiScale(1), c, 1);
    FillRectC(dc, { cx - 1, cy + DpiScale(1), cx + 1, cy + DpiScale(3) }, c);
}


void DrawTriangleDown(HDC dc, int cx, int cy, int size, COLORREF c)
{
    POINT pts[3] = { { cx - size, cy - size / 2 }, { cx + size, cy - size / 2 }, { cx, cy + size / 2 } };
    HBRUSH b = Brush(c);
    HGDIOBJ ob = SelectObject(dc, b);
    HGDIOBJ op = SelectObject(dc, GetStockObject(NULL_PEN));
    Polygon(dc, pts, 3);
    SelectObject(dc, ob);
    SelectObject(dc, op);
}


void DrawMark(HDC dc, int x, int y, int size, COLORREF bg, COLORREF fg)
{
    FillRectC(dc, { x, y, x + size, y + size }, bg);
    FrameRectC(dc, { x, y, x + size, y + size }, fg);
}


