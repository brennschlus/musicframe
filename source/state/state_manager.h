#ifndef STATE_MANAGER_H
#define STATE_MANAGER_H

#include "../app_context.h"
#include "app_state.h"

// ---------------------------------------------------------------------------
// State Manager
//
// Owns the registry of AppState instances (one per AppStateId).
// Handles transitions safely between frames.
// ---------------------------------------------------------------------------

// Initialize state manager: register all known states
void state_manager_init(void);

// Clean up all states
void state_manager_shutdown(void);

// Request a state transition. The actual switch happens in
// state_manager_apply_transition() — called between update and render.
void state_manager_transition(AppContext* ctx, AppStateId next);

// Process any pending state transition (call between frames)
void state_manager_apply_transition(AppContext* ctx);

// Delegate update to the current active state
void state_manager_update(AppContext* ctx);

// Render the top screen. Handles C3D_FrameBegin/End internally for normal
// states, or gfxFlush/Swap for states with uses_direct_framebuffer == true.
void state_manager_render_top(AppContext* ctx);

// Delegate render_bottom to the current active state
void state_manager_render_bottom(AppContext* ctx);

#endif // STATE_MANAGER_H
