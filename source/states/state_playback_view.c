#include "state_playback_view.h"
#include "../image/image_filters.h"
#include "../image/image_frames.h"
#include "../state/state_manager.h"
#include "../ui/ui_panel.h"
#include "../ui/ui_text.h"
#include <3ds.h>
#include <string.h>
#include "../state/transitions.h"

// ---------------------------------------------------------------------------
// Playback View screen
//
// Top screen:    Final framed photo (filtered image + frame overlay)
// Bottom screen: Music playback controls rendered with citro2d
// Input:         A = play/pause, L/R = seek, Y = toggle loop, B = back
// ---------------------------------------------------------------------------

static void playback_view_enter(AppState* self, AppContext* ctx)
{
    (void)self;
    if (ctx->scene.music_selected) {
        audio_player_load(&ctx->audio, ctx->scene.music_path);
        audio_player_play(&ctx->audio);
    }
}

static void playback_view_exit(AppState* self, AppContext* ctx)
{
    (void)self;
    (void)ctx;
}

static void playback_view_update(AppState* self, AppContext* ctx)
{
    (void)self;

    u32 kDown = hidKeysDown();

    if (kDown & KEY_A) {
        audio_player_toggle_pause(&ctx->audio);
    }
    if (kDown & KEY_R) {
        audio_player_seek(&ctx->audio, 5);
    }
    if (kDown & KEY_L) {
        audio_player_seek(&ctx->audio, -5);
    }
    if (kDown & KEY_Y) {
        audio_player_set_loop(&ctx->audio, !ctx->audio.loop);
    }

    if (kDown & KEY_B) {
        audio_player_stop(&ctx->audio);     // side effect остаётся здесь
        scene_model_cleanup(&ctx->scene);   // side effect остаётся здесь
        state_manager_transition(ctx, app_next_state(ctx->current_state, TRIGGER_KEY_B));
    }

    if (ctx->audio.loop && audio_player_finished(&ctx->audio)) {
        audio_player_restart(&ctx->audio);
    }
}

static void playback_view_render_top(AppState* self, AppContext* ctx)
{
    (void)self;

    C3D_RenderTarget* target = ctx->top_target;

    u32 clrBg = C2D_Color32(0x1C, 0x11, 0x28, 0xFF);
    C2D_TargetClear(target, clrBg);
    C2D_SceneBegin(target);

    if (ctx->scene.photo_loaded && ctx->scene.texture.initialized) {
        scene_model_draw_photo_centered(&ctx->scene, 0.5f);
        image_frame_draw(ctx->scene.selected_frame, 0.0f, 0.0f, TOP_SCREEN_W, TOP_SCREEN_H, 0.6f);
    }
}

// Draws a progress bar at (x, y) of size (w, h) with progress 0..1.
static void draw_progress_bar(float x, float y, float w, float h, float progress)
{
    u32 border = ui_color_dim();
    u32 track  = ui_color_panel();
    u32 fill   = ui_color_gold();

    // Border
    C2D_DrawRectSolid(x - 1.0f, y - 1.0f, 0, w + 2.0f, h + 2.0f, border);
    // Track
    C2D_DrawRectSolid(x, y, 0, w, h, track);

    // Fill
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    float fw = w * progress;
    if (fw > 0.0f) C2D_DrawRectSolid(x, y, 0, fw, h, fill);
}

