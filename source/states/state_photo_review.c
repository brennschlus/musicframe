#include "state_photo_review.h"
#include "../state/state_manager.h"
#include "../ui/ui_panel.h"
#include "../ui/ui_text.h"
#include <3ds.h>
#include "../state/transitions.h"

// ---------------------------------------------------------------------------
// Photo Review screen
//
// Top screen:    Display the loaded photo (camera capture)
// Bottom screen: Navigation hints rendered with citro2d
// Input:         A = go to filter select, B = back to menu
// ---------------------------------------------------------------------------

static void photo_review_enter(AppState* self, AppContext* ctx)
{
    (void)self;
    (void)ctx;
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

    if (kDown & KEY_A)
        state_manager_transition(ctx, app_next_state(ctx->current_state, TRIGGER_KEY_A));

    if (kDown & KEY_B) {
        scene_model_cleanup(&ctx->scene);   // side effect остаётся здесь
        state_manager_transition(ctx, app_next_state(ctx->current_state, TRIGGER_KEY_B));
    }
}

static void photo_review_render_top(AppState* self, AppContext* ctx)
{
    (void)self;

    C3D_RenderTarget* target = ctx->top_target;

    u32 clrBg = C2D_Color32(0x1A, 0x1A, 0x2E, 0xFF);
    C2D_TargetClear(target, clrBg);
    C2D_SceneBegin(target);

    scene_model_draw_photo_centered(&ctx->scene, 0.5f);

    // Accent lines
    u32 clrAccent = C2D_Color32(0xE8, 0x6D, 0x50, 0xFF);
    C2D_DrawRectSolid(0, 0, 0, TOP_SCREEN_W, 3, clrAccent);
    C2D_DrawRectSolid(0, TOP_SCREEN_H - 3, 0, TOP_SCREEN_W, 3, clrAccent);
}

static void photo_review_render_bottom(AppState* self, AppContext* ctx)
{
    (void)self;

    ui_panel_bg_dark();
    ui_panel_title("Photo Review");

    // Info panel
    C2D_DrawRectSolid(20.0f, 50.0f, 0, BOTTOM_W - 40.0f, 50.0f, ui_color_panel());

    if (ctx->scene.photo_loaded && ctx->scene.original) {
        ui_draw(32.0f, 58.0f, 0.0f, 0.45f, ui_color_dim(),
                "Image loaded:");
        ui_draw(32.0f, 76.0f, 0.0f, 0.55f, ui_color_gold(),
                "%d x %d",
                ctx->scene.original->width,
                ctx->scene.original->height);
    } else {
        ui_draw(32.0f, 66.0f, 0.0f, 0.50f, ui_color_accent(),
                "Failed to load image");
    }

    // Controls
    ui_draw(28.0f, 128.0f, 0.0f, 0.50f, ui_color_gold(), "[A]");
    ui_draw(70.0f, 128.0f, 0.0f, 0.50f, ui_color_text(), "Choose a filter");

    ui_draw(28.0f, 152.0f, 0.0f, 0.50f, ui_color_gold(), "[B]");
    ui_draw(70.0f, 152.0f, 0.0f, 0.50f, ui_color_text(), "Back to main menu");

    ui_panel_footer_hint("Review your photo");
}

// -- Factory ----------------------------------------------------------------

static AppState s_photo_review = {
    .uses_direct_framebuffer = false,
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
