#include "state_moment_browser.h"
#include "../fs/moment_store.h"
#include "../image/image_filters.h"
#include "../image/image_frames.h"
#include "../state/state_manager.h"
#include "../state/transitions.h"
#include "../ui/ui_panel.h"
#include "../ui/ui_text.h"
#include <3ds.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// Moment Browser screen
//
// Top screen:    Decorative background (no photo preview — nothing loaded yet)
// Bottom screen: Scrollable list of saved .moment files
// Input:         UP/DOWN = navigate, A = load, B = back to main menu
// ---------------------------------------------------------------------------

#define VISIBLE_ROWS 7

static MomentInfo s_moments[MAX_MOMENTS];
static int  s_count       = 0;
static int  s_cursor      = 0;
static int  s_scroll      = 0;
static bool s_load_failed = false;

static void moment_browser_enter(AppState *self, AppContext *ctx)
{
    (void)self;
    (void)ctx;
    s_cursor      = 0;
    s_scroll      = 0;
    s_load_failed = false;
    s_count       = moment_store_list(s_moments, MAX_MOMENTS);
}

static void moment_browser_exit(AppState *self, AppContext *ctx)
{
    (void)self;
    (void)ctx;
}

static void moment_browser_update(AppState *self, AppContext *ctx)
{
    (void)self;

    u32 kDown = hidKeysDown();

    s_load_failed = false; // clear error each frame so it shows only briefly

    if (s_count > 0) {
        if (kDown & KEY_DOWN) {
            if (s_cursor < s_count - 1) {
                s_cursor++;
                if (s_cursor >= s_scroll + VISIBLE_ROWS) s_scroll++;
            }
        }
        if (kDown & KEY_UP) {
            if (s_cursor > 0) {
                s_cursor--;
                if (s_cursor < s_scroll) s_scroll--;
            }
        }

        if (kDown & KEY_A) {
            int result = moment_store_load(s_moments[s_cursor].filename, &ctx->scene);
            if (result == -1) {
                s_load_failed = true;
            } else {
                // result == 0 (ok) or -2 (music missing, scene still loaded)
                state_manager_transition(
                    ctx,
                    app_next_state(ctx->current_state, TRIGGER_MOMENT_SELECTED));
            }
        }
    }

    if (kDown & KEY_B)
        state_manager_transition(ctx, app_next_state(ctx->current_state, TRIGGER_KEY_B));
}

static void moment_browser_render_top(AppState *self, AppContext *ctx)
{
    (void)self;
    (void)ctx;

    C3D_RenderTarget *target = ctx->top_target;

    u32 clrBg     = C2D_Color32(0x2D, 0x1B, 0x3D, 0xFF); // deep purple
    u32 clrAccent = C2D_Color32(0xE8, 0x6D, 0x50, 0xFF); // coral
    u32 clrGold   = C2D_Color32(0xF2, 0xC1, 0x4E, 0xFF); // gold

    C2D_TargetClear(target, clrBg);
    C2D_SceneBegin(target);

    C2D_DrawRectSolid(0, 0, 0, TOP_SCREEN_W, 4, clrAccent);
    C2D_DrawRectSolid(0, TOP_SCREEN_H - 4, 0, TOP_SCREEN_W, 4, clrAccent);

    // Central decorative "film strip" hint
    float cx = TOP_SCREEN_W / 2.0f;
    float cy = TOP_SCREEN_H / 2.0f;
    C2D_DrawRectSolid(cx - 84, cy - 64, 0, 168, 128, clrGold);
    u32 clrInner = C2D_Color32(0x1A, 0x0F, 0x25, 0xFF);
    C2D_DrawRectSolid(cx - 80, cy - 60, 0, 160, 120, clrInner);

    // Three small photo placeholders
    u32 clrSlot = C2D_Color32(0x5B, 0x3A, 0x6E, 0xFF);
    C2D_DrawRectSolid(cx - 70, cy - 50, 0, 44, 44, clrSlot);
    C2D_DrawRectSolid(cx - 22, cy - 50, 0, 44, 44, clrSlot);
    C2D_DrawRectSolid(cx + 26, cy - 50, 0, 44, 44, clrSlot);

    ui_draw_centered(cx, cy + 20.0f, 0.0f, 0.55f, clrGold, "My Moments");
}

static void moment_browser_render_bottom(AppState *self, AppContext *ctx)
{
    (void)self;
    (void)ctx;

    ui_panel_bg_dark();
    ui_panel_title("My Moments");

    if (s_load_failed) {
        ui_draw_centered(BOTTOM_W * 0.5f, ui_panel_content_top() + 8.0f,
                         0.0f, 0.45f, ui_color_accent(), "Load failed!");
    }

    if (s_count == 0) {
        ui_draw_centered(BOTTOM_W * 0.5f, 80.0f, 0.0f, 0.55f,
                         ui_color_accent(), "No saved moments");
        ui_draw_centered(BOTTOM_W * 0.5f, 108.0f, 0.0f, 0.40f,
                         ui_color_dim(), "Save a scene from Playback View");
        ui_draw_centered(BOTTOM_W * 0.5f, 124.0f, 0.0f, 0.40f,
                         ui_color_dim(), "using [SELECT]");
        ui_panel_footer_hint("[B] Back");
        return;
    }

    // Scrollable list
    int end = s_scroll + VISIBLE_ROWS;
    if (end > s_count) end = s_count;

    for (int i = s_scroll; i < end; i++) {
        MomentInfo *m = &s_moments[i];

        char row[128];
        if (m->music_name[0] != '\0') {
            snprintf(row, sizeof(row), "%s | %s | %s",
                     image_filter_name(m->filter_id),
                     image_frame_name(m->frame_id),
                     m->music_name);
        } else {
            snprintf(row, sizeof(row), "%s | %s | (no track)",
                     image_filter_name(m->filter_id),
                     image_frame_name(m->frame_id));
        }

        ui_panel_list_row(i - s_scroll, i == s_cursor, row);
    }

    // Scroll arrows
    if (s_scroll > 0) {
        ui_draw(BOTTOM_W - 18.0f, ui_panel_content_top() + 2.0f,
                0.0f, 0.45f, ui_color_gold(), "^");
    }
    if (end < s_count) {
        float last_y = ui_panel_content_top() + VISIBLE_ROWS * ui_panel_row_h() - 12.0f;
        ui_draw(BOTTOM_W - 18.0f, last_y, 0.0f, 0.45f, ui_color_gold(), "v");
    }

    char hint[64];
    snprintf(hint, sizeof(hint), "[A] Load   [B] Back   %d/%d",
             s_cursor + 1, s_count);
    ui_panel_footer_hint(hint);
}

// -- Factory ----------------------------------------------------------------

static AppState s_moment_browser = {
    .uses_direct_framebuffer = false,
    .enter         = moment_browser_enter,
    .exit          = moment_browser_exit,
    .update        = moment_browser_update,
    .render_top    = moment_browser_render_top,
    .render_bottom = moment_browser_render_bottom,
};

AppState *state_moment_browser_create(void) { return &s_moment_browser; }