static void playback_view_render_bottom(AppState* self, AppContext* ctx)
{
    (void)self;

    ui_panel_bg_dark();
    ui_panel_title("Playback View");

    // Filter / Frame / Loop summary
    ui_draw(14.0f, 36.0f, 0.0f, 0.40f, ui_color_dim(), "Filter:");
    ui_draw(70.0f, 36.0f, 0.0f, 0.40f, ui_color_text(), "%s",
            image_filter_name(ctx->scene.selected_filter));

    ui_draw(14.0f, 52.0f, 0.0f, 0.40f, ui_color_dim(), "Frame:");
    ui_draw(70.0f, 52.0f, 0.0f, 0.40f, ui_color_text(), "%s",
            image_frame_name(ctx->scene.selected_frame));

    ui_draw(14.0f, 68.0f, 0.0f, 0.40f, ui_color_dim(), "Loop:");
    ui_draw(70.0f, 68.0f, 0.0f, 0.40f,
            ctx->audio.loop ? ui_color_gold() : ui_color_dim(),
            "%s", ctx->audio.loop ? "On" : "Off");

    // Now playing panel
    float panel_y = 90.0f;
    float panel_h = 90.0f;
    C2D_DrawRectSolid(12.0f, panel_y, 0, BOTTOM_W - 24.0f, panel_h, ui_color_panel());

    if (!ctx->scene.music_selected) {
        ui_draw_centered(BOTTOM_W * 0.5f, panel_y + 28.0f, 0.0f, 0.50f,
                         ui_color_dim(), "(no track selected)");
        ui_draw_centered(BOTTOM_W * 0.5f, panel_y + 52.0f, 0.0f, 0.45f,
                         ui_color_dim(), "00:00 / 00:00");
    } else if (!ctx->audio.ndsp_ok) {
        ui_draw_centered(BOTTOM_W * 0.5f, panel_y + 20.0f, 0.0f, 0.55f,
                         ui_color_accent(), "[NDSP INIT FAILED]");
        ui_draw_centered(BOTTOM_W * 0.5f, panel_y + 44.0f, 0.0f, 0.40f,
                         ui_color_dim(), "Missing DSP firmware?");
    } else {
        const char* name = strrchr(ctx->scene.music_path, '/');
        name = name ? name + 1 : ctx->scene.music_path;

        ui_draw(22.0f, panel_y + 8.0f, 0.0f, 0.40f, ui_color_dim(),
                "Now playing:");
        ui_draw(22.0f, panel_y + 24.0f, 0.0f, 0.50f, ui_color_gold(),
                "%s", name);

        float pos = audio_player_position_sec(&ctx->audio);
        float dur = audio_player_duration_sec(&ctx->audio);
        int p_m = (int)pos / 60, p_s = (int)pos % 60;
        int d_m = (int)dur / 60, d_s = (int)dur % 60;

        // Progress bar
        float progress = dur > 0.0f ? pos / dur : 0.0f;
        draw_progress_bar(22.0f, panel_y + 52.0f, BOTTOM_W - 44.0f, 8.0f, progress);

        // Time + status
        const char* status = ctx->audio.paused
            ? "PAUSED"
            : (audio_player_finished(&ctx->audio) ? "STOPPED" : "PLAYING");
        u32 status_color = ctx->audio.paused
            ? ui_color_dim()
            : (audio_player_finished(&ctx->audio) ? ui_color_accent() : ui_color_gold());

        ui_draw(22.0f, panel_y + 68.0f, 0.0f, 0.40f, ui_color_text(),
                "%02d:%02d / %02d:%02d", p_m, p_s, d_m, d_s);
        ui_draw(BOTTOM_W - 80.0f, panel_y + 68.0f, 0.0f, 0.40f,
                status_color, "%s", status);
    }

    // Controls
    float ctrl_y = 190.0f;
    ui_draw( 12.0f, ctrl_y, 0.0f, 0.40f, ui_color_gold(), "[A] Play/Pause");
    ui_draw(120.0f, ctrl_y, 0.0f, 0.40f, ui_color_gold(), "[L/R] Seek +/-5s");
    ui_draw( 12.0f, ctrl_y + 14.0f, 0.0f, 0.40f, ui_color_gold(), "[Y] Toggle loop");
    ui_draw(120.0f, ctrl_y + 14.0f, 0.0f, 0.40f, ui_color_gold(), "[B] Back to menu");

    ui_panel_footer_hint("Playing your scene");
}

// -- Factory ----------------------------------------------------------------

static AppState s_playback_view = {
    .uses_direct_framebuffer = false,
    .enter         = playback_view_enter,
    .exit          = playback_view_exit,
    .update        = playback_view_update,
    .render_top    = playback_view_render_top,
    .render_bottom = playback_view_render_bottom,
};

AppState* state_playback_view_create(void) { return &s_playback_view; }
