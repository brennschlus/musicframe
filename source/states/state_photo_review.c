#include "state_photo_review.h"
#include "../state/state_manager.h"
#include <stdio.h>
#include <3ds.h>

// ---------------------------------------------------------------------------
// Photo Review screen
//
// Top screen:    Display the loaded photo (test image or camera capture)
// Bottom screen: Navigation hints
// Input:         A = go to filter select, B = back to menu
// ---------------------------------------------------------------------------

#define TOP_W 400
#define TOP_H 240

// -- State callbacks --------------------------------------------------------

static void photo_review_enter(AppState* self, AppContext* ctx)
{
    (void)self;

    // Load test image if not already loaded
    if (!ctx->scene.photo_loaded) {
        scene_model_load_test_image(&ctx->scene);
    }

    consoleClear();
    printf("\x1b[1;1H");
    printf("===========================\n");
    printf("   Photo Review\n");
    printf("===========================\n");
    printf("\n");
    if (ctx->scene.photo_loaded) {
        printf("  Image: %dx%d test pattern\n",
               ctx->scene.original->width,
               ctx->scene.original->height);
    } else {
        printf("  (failed to load image)\n");
    }
    printf("\n");
    printf("  [A] Choose filter >>\n");
    printf("  [B] << Back to menu\n");
    printf("\n");
    printf("---------------------------\n");
}

static void photo_review_exit(AppState* self, AppContext* ctx)
{
    (void)self;
    (void)ctx;
}

static void photo_review_update(AppState* self, AppContext* ctx)
{
    (void)self;

    u32 kDown = hidKeysDown();

    if (kDown & KEY_A) {
        state_manager_transition(ctx, STATE_FILTER_SELECT);
    }

    if (kDown & KEY_B) {
        // Cleanup scene when going back to menu
        scene_model_cleanup(&ctx->scene);
        state_manager_transition(ctx, STATE_MAIN_MENU);
    }
}

static void photo_review_render_top(AppState* self, AppContext* ctx, C3D_RenderTarget* target)
{
    (void)self;

    u32 clrBg = C2D_Color32(0x1A, 0x1A, 0x2E, 0xFF);
    C2D_TargetClear(target, clrBg);
    C2D_SceneBegin(target);

    // Draw the photo texture
    if (ctx->scene.photo_loaded && ctx->scene.tex_initialized) {
        C2D_Image img;
        img.tex    = &ctx->scene.tex;
        img.subtex = &ctx->scene.subtex;

        float x = (TOP_W - ctx->scene.subtex.width) / 2.0f;
        float y = (TOP_H - ctx->scene.subtex.height) / 2.0f;

        C2D_DrawImageAt(img, x, y, 0.5f, NULL, 1.0f, 1.0f);
    }

    // Accent lines
    u32 clrAccent = C2D_Color32(0xE8, 0x6D, 0x50, 0xFF);
    C2D_DrawRectSolid(0, 0, 0, TOP_W, 3, clrAccent);
    C2D_DrawRectSolid(0, TOP_H - 3, 0, TOP_W, 3, clrAccent);
}

static void photo_review_render_bottom(AppState* self, AppContext* ctx)
{
    (void)self;
    (void)ctx;
}

// -- Factory ----------------------------------------------------------------

static AppState s_photo_review = {
    .enter         = photo_review_enter,
    .exit          = photo_review_exit,
    .update        = photo_review_update,
    .render_top    = photo_review_render_top,
    .render_bottom = photo_review_render_bottom,
};

AppState* state_photo_review_create(void)
{
    return &s_photo_review;
}
