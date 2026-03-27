#ifndef APP_STATE_H
#define APP_STATE_H

#include <citro3d.h>

// AppContext is defined in app_context.h — include it here so all
// state implementations get access through this single header.
#include "../app_context.h"

// ---------------------------------------------------------------------------
// AppState — virtual interface for application screens
//
// Each screen (main menu, photo review, playback, etc.) implements
// this interface. The state manager calls these in the correct order.
// ---------------------------------------------------------------------------
typedef struct AppState {
    // Called once when entering this state
    void (*enter)(struct AppState* self, AppContext* ctx);

    // Called once when leaving this state
    void (*exit)(struct AppState* self, AppContext* ctx);

    // Called every frame — handle input, update logic, request transitions
    void (*update)(struct AppState* self, AppContext* ctx);

    // Called every frame inside C3D_FrameBegin/End — draw on top screen
    void (*render_top)(struct AppState* self, AppContext* ctx, C3D_RenderTarget* target);

    // Called every frame — output to bottom screen (console printf for now)
    void (*render_bottom)(struct AppState* self, AppContext* ctx);
} AppState;

#endif // APP_STATE_H
