#include "hardware_camera.h"
#include <string.h>

bool hw_camera_init(HardwareCamera *cam) {
  cam->initialized = false;
  cam->capturing = false;
  cam->receive_event = 0;

  Result res = camInit();
  if (R_FAILED(res))
    return false;

  // Configure BOTH outer cameras simultaneously (hardware requirement for stability)
  res = CAMU_SetSize(SELECT_OUT1_OUT2, SIZE_CTR_TOP_LCD, CONTEXT_A);
  if (R_FAILED(res))
    goto fail;

  res = CAMU_SetOutputFormat(SELECT_OUT1_OUT2, OUTPUT_RGB_565, CONTEXT_A);
  if (R_FAILED(res))
    goto fail;

  CAMU_SetNoiseFilter(SELECT_OUT1_OUT2, true);
  CAMU_SetAutoExposure(SELECT_OUT1_OUT2, true);
  CAMU_SetAutoWhiteBalance(SELECT_OUT1_OUT2, true);
  CAMU_SetTrimming(PORT_CAM1, false);
  CAMU_SetTrimming(PORT_CAM2, false);

  res = CAMU_GetMaxBytes(&cam->buffer_size, 400, 240); // Getting chunk size (usually 2560 bytes)
  if (R_FAILED(res))
    goto fail;

  u32 frame_size = 400 * 240 * 2; // 192000 bytes
  cam->receive_buffer = (u8 *)linearAlloc(frame_size); // Must allocate full frame
  if (!cam->receive_buffer)
    goto fail;

  res = CAMU_SetTransferBytes(PORT_BOTH, cam->buffer_size, 400, 240); // Uses chunk size!
  if (R_FAILED(res)) {
    linearFree(cam->receive_buffer);
    goto fail;
  }

  cam->initialized = true;
  return true;

fail:
  camExit();
  return false;
}

void hw_camera_shutdown(HardwareCamera *cam) {
  if (!cam->initialized)
    return;

  if (cam->capturing) {
    hw_camera_stop(cam);
  }

  if (cam->receive_buffer) {
    linearFree(cam->receive_buffer);
    cam->receive_buffer = NULL;
  }

  camExit();
  cam->initialized = false;
}

bool hw_camera_start(HardwareCamera *cam) {
  if (!cam->initialized || cam->capturing)
    return false;

  Result res = CAMU_Activate(SELECT_OUT1_OUT2);
  if (R_FAILED(res))
    return false;

  res = CAMU_ClearBuffer(PORT_BOTH);
  if (R_FAILED(res))
    return false;

  res = CAMU_StartCapture(PORT_BOTH);
  if (R_FAILED(res))
    return false;

  // Set up continuous receiving ONCE
  u32 frame_size = 400 * 240 * 2;
  res = CAMU_SetReceiving(&cam->receive_event, cam->receive_buffer, PORT_CAM1,
                          frame_size, (s16)cam->buffer_size);
  if (R_FAILED(res)) {
    CAMU_StopCapture(PORT_BOTH);
    return false;
  }

  cam->capturing = true;
  return true;
}

void hw_camera_stop(HardwareCamera *cam) {
  if (!cam->capturing)
    return;

  CAMU_StopCapture(PORT_BOTH);
  CAMU_Activate(SELECT_NONE);

  if (cam->receive_event != 0) {
    svcCloseHandle(cam->receive_event);
    cam->receive_event = 0;
  }

  cam->capturing = false;
}

bool hw_camera_wait_next_frame(HardwareCamera *cam, u64 timeout_ns) {
  if (!cam->capturing || cam->receive_event == 0)
    return false;

  Result res = svcWaitSynchronization(cam->receive_event, timeout_ns);
  if (R_SUCCEEDED(res)) {
    // Frame arrived! Clear the event so it can be signaled by the next frame.
    svcClearEvent(cam->receive_event);

    // Invalidate CPU cache so we see the DMA update
    GSPGPU_InvalidateDataCache(cam->receive_buffer, 400 * 240 * 2);

    return true;
  }

  return false;
}

void hw_camera_get_frame_rgba8(HardwareCamera *cam, ImageBuffer *out_img) {
  if (!cam->initialized || !cam->receive_buffer || !out_img->pixels)
    return;

  u16 *src = (u16 *)cam->receive_buffer;
  u32 *dst = (u32 *)out_img->pixels;
  int w = out_img->width;
  int h = out_img->height;

  // We assume out_img is 400x240
  // The camera hardware outputs the image bottom-up. Flip it vertically to be top-down.
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

void hw_camera_play_shutter(HardwareCamera *cam) {
  (void)cam; // Not strictly needing context, but good for abstraction
  CAMU_PlayShutterSound(SHUTTER_SOUND_TYPE_NORMAL);
}
