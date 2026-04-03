#ifndef SCENE_MODEL_H
#define SCENE_MODEL_H

#include <stdbool.h>
#include <citro2d.h>
#include "../image/image_buffer.h"
#include "../image/image_filters.h"
#include "../image/image_texture.h"

// ---------------------------------------------------------------------------
// SceneModel — data for the current "photo + style + music" composition
// ---------------------------------------------------------------------------
typedef struct {
    ImageBuffer*  original;        // Source image (immutable after load)
    ImageBuffer*  filtered;        // After filter applied

    ImageTexture  texture;         // GPU texture + UV sub-region

    bool          photo_loaded;    // Whether a photo is loaded
    int           selected_filter; // Current filter id (FILTER_*)
    int           selected_frame;  // Current frame id

    char          music_path[256]; // Path to selected .wav file
    bool          music_selected;  // Whether a music track is chosen
} SceneModel;

// Free all resources (image buffers + GPU texture)
void scene_model_cleanup(SceneModel* scene);

// Set a new photo, transferring ownership of the ImageBuffer to the scene
void scene_model_set_photo(SceneModel* scene, ImageBuffer* photo);

// Re-apply the current filter and re-upload texture
void scene_model_apply_filter(SceneModel* scene);

// Draw the photo centred on the top screen at depth z.
// Must be called inside C2D_SceneBegin / C3D_FrameBegin block.
void scene_model_draw_photo_centered(const SceneModel* scene, float z);

#endif // SCENE_MODEL_H
