// ---------------------------------------------------------------------------
// Music Photo Frame — main entry point
//
// Initializes 3DS services, citro2d graphics, and runs the main
// application loop with state-driven screen management.
// ---------------------------------------------------------------------------


#include <3ds.h>
#include <citro2d.h>

#include "app_context.h"
#include "state/state_manager.h"

// ---------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    // --- Initialize 3DS services -------------------------------------------
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    // Bottom screen: text console (for debug and simple UI in early phases)
    consoleInit(GFX_BOTTOM, NULL);

    // Top screen: citro2d render target
    C3D_RenderTarget* top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);

    AppContext ctx = {
        .running       = true,
        .current_state = STATE_NONE,
        .next_state    = STATE_MAIN_MENU,
        .top_target    = top,

        .scene = {
            .selected_filter = FILTER_NONE,
            .selected_frame  = 0,
        }
    };

    // Initialize NDSP
    Result ndsp_res = ndspInit();
    audio_player_init(&ctx.audio);

    if (R_FAILED(ndsp_res)) {
        // We'll write this error state to the struct so UI can show it
        ctx.audio.ndsp_ok = false;
    } else {
        ctx.audio.ndsp_ok = true;
        ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    }

    // --- Initialize state manager ------------------------------------------
    state_manager_init();

    // Apply the initial transition (NONE -> MAIN_MENU)
    state_manager_apply_transition(&ctx);

    // --- Main loop ---------------------------------------------------------
    while (aptMainLoop() && ctx.running)
    {
        // --- Input ---------------------------------------------------------
        hidScanInput();

        // --- Update --------------------------------------------------------
        state_manager_update(&ctx);

        // --- Render top screen (citro2d) -----------------------------------
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        state_manager_render_top(&ctx, top);
        C3D_FrameEnd(0);

        // --- Render bottom screen (console) --------------------------------
        state_manager_render_bottom(&ctx);

        // --- Apply pending state transition (between frames) ---------------
        state_manager_apply_transition(&ctx);
    }

    // --- Cleanup -----------------------------------------------------------
    audio_player_shutdown(&ctx.audio);
    ndspExit();

    scene_model_cleanup(&ctx.scene);
    state_manager_shutdown();

    C2D_Fini();
    C3D_Fini();
    gfxExit();

    return 0;
}
