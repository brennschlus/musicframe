#include "scene_model.h"
#include "../image/image_texture.h"

#define TOP_SCREEN_W 400
#define TOP_SCREEN_H 240

// ---------------------------------------------------------------------------
void scene_model_cleanup(SceneModel* scene)
{
    image_texture_cleanup(&scene->texture);
    image_buffer_destroy(scene->original);
    image_buffer_destroy(scene->filtered);
    scene->original       = NULL;
    scene->filtered       = NULL;
    scene->photo_loaded   = false;
    scene->music_selected = false;
    scene->music_path[0]  = '\0';
}

// ---------------------------------------------------------------------------
void scene_model_set_photo(SceneModel* scene, ImageBuffer* photo)
{
    if (!photo) return;
    scene_model_cleanup(scene);

    scene->original       = photo;
    scene->filtered       = image_buffer_create(photo->width, photo->height);
    scene->photo_loaded   = true;
    scene->selected_filter = FILTER_NONE;
    scene->selected_frame  = 0;

    scene_model_apply_filter(scene);
}

// ---------------------------------------------------------------------------
void scene_model_apply_filter(SceneModel* scene)
{
    if (!scene->photo_loaded || !scene->original || !scene->filtered)
        return;

    image_filter_apply(scene->selected_filter, scene->original, scene->filtered);
    image_texture_upload(scene->filtered, &scene->texture);
}

// ---------------------------------------------------------------------------
void scene_model_draw_photo_centered(const SceneModel* scene, float z)
{
    if (!scene->photo_loaded || !scene->texture.initialized)
        return;

    C2D_Image img;
    img.tex    = (C3D_Tex*)&scene->texture.tex;
    img.subtex = &scene->texture.subtex;

    float x = (TOP_SCREEN_W - scene->texture.subtex.width)  / 2.0f;
    float y = (TOP_SCREEN_H - scene->texture.subtex.height) / 2.0f;

    C2D_DrawImageAt(img, x, y, z, NULL, 1.0f, 1.0f);
}
