#include "state_camera_preview.h"
#include "../image/image_texture.h"
#include "../state/state_manager.h"
#include <stdio.h>

#define TOP_W 400
#define TOP_H 240

static void camera_preview_enter(AppState *self, AppContext *ctx) {
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

  if (!hw_camera_start(&ctx->camera)) {
    printf("  [ERROR] Failed to start preview\n");
    printf("  [B] Back\n");
    return;
  }

  printf("  [A] Take Photo\n");
  printf("  [B] Cancel / Back\n\n");
  printf("  Camera is active\n");
}

static void camera_preview_exit(AppState *self, AppContext *ctx) {
  (void)self;
  if (ctx->camera.preview_active) {
    hw_camera_stop(&ctx->camera);
  }
  // We do NOT free s_preview_img if we transition to PHOTO_REVIEW because we
  // transfer ownership. If we transition to Main Menu (Cancel), we can free it.
  // Actually, state callbacks don't know the destination state!
  // So we'll handle ownership transfer explicitly on KEY_A.
}

static void camera_preview_update(AppState *self, AppContext *ctx) {
  (void)self;

  u32 kDown = hidKeysDown();

  // If cancel
  if (kDown & KEY_B) {
    hw_camera_stop(&ctx->camera);
    state_manager_transition(ctx, STATE_MAIN_MENU);
    return;
  }

  if (!ctx->camera.preview_active) {
    return; // camera failed to start, just wait for B
  }

  (void)hw_camera_poll_frame(&ctx->camera);

  // Take photo
  if (kDown & KEY_A) {
    if (!ctx->camera.frame_ready) {
      return;
    }

    ImageBuffer *captured = image_buffer_create(TOP_W, TOP_H);
    if (!captured) {
      return;
    }

    hw_camera_play_shutter(&ctx->camera);
    hw_camera_stop(&ctx->camera);
    //TODO: delete this after test
    // hw_camera_get_frame_rgba8(&ctx->camera, captured);
    // scene_model_set_photo(&ctx->scene, captured);
    // state_manager_transition(ctx, STATE_PHOTO_REVIEW);
    return;
  }
}

static void camera_preview_render_top(AppState *self, AppContext *ctx,
                                      C3D_RenderTarget *target) {
  (void)self;
  (void)target;

  if (!ctx->camera.preview_active || !ctx->camera.frame_ready) {
    return;
  }

  hw_draw_camera_preview_top(ctx->camera.frame_buffer);
}

static void camera_preview_render_bottom(AppState *self, AppContext *ctx) {
  (void)self;
  (void)ctx;
}

// ---------------------------------------------------------------------------
static AppState s_camera_preview = {
    .enter = camera_preview_enter,
    .exit = camera_preview_exit,
    .update = camera_preview_update,
    .render_top = camera_preview_render_top,
    .render_bottom = camera_preview_render_bottom,
};

AppState *state_camera_preview_create(void) { return &s_camera_preview; }
