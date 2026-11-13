// ============================================================================
//  p3_formula.cpp
//  Responsibility: formula engine
//  Formula evaluation, style application, reference shifting.
//
//  Shared contract: core.h + only the api_*.h headers this module references.
//  This translation unit contains no type or state definitions of its own (state.cpp does).
// ============================================================================
#include "core.h"
#include "api_p1_core.h"
#include "api_p3_formula.h"
#include "api_p6b_grid_input.h"
// ---- src\p3_formula.inc ----
// ============================================================================
//  Excel - part 3: formula engine + style application
// ============================================================================




std::wstring EvalFormula(Doc& doc, const std::wstring& formula, int depth )
{
    if (formula.empty()) return L"";
    if (depth == 0) g_evalBudget = 5000;
    FormulaEval ev(doc, depth);
    FVal v = ev.EvalString(formula);
    return v.AsString();
}


// Truncate a numeric string to `digits` decimal places, trimming trailing
// zeros. String surgery (not scaling by 100) keeps 1.15 from becoming 1.14
// through binary rounding. Non-numeric strings are never passed in.
static std::wstring TruncateDecimals(const std::wstring& s, int digits)
{
    size_t dot = s.find(L'.');
    if (dot == std::wstring::npos) return s;
    if ((int)(s.size() - dot - 1) <= digits) return s;
    std::wstring r = s.substr(0, dot + digits + 1);
    while (!r.empty() && r.back() == L'0') r.pop_back();
    if (!r.empty() && r.back() == L'.') r.pop_back();
    if (r == L"-0") r = L"0";
    return r;
}


std::wstring CellDisplay(Doc& doc, int r, int c)
{
    Cell* cell = doc.Find(r, c);
    if (!cell) return L"";
    if (cell->displayRev == g_contentRev) return cell->display;   // O(1) hit
    std::wstring value;
    if (!cell->formula.empty()) value = EvalFormula(doc, cell->formula, 0);
    else if (!cell->text.empty()) value = cell->text;
    if (g_numMode != 0) {
        double num;
        if (ParseNum(value, num))
            value = TruncateDecimals(value, g_numMode == 1 ? 2 : 0);
    }
    value += cell->unit;
    cell->display = value;
    cell->displayRev = g_contentRev;
    return value;
}


// ---------------------------------------------------------------------------
//  style application
// ---------------------------------------------------------------------------
void ApplyStyleToSelection(Doc& doc, const std::function<void(CellStyle&)>& fn)
{
    int r1, c1, r2, c2;
    doc.SelectionRect(r1, c1, r2, c2);
    std::vector<MergeRange> ranges;
    ranges.push_back({ r1, c1, r2, c2 });
    for (auto& e : doc.selExtra) ranges.push_back(e);
    long long count = 0;
    for (auto& e : ranges) count += (long long)(e.r2 - e.r1 + 1) * (e.c2 - e.c1 + 1);
    if (count <= 0 || count > 200000) return;
    doc.PushUndo();
    for (auto& e : ranges) {
        for (int r = e.r1; r <= e.r2; ++r) {
            for (int c = e.c1; c <= e.c2; ++c) {
                Cell& cell = doc.Ensure(r, c);
                fn(cell.st);
            }
        }
    }
    doc.Touch();
}


void ApplyColorToSelection(int target, COLORREF color)
{
    Doc* doc = ActiveDoc();
    if (!doc) return;
    int value = (color == CLR_NONE) ? -1 : (int)color;
    ApplyStyleToSelection(*doc, [target, value](CellStyle& st) {
        if (target == 0) st.fg = value;
        else if (target == 1) st.outline = value;
        else st.cellBg = value;
    });
}


void ApplyFontToSelection(const std::wstring& face)
{
    Doc* doc = ActiveDoc();
    if (!doc) return;
    ApplyStyleToSelection(*doc, [face](CellStyle& st) { st.font = face; });
}


void ApplySizeToSelection(int pt)
{
    Doc* doc = ActiveDoc();
    if (!doc) return;
    ApplyStyleToSelection(*doc, [pt](CellStyle& st) { st.size = pt; });
}



void ApplyAlignToSelection(int align)
{
    Doc* doc = ActiveDoc();
    if (!doc) return;
    ApplyStyleToSelection(*doc, [align](CellStyle& st) {
        int v = EffectiveVAlign(st);
        st.align = align | (v << 2) | 16;
    });
}


