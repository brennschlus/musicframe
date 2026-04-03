#include "state_frame_select.h"
#include "../image/image_filters.h"
#include "../image/image_frames.h"
#include "../state/state_manager.h"
#include <3ds.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// Frame Select screen
//
// Top screen:    Preview of photo + filter + selected frame overlay
// Bottom screen: Frame name + navigation hints
// Input:         L/R = cycle frames, A = confirm, B = back
// ---------------------------------------------------------------------------

static void update_bottom_text(AppContext* ctx)
{
    consoleClear();
    printf("\x1b[1;1H");
    printf("===========================\n");
    printf("   Frame Select\n");
    printf("===========================\n");
    printf("\n");
    printf("  Filter: %s\n", image_filter_name(ctx->scene.selected_filter));
    printf("  Frame:  %s (%d/%d)\n", image_frame_name(ctx->scene.selected_frame),
           ctx->scene.selected_frame + 1, FRAME_COUNT);
    printf("\n");
    printf("  [L/R] Change frame\n");
    printf("  [A]   Confirm >>\n");
    printf("  [B]   << Back to filters\n");
    printf("\n");
    printf("---------------------------\n");
}

static void frame_select_enter(AppState* self, AppContext* ctx)
{
    (void)self;
    update_bottom_text(ctx);
}

static void frame_select_exit(AppState* self, AppContext* ctx)
{
    (void)self;
    (void)ctx;
}

static void frame_select_update(AppState* self, AppContext* ctx)
{
    (void)self;

    u32 kDown = hidKeysDown();
    bool changed = false;

    if (kDown & KEY_R) {
        ctx->scene.selected_frame = (ctx->scene.selected_frame + 1) % FRAME_COUNT;
        changed = true;
    }
    if (kDown & KEY_L) {
        ctx->scene.selected_frame = (ctx->scene.selected_frame + FRAME_COUNT - 1) % FRAME_COUNT;
        changed = true;
    }

    if (changed) {
        update_bottom_text(ctx);
    }

    if (kDown & KEY_A) {
        state_manager_transition(ctx, STATE_MUSIC_SELECT);
    }
    if (kDown & KEY_B) {
        state_manager_transition(ctx, STATE_FILTER_SELECT);
    }
}

static void frame_select_render_top(AppState* self, AppContext* ctx)
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

static void frame_select_render_bottom(AppState* self, AppContext* ctx)
{
    (void)self;
    (void)ctx;
}

// -- Factory ----------------------------------------------------------------

static AppState s_frame_select = {
    .uses_direct_framebuffer = false,
    .enter         = frame_select_enter,
    .exit          = frame_select_exit,
    .update        = frame_select_update,
    .render_top    = frame_select_render_top,
    .render_bottom = frame_select_render_bottom,
};

AppState* state_frame_select_create(void) { return &s_frame_select; }
