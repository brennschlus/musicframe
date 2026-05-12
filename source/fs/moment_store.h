#ifndef MOMENT_STORE_H
#define MOMENT_STORE_H

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

#endif // MOMENT_STORE_H
