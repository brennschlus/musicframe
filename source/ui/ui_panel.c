#include "ui_panel.h"
#include "ui_text.h"

// ---------------------------------------------------------------------------
// Palette — kept in sync with the top-screen palette in state_main_menu.c
// (deep purple / coral / gold).
// ---------------------------------------------------------------------------

u32 ui_color_bg(void)        { return C2D_Color32(0x2D, 0x1B, 0x3D, 0xFF); }
u32 ui_color_panel(void)     { return C2D_Color32(0x1A, 0x0F, 0x25, 0xFF); }
u32 ui_color_title(void)     { return C2D_Color32(0xF2, 0xC1, 0x4E, 0xFF); }
u32 ui_color_accent(void)    { return C2D_Color32(0xE8, 0x6D, 0x50, 0xFF); }
u32 ui_color_gold(void)      { return C2D_Color32(0xF2, 0xC1, 0x4E, 0xFF); }
u32 ui_color_text(void)      { return C2D_Color32(0xF5, 0xF0, 0xE8, 0xFF); }
u32 ui_color_dim(void)       { return C2D_Color32(0x8A, 0x7E, 0x95, 0xFF); }
u32 ui_color_select_bg(void) { return C2D_Color32(0x5B, 0x3A, 0x6E, 0xFF); }

// ---------------------------------------------------------------------------
#define ROW_H 16.0f

float ui_panel_content_top(void) { return TITLE_BAR_H + 6.0f; }
float ui_panel_row_h(void)       { return ROW_H; }

// ---------------------------------------------------------------------------
void ui_panel_bg_dark(void)
{
    // Background has already been cleared by the caller; draw accent stripes
    // and a subtle decorative band so the bottom screen visually matches the
    // top screen.
    u32 accent = ui_color_accent();
    u32 gold = ui_color_gold();

    // Top accent stripe (right below the title bar)
    C2D_DrawRectSolid(0, TITLE_BAR_H, 0, BOTTOM_W, 2, gold);

    // Decorative side rails
    C2D_DrawRectSolid(0, TITLE_BAR_H, 0, 3, BOTTOM_H - TITLE_BAR_H - FOOTER_BAR_H, accent);
    C2D_DrawRectSolid(BOTTOM_W - 3, TITLE_BAR_H, 0,
                      3, BOTTOM_H - TITLE_BAR_H - FOOTER_BAR_H, accent);

    // Bottom accent stripe (just above the footer bar)
    C2D_DrawRectSolid(0, BOTTOM_H - FOOTER_BAR_H - 2, 0, BOTTOM_W, 2, gold);
}

// ---------------------------------------------------------------------------
void ui_panel_title(const char* title)
{
    u32 bar = ui_color_panel();
    u32 accent = ui_color_accent();

    C2D_DrawRectSolid(0, 0, 0, BOTTOM_W, TITLE_BAR_H, bar);
    // Accent underline
    C2D_DrawRectSolid(0, TITLE_BAR_H - 2, 0, BOTTOM_W, 2, accent);

    if (title && *title) {
        ui_draw_centered(BOTTOM_W * 0.5f, 6.0f, 0.0f, 0.65f,
                         ui_color_title(), "%s", title);
    }
}

// ---------------------------------------------------------------------------
void ui_panel_footer_hint(const char* hint)
{
    u32 bar = ui_color_panel();
    u32 gold = ui_color_gold();

    float top = BOTTOM_H - FOOTER_BAR_H;
    C2D_DrawRectSolid(0, top, 0, BOTTOM_W, FOOTER_BAR_H, bar);
    C2D_DrawRectSolid(0, top, 0, BOTTOM_W, 2, gold);

    if (hint && *hint) {
        ui_draw(8.0f, top + 5.0f, 0.0f, 0.45f, ui_color_text(), "%s", hint);
    }
}

// ---------------------------------------------------------------------------
float ui_panel_list_row(int row_idx, bool selected, const char* text)
{
    float y = ui_panel_content_top() + (float)row_idx * ROW_H;

    if (selected) {
        C2D_DrawRectSolid(6.0f, y, 0, BOTTOM_W - 12.0f, ROW_H - 2.0f,
                          ui_color_select_bg());
        // Caret
        ui_draw(10.0f, y + 1.0f, 0.0f, 0.45f, ui_color_gold(), ">");
        ui_draw(22.0f, y + 1.0f, 0.0f, 0.45f, ui_color_text(), "%s", text);
    } else {
        ui_draw(22.0f, y + 1.0f, 0.0f, 0.45f, ui_color_dim(), "%s", text);
    }

    return y + ROW_H;
}
