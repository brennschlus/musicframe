#include "state_music_select.h"
#include "../state/state_manager.h"
#include "../fs/music_library.h"
#include "../image/image_frames.h"
#include "../ui/ui_panel.h"
#include "../ui/ui_text.h"
#include <3ds.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// Music Select screen
//
// Top screen:    Preview of photo + filter + frame
// Bottom screen: List of .wav files rendered with citro2d
// Input:         UP/DOWN = navigate, A = select, B = back
// ---------------------------------------------------------------------------

#define VISIBLE_ROWS 8

static MusicLibrary s_library;
static int s_cursor = 0;
static int s_scroll = 0;

static void music_select_enter(AppState* self, AppContext* ctx)
{
    (void)self;
    (void)ctx;
    s_cursor = 0;
    s_scroll = 0;
    music_library_scan(&s_library);
}

static void music_select_exit(AppState* self, AppContext* ctx)
{
    (void)self;
    (void)ctx;
}

static void music_select_update(AppState* self, AppContext* ctx)
{
    (void)self;

    u32 kDown = hidKeysDown();

    if (s_library.count > 0) {
        if (kDown & KEY_DOWN) {
            if (s_cursor < s_library.count - 1) {
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
            music_library_get_path(&s_library, s_cursor,
                                   ctx->scene.music_path,
                                   sizeof(ctx->scene.music_path));
            ctx->scene.music_selected = true;
            state_manager_transition(ctx, STATE_PLAYBACK_VIEW);
        }
    }

    if (kDown & KEY_B) {
        state_manager_transition(ctx, STATE_FRAME_SELECT);
    }
}

static void music_select_render_top(AppState* self, AppContext* ctx)
{
    (void)self;

    C3D_RenderTarget* target = ctx->top_target;

    u32 clrBg = C2D_Color32(0x1A, 0x1A, 0x2E, 0xFF);
    C2D_TargetClear(target, clrBg);
    C2D_SceneBegin(target);

    if (ctx->scene.photo_loaded && ctx->scene.texture.initialized) {
        scene_model_draw_photo_centered(&ctx->scene, 0.5f);
        image_frame_draw(ctx->scene.selected_frame, 0.0f, 0.0f, TOP_SCREEN_W, TOP_SCREEN_H, 0.6f);
    }
}

static void music_select_render_bottom(AppState* self, AppContext* ctx)
{
    (void)self;
    (void)ctx;

    ui_panel_bg_dark();
    ui_panel_title("Music Select");

    if (s_library.count == 0) {
        ui_draw_centered(BOTTOM_W * 0.5f, 70.0f, 0.0f, 0.55f, ui_color_accent(),
                         "No .wav files found");
        ui_draw_centered(BOTTOM_W * 0.5f, 100.0f, 0.0f, 0.40f, ui_color_dim(),
                         "Put files in:");
        ui_draw_centered(BOTTOM_W * 0.5f, 116.0f, 0.0f, 0.40f, ui_color_text(),
                         "%s", MUSIC_DIR);
        ui_panel_footer_hint("[B] Back");
        return;
    }

    // Scrollable list
    int end = s_scroll + VISIBLE_ROWS;
    if (end > s_library.count) end = s_library.count;

    for (int i = s_scroll; i < end; i++) {
        ui_panel_list_row(i - s_scroll, i == s_cursor,
                          s_library.filenames[i]);
    }

    // Scroll hint (up / down arrows when clipped)
    if (s_scroll > 0) {
        ui_draw(BOTTOM_W - 18.0f, ui_panel_content_top() + 2.0f,
                0.0f, 0.45f, ui_color_gold(), "^");
    }
    if (end < s_library.count) {
        float last_y = ui_panel_content_top() + VISIBLE_ROWS * ui_panel_row_h() - 12.0f;
        ui_draw(BOTTOM_W - 18.0f, last_y, 0.0f, 0.45f, ui_color_gold(), "v");
    }

    char hint[64];
    snprintf(hint, sizeof(hint), "[A] Select   [B] Back     %d/%d",
             s_cursor + 1, s_library.count);
    ui_panel_footer_hint(hint);
}

// -- Factory ----------------------------------------------------------------

static AppState s_music_select = {
    .uses_direct_framebuffer = false,
    .enter         = music_select_enter,
    .exit          = music_select_exit,
    .update        = music_select_update,
    .render_top    = music_select_render_top,
    .render_bottom = music_select_render_bottom,
};

AppState* state_music_select_create(void)
{
    return &s_music_select;
}
