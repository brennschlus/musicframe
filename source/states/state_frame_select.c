#include "state_frame_select.h"
#include "../image/image_filters.h"
#include "../image/image_frames.h"
#include "../state/state_manager.h"
#include "../ui/ui_panel.h"
#include "../ui/ui_text.h"
#include <3ds.h>
#include <stdio.h>
#include "../state/transitions.h"

// ---------------------------------------------------------------------------
// Frame Select screen
//
// Top screen:    Preview of photo + filter + selected frame overlay
// Bottom screen: Frame name + navigation hints (citro2d)
// Input:         L/R = cycle frames, A = confirm, B = back
// ---------------------------------------------------------------------------

static void frame_select_enter(AppState* self, AppContext* ctx)
{
    (void)self;
    (void)ctx;
}

static void frame_select_exit(AppState* self, AppContext* ctx)
{
    (void)self;
    (void)ctx;
}

static void frame_select_update(AppState* self, AppContext* ctx)
{
    (void)self;

    u32 kDown = hidKeysDown();

    if (kDown & KEY_R) {
        ctx->scene.selected_frame = (ctx->scene.selected_frame + 1) % FRAME_COUNT;
    }
    if (kDown & KEY_L) {
        ctx->scene.selected_frame = (ctx->scene.selected_frame + FRAME_COUNT - 1) % FRAME_COUNT;
    }

    if (kDown & KEY_A)
        state_manager_transition(ctx, app_next_state(ctx->current_state, TRIGGER_KEY_A));
    if (kDown & KEY_B)
        state_manager_transition(ctx, app_next_state(ctx->current_state, TRIGGER_KEY_B));
}

static void frame_select_render_top(AppState* self, AppContext* ctx)
{
    (void)self;

    C3D_RenderTarget* target = ctx->top_target;

    u32 clrBg = C2D_Color32(0x1A, 0x1A, 0x2E, 0xFF);
    C2D_TargetClear(target, clrBg);
    C2D_SceneBegin(target);

    if (ctx->scene.photo_loaded && ctx->scene.texture.initialized) {
        scene_model_draw_photo_centered(&ctx->scene, 0.5f);
        image_frame_draw(ctx->scene.selected_frame, 0.0f, 0.0f, TOP_SCREEN_W, TOP_SCREEN_H, 0.6f);
    }
}

static void draw_carousel_dots(float y, int active, int count)
{
    float spacing = 18.0f;
    float total_w = (count - 1) * spacing;
    float start_x = BOTTOM_W * 0.5f - total_w * 0.5f;

    for (int i = 0; i < count; i++) {
        u32 c = (i == active) ? ui_color_gold() : ui_color_dim();
        float size = (i == active) ? 8.0f : 5.0f;
        C2D_DrawRectSolid(start_x + i * spacing - size * 0.5f,
                          y - size * 0.5f, 0, size, size, c);
    }
}

static void frame_select_render_bottom(AppState* self, AppContext* ctx)
{
    (void)self;

    ui_panel_bg_dark();
    ui_panel_title("Frame Select");

    // Filter reminder
    ui_draw(20.0f, 40.0f, 0.0f, 0.40f, ui_color_dim(),
            "Filter: %s", image_filter_name(ctx->scene.selected_filter));

    // Current frame card
    C2D_DrawRectSolid(20.0f, 60.0f, 0, BOTTOM_W - 40.0f, 60.0f, ui_color_panel());
    ui_draw_centered(BOTTOM_W * 0.5f, 70.0f, 0.0f, 0.45f, ui_color_dim(),
                     "Current frame");
    ui_draw_centered(BOTTOM_W * 0.5f, 88.0f, 0.0f, 0.70f, ui_color_gold(),
                     "%s", image_frame_name(ctx->scene.selected_frame));

    draw_carousel_dots(135.0f, ctx->scene.selected_frame, FRAME_COUNT);

    // Controls
    ui_draw(28.0f, 156.0f, 0.0f, 0.45f, ui_color_gold(), "[L/R]");
    ui_draw(80.0f, 156.0f, 0.0f, 0.45f, ui_color_text(), "Change frame");

    ui_draw(28.0f, 176.0f, 0.0f, 0.45f, ui_color_gold(), "[A]");
    ui_draw(80.0f, 176.0f, 0.0f, 0.45f, ui_color_text(), "Confirm & continue");

    ui_draw(28.0f, 196.0f, 0.0f, 0.45f, ui_color_gold(), "[B]");
    ui_draw(80.0f, 196.0f, 0.0f, 0.45f, ui_color_text(), "Back to filters");

    char hint[64];
    snprintf(hint, sizeof(hint), "%d / %d",
             ctx->scene.selected_frame + 1, FRAME_COUNT);
    ui_panel_footer_hint(hint);
}

// -- Factory ----------------------------------------------------------------

static AppState s_frame_select = {
    .uses_direct_framebuffer = false,
    .enter         = frame_select_enter,
    .exit          = frame_select_exit,
    .update        = frame_select_update,
    .render_top    = frame_select_render_top,
    .render_bottom = frame_select_render_bottom,
};

AppState* state_frame_select_create(void) { return &s_frame_select; }
