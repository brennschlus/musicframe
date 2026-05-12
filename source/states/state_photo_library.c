#include "state_photo_library.h"
#include "../fs/photo_library.h"
#include "../image/image_jpeg.h"
#include "../state/state_manager.h"
#include "../state/transitions.h"
#include "../ui/ui_panel.h"
#include "../ui/ui_text.h"
#include <3ds.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// Photo Library screen
//
// Top screen:    Decorative background
// Bottom screen: Scrollable list of JPEGs from sdmc:/DCIM/
// Input:         UP/DOWN = navigate, A = load photo, B = back
// ---------------------------------------------------------------------------

#define VISIBLE_ROWS 7

static PhotoLibrary s_lib;
static int  s_cursor      = 0;
static int  s_scroll      = 0;
static bool s_load_failed = false;

static void photo_library_enter(AppState *self, AppContext *ctx)
{
    (void)self;
    (void)ctx;
    s_cursor      = 0;
    s_scroll      = 0;
    s_load_failed = false;
    photo_library_scan(&s_lib);
}

static void photo_library_exit(AppState *self, AppContext *ctx)
{
    (void)self;
    (void)ctx;
}

static void photo_library_update(AppState *self, AppContext *ctx)
{
    (void)self;

    u32 kDown = hidKeysDown();

    s_load_failed = false;

    if (s_lib.count > 0) {
        if (kDown & KEY_DOWN) {
            if (s_cursor < s_lib.count - 1) {
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
            ImageBuffer *img = image_jpeg_load(
                s_lib.entries[s_cursor].path, TOP_SCREEN_W, TOP_SCREEN_H);
            if (!img) {
                s_load_failed = true;
            } else {
                scene_model_set_photo(&ctx->scene, img);
                state_manager_transition(
                    ctx,
                    app_next_state(ctx->current_state, TRIGGER_PHOTO_SELECTED));
            }
        }
    }

    if (kDown & KEY_B)
        state_manager_transition(ctx, app_next_state(ctx->current_state, TRIGGER_KEY_B));
}

static void photo_library_render_top(AppState *self, AppContext *ctx)
{
    (void)self;
    (void)ctx;

    C3D_RenderTarget *target = ctx->top_target;

    u32 clrBg     = C2D_Color32(0x2D, 0x1B, 0x3D, 0xFF);
    u32 clrAccent = C2D_Color32(0xE8, 0x6D, 0x50, 0xFF);
    u32 clrGold   = C2D_Color32(0xF2, 0xC1, 0x4E, 0xFF);

    C2D_TargetClear(target, clrBg);
    C2D_SceneBegin(target);

    C2D_DrawRectSolid(0, 0, 0, TOP_SCREEN_W, 4, clrAccent);
    C2D_DrawRectSolid(0, TOP_SCREEN_H - 4, 0, TOP_SCREEN_W, 4, clrAccent);

    // Three overlapping photo-frame rectangles as decoration
    float cx = TOP_SCREEN_W / 2.0f;
    float cy = TOP_SCREEN_H / 2.0f;
    u32 clrInner = C2D_Color32(0x1A, 0x0F, 0x25, 0xFF);
    u32 clrSlot  = C2D_Color32(0x5B, 0x3A, 0x6E, 0xFF);

    C2D_DrawRectSolid(cx - 90, cy - 52, 0, 72, 60, clrGold);
    C2D_DrawRectSolid(cx - 86, cy - 48, 0, 64, 52, clrSlot);

    C2D_DrawRectSolid(cx - 36, cy - 56, 0, 72, 60, clrGold);
    C2D_DrawRectSolid(cx - 32, cy - 52, 0, 64, 52, clrInner);

    C2D_DrawRectSolid(cx + 18, cy - 50, 0, 72, 60, clrGold);
    C2D_DrawRectSolid(cx + 22, cy - 46, 0, 64, 52, clrSlot);

    ui_draw_centered(cx, cy + 24.0f, 0.0f, 0.55f, clrGold, "Photo Library");
}

static void photo_library_render_bottom(AppState *self, AppContext *ctx)
{
    (void)self;
    (void)ctx;

    ui_panel_bg_dark();
    ui_panel_title("Photo Library");

    if (s_load_failed) {
        ui_draw_centered(BOTTOM_W * 0.5f, ui_panel_content_top() + 6.0f,
                         0.0f, 0.45f, ui_color_accent(), "Failed to load photo");
    }

    if (s_lib.count == 0) {
        ui_draw_centered(BOTTOM_W * 0.5f, 80.0f, 0.0f, 0.55f,
                         ui_color_accent(), "No photos found");
        ui_draw_centered(BOTTOM_W * 0.5f, 108.0f, 0.0f, 0.40f,
                         ui_color_dim(), "Take photos with the 3DS Camera app");
        ui_draw_centered(BOTTOM_W * 0.5f, 124.0f, 0.0f, 0.40f,
                         ui_color_dim(), "They are saved in sdmc:/DCIM/");
        ui_panel_footer_hint("[B] Back");
        return;
    }

    // Scrollable list
    int end = s_scroll + VISIBLE_ROWS;
    if (end > s_lib.count) end = s_lib.count;

    for (int i = s_scroll; i < end; i++) {
        ui_panel_list_row(i - s_scroll, i == s_cursor, s_lib.entries[i].display);
    }

    if (s_scroll > 0)
        ui_draw(BOTTOM_W - 18.0f, ui_panel_content_top() + 2.0f,
                0.0f, 0.45f, ui_color_gold(), "^");
    if (end < s_lib.count) {
        float last_y = ui_panel_content_top() + VISIBLE_ROWS * ui_panel_row_h() - 12.0f;
        ui_draw(BOTTOM_W - 18.0f, last_y, 0.0f, 0.45f, ui_color_gold(), "v");
    }

    char hint[64];
    snprintf(hint, sizeof(hint), "[A] Load   [B] Back   %d/%d",
             s_cursor + 1, s_lib.count);
    ui_panel_footer_hint(hint);
}

// -- Factory ----------------------------------------------------------------

static AppState s_photo_library = {
    .uses_direct_framebuffer = false,
    .enter         = photo_library_enter,
    .exit          = photo_library_exit,
    .update        = photo_library_update,
    .render_top    = photo_library_render_top,
    .render_bottom = photo_library_render_bottom,
};

AppState *state_photo_library_create(void) { return &s_photo_library; }
