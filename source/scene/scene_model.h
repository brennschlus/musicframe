#ifndef SCENE_MODEL_H
#define SCENE_MODEL_H

#include <stdbool.h>
#include <citro2d.h>
#include "../image/image_buffer.h"
#include "../image/image_filters.h"

// ---------------------------------------------------------------------------
// SceneModel — data for the current "photo + style + music" composition
// ---------------------------------------------------------------------------
typedef struct {
    ImageBuffer*      original;        // Source image (immutable after load)
    ImageBuffer*      filtered;        // After filter applied

    C3D_Tex           tex;             // GPU texture for display
    Tex3DS_SubTexture subtex;          // UV region within pow2 texture
    bool              tex_initialized; // Whether tex has been allocated

    bool              photo_loaded;    // Whether a photo is loaded
    int               selected_filter; // Current filter id (FILTER_*)
    int               selected_frame;  // Current frame id (for Phase 4)

    char              music_path[256]; // Path to selected .wav file
    bool              music_selected;  // Whether a music track is chosen
} SceneModel;

// Zero-initialize a scene
void scene_model_init(SceneModel* scene);

// Free all resources (image buffers + GPU texture)
void scene_model_cleanup(SceneModel* scene);

// Load a procedural test image into the scene
void scene_model_load_test_image(SceneModel* scene);

// Re-apply the current filter and re-upload texture
void scene_model_apply_filter(SceneModel* scene);

#endif // SCENE_MODEL_H
