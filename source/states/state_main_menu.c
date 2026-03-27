#include "state_main_menu.h"
#include "../state/state_manager.h"
#include <stdio.h>
#include <3ds.h>

// ---------------------------------------------------------------------------
// Main Menu screen
//
// Top screen:    Application title + decorative background
// Bottom screen: Menu options (console text)
// Input:         A = start new scene, START = exit app
// ---------------------------------------------------------------------------

#define TOP_SCREEN_WIDTH  400
#define TOP_SCREEN_HEIGHT 240

// -- State callbacks --------------------------------------------------------

static void main_menu_enter(AppState* self, AppContext* ctx)
{
    (void)self;
    (void)ctx;

    consoleClear();
    printf("\x1b[1;1H");  // Move cursor to top-left
    printf("===========================\n");
    printf("   Music Photo Frame\n");
    printf("===========================\n");
    printf("\n");
    printf("  [A]     New Photo Scene\n");
    printf("  [START] Exit\n");
    printf("\n");
    printf("---------------------------\n");
}

static void main_menu_exit(AppState* self, AppContext* ctx)
{
    (void)self;
    (void)ctx;
}

static void main_menu_update(AppState* self, AppContext* ctx)
{
    (void)self;

    u32 kDown = hidKeysDown();

    if (kDown & KEY_A) {
        state_manager_transition(ctx, STATE_PHOTO_REVIEW);
    }

    if (kDown & KEY_START) {
        ctx->running = false;
    }
}

static void main_menu_render_top(AppState* self, AppContext* ctx, C3D_RenderTarget* target)
{
    (void)self;
    (void)ctx;

    // Background — warm gradient-like feel using overlapping rectangles
    u32 clrBg     = C2D_Color32(0x2D, 0x1B, 0x3D, 0xFF);  // Deep purple
    u32 clrAccent = C2D_Color32(0xE8, 0x6D, 0x50, 0xFF);  // Warm coral
    u32 clrGold   = C2D_Color32(0xF2, 0xC1, 0x4E, 0xFF);  // Gold

    C2D_TargetClear(target, clrBg);
    C2D_SceneBegin(target);

    // Decorative stripe at top
    C2D_DrawRectSolid(0, 0, 0, TOP_SCREEN_WIDTH, 4, clrAccent);

    // Central decorative block — represents the "frame" concept
    float cx = TOP_SCREEN_WIDTH / 2.0f;
    float cy = TOP_SCREEN_HEIGHT / 2.0f;
    float frame_w = 160;
    float frame_h = 120;

    // Outer frame border (gold)
    C2D_DrawRectSolid(cx - frame_w/2 - 4, cy - frame_h/2 - 4, 0,
                      frame_w + 8, frame_h + 8, clrGold);

    // Inner frame (darker)
    u32 clrInner = C2D_Color32(0x1A, 0x0F, 0x25, 0xFF);
    C2D_DrawRectSolid(cx - frame_w/2, cy - frame_h/2, 0,
                      frame_w, frame_h, clrInner);

    // Small accent squares inside frame (like a photo placeholder)
    u32 clrSoft = C2D_Color32(0x5B, 0x3A, 0x6E, 0xFF);
    C2D_DrawRectSolid(cx - 40, cy - 30, 0, 80, 60, clrSoft);

    // Bottom stripe
    C2D_DrawRectSolid(0, TOP_SCREEN_HEIGHT - 4, 0, TOP_SCREEN_WIDTH, 4, clrAccent);
}

static void main_menu_render_bottom(AppState* self, AppContext* ctx)
{
    (void)self;
    (void)ctx;
    // Console text is persistent — set in enter()
}

// -- Factory ----------------------------------------------------------------

static AppState s_main_menu = {
    .enter         = main_menu_enter,
    .exit          = main_menu_exit,
    .update        = main_menu_update,
    .render_top    = main_menu_render_top,
    .render_bottom = main_menu_render_bottom,
};

AppState* state_main_menu_create(void)
{
    return &s_main_menu;
}
