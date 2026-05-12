# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

**Music Photo Frame** — a Nintendo 3DS homebrew app. Take a photo, apply a filter and frame, pick a `.wav` track, and vibe. Targets ARM (ARMv6K/MPCore) via devkitPro.

## Build commands

Requires `DEVKITARM` to be set in the environment:

```bash
export DEVKITARM=<path to>/devkitARM
```

| Command | Effect |
|---|---|
| `make` / `make all` | Build Zig library then full project → `musicframe.3dsx` |
| `make clean` | Remove `build/`, `.elf`, `.3dsx`, `.smdh`, Zig lib |
| `make run` | Clean + build + send over Wi-Fi via `3dslink` |
| `make send` | Send current `.3dsx` over Wi-Fi without rebuilding |

**Testing the Zig state machine** (runs on desktop, no hardware needed):

```bash
zig test zig/transitions.zig
```

## Architecture

### Dual-language design

The project is C + Zig. The state transition table lives entirely in `zig/transitions.zig` and is compiled into `zig/libtransitions.a`. The C code links against it via the C ABI header `source/state/transitions.h`. **Keep `AppStateId` and `Trigger` enums in sync between the two files.**

### State machine

`AppContext` (`source/app_context.h`) is the central struct passed to every state. It holds:
- `current_state` / `next_state` — the active `AppStateId`
- `SceneModel scene` — the photo + filter + frame + music selection
- `AudioPlayer audio` — NDSP-backed WAV player
- `HardwareCamera camera` — camera service handle

`state_manager` (`source/state/state_manager.c`) owns a registry of `AppState*` (one per `AppStateId`). Each state implements the `AppState` vtable: `enter`, `exit`, `update`, `render_top`, `render_bottom`.

State flow (forward):
```
main_menu → camera_preview → photo_review → filter_select → frame_select → music_select → playback_view
```
Back navigation (KEY_B) unwinds one step. The authoritative table is in `zig/transitions.zig`.

### Rendering

The state manager drives a single `C3D_FrameBegin / C3D_FrameEnd` block per frame covering both screens, except when `AppState.uses_direct_framebuffer == true` (camera preview): in that case the top screen writes directly to the raw framebuffer and the bottom screen gets its own separate `C3D_FrameBegin` block.

### Source layout

```
source/
  app_context.h          # Central AppContext struct
  main.c                 # Init, main loop, cleanup
  state/                 # State manager + AppState vtable + transition header
  states/                # One .c/.h pair per screen
  image/                 # ImageBuffer, ImageTexture, image_filters, image_frames
  scene/                 # SceneModel (photo + filter + frame + music)
  audio/                 # AudioPlayer (NDSP, PCM WAV, ≤8 MB)
  camera/                # HardwareCamera (cam service)
  fs/                    # MusicLibrary (scans sdmc:/3ds/musicframe/music/)
  ui/                    # ui_text (system font), ui_panel (colored rects)
zig/
  transitions.zig        # State machine table + Zig tests
```

### Adding a new state

1. Add the enum value to both `AppStateId` in `source/state/transitions.h` and `pub const AppStateId` in `zig/transitions.zig`.
2. Add transitions to the table in `zig/transitions.zig`.
3. Create `source/states/state_<name>.c/.h` implementing the `AppState` vtable.
4. Register it in `state_manager_init()` in `source/state/state_manager.c`.

### Adding a new filter or frame

- Filters: increment `FILTER_COUNT` in `source/image/image_filters.h`, add a case in `image_filter_apply()` and `image_filter_name()`.
- Frames: increment `FRAME_COUNT` in `source/image/image_frames.h`, add a case in `image_frame_draw()` and `image_frame_name()`.

## WAV constraints

Audio loader (`source/audio/audio_player.c`) only accepts: RIFF/WAVE, PCM (`audio_format == 1`), 16-bit, mono or stereo, PCM data chunk ≤ 8 MB.
