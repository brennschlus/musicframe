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

#define TOP_W 400
#define TOP_H 240

static void update_bottom_text(AppContext *ctx) {
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

// -- State callbacks --------------------------------------------------------

static void frame_select_enter(AppState *self, AppContext *ctx) {
  (void)self;
  update_bottom_text(ctx);
}

static void frame_select_exit(AppState *self, AppContext *ctx) {
  (void)self;
  (void)ctx;
}

static void frame_select_update(AppState *self, AppContext *ctx) {
  (void)self;

  u32 kDown = hidKeysDown();
  bool changed = false;

  if (kDown & KEY_R) {
    ctx->scene.selected_frame = (ctx->scene.selected_frame + 1) % FRAME_COUNT;
    changed = true;
  }
  if (kDown & KEY_L) {
    ctx->scene.selected_frame =
        (ctx->scene.selected_frame + FRAME_COUNT - 1) % FRAME_COUNT;
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

static void frame_select_render_top(AppState *self, AppContext *ctx,
                                    C3D_RenderTarget *target) {
  (void)self;

  u32 clrBg = C2D_Color32(0x1A, 0x1A, 0x2E, 0xFF);
  C2D_TargetClear(target, clrBg);
  C2D_SceneBegin(target);

  if (ctx->scene.photo_loaded && ctx->scene.tex_initialized) {
    float img_w = ctx->scene.subtex.width;
    float img_h = ctx->scene.subtex.height;
    float x = (TOP_W - img_w) / 2.0f;
    float y = (TOP_H - img_h) / 2.0f;

    // Draw photo
    C2D_Image img;
    img.tex = &ctx->scene.tex;
    img.subtex = &ctx->scene.subtex;
    C2D_DrawImageAt(img, x, y, 0.5f, NULL, 1.0f, 1.0f);
    // Draw frame overlay on top
    image_frame_draw(ctx->scene.selected_frame, 0.0f, 0.0f, TOP_W, TOP_H, 0.6f);
  }
}

static void frame_select_render_bottom(AppState *self, AppContext *ctx) {
  (void)self;
  (void)ctx;
}

// -- Factory ----------------------------------------------------------------

static AppState s_frame_select = {
    .enter = frame_select_enter,
    .exit = frame_select_exit,
    .update = frame_select_update,
    .render_top = frame_select_render_top,
    .render_bottom = frame_select_render_bottom,
};

AppState *state_frame_select_create(void) { return &s_frame_select; }
