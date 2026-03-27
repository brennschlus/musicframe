#include "state_manager.h"

// Include all concrete state headers
#include "../states/state_main_menu.h"
#include "../states/state_photo_review.h"
#include "../states/state_filter_select.h"
#include "../states/state_frame_select.h"
#include "../states/state_music_select.h"
#include "../states/state_playback_view.h"

// ---------------------------------------------------------------------------
// State registry — one AppState* per AppStateId
// ---------------------------------------------------------------------------
static AppState* s_states[STATE_COUNT];
static AppState* s_current = NULL;

// ---------------------------------------------------------------------------
void state_manager_init(void)
{
    // Clear registry
    for (int i = 0; i < STATE_COUNT; i++) {
        s_states[i] = NULL;
    }

    // Register concrete states
    s_states[STATE_MAIN_MENU]     = state_main_menu_create();
    s_states[STATE_PHOTO_REVIEW]  = state_photo_review_create();
    s_states[STATE_FILTER_SELECT] = state_filter_select_create();
    s_states[STATE_FRAME_SELECT]  = state_frame_select_create();
    s_states[STATE_MUSIC_SELECT]  = state_music_select_create();
    s_states[STATE_PLAYBACK_VIEW] = state_playback_view_create();

    s_current = NULL;
}

// ---------------------------------------------------------------------------
void state_manager_shutdown(void)
{
    s_current = NULL;

    // States are statically allocated, no need to free.
    for (int i = 0; i < STATE_COUNT; i++) {
        s_states[i] = NULL;
    }
}

// ---------------------------------------------------------------------------
void state_manager_transition(AppContext* ctx, AppStateId next)
{
    if (next > STATE_NONE && next < STATE_COUNT) {
        ctx->next_state = next;
    }
}

// ---------------------------------------------------------------------------
void state_manager_apply_transition(AppContext* ctx)
{
    if (ctx->next_state == STATE_NONE) {
        return; // No pending transition
    }

    AppState* next = s_states[ctx->next_state];
    if (!next) {
        ctx->next_state = STATE_NONE;
        return; // Target state not registered
    }

    // Exit current state
    if (s_current && s_current->exit) {
        s_current->exit(s_current, ctx);
    }

    // Switch
    ctx->current_state = ctx->next_state;
    ctx->next_state = STATE_NONE;
    s_current = next;

    // Enter new state
    if (s_current->enter) {
        s_current->enter(s_current, ctx);
    }
}

// ---------------------------------------------------------------------------
void state_manager_update(AppContext* ctx)
{
    if (s_current && s_current->update) {
        s_current->update(s_current, ctx);
    }
}

// ---------------------------------------------------------------------------
void state_manager_render_top(AppContext* ctx, C3D_RenderTarget* target)
{
    if (s_current && s_current->render_top) {
        s_current->render_top(s_current, ctx, target);
    }
}

// ---------------------------------------------------------------------------
void state_manager_render_bottom(AppContext* ctx)
{
    if (s_current && s_current->render_bottom) {
        s_current->render_bottom(s_current, ctx);
    }
}
