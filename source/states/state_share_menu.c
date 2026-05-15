#include "state_share_menu.h"
#include "../net/uds_transport.h"
#include "../state/state_manager.h"
#include "../state/transitions.h"
#include "../ui/ui_panel.h"
#include "../ui/ui_text.h"
#include <3ds.h>

// ---------------------------------------------------------------------------
// Share Menu — pick a role for local-wireless scene sharing.
//
// Top screen:    Decorative + status text (UDS init result).
// Bottom screen: Two buttons — Host / Discover.
// Input:         UP/DOWN = navigate, A = pick role, B = back to moment_browser.
// ---------------------------------------------------------------------------

#define ROLE_HOST     0
#define ROLE_DISCOVER 1
#define ROLE_COUNT    2

static int  s_cursor    = ROLE_HOST;
static bool s_uds_ready = false;
static bool s_uds_error = false;

static void share_menu_enter(AppState *self, AppContext *ctx)
{
    (void)self;
    (void)ctx;
    s_cursor = ROLE_HOST;
    s_uds_ready = (uds_init() == 0);
    s_uds_error = !s_uds_ready;
}

static void share_menu_exit(AppState *self, AppContext *ctx)
{
    (void)self;
    (void)ctx;
    // Note: we do NOT shut UDS down here — the host/client states need it
    // active. uds_shutdown happens when we leave back to moment_browser.
    if (ctx->next_state == STATE_MOMENT_BROWSER) {
        uds_shutdown();
        s_uds_ready = false;
    }
}

static void share_menu_update(AppState *self, AppContext *ctx)
{
    (void)self;
    u32 kDown = hidKeysDown();

    if (kDown & KEY_UP)   { if (s_cursor > 0)             s_cursor--; }
    if (kDown & KEY_DOWN) { if (s_cursor < ROLE_COUNT - 1) s_cursor++; }

    if (kDown & KEY_A) {
        if (!s_uds_ready) return;
        Trigger t = (s_cursor == ROLE_HOST)
                    ? TRIGGER_SHARE_ROLE_HOST
                    : TRIGGER_SHARE_ROLE_DISCOVER;
        state_manager_transition(ctx, app_next_state(ctx->current_state, t));
    }

    if (kDown & KEY_B) {
        state_manager_transition(ctx,
                                 app_next_state(ctx->current_state, TRIGGER_KEY_B));
    }
}

static void share_menu_render_top(AppState *self, AppContext *ctx)
{
    (void)self;

    C3D_RenderTarget *target = ctx->top_target;
    u32 clrBg     = C2D_Color32(0x2D, 0x1B, 0x3D, 0xFF);
    u32 clrAccent = C2D_Color32(0xE8, 0x6D, 0x50, 0xFF);
    u32 clrGold   = C2D_Color32(0xF2, 0xC1, 0x4E, 0xFF);

    C2D_TargetClear(target, clrBg);
    C2D_SceneBegin(target);

    C2D_DrawRectSolid(0, 0, 0, TOP_SCREEN_W, 4, clrAccent);
    C2D_DrawRectSolid(0, TOP_SCREEN_H - 4, 0, TOP_SCREEN_W, 4, clrAccent);

    float cx = TOP_SCREEN_W / 2.0f;
    float cy = TOP_SCREEN_H / 2.0f;

    ui_draw_centered(cx, cy - 30.0f, 0.0f, 0.65f, clrGold, "Share Scene");
    ui_draw_centered(cx, cy + 4.0f,  0.0f, 0.45f, clrAccent,
                     "Local Wireless");
    ui_draw_centered(cx, cy + 24.0f, 0.0f, 0.40f,
                     C2D_Color32(0xF5, 0xF0, 0xE8, 0xFF),
                     "Two devices, no internet needed");
}

static void draw_role_button(float y, bool selected, const char *label,
                             const char *hint)
{
    float btn_w = BOTTOM_W - 60.0f;
    float btn_x = 30.0f;
    float btn_h = 40.0f;

    u32 bg = selected ? ui_color_select_bg() : ui_color_panel();
    C2D_DrawRectSolid(btn_x, y, 0, btn_w, btn_h, bg);
    C2D_DrawRectSolid(btn_x, y, 0, btn_w, 2, ui_color_gold());
    C2D_DrawRectSolid(btn_x, y + btn_h - 2, 0, btn_w, 2, ui_color_gold());

    ui_draw(btn_x + 12.0f, y + 8.0f,  0.0f, 0.55f, ui_color_gold(), "%s", label);
    ui_draw(btn_x + 12.0f, y + 22.0f, 0.0f, 0.40f, ui_color_text(), "%s", hint);
}

static void share_menu_render_bottom(AppState *self, AppContext *ctx)
{
    (void)self;
    (void)ctx;

    ui_panel_bg_dark();
    ui_panel_title("Share Scene");

    if (s_uds_error) {
        ui_draw_centered(BOTTOM_W * 0.5f, ui_panel_content_top() + 8.0f,
                         0.0f, 0.45f, ui_color_accent(),
                         "Wireless not available!");
    }

    draw_role_button(60.0f,  s_cursor == ROLE_HOST,
                     "Host",     "Wait for someone to connect");
    draw_role_button(112.0f, s_cursor == ROLE_DISCOVER,
                     "Discover", "Find a host nearby");

    ui_panel_footer_hint("[A] Pick   [B] Back");
}

static AppState s_share_menu = {
    .uses_direct_framebuffer = false,
    .enter         = share_menu_enter,
    .exit          = share_menu_exit,
    .update        = share_menu_update,
    .render_top    = share_menu_render_top,
    .render_bottom = share_menu_render_bottom,
};

AppState *state_share_menu_create(void) { return &s_share_menu; }