void ApplyVAlignToSelection(int valign)
{
    Doc* doc = ActiveDoc();
    if (!doc) return;
    ApplyStyleToSelection(*doc, [valign](CellStyle& st) {
        int h = st.align < 0 ? 0 : st.align & 3;
        st.align = h | (valign << 2) | 16;
    });
}


// effective style helpers
std::wstring EffectiveFont(const CellStyle& st)
{
    return st.font.empty() ? g_defaultFont : st.font;
}

int EffectiveSize(const CellStyle& st)
{
    return st.size > 0 ? st.size : g_defaultSize;
}

int EffectiveHAlign(const CellStyle& st)
{
    return st.align >= 0 ? (st.align & 3) : 0;
}


int EffectiveVAlign(const CellStyle& st)
{
    if (st.align < 0 || (st.align & 16) == 0) return 1; // legacy/default: vertical middle
    return (st.align >> 2) & 3;
}

// ---------------------------------------------------------------------------
//  formula reference shifting on row/column insert
// ---------------------------------------------------------------------------
std::wstring ColNameOf(int c)
{
    std::wstring name;
    int n = c;
    do {
        name.insert(name.begin(), (wchar_t)(L'A' + (n % 26)));
        n = n / 26 - 1;
    } while (n >= 0);
    return name;
}


bool TryRefAt(const std::wstring& s, size_t i, int& row, int& col, bool& absCol, bool& absRow, size_t& end)
{
    size_t p = i;
    absCol = false;
    absRow = false;
    if (p < s.size() && s[p] == L'$') { absCol = true; ++p; }
    int letters = 0;
    long c = 0;
    while (p < s.size() && ((s[p] >= L'A' && s[p] <= L'Z') || (s[p] >= L'a' && s[p] <= L'z'))) {
        c = c * 26 + (towupper(s[p]) - L'A' + 1);
        ++letters;
        ++p;
        if (letters > 3) { end = i; return false; }
    }
    if (letters == 0) { end = i; return false; }
    if (p < s.size() && s[p] == L'$') { absRow = true; ++p; }
    int digits = 0;
    long r = 0;
    while (p < s.size() && s[p] >= L'0' && s[p] <= L'9') {
        r = r * 10 + (s[p] - L'0');
        ++digits;
        ++p;
        if (digits > 7) { end = i; return false; }
    }
    if (digits == 0 || r <= 0) { end = i; return false; }
    if (p < s.size() && (iswalnum(s[p]) || s[p] == L'_')) { end = i; return false; }
    row = (int)r - 1;
    col = (int)c - 1;
    end = p;
    return true;
}


std::wstring ShiftFormulaRefs(const std::wstring& f, bool rowInsert, int at, int count)
{
    std::wstring out;
    out.reserve(f.size() + 8);
    size_t i = 0;
    while (i < f.size()) {
        wchar_t ch = f[i];
        if (ch == L'"') {
            out.push_back(ch);
            ++i;
            while (i < f.size()) {
                out.push_back(f[i]);
                if (f[i] == L'"') {
                    if (i + 1 < f.size() && f[i + 1] == L'"') {
                        out.push_back(f[i + 1]);
                        i += 2;
                        continue;
                    }
                    ++i;
                    break;
                }
                ++i;
            }
            continue;
        }
        if (ch == L'$' || iswalpha(ch)) {
            int row = 0, col = 0;
            bool ac = false, ar = false;
            size_t end = i;
            if (TryRefAt(f, i, row, col, ac, ar, end)) {
                if (rowInsert) {
                    if (row >= at) row += count;
                } else {
                    if (col >= at) col += count;
                }
                if (row >= 0 && col >= 0) {
                    if (ac) out.push_back(L'$');
                    out += ColNameOf(col);
                    if (ar) out.push_back(L'$');
                    out += std::to_wstring(row + 1);
                }
                i = end;
                continue;
            }
        }
        out.push_back(ch);
        ++i;
    }
    return out;
}


void AdjustFormulasOnInsert(Doc& d, bool rowInsert, int at, int count)
{
    for (auto& kv : d.cells) {
        if (kv.second.formula.empty()) continue;
        kv.second.formula = ShiftFormulaRefs(kv.second.formula, rowInsert, at, count);
    }
}



