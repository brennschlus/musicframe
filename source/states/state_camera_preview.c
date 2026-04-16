#include "state_camera_preview.h"

#include "../state/state_manager.h"
#include "../ui/ui_panel.h"
#include "../ui/ui_text.h"
#include <string.h>
#include "../state/transitions.h"

static void camera_preview_enter(AppState* self, AppContext* ctx)
{
    (void)self;

    if (!ctx->camera.initialized) {
        return;
    }

    ctx->camera.preview_active = true;
    ctx->camera.frame_ready = false;
}

static void camera_preview_exit(AppState* self, AppContext* ctx)
{
    (void)self;
    hw_camera_stop(&ctx->camera);
}

static void camera_preview_update(AppState* self, AppContext* ctx)
{
    (void)self;

    u32 kDown = hidKeysDown();

    if (kDown & KEY_B) {
        state_manager_transition(ctx, app_next_state(ctx->current_state, TRIGGER_KEY_B));
        return;
    }

    if (!ctx->camera.preview_active) {
        return;
    }

    if (kDown & KEY_A) {
        if (!ctx->camera.frame_ready) {
            return;
        }

        ImageBuffer* captured = image_buffer_create(TOP_SCREEN_W, TOP_SCREEN_H);
        if (!captured) {
            return;
        }

        hw_camera_play_shutter(&ctx->camera);
        hw_camera_get_frame_rgba8(&ctx->camera, captured);

        scene_model_set_photo(&ctx->scene, captured);
        // Guard (frame_ready) уже проверен выше — шлём составной триггер
        state_manager_transition(ctx, app_next_state(ctx->current_state, TRIGGER_PHOTO_CAPTURED));
        return;
    }

    (void)hw_camera_capture_preview_frame(&ctx->camera);
}

static void clear_top_fb(void)
{
    u16 fbWidth, fbHeight;
    u8* fb = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &fbWidth, &fbHeight);
    if (!fb) return;

    memset(fb, 0, fbWidth * fbHeight * 3);
}

static void camera_preview_render_top(AppState* self, AppContext* ctx)
{
    (void)self;

    clear_top_fb();

    if (!ctx->camera.frame_ready) {
        return;
    }

    hw_draw_camera_preview_top(ctx->camera.frame_buffer);
}

static void camera_preview_render_bottom(AppState* self, AppContext* ctx)
{
    (void)self;

    ui_panel_bg_dark();
    ui_panel_title("Camera Preview");

    if (!ctx->camera.initialized) {
        // Error panel
        u32 clrErr = C2D_Color32(0xB8, 0x3A, 0x3A, 0xFF);
        C2D_DrawRectSolid(20.0f, 60.0f, 0, BOTTOM_W - 40.0f, 60.0f, clrErr);
        ui_draw_centered(BOTTOM_W * 0.5f, 72.0f, 0.0f, 0.55f,
                         ui_color_text(), "Camera not initialized");
        ui_draw_centered(BOTTOM_W * 0.5f, 92.0f, 0.0f, 0.45f,
                         ui_color_text(), "Check hardware / permissions");
        ui_panel_footer_hint("[B] Back");
        return;
    }

    // Status indicator
    const char* status = ctx->camera.frame_ready ? "LIVE" : "starting...";
    u32 status_color = ctx->camera.frame_ready ? ui_color_gold() : ui_color_dim();

    C2D_DrawRectSolid(20.0f, 50.0f, 0, BOTTOM_W - 40.0f, 44.0f, ui_color_panel());
    ui_draw(32.0f, 58.0f, 0.0f, 0.50f, ui_color_text(), "Viewfinder:");
    ui_draw(32.0f, 74.0f, 0.0f, 0.60f, status_color,    "%s", status);

    // Controls
    ui_draw(28.0f, 120.0f, 0.0f, 0.50f, ui_color_gold(), "[A]");
    ui_draw(70.0f, 120.0f, 0.0f, 0.50f, ui_color_text(), "Take photo");

    ui_draw(28.0f, 144.0f, 0.0f, 0.50f, ui_color_gold(), "[B]");
    ui_draw(70.0f, 144.0f, 0.0f, 0.50f, ui_color_text(), "Cancel / back");

    ui_panel_footer_hint("Point at subject, then press [A]");
}

static AppState s_camera_preview = {
    .uses_direct_framebuffer = true,
    .enter         = camera_preview_enter,
    .exit          = camera_preview_exit,
    .update        = camera_preview_update,
    .render_top    = camera_preview_render_top,
    .render_bottom = camera_preview_render_bottom,
};

AppState* state_camera_preview_create(void)
{
    return &s_camera_preview;
}
