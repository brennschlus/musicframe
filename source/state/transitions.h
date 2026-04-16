#ifndef TRANSITIONS_H
#define TRANSITIONS_H

// ---------------------------------------------------------------------------
// transitions.h
//
// C interface for the Zig state machine module (libtransitions.a).
// Keep in sync with transitions.zig.
// ---------------------------------------------------------------------------

typedef enum {
    STATE_NONE = 0,
    STATE_MAIN_MENU,
    STATE_CAMERA_PREVIEW,
    STATE_PHOTO_REVIEW,
    STATE_FILTER_SELECT,
    STATE_FRAME_SELECT,
    STATE_MUSIC_SELECT,
    STATE_PLAYBACK_VIEW,
    STATE_COUNT
} AppStateId;

/// Input triggers — platform translates hardware + guards into these.
typedef enum {
    TRIGGER_KEY_A = 0,
    TRIGGER_KEY_B,
    TRIGGER_KEY_START,
    TRIGGER_PHOTO_CAPTURED,  // KEY_A && camera.frame_ready
    TRIGGER_TRACK_SELECTED,  // KEY_A && library.count > 0
} Trigger;

/// Returns the next state for (current, trigger).
/// Returns `current` if no transition is defined.
AppStateId app_next_state(AppStateId current, Trigger trigger);

#endif // TRANSITIONS_H
