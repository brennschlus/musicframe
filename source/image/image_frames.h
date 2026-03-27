#ifndef IMAGE_FRAMES_H
#define IMAGE_FRAMES_H

#include <citro2d.h>

#define FRAME_NONE          0
#define FRAME_CLASSIC_GOLD  1
#define FRAME_POLAROID      2
#define FRAME_VINTAGE_WOOD  3
#define FRAME_FILM_STRIP    4
#define FRAME_COUNT         5

// Get human-readable frame name
const char* image_frame_name(int frame_id);

// Draw frame overlay around a photo at (x, y) with size (w, h).
// Call AFTER drawing the photo, so frame elements render on top.
// depth should be higher than the photo's depth (e.g. 0.6).
void image_frame_draw(int frame_id, float x, float y, float w, float h, float depth);

#endif // IMAGE_FRAMES_H
