#include "state_filter_select.h"
#include "../state/state_manager.h"
#include "../image/image_filters.h"
#include <stdio.h>
#include <3ds.h>

// ---------------------------------------------------------------------------
// Filter Select screen
//
// Top screen:    Live preview of current filter applied to photo
// Bottom screen: Filter name + navigation hints
// Input:         L/R = cycle filters, A = confirm, B = back
// ---------------------------------------------------------------------------

#define TOP_W 400
#define TOP_H 240

static void update_bottom_text(AppContext* ctx)
{
    consoleClear();
    printf("\x1b[1;1H");
    printf("===========================\n");
    printf("   Filter Select\n");
    printf("===========================\n");
    printf("\n");
    printf("  Current: %s (%d/%d)\n",
           image_filter_name(ctx->scene.selected_filter),
           ctx->scene.selected_filter + 1,
           FILTER_COUNT);
    printf("\n");
    printf("  [L/R] Change filter\n");
    printf("  [A]   Apply & continue >>\n");
    printf("  [B]   << Back to photo\n");
    printf("\n");
    printf("---------------------------\n");
}

// -- State callbacks --------------------------------------------------------

static void filter_select_enter(AppState* self, AppContext* ctx)
{
    (void)self;
    update_bottom_text(ctx);
}

static void filter_select_exit(AppState* self, AppContext* ctx)
{
    (void)self;
    (void)ctx;
}

static void filter_select_update(AppState* self, AppContext* ctx)
{
    (void)self;

    u32 kDown = hidKeysDown();
    bool changed = false;

    if (kDown & KEY_R) {
        ctx->scene.selected_filter = (ctx->scene.selected_filter + 1) % FILTER_COUNT;
        changed = true;
    }
    if (kDown & KEY_L) {
        ctx->scene.selected_filter = (ctx->scene.selected_filter + FILTER_COUNT - 1) % FILTER_COUNT;
        changed = true;
    }

    if (changed) {
        scene_model_apply_filter(&ctx->scene);
        update_bottom_text(ctx);
    }

    if (kDown & KEY_A) {
        state_manager_transition(ctx, STATE_FRAME_SELECT);
    }
    if (kDown & KEY_B) {
        state_manager_transition(ctx, STATE_PHOTO_REVIEW);
    }
}

static void filter_select_render_top(AppState* self, AppContext* ctx, C3D_RenderTarget* target)
{
    (void)self;

    u32 clrBg = C2D_Color32(0x1A, 0x1A, 0x2E, 0xFF);
    C2D_TargetClear(target, clrBg);
    C2D_SceneBegin(target);

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

static void filter_select_render_bottom(AppState* self, AppContext* ctx)
{
    (void)self;
    (void)ctx;
}

// -- Factory ----------------------------------------------------------------

static AppState s_filter_select = {
    .enter         = filter_select_enter,
    .exit          = filter_select_exit,
    .update        = filter_select_update,
    .render_top    = filter_select_render_top,
    .render_bottom = filter_select_render_bottom,
};

AppState* state_filter_select_create(void)
{
    return &s_filter_select;
}
