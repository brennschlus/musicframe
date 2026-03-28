#ifndef HARDWARE_CAMERA_H
#define HARDWARE_CAMERA_H

#include <3ds.h>
#include <stdbool.h>
#include "../image/image_buffer.h"

typedef struct {
    bool initialized;
    bool preview_active;

    u16* frame_buffer;
    u32 frame_size;
    u32 transfer_bytes;

    Handle receive_event;
    bool frame_ready;
} HardwareCamera;

// Initializes the cam service, outer camera, OUT1 port, RGB565
bool hw_camera_init(HardwareCamera* cam);

// Shutdown the cam service
void hw_camera_shutdown(HardwareCamera* cam);

// Start the continuous framing/capture cycle
bool hw_camera_start(HardwareCamera* cam);

// Stop the continuous capture cycle
void hw_camera_stop(HardwareCamera* cam);

// Poll for the next frame (non-blocking)
bool hw_camera_poll_frame(HardwareCamera* cam);


// Fetch latest frame and convert from RGB565 to the given ImageBuffer (RGBA8)
void hw_camera_get_frame_rgba8(HardwareCamera* cam, ImageBuffer* out_img);

// Draw the camera preview on the top screen (raw RGB565 data)
void hw_draw_camera_preview_top(const u16* src);

// Play standard shutter sound
void hw_camera_play_shutter(HardwareCamera* cam);

#endif // HARDWARE_CAMERA_H
