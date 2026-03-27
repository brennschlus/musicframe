#include "state_music_select.h"
#include "../state/state_manager.h"
#include "../fs/music_library.h"
#include "../image/image_frames.h"
#include <stdio.h>
#include <3ds.h>

// ---------------------------------------------------------------------------
// Music Select screen
//
// Top screen:    Preview of photo + filter + frame
// Bottom screen: List of .wav files from sdmc:/3ds/musicframe/music/
// Input:         UP/DOWN = navigate, A = select, B = back
// ---------------------------------------------------------------------------

#define TOP_W 400
#define TOP_H 240
#define VISIBLE_ROWS 10

static MusicLibrary s_library;
static int s_cursor = 0;
static int s_scroll = 0;

static void redraw_list(AppContext* ctx)
{
    (void)ctx;
    consoleClear();
    printf("\x1b[1;1H");
    printf("===========================\n");
    printf("   Music Select\n");
    printf("===========================\n");

    if (s_library.count == 0) {
        printf("\n  No .wav files found!\n");
        printf("\n  Put files in:\n");
        printf("  %s\n", MUSIC_DIR);
        printf("\n  [B] << Back\n");
        return;
    }

    // Scrollable list
    int end = s_scroll + VISIBLE_ROWS;
    if (end > s_library.count) end = s_library.count;

    for (int i = s_scroll; i < end; i++) {
        if (i == s_cursor) {
            printf("  > %s\n", s_library.filenames[i]);
        } else {
            printf("    %s\n", s_library.filenames[i]);
        }
    }

    printf("\n  [A] Select  [B] Back\n");
    printf("  %d/%d\n", s_cursor + 1, s_library.count);
}

// -- State callbacks --------------------------------------------------------

static void music_select_enter(AppState* self, AppContext* ctx)
{
    (void)self;
    s_cursor = 0;
    s_scroll = 0;
    music_library_scan(&s_library);
    redraw_list(ctx);
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
        bool changed = false;

        if (kDown & KEY_DOWN) {
            if (s_cursor < s_library.count - 1) {
                s_cursor++;
                if (s_cursor >= s_scroll + VISIBLE_ROWS) s_scroll++;
                changed = true;
            }
        }
        if (kDown & KEY_UP) {
            if (s_cursor > 0) {
                s_cursor--;
                if (s_cursor < s_scroll) s_scroll--;
                changed = true;
            }
        }

        if (changed) {
            redraw_list(ctx);
        }

        if (kDown & KEY_A) {
            // Store selected music path in scene
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

static void music_select_render_top(AppState* self, AppContext* ctx, C3D_RenderTarget* target)
{
    (void)self;

    u32 clrBg = C2D_Color32(0x1A, 0x1A, 0x2E, 0xFF);
    C2D_TargetClear(target, clrBg);
    C2D_SceneBegin(target);

    if (ctx->scene.photo_loaded && ctx->scene.tex_initialized) {
        float img_w = ctx->scene.subtex.width;
        float img_h = ctx->scene.subtex.height;
        float x = (TOP_W - img_w) / 2.0f;
        float y = (TOP_H - img_h) / 2.0f;

        C2D_Image img;
        img.tex    = &ctx->scene.tex;
        img.subtex = &ctx->scene.subtex;
        C2D_DrawImageAt(img, x, y, 0.5f, NULL, 1.0f, 1.0f);
        image_frame_draw(ctx->scene.selected_frame, x, y, img_w, img_h, 0.6f);
    }

    u32 clrAccent = C2D_Color32(0xE8, 0x6D, 0x50, 0xFF);
    C2D_DrawRectSolid(0, 0, 0, TOP_W, 3, clrAccent);
    C2D_DrawRectSolid(0, TOP_H - 3, 0, TOP_W, 3, clrAccent);
}

static void music_select_render_bottom(AppState* self, AppContext* ctx)
{
    (void)self;
    (void)ctx;
}

// -- Factory ----------------------------------------------------------------

static AppState s_music_select = {
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
