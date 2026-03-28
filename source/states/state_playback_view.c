#include "state_playback_view.h"
#include "../image/image_filters.h"
#include "../image/image_frames.h"
#include "../state/state_manager.h"
#include <3ds.h>
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Playback View screen
//
// Top screen:    Final framed photo (gold frame around the filtered image)
// Bottom screen: Music playback controls placeholder
// Input:         B = back to menu
// ---------------------------------------------------------------------------

#define TOP_W 400
#define TOP_H 240

// -- State callbacks --------------------------------------------------------

static void playback_view_enter(AppState *self, AppContext *ctx) {
  (void)self;
  if (ctx->scene.music_selected) {
    audio_player_load(&ctx->audio, ctx->scene.music_path);
    audio_player_play(&ctx->audio);
  }
  // TODO: do not fortget to delete this code, after moving away from text rendering.
  consoleClear();
}

static void playback_view_exit(AppState *self, AppContext *ctx) {
  (void)self;
  (void)ctx;
}

static void playback_view_update(AppState *self, AppContext *ctx) {
  (void)self;

  u32 kDown = hidKeysDown();

  if (kDown & KEY_A) {
    audio_player_toggle_pause(&ctx->audio);
  }
  if (kDown & KEY_R) {
    audio_player_seek(&ctx->audio, 5);
  }
  if (kDown & KEY_L) {
    audio_player_seek(&ctx->audio, -5);
  }
  if(kDown & KEY_Y){
      audio_player_set_loop(&ctx->audio, !ctx->audio.loop);
  }

  if (kDown & KEY_B) {
    audio_player_stop(&ctx->audio);
    scene_model_cleanup(&ctx->scene);
    state_manager_transition(ctx, STATE_MAIN_MENU);
  }

  if (ctx->audio.loop && audio_player_finished(&ctx->audio)) {
      audio_player_restart(&ctx->audio);
  }

}

static void playback_view_render_top(AppState *self, AppContext *ctx,
                                     C3D_RenderTarget *target) {
  (void)self;

  u32 clrBg = C2D_Color32(0x1C, 0x11, 0x28, 0xFF);
  C2D_TargetClear(target, clrBg);
  C2D_SceneBegin(target);

  if (ctx->scene.photo_loaded && ctx->scene.tex_initialized) {
    float img_w = ctx->scene.subtex.width;
    float img_h = ctx->scene.subtex.height;
    float x = (TOP_W - img_w) / 2.0f;
    float y = (TOP_H - img_h) / 2.0f;

    // Photo
    C2D_Image img;
    img.tex = &ctx->scene.tex;
    img.subtex = &ctx->scene.subtex;
    C2D_DrawImageAt(img, x, y, 0.5f, NULL, 1.0f, 1.0f);

    // Selected frame overlay
    image_frame_draw(ctx->scene.selected_frame, 0.0f, 0.0f, TOP_W, TOP_H, 0.6f);
  }
}

static void playback_view_render_bottom(AppState *self, AppContext *ctx) {
  (void)self;

  printf("\x1b[1;1H");
  printf("===========================\n");
  printf("   Playback View\n");
  printf("===========================\n");
  printf("\n");
  printf("  Filter: %s\n", image_filter_name(ctx->scene.selected_filter));
  printf("  Frame:  %s\n", image_frame_name(ctx->scene.selected_frame));
  printf("  Loop:   %s\n", ctx->audio.loop ? "Yes" : "No ");


  if (ctx->scene.music_selected) {
    if (!ctx->audio.ndsp_ok) {
      printf("\n  [NDSP INIT FAILED]\n");
      printf("  Missing DSP firmware?\n");
    } else {
      const char *name = strrchr(ctx->scene.music_path, '/');
      name = name ? name + 1 : ctx->scene.music_path;
      printf("\n  Now playing: %s\n", name);

      float pos = audio_player_position_sec(&ctx->audio);
      float dur = audio_player_duration_sec(&ctx->audio);
      int p_m = (int)pos / 60, p_s = (int)pos % 60;
      int d_m = (int)dur / 60, d_s = (int)dur % 60;

      printf(
          "  %02d:%02d / %02d:%02d  [%s]\n", p_m, p_s, d_m, d_s,
          ctx->audio.paused
              ? "PAUSED "
              : (audio_player_finished(&ctx->audio) ? "STOPPED" : "PLAYING"));
    }
  } else {
    printf("\n  Now playing: (none)\n");
    printf("  00:00 / 00:00\n");
  }

  printf("\n");
  printf("  [A]     Play / Pause\n");
  printf("  [L/R]   Seek -/+ 5s\n");
  printf("  [B]     Back to menu\n");
  printf("  [Y]     Toggle loop\n");
  printf("\n");
  printf("---------------------------\n");
}

// -- Factory ----------------------------------------------------------------

static AppState s_playback_view = {
    .enter = playback_view_enter,
    .exit = playback_view_exit,
    .update = playback_view_update,
    .render_top = playback_view_render_top,
    .render_bottom = playback_view_render_bottom,
};

AppState *state_playback_view_create(void) { return &s_playback_view; }
