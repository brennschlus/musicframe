#include "state_main_menu.h"
#include "../state/state_manager.h"
#include "../state/transitions.h"
#include "../ui/ui_panel.h"
#include "../ui/ui_text.h"
#include <3ds.h>

// ---------------------------------------------------------------------------
// Main Menu screen
//
// Top screen:    Application title + decorative background
// Bottom screen: Menu options rendered with citro2d
// Input:         A = start new scene, START = exit app
// ---------------------------------------------------------------------------

static void main_menu_enter(AppState *self, AppContext *ctx) {
  (void)self;
  (void)ctx;
}

static void main_menu_exit(AppState *self, AppContext *ctx) {
  (void)self;
  (void)ctx;
}

static void main_menu_update(AppState *self, AppContext *ctx) {
  (void)self;

  u32 kDown = hidKeysDown();

  if (kDown & KEY_A)
    state_manager_transition(ctx,
                             app_next_state(ctx->current_state, TRIGGER_KEY_A));

  if (kDown & KEY_SELECT)
    state_manager_transition(ctx,
                             app_next_state(ctx->current_state, TRIGGER_KEY_SELECT));

  if (kDown & KEY_Y)
    state_manager_transition(ctx,
                             app_next_state(ctx->current_state, TRIGGER_KEY_Y));

  if (kDown & KEY_START) {
    ctx->running = false;
  }
}

static void main_menu_render_top(AppState *self, AppContext *ctx) {
  (void)self;

  C3D_RenderTarget *target = ctx->top_target;

  // Background — warm gradient-like feel using overlapping rectangles
  u32 clrBg = C2D_Color32(0x2D, 0x1B, 0x3D, 0xFF);     // Deep purple
  u32 clrAccent = C2D_Color32(0xE8, 0x6D, 0x50, 0xFF); // Warm coral
  u32 clrGold = C2D_Color32(0xF2, 0xC1, 0x4E, 0xFF);   // Gold

  C2D_TargetClear(target, clrBg);
  C2D_SceneBegin(target);

  // Decorative stripe at top
  C2D_DrawRectSolid(0, 0, 0, TOP_SCREEN_W, 4, clrAccent);

  // Central decorative block — represents the "frame" concept
  float cx = TOP_SCREEN_W / 2.0f;
  float cy = TOP_SCREEN_H / 2.0f;
  float frame_w = 160;
  float frame_h = 120;

  // Outer frame border (gold)
  C2D_DrawRectSolid(cx - frame_w / 2 - 4, cy - frame_h / 2 - 4, 0, frame_w + 8,
                    frame_h + 8, clrGold);

  // Inner frame (darker)
  u32 clrInner = C2D_Color32(0x1A, 0x0F, 0x25, 0xFF);
  C2D_DrawRectSolid(cx - frame_w / 2, cy - frame_h / 2, 0, frame_w, frame_h,
                    clrInner);

  // Small accent squares inside frame (like a photo placeholder)
  u32 clrSoft = C2D_Color32(0x5B, 0x3A, 0x6E, 0xFF);
  C2D_DrawRectSolid(cx - 40, cy - 30, 0, 80, 60, clrSoft);

  // Bottom stripe
  C2D_DrawRectSolid(0, TOP_SCREEN_H - 4, 0, TOP_SCREEN_W, 4, clrAccent);
}

static void draw_menu_button(float y, const char *label, const char *hint) {
  float btn_w = BOTTOM_W - 60.0f;
  float btn_x = 30.0f;
  float btn_h = 36.0f;

  C2D_DrawRectSolid(btn_x, y, 0, btn_w, btn_h, ui_color_panel());
  C2D_DrawRectSolid(btn_x, y, 0, btn_w, 2, ui_color_gold());
  C2D_DrawRectSolid(btn_x, y + btn_h - 2, 0, btn_w, 2, ui_color_gold());

  ui_draw(btn_x + 12.0f, y + 8.0f, 0.0f, 0.55f, ui_color_gold(), "%s", label);
  ui_draw(btn_x + 12.0f, y + 20.0f, 0.0f, 0.40f, ui_color_text(), "%s", hint);
}

static void main_menu_render_bottom(AppState *self, AppContext *ctx) {
  (void)self;
  (void)ctx;

  ui_panel_bg_dark();
  ui_panel_title("Music Photo Frame");

  // Tagline
  ui_draw_centered(BOTTOM_W * 0.5f, 48.0f, 0.0f, 0.45f, ui_color_dim(),
                   "Atmospheric photo frame with music");

  draw_menu_button(58.0f,  "[A]      New Photo Scene",
                   "Take a picture & compose a scene");
  draw_menu_button(102.0f, "[Y]      Photo Library",
                   "Load a photo from sdmc:/DCIM/");
  draw_menu_button(146.0f, "[SELECT] My Moments",
                   "Browse & load saved scenes");
  draw_menu_button(190.0f, "[START]  Exit", "Leave the application");

  ui_panel_footer_hint("Choose an option");
}

// -- Factory ----------------------------------------------------------------

static AppState s_main_menu = {
    .uses_direct_framebuffer = false,
    .enter = main_menu_enter,
    .exit = main_menu_exit,
    .update = main_menu_update,
    .render_top = main_menu_render_top,
    .render_bottom = main_menu_render_bottom,
};

AppState *state_main_menu_create(void) { return &s_main_menu; }
