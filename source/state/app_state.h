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
    // If true, render_top writes directly to the raw framebuffer (no C3D).
    // The state manager will call gfxFlushBuffers / gspWaitForVBlank /
    // gfxSwapBuffers instead of C3D_FrameBegin / C3D_FrameEnd.
    bool uses_direct_framebuffer;

    // Called once when entering this state
    void (*enter)(struct AppState* self, AppContext* ctx);

    // Called once when leaving this state
    void (*exit)(struct AppState* self, AppContext* ctx);

    // Called every frame — handle input, update logic, request transitions
    void (*update)(struct AppState* self, AppContext* ctx);

    // Called every frame — draw on top screen.
    // Access ctx->top_target for the citro2d render target.
    void (*render_top)(struct AppState* self, AppContext* ctx);

    // Called every frame — draw on bottom screen using citro2d.
    // Invoked inside C2D_SceneBegin(ctx->bottom_target) by the state manager.
    void (*render_bottom)(struct AppState* self, AppContext* ctx);
} AppState;

#endif // APP_STATE_H
