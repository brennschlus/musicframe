#include "state_filter_select.h"
#include "../state/state_manager.h"
#include "../image/image_filters.h"
#include "../ui/ui_panel.h"
#include "../ui/ui_text.h"
#include <3ds.h>
#include <stdio.h>
#include "../state/transitions.h"

// ---------------------------------------------------------------------------
// Filter Select screen
//
// Top screen:    Live preview of current filter applied to photo
// Bottom screen: Filter name + navigation hints (citro2d)
// Input:         L/R = cycle filters, A = confirm, B = back
// ---------------------------------------------------------------------------

static void filter_select_enter(AppState* self, AppContext* ctx)
{
    (void)self;
    (void)ctx;
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
    }

    if (kDown & KEY_A)
        state_manager_transition(ctx, app_next_state(ctx->current_state, TRIGGER_KEY_A));
    if (kDown & KEY_B)
        state_manager_transition(ctx, app_next_state(ctx->current_state, TRIGGER_KEY_B));
}

static void filter_select_render_top(AppState* self, AppContext* ctx)
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

// Draws a small dot indicator for each option, highlighting the active one.
static void draw_carousel_dots(float y, int active, int count)
{
    float dot_r = 4.0f;
    float spacing = 16.0f;
    float total_w = (count - 1) * spacing;
    float start_x = BOTTOM_W * 0.5f - total_w * 0.5f;

    for (int i = 0; i < count; i++) {
        u32 c = (i == active) ? ui_color_gold() : ui_color_dim();
        float size = (i == active) ? dot_r * 2.0f : dot_r * 1.2f;
        C2D_DrawRectSolid(start_x + i * spacing - size * 0.5f,
                          y - size * 0.5f, 0, size, size, c);
    }
}

static void filter_select_render_bottom(AppState* self, AppContext* ctx)
{
    (void)self;

    ui_panel_bg_dark();
    ui_panel_title("Filter Select");

    // Big filter name in the middle
    C2D_DrawRectSolid(20.0f, 60.0f, 0, BOTTOM_W - 40.0f, 60.0f, ui_color_panel());
    ui_draw_centered(BOTTOM_W * 0.5f, 70.0f, 0.0f, 0.45f, ui_color_dim(),
                     "Current filter");
    ui_draw_centered(BOTTOM_W * 0.5f, 88.0f, 0.0f, 0.70f, ui_color_gold(),
                     "%s", image_filter_name(ctx->scene.selected_filter));

    // Carousel indicator
    draw_carousel_dots(135.0f, ctx->scene.selected_filter, FILTER_COUNT);

    // Controls
    ui_draw(28.0f, 156.0f, 0.0f, 0.45f, ui_color_gold(), "[L/R]");
    ui_draw(80.0f, 156.0f, 0.0f, 0.45f, ui_color_text(), "Change filter");

    ui_draw(28.0f, 176.0f, 0.0f, 0.45f, ui_color_gold(), "[A]");
    ui_draw(80.0f, 176.0f, 0.0f, 0.45f, ui_color_text(), "Apply & continue");

    ui_draw(28.0f, 196.0f, 0.0f, 0.45f, ui_color_gold(), "[B]");
    ui_draw(80.0f, 196.0f, 0.0f, 0.45f, ui_color_text(), "Back to photo");

    char hint[64];
    snprintf(hint, sizeof(hint), "%d / %d",
             ctx->scene.selected_filter + 1, FILTER_COUNT);
    ui_panel_footer_hint(hint);
}

// -- Factory ----------------------------------------------------------------

static AppState s_filter_select = {
    .uses_direct_framebuffer = false,
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
