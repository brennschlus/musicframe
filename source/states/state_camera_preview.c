#include "state_camera_preview.h"
#include "../state/state_manager.h"
#include "../image/image_texture.h"
#include <stdio.h>

#define TOP_W 400
#define TOP_H 240

// We keep a temporary image buffer for preview
static ImageBuffer* s_preview_img = NULL;

static void camera_preview_enter(AppState* self, AppContext* ctx)
{
    (void)self;
    
    // Allocate temporary image buffer for the preview
    if (!s_preview_img) {
        s_preview_img = image_buffer_create(TOP_W, TOP_H);
    }
    
    consoleClear();
    printf("\x1b[1;1H");
    printf("===========================\n");
    printf("   Camera Preview\n");
    printf("===========================\n\n");

    if (ctx->camera.initialized) {
        printf("  [A] Take Photo\n");
        printf("  [B] Cancel / Back\n\n");
        printf("  Starting camera...\n");
        hw_camera_start(&ctx->camera);
    } else {
        printf("  [ERROR] Camera failed to init\n");
        printf("  [B] Back\n");
    }
}

static void camera_preview_exit(AppState* self, AppContext* ctx)
{
    (void)self;
    if (ctx->camera.capturing) {
        hw_camera_stop(&ctx->camera);
    }
    // We do NOT free s_preview_img if we transition to PHOTO_REVIEW because we transfer ownership.
    // If we transition to Main Menu (Cancel), we can free it.
    // Actually, state callbacks don't know the destination state!
    // So we'll handle ownership transfer explicitly on KEY_A.
}

static void camera_preview_update(AppState* self, AppContext* ctx)
{
    (void)self;

    u32 kDown = hidKeysDown();

    // If cancel
    if (kDown & KEY_B) {
        hw_camera_stop(&ctx->camera);
        if (s_preview_img) {
            image_buffer_destroy(s_preview_img);
            s_preview_img = NULL;
        }
        state_manager_transition(ctx, STATE_MAIN_MENU);
        return;
    }

    if (!ctx->camera.capturing) {
        return; // camera failed to start, just wait for B
    }

    // Try to get next frame from hardware camera
    if (hw_camera_wait_next_frame(&ctx->camera, 0)) { // 0 timeout = non-blocking poll
        if (s_preview_img) {
            hw_camera_get_frame_rgba8(&ctx->camera, s_preview_img);
            
            // To preview it live, upload to scene texture
            image_texture_upload(s_preview_img, &ctx->scene.tex, &ctx->scene.subtex, &ctx->scene.tex_initialized);
            ctx->scene.photo_loaded = true;
        }
    }

    // Take photo
    if (kDown & KEY_A) {
        hw_camera_play_shutter(&ctx->camera);
        hw_camera_stop(&ctx->camera);
        
        // Transfer ownership of s_preview_img to scene
        scene_model_set_photo(&ctx->scene, s_preview_img);
        s_preview_img = NULL; // Scene owns it now
        
        state_manager_transition(ctx, STATE_PHOTO_REVIEW);
    }
}

static void camera_preview_render_top(AppState* self, AppContext* ctx, C3D_RenderTarget* target)
{
    (void)self;

    u32 clrBg = C2D_Color32(0, 0, 0, 0xFF);
    C2D_TargetClear(target, clrBg);
    C2D_SceneBegin(target);

    // If texture is active, draw the live preview
    if (ctx->scene.tex_initialized) {
        C2D_Image img;
        img.tex    = &ctx->scene.tex;
        img.subtex = &ctx->scene.subtex;
        C2D_DrawImageAt(img, 0, 0, 0.5f, NULL, 1.0f, 1.0f);
    }
}

static void camera_preview_render_bottom(AppState* self, AppContext* ctx)
{
    (void)self;
    (void)ctx;
}

// ---------------------------------------------------------------------------
static AppState s_camera_preview = {
    .enter         = camera_preview_enter,
    .exit          = camera_preview_exit,
    .update        = camera_preview_update,
    .render_top    = camera_preview_render_top,
    .render_bottom = camera_preview_render_bottom,
};

AppState* state_camera_preview_create(void)
{
    return &s_camera_preview;
}
