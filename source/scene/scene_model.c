#include "scene_model.h"
#include "../image/image_texture.h"



// ---------------------------------------------------------------------------
void scene_model_cleanup(SceneModel* scene)
{
    image_texture_cleanup(&scene->tex, &scene->tex_initialized);
    image_buffer_destroy(scene->original);
    image_buffer_destroy(scene->filtered);
    scene->original     = NULL;
    scene->filtered     = NULL;
    scene->photo_loaded = false;
    scene->music_selected = false;
    scene->music_path[0]  = '\0';
}

// ---------------------------------------------------------------------------
void scene_model_load_test_image(SceneModel* scene)
{
    // Clean up any previous image
    if (scene->photo_loaded) {
        scene_model_cleanup(scene);
    }

    scene->original = image_buffer_create_test_pattern();
    if (!scene->original) return;

    scene->filtered = image_buffer_create(scene->original->width,
                                          scene->original->height);
    if (!scene->filtered) {
        image_buffer_destroy(scene->original);
        scene->original = NULL;
        return;
    }

    scene->photo_loaded    = true;
    scene->selected_filter = FILTER_NONE;

    // Apply initial filter (none = copy) and upload to GPU
    scene_model_apply_filter(scene);
}

// ---------------------------------------------------------------------------
void scene_model_apply_filter(SceneModel* scene)
{
    if (!scene->photo_loaded || !scene->original || !scene->filtered)
        return;

    image_filter_apply(scene->selected_filter,
                       scene->original, scene->filtered);

    image_texture_upload(scene->filtered, &scene->tex,
                         &scene->subtex, &scene->tex_initialized);
}
