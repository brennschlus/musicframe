#ifndef UI_PANEL_H
#define UI_PANEL_H

#include <citro2d.h>

// ---------------------------------------------------------------------------
// ui_panel — common bottom-screen chrome (background, title bar, footer)
// shared across all application states so the bottom screen has a consistent
// visual language with the top screen (purple / coral / gold palette).
// ---------------------------------------------------------------------------

// Screen constants
#define BOTTOM_W      320.0f
#define BOTTOM_H      240.0f
#define TITLE_BAR_H    28.0f
#define FOOTER_BAR_H   22.0f

// Shared palette (ABGR8 — via C2D_Color32 helpers)
u32 ui_color_bg(void);       // deep purple background
u32 ui_color_panel(void);    // slightly lighter panel
u32 ui_color_title(void);    // title text color (gold)
u32 ui_color_accent(void);   // coral accent
u32 ui_color_gold(void);     // gold accent
u32 ui_color_text(void);     // primary text color
u32 ui_color_dim(void);      // dim / secondary text
u32 ui_color_select_bg(void);// highlighted row background

// Render full bottom-screen background including decorative accent stripes.
// Must be called after C2D_TargetClear / C2D_SceneBegin on the bottom target.
void ui_panel_bg_dark(void);

// Render a title bar at the top of the bottom screen with the given title.
void ui_panel_title(const char* title);

// Render a footer bar with a short hint string (e.g. "[A] Select  [B] Back").
void ui_panel_footer_hint(const char* hint);

// Render a single list row at index row_idx (0-based, from the top of the
// list area). If `selected` is true, a highlighted background is drawn.
// Returns the y-coordinate of the next row for convenience.
float ui_panel_list_row(int row_idx, bool selected, const char* text);

// Y-coordinate where the list / content area begins (just below title bar).
float ui_panel_content_top(void);

// Height of one list row.
float ui_panel_row_h(void);

#endif // UI_PANEL_H
