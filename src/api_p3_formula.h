// ============================================================================
//  api_p3_formula.h - module contract: state + interfaces of p3_formula.
//  Included only by translation units that reference this module.
// ============================================================================
#pragma once
#include "core.h"

std::wstring EvalFormula(Doc& doc, const std::wstring& formula, int depth = 0);
void ApplyStyleToSelection(Doc& doc, const std::function<void(CellStyle&)>& fn);
void ApplyFontToSelection(const std::wstring& face);
void ApplySizeToSelection(int pt);
int EffectiveVAlign(const CellStyle& st);
void ApplyAlignToSelection(int align);
void ApplyVAlignToSelection(int valign);
std::wstring EffectiveFont(const CellStyle& st);
int EffectiveSize(const CellStyle& st);
int EffectiveHAlign(const CellStyle& st);
int EffectiveVAlign(const CellStyle& st);
std::wstring ColNameOf(int c);
bool TryRefAt(const std::wstring& s, size_t i, int& row, int& col,                      bool& absCol, bool& absRow, size_t& end);
std::wstring ShiftFormulaRefs(const std::wstring& f, bool rowInsert, int at, int count);
void AdjustFormulasOnInsert(Doc& d, bool rowInsert, int at, int count);
