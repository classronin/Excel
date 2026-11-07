// ============================================================================
//  state.cpp - single definition site for application-level globals.
//  These are the globals class-inline members reference; declaring them
//  extern in core.h keeps core.h free of state definitions.
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

ActionBus g_actions;
int g_active = -1;
bool      g_autoCopy   = false;
COLORREF  g_btnCellBg  = CLR_NONE;
COLORREF  g_btnFg      = CLR_NONE;
COLORREF  g_btnOutline = CLR_NONE;
int       g_colW       = 100;
uint64_t g_contentRev = 1;
bool      g_dark       = true;
thread_local std::unordered_set<uint64_t> g_evalActive;
thread_local int g_evalBudget;
int       g_gridZoom   = 100;
HWND      g_hwnd = nullptr;
HINSTANCE g_inst = nullptr;
int       g_rowH       = 30;
Theme g_th;
int       g_uiZoom     = 100;