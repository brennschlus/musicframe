#ifndef APP_CONTEXT_H
#define APP_CONTEXT_H

#include <stdbool.h>
#include <citro2d.h>
#include "scene/scene_model.h"
#include "audio/audio_player.h"

// ---------------------------------------------------------------------------
// Application state identifiers
// ---------------------------------------------------------------------------
typedef enum {
    STATE_NONE = 0,
    STATE_MAIN_MENU,
    STATE_PHOTO_REVIEW,
    STATE_FILTER_SELECT,
    STATE_FRAME_SELECT,
    STATE_MUSIC_SELECT,
    STATE_PLAYBACK_VIEW,
    // Future states:
    // STATE_CAMERA_PREVIEW,
    STATE_COUNT
} AppStateId;

// ---------------------------------------------------------------------------
// Application context — passed to every state
// ---------------------------------------------------------------------------
typedef struct {
    // State machine
    AppStateId current_state;
    AppStateId next_state;

    // Lifecycle
    bool running;

    // Render targets (owned by main, shared read-only)
    C3D_RenderTarget* top_target;

    // Current scene (photo + filter + frame + music)
    SceneModel scene;

    // Audio player
    AudioPlayer audio;
} AppContext;

#endif // APP_CONTEXT_H
