// ---------------------------------------------------------------------------
// Music Photo Frame — main entry point
//
// Initializes 3DS services, citro2d graphics, and runs the main
// application loop with state-driven screen management.
// ---------------------------------------------------------------------------

#include <3ds.h>
#include <citro2d.h>

#include "app_context.h"
#include "camera/hardware_camera.h"
#include "state/state_manager.h"
#include "ui/ui_text.h"
#include "state/transitions.h"

// ---------------------------------------------------------------------------
int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  // --- Initialize 3DS services -------------------------------------------
  gfxInitDefault();
  C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
  C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
  C2D_Prepare();

  // Both screens are now citro2d render targets — no text console.
  C3D_RenderTarget *top    = C2D_CreateScreenTarget(GFX_TOP,    GFX_LEFT);
  C3D_RenderTarget *bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

  // Shared text renderer (uses the 3DS system font).
  ui_text_init();

  AppContext ctx = {.running = true,
                    .current_state = STATE_NONE,
                    .next_state = STATE_MAIN_MENU,
                    .top_target = top,
                    .bottom_target = bottom,

                    .scene = {
                        .selected_filter = FILTER_NONE,
                        .selected_frame = 0,
                    }};

  // Initialize NDSP
  Result ndsp_res = ndspInit();
  audio_player_init(&ctx.audio);

  if (R_FAILED(ndsp_res)) {
    ctx.audio.ndsp_ok = false;
  } else {
    ctx.audio.ndsp_ok = true;
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
  }

  // Initialize Camera
  hw_camera_init(&ctx.camera);

  // --- Initialize state manager ------------------------------------------
  state_manager_init();

  // Apply the initial transition (NONE -> MAIN_MENU)
  state_manager_apply_transition(&ctx);

  // --- Main loop ---------------------------------------------------------
  while (aptMainLoop() && ctx.running) {
    hidScanInput();

    state_manager_update(&ctx);
    state_manager_render_frame(&ctx);

    state_manager_apply_transition(&ctx);
  }

  // --- Cleanup -----------------------------------------------------------
  hw_camera_shutdown(&ctx.camera);
  audio_player_shutdown(&ctx.audio);
  ndspExit();

  scene_model_cleanup(&ctx.scene);
  state_manager_shutdown();

  ui_text_shutdown();

  C2D_Fini();
  C3D_Fini();
  gfxExit();

  return 0;
}
