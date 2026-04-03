#include "state_camera_preview.h"

#include "../state/state_manager.h"
#include <stdio.h>

static void camera_preview_enter(AppState* self, AppContext* ctx)
{
    (void)self;

    consoleClear();
    printf("\x1b[1;1H");
    printf("===========================\n");
    printf("   Camera Preview\n");
    printf("===========================\n\n");

    if (!ctx->camera.initialized) {
        printf("  [ERROR] Camera is not initialized\n");
        printf("  [B] Back\n");
        return;
    }

    ctx->camera.preview_active = true;
    ctx->camera.frame_ready = false;

    printf("  [A] Take Photo\n");
    printf("  [B] Cancel / Back\n\n");
    printf("  Camera preview mode\n");
}

static void camera_preview_exit(AppState* self, AppContext* ctx)
{
    (void)self;
    hw_camera_stop(&ctx->camera);
}

static void camera_preview_update(AppState* self, AppContext* ctx)
{
    (void)self;

    u32 kDown = hidKeysDown();

    if (kDown & KEY_B) {
        state_manager_transition(ctx, STATE_MAIN_MENU);
        return;
    }

    if (!ctx->camera.preview_active) {
        return;
    }

    if (kDown & KEY_A) {
        if (!ctx->camera.frame_ready) {
            return;
        }

        ImageBuffer* captured = image_buffer_create(TOP_SCREEN_W, TOP_SCREEN_H);
        if (!captured) {
            return;
        }

        hw_camera_play_shutter(&ctx->camera);
        hw_camera_get_frame_rgba8(&ctx->camera, captured);

        scene_model_set_photo(&ctx->scene, captured);
        state_manager_transition(ctx, STATE_PHOTO_REVIEW);
        return;
    }

    (void)hw_camera_capture_preview_frame(&ctx->camera);
}

static void clear_top_fb(void)
{
    u16 fbWidth, fbHeight;
    u8* fb = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &fbWidth, &fbHeight);
    if (!fb) return;

    memset(fb, 0, fbWidth * fbHeight * 3);
}

static void camera_preview_render_top(AppState* self, AppContext* ctx)
{
    (void)self;

    clear_top_fb();

    if (!ctx->camera.frame_ready) {
        return;
    }

    hw_draw_camera_preview_top(ctx->camera.frame_buffer);
}

static void camera_preview_render_bottom(AppState* self, AppContext* ctx)
{
    (void)self;
    (void)ctx;
}

static AppState s_camera_preview = {
    .uses_direct_framebuffer = true,
    .enter         = camera_preview_enter,
    .exit          = camera_preview_exit,
    .update        = camera_preview_update,
    .render_top    = camera_preview_render_top,
    .render_bottom = camera_preview_render_bottom,
};

AppState* state_camera_preview_create(void)
{
    return &s_camera_preview;
}
