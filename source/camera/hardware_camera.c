#include "hardware_camera.h"
#include "3ds/allocator/linear.h"
#include <stdbool.h>
#include <string.h>

#define CAM_W 400
#define CAM_H 240
#define CAM_BPP 2

bool hw_camera_init(HardwareCamera *cam) {
  cam->initialized = false;
  cam->preview_active = false;
  cam->receive_event = 0;
  cam->frame_size = CAM_W * CAM_H * CAM_BPP;
  cam->frame_buffer = NULL;
  cam->transfer_bytes = 0;
  cam->frame_ready = false;

  Result res = camInit();
  if (R_FAILED(res))
    return false;

  res = CAMU_SetSize(SELECT_OUT1, SIZE_CTR_TOP_LCD, CONTEXT_A);
  if (R_FAILED(res))
    goto fail;

  res = CAMU_SetOutputFormat(SELECT_OUT1, OUTPUT_RGB_565, CONTEXT_A);
  if (R_FAILED(res))
    goto fail;

  res = CAMU_SetNoiseFilter(SELECT_OUT1, true);
  if (R_FAILED(res))
    goto fail;
  CAMU_SetAutoExposure(SELECT_OUT1, true);
  CAMU_SetAutoWhiteBalance(SELECT_OUT1, true);
  CAMU_SetTrimming(PORT_CAM1, false);

  cam->frame_buffer = (u16 *)linearAlloc(cam->frame_size);
  if (!cam->frame_buffer)
    goto fail;

  res = CAMU_GetMaxBytes(&cam->transfer_bytes, CAM_W, CAM_H);
  if (R_FAILED(res))
    goto fail;

  res = CAMU_SetTransferBytes(PORT_CAM1, cam->transfer_bytes, CAM_W, CAM_H);
  if (R_FAILED(res))
    goto fail;

  cam->initialized = true;
  return true;

fail:
  if (cam->frame_buffer) {
    linearFree(cam->frame_buffer);
    cam->frame_buffer = NULL;
  }
  cam->initialized = false;
  cam->preview_active = false;
  cam->receive_event = 0;
  cam->transfer_bytes = 0;
  cam->frame_ready = false;
  camExit();
  return false;
}

void hw_camera_shutdown(HardwareCamera *cam) {
  if (!cam->initialized)
    return;

  if (cam->preview_active) {
    hw_camera_stop(cam);
  }

  if (cam->frame_buffer) {
    linearFree(cam->frame_buffer);
    cam->frame_buffer = NULL;
  }
  cam->initialized = false;
  cam->preview_active = false;
  cam->receive_event = 0;
  cam->transfer_bytes = 0;
  cam->frame_ready = false;
  camExit();
}

bool hw_camera_start(HardwareCamera *cam) {
  if (!cam->initialized || cam->preview_active)
    return false;

  cam->frame_ready = false;
  cam->receive_event = 0;

  Result res = CAMU_Activate(SELECT_OUT1);
  if (R_FAILED(res))
    return false;

  res = CAMU_ClearBuffer(PORT_CAM1);
  if (R_FAILED(res))
    goto fail;

  res = CAMU_StartCapture(PORT_CAM1);
  if (R_FAILED(res))
    goto fail;

  res = CAMU_SetReceiving(&cam->receive_event, cam->frame_buffer, PORT_CAM1,
                          cam->frame_size, (s16)cam->transfer_bytes);
  if (R_FAILED(res))
    goto fail;

  cam->preview_active = true;
  cam->frame_ready = false;
  return true;

fail:
  (void)CAMU_StopCapture(PORT_CAM1);
  (void)CAMU_Activate(SELECT_NONE);

  if (cam->receive_event != 0) {
    svcCloseHandle(cam->receive_event);
    cam->receive_event = 0;
  }

  cam->preview_active = false;
  cam->frame_ready = false;
  return false;
}

void hw_camera_stop(HardwareCamera *cam) {
  if (!cam->preview_active)
    return;

  (void)CAMU_StopCapture(PORT_CAM1);
  (void)CAMU_Activate(SELECT_NONE);

  if (cam->receive_event != 0) {
    svcCloseHandle(cam->receive_event);
    cam->receive_event = 0;
  }

  cam->preview_active = false;
  cam->frame_ready = false;
}

bool hw_camera_poll_frame(HardwareCamera *cam) {
  if (!cam->preview_active || cam->receive_event == 0) {
    return false;
  }

  /// Time out would be same as no frame at all
  Result res = svcWaitSynchronization(cam->receive_event, 0);
  if (R_FAILED(res)) {
    return false;
  }

  svcCloseHandle(cam->receive_event);
  cam->receive_event = 0;

  cam->frame_ready = true;

  res = CAMU_SetReceiving(&cam->receive_event, cam->frame_buffer, PORT_CAM1,
                          cam->frame_size, (s16)cam->transfer_bytes);
  if (R_FAILED(res)) {
    (void)CAMU_StopCapture(PORT_CAM1);
    (void)CAMU_Activate(SELECT_NONE);

    cam->preview_active = false;
    cam->frame_ready = false;
    if (cam->receive_event != 0) {
      svcCloseHandle(cam->receive_event);
      cam->receive_event = 0;
    }
    return false;
  }

  return true;
}

void hw_camera_get_frame_rgba8(HardwareCamera *cam, ImageBuffer *out_img) {
  if (!cam->initialized || !cam->frame_ready || !out_img || !out_img->pixels)
    return;
  if (out_img->width != CAM_W || out_img->height != CAM_H)
    return;

  u16 *src = (u16 *)cam->frame_buffer;
  u32 *dst = (u32 *)out_img->pixels;
  int w = out_img->width;
  int h = out_img->height;

  // We assume out_img is 400x240
  // The camera hardware outputs the image bottom-up. Flip it vertically to be
  // top-down.
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      u16 px = src[y * w + x];
      // 3DS Camera output is effectively BGR565 when read as u16 on ARM
      u32 b = ((px >> 11) & 0x1F) << 3;
      u32 g = ((px >> 5) & 0x3F) << 2;
      u32 r = (px & 0x1F) << 3;
      dst[(h - 1 - y) * w + x] = (0xFF << 24) | (b << 16) | (g << 8) | r;
    }
  }
}

void hw_draw_camera_preview_top(const u16* src)
{
    if (!src) return;

    u16 fbWidth, fbHeight;
    u8* fb = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &fbWidth, &fbHeight);

    for (int y = 0; y < CAM_H; y++) {
        for (int x = 0; x < CAM_W; x++) {
            u16 px = src[y * CAM_W + x];

            int drawY = 239 - y;
            int drawX = x;
            u32 dst = (drawY + drawX * CAM_H) * 3;

            u8 b = ((px >> 11) & 0x1F) << 3;
            u8 g = ((px >> 5) & 0x3F) << 2;
            u8 r = (px & 0x1F) << 3;

            fb[dst + 0] = r;
            fb[dst + 1] = g;
            fb[dst + 2] = b;
        }
    }
}

void hw_camera_play_shutter(HardwareCamera *cam) {
  (void)cam; // Not strictly needing context, but good for abstraction
  CAMU_PlayShutterSound(SHUTTER_SOUND_TYPE_NORMAL);
}
