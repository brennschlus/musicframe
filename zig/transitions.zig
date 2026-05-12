// transitions.zig
//
// Pure state machine for musicframe.
// No allocations, no platform dependencies.

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

pub const AppStateId = enum(c_int) {
    none = 0,
    main_menu,
    camera_preview,
    photo_review,
    filter_select,
    frame_select,
    music_select,
    playback_view,
    moment_browser,
    photo_library,
};

/// Platform translates hardware input + conditions into these triggers.
/// Composite triggers (photo_captured, track_selected) let the platform
/// evaluate guard conditions before firing — keeps this module condition-free.
pub const Trigger = enum(u8) {
    key_a,
    key_b,
    key_start,
    photo_captured,  // KEY_A && camera.frame_ready
    track_selected,  // KEY_A && library.count > 0
    key_select,      // open moment browser (main_menu) or inline save (playback_view)
    moment_selected, // moment loaded successfully in browser
    key_y,           // open photo library from main_menu
    photo_selected,  // photo from library loaded into scene → photo_review
};

// ---------------------------------------------------------------------------
// Transition table
// ---------------------------------------------------------------------------

const Transition = struct {
    from: AppStateId,
    trigger: Trigger,
    to: AppStateId,
};

const table = [_]Transition{
    // main_menu
    .{ .from = .main_menu, .trigger = .key_a, .to = .camera_preview },
    // camera_preview
    .{ .from = .camera_preview, .trigger = .photo_captured, .to = .photo_review },
    .{ .from = .camera_preview, .trigger = .key_b, .to = .main_menu },
    // photo_review
    .{ .from = .photo_review, .trigger = .key_a, .to = .filter_select },
    .{ .from = .photo_review, .trigger = .key_b, .to = .main_menu },
    // filter_select
    .{ .from = .filter_select, .trigger = .key_a, .to = .frame_select },
    .{ .from = .filter_select, .trigger = .key_b, .to = .photo_review },
    // frame_select
    .{ .from = .frame_select, .trigger = .key_a, .to = .music_select },
    .{ .from = .frame_select, .trigger = .key_b, .to = .filter_select },
    // music_select
    .{ .from = .music_select, .trigger = .track_selected, .to = .playback_view },
    .{ .from = .music_select, .trigger = .key_b, .to = .frame_select },
    // playback_view
    .{ .from = .playback_view,   .trigger = .key_b,           .to = .main_menu      },
    // moment_browser
    .{ .from = .main_menu,      .trigger = .key_select,      .to = .moment_browser },
    .{ .from = .moment_browser, .trigger = .key_b,           .to = .main_menu      },
    .{ .from = .moment_browser, .trigger = .moment_selected, .to = .playback_view  },
    // photo_library
    .{ .from = .main_menu,      .trigger = .key_y,           .to = .photo_library  },
    .{ .from = .photo_library,  .trigger = .key_b,           .to = .main_menu      },
    .{ .from = .photo_library,  .trigger = .photo_selected,  .to = .photo_review   },
};

// ---------------------------------------------------------------------------
// Public API — exported with C ABI for linkage from C
// ---------------------------------------------------------------------------

/// Returns the next state given the current state and a trigger.
/// If no transition matches, returns `current` unchanged.
export fn app_next_state(current: AppStateId, trigger: Trigger) AppStateId {
    for (table) |t| {
        if (t.from == current and t.trigger == trigger) return t.to;
    }
    return current;
}

// ---------------------------------------------------------------------------
// Tests — run on desktop with: zig test transitions.zig
// ---------------------------------------------------------------------------

test "happy path: full scene flow" {
    const t = std.testing;

    var s: AppStateId = .main_menu;

    s = app_next_state(s, .key_a);
    try t.expectEqual(AppStateId.camera_preview, s);

    s = app_next_state(s, .photo_captured);
    try t.expectEqual(AppStateId.photo_review, s);

    s = app_next_state(s, .key_a);
    try t.expectEqual(AppStateId.filter_select, s);

    s = app_next_state(s, .key_a);
    try t.expectEqual(AppStateId.frame_select, s);

    s = app_next_state(s, .key_a);
    try t.expectEqual(AppStateId.music_select, s);

    s = app_next_state(s, .track_selected);
    try t.expectEqual(AppStateId.playback_view, s);

    s = app_next_state(s, .key_b);
    try t.expectEqual(AppStateId.main_menu, s);
}

test "back navigation unwinds correctly" {
    const t = std.testing;

    var s: AppStateId = .frame_select;

    s = app_next_state(s, .key_b);
    try t.expectEqual(AppStateId.filter_select, s);

    s = app_next_state(s, .key_b);
    try t.expectEqual(AppStateId.photo_review, s);

    s = app_next_state(s, .key_b);
    try t.expectEqual(AppStateId.main_menu, s);
}

test "unknown trigger leaves state unchanged" {
    const t = std.testing;

    // photo_captured has no meaning in main_menu
    const s = app_next_state(.main_menu, .photo_captured);
    try t.expectEqual(AppStateId.main_menu, s);
}

test "key_a without frame_ready never fires from platform side" {
    // This is a platform responsibility — but we verify that
    // key_a alone has no transition from camera_preview.
    const t = std.testing;

    const s = app_next_state(.camera_preview, .key_a);
    try t.expectEqual(AppStateId.camera_preview, s);
}

test "moment browser: open, load, back" {
    const t = std.testing;

    // main_menu → moment_browser
    var s = app_next_state(.main_menu, .key_select);
    try t.expectEqual(AppStateId.moment_browser, s);

    // moment_browser → playback_view on successful load
    s = app_next_state(.moment_browser, .moment_selected);
    try t.expectEqual(AppStateId.playback_view, s);

    // moment_browser → main_menu on back
    s = app_next_state(.moment_browser, .key_b);
    try t.expectEqual(AppStateId.main_menu, s);
}

test "photo library: open, select, back" {
    const t = std.testing;

    var s = app_next_state(.main_menu, .key_y);
    try t.expectEqual(AppStateId.photo_library, s);

    s = app_next_state(.photo_library, .photo_selected);
    try t.expectEqual(AppStateId.photo_review, s);

    s = app_next_state(.photo_library, .key_b);
    try t.expectEqual(AppStateId.main_menu, s);
}

test "key_select has no effect outside main_menu" {
    const t = std.testing;

    // key_select in other states leaves state unchanged
    const s = app_next_state(.camera_preview, .key_select);
    try t.expectEqual(AppStateId.camera_preview, s);
}

const std = @import("std");

pub fn panic(msg: []const u8, _: ?*std.builtin.StackTrace, _: ?usize) noreturn {
    _ = msg;
    while (true) {}
}
