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
    STATE_MOMENT_BROWSER,
    STATE_PHOTO_LIBRARY,
    STATE_SHARE_MENU,
    STATE_SHARE_HOST,
    STATE_SHARE_CLIENT,
    STATE_COUNT
} AppStateId;

/// Input triggers — platform translates hardware + guards into these.
typedef enum {
    TRIGGER_KEY_A = 0,
    TRIGGER_KEY_B,
    TRIGGER_KEY_START,
    TRIGGER_PHOTO_CAPTURED,         // KEY_A && camera.frame_ready
    TRIGGER_TRACK_SELECTED,         // KEY_A && library.count > 0
    TRIGGER_KEY_SELECT,             // open moment browser (main_menu) or inline save (playback_view)
    TRIGGER_MOMENT_SELECTED,        // moment loaded successfully in browser
    TRIGGER_KEY_Y,                  // open photo library from main_menu
    TRIGGER_PHOTO_SELECTED,         // photo from library loaded into scene
    TRIGGER_SHARE,                  // KEY_X in moment_browser → share menu
    TRIGGER_SHARE_ROLE_HOST,        // share_menu picked Host
    TRIGGER_SHARE_ROLE_DISCOVER,    // share_menu picked Discover
} Trigger;

/// Returns the next state for (current, trigger).
/// Returns `current` if no transition is defined.
AppStateId app_next_state(AppStateId current, Trigger trigger);

#endif // TRANSITIONS_H
