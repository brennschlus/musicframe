#ifndef HARDWARE_CAMERA_H
#define HARDWARE_CAMERA_H

#include <3ds.h>
#include <stdbool.h>
#include "../image/image_buffer.h"

typedef struct {
    bool initialized;
    bool capturing;
    u8*  receive_buffers[2];
    u32  buffer_size;
    u32  frame_size;
    
    Handle receive_event;
    u8 current_receive_idx;
    u8 latest_frame_idx;
} HardwareCamera;

// Initializes the cam service, outer camera, OUT1 port, RGB565
bool hw_camera_init(HardwareCamera* cam);

// Shutdown the cam service
void hw_camera_shutdown(HardwareCamera* cam);

// Start the continuous framing/capture cycle
bool hw_camera_start(HardwareCamera* cam);

// Stop the continuous capture cycle
void hw_camera_stop(HardwareCamera* cam);

// Block until the next frame arrives (or timeout occurs)
bool hw_camera_wait_next_frame(HardwareCamera* cam, u64 timeout_ns);

// Fetch latest frame and convert from RGB565 to the given ImageBuffer (RGBA8)
void hw_camera_get_frame_rgba8(HardwareCamera* cam, ImageBuffer* out_img);

// Play standard shutter sound
void hw_camera_play_shutter(HardwareCamera* cam);

#endif // HARDWARE_CAMERA_H
