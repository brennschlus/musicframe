#ifndef MOMENT_STORE_H
#define MOMENT_STORE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "../scene/scene_model.h"

#define MOMENTS_DIR  "sdmc:/3ds/musicframe/moments/"
#define MAX_MOMENTS  32

// Metadata read from the file header only (no pixel data loaded).
typedef struct {
    char filename[64];   // e.g. "20260512_143022.moment"
    int  filter_id;
    int  frame_id;
    char music_name[64]; // basename of music_path, or "" if none
} MomentInfo;

// Save current scene to a timestamped .moment file in MOMENTS_DIR.
// Returns 0 on success, -1 on error.
int moment_store_save(const SceneModel *scene);

// Fill `out` by reading only the header of each .moment file (no pixels loaded).
// Returns the number of entries filled (up to max_count).
int moment_store_list(MomentInfo *out, int max_count);

// Load a moment by filename (basename only) into scene. Does NOT start audio.
// Returns:
//   0  — success
//  -1  — file error (scene unchanged)
//  -2  — music file missing (scene loaded without music, music_selected = false)
int moment_store_load(const char *filename, SceneModel *scene);

// ---------------------------------------------------------------------------
// Byte-level helpers for sharing — read/write whole files as opaque blobs.
// ---------------------------------------------------------------------------

// Validate that `name` is a safe basename: only [A-Za-z0-9_.-], no '/',
// no leading dot, ends with the given extension. Returns true if safe.
bool moment_store_basename_safe(const char *name, const char *ext);

// Read an entire .moment file into a freshly-malloc'd buffer.
// Caller is responsible for free()-ing *out_buf on success.
// Returns 0 on success, -1 on error.
int moment_store_read_bytes(const char *filename, uint8_t **out_buf, size_t *out_len);

// Write `buf` to MOMENTS_DIR/`filename`. Validates the MFMM magic + version
// in the buffer before writing. Filename must pass moment_store_basename_safe().
// Returns 0 on success, -1 on error.
int moment_store_write_bytes(const char *filename, const uint8_t *buf, size_t len);

// Resolve a music basename (e.g. "song.wav") to its full sdmc path.
// Returns 0 on success and writes the path into out_path, -1 on overflow.
int moment_store_resolve_wav(const char *basename, char *out_path, size_t cap);

// Read an entire .wav file from MUSIC_DIR/`basename` into a fresh buffer.
// Caller frees *out_buf on success. Returns 0 on success, -1 on error.
int moment_store_read_wav(const char *basename, uint8_t **out_buf, size_t *out_len);

// Write a .wav blob into MUSIC_DIR/`basename`. Basename must pass
// moment_store_basename_safe() with ext ".wav".
// Returns 0 on success, -1 on error.
int moment_store_write_wav(const char *basename, const uint8_t *buf, size_t len);

#endif // MOMENT_STORE_H
