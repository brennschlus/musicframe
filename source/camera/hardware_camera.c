#include "hardware_camera.h"
#include "3ds/allocator/linear.h"

#include <stdbool.h>
#include <string.h>

#define CAM_W 400
#define CAM_H 240
#define CAM_BPP 2
#define CAM_FRAME_WAIT_NS 300000000ULL

bool hw_camera_init(HardwareCamera* cam)
{
    if (!cam) return false;

    cam->initialized = false;
    cam->preview_active = false;
    cam->frame_ready = false;
    cam->receive_event = 0;
    cam->frame_buffer = NULL;
    cam->frame_size = CAM_W * CAM_H * CAM_BPP;
    cam->transfer_bytes = 0;

    Result res = camInit();
    if (R_FAILED(res)) {
        return false;
    }

    res = CAMU_SetSize(SELECT_OUT1, SIZE_CTR_TOP_LCD, CONTEXT_A);
    if (R_FAILED(res)) goto fail;

    res = CAMU_SetOutputFormat(SELECT_OUT1, OUTPUT_RGB_565, CONTEXT_A);
    if (R_FAILED(res)) goto fail;

    res = CAMU_SetNoiseFilter(SELECT_OUT1, true);
    if (R_FAILED(res)) goto fail;

    res = CAMU_SetAutoExposure(SELECT_OUT1, true);
    if (R_FAILED(res)) goto fail;

    res = CAMU_SetAutoWhiteBalance(SELECT_OUT1, true);
    if (R_FAILED(res)) goto fail;

    res = CAMU_SetTrimming(PORT_CAM1, false);
    if (R_FAILED(res)) goto fail;

    cam->frame_buffer = (u16*)linearAlloc(cam->frame_size);
    if (!cam->frame_buffer) goto fail;

    memset(cam->frame_buffer, 0, cam->frame_size);

    res = CAMU_GetMaxBytes(&cam->transfer_bytes, CAM_W, CAM_H);
    if (R_FAILED(res)) goto fail;

    res = CAMU_SetTransferBytes(PORT_CAM1, cam->transfer_bytes, CAM_W, CAM_H);
    if (R_FAILED(res)) goto fail;

    cam->initialized = true;
    return true;

fail:
    if (cam->frame_buffer) {
        linearFree(cam->frame_buffer);
        cam->frame_buffer = NULL;
    }

    cam->initialized = false;
    cam->preview_active = false;
    cam->frame_ready = false;
    cam->receive_event = 0;
    cam->transfer_bytes = 0;

    camExit();
    return false;
}

void hw_camera_stop(HardwareCamera* cam)
{
    if (!cam) return;

    if (cam->receive_event != 0) {
        svcCloseHandle(cam->receive_event);
        cam->receive_event = 0;
    }

    (void)CAMU_StopCapture(PORT_CAM1);
    (void)CAMU_Activate(SELECT_NONE);

    cam->preview_active = false;
}

void hw_camera_shutdown(HardwareCamera* cam)
{
    if (!cam) return;

    hw_camera_stop(cam);

    if (cam->frame_buffer) {
        linearFree(cam->frame_buffer);
        cam->frame_buffer = NULL;
    }

    if (cam->initialized) {
        camExit();
    }

    cam->initialized = false;
    cam->preview_active = false;
    cam->frame_ready = false;
    cam->receive_event = 0;
    cam->transfer_bytes = 0;
    cam->frame_size = 0;
}

bool hw_camera_capture_preview_frame(HardwareCamera* cam)
{
    if (!cam || !cam->initialized || !cam->preview_active) {
        return false;
    }

    cam->frame_ready = false;

    if (cam->receive_event != 0) {
        svcCloseHandle(cam->receive_event);
        cam->receive_event = 0;
    }

    Result res = CAMU_Activate(SELECT_OUT1);
    if (R_FAILED(res)) {
        return false;
    }

    res = CAMU_ClearBuffer(PORT_CAM1);
    if (R_FAILED(res)) goto fail;

    res = CAMU_StartCapture(PORT_CAM1);
    if (R_FAILED(res)) goto fail;

    res = CAMU_SetReceiving(&cam->receive_event,
                            cam->frame_buffer,
                            PORT_CAM1,
                            cam->frame_size,
                            (s16)cam->transfer_bytes);
    if (R_FAILED(res)) goto fail;

    res = svcWaitSynchronization(cam->receive_event, CAM_FRAME_WAIT_NS);
    if (R_FAILED(res)) goto fail;
    GSPGPU_InvalidateDataCache(cam->frame_buffer, cam->frame_size);

    svcCloseHandle(cam->receive_event);
    cam->receive_event = 0;

    (void)CAMU_StopCapture(PORT_CAM1);
    (void)CAMU_Activate(SELECT_NONE);

    cam->frame_ready = true;
    return true;

fail:
    if (cam->receive_event != 0) {
        svcCloseHandle(cam->receive_event);
        cam->receive_event = 0;
    }

    (void)CAMU_StopCapture(PORT_CAM1);
    (void)CAMU_Activate(SELECT_NONE);

    cam->frame_ready = false;
    return false;
}



// DO NOT TOUCH, THIS FUNCTION WORKS FLAWLESSLY!
void hw_draw_camera_preview_top(const u16* src)
{
    u16 fbWidth, fbHeight;
    u8* fb = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &fbWidth, &fbHeight);

    for (int y = 0; y < CAM_H; y++) {
        for (int x = 0; x < CAM_W; x++) {
            u16 pixel = src[y * CAM_W + x];

            int drawY = CAM_H - 1 - y;
            int drawX = x;

            // top framebuffer в RGB8 (3 байта на пиксель)
            u32 dst = (drawY + drawX * CAM_H) * 3;

            u8 r5 = (pixel >> 0)  & 0x1F;
            u8 g6 = (pixel >> 5)  & 0x3F;
            u8 b5 = (pixel >> 11) & 0x1F;

            fb[dst + 0] = (r5 << 3);
            fb[dst + 1] = (g6 << 2);
            fb[dst + 2] = (b5 << 3);
        }
    }
}

void hw_camera_get_frame_rgba8(HardwareCamera* cam, ImageBuffer* out_img)
{
    if (!cam || !cam->initialized || !cam->frame_ready || !out_img || !out_img->pixels) {
        return;
    }

    if (out_img->width != CAM_W || out_img->height != CAM_H) {
        return;
    }

    u16* src = cam->frame_buffer;
    u32* dst = (u32*)out_img->pixels;
    int w = out_img->width;
    int h = out_img->height;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            u16 px = src[y * w + x];

            u32 b = ((px >> 11) & 0x1F) << 3;
            u32 g = ((px >> 5) & 0x3F) << 2;
            u32 r = (px & 0x1F) << 3;

            dst[(h - 1 - y) * w + x] = (0xFFu << 24) | (b << 16) | (g << 8) | r;
        }
    }
}

void hw_camera_play_shutter(HardwareCamera* cam)
{
    (void)cam;
    CAMU_PlayShutterSound(SHUTTER_SOUND_TYPE_NORMAL);
}
