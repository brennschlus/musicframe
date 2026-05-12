#ifndef IMAGE_JPEG_H
#define IMAGE_JPEG_H

#include "image_buffer.h"
#include <3ds/types.h>

// Load a JPEG from path, scaling down proportionally to fit within max_w x max_h.
// Returns a newly allocated ImageBuffer (caller owns it), or NULL on failure.
ImageBuffer *image_jpeg_load(const char *path, u16 max_w, u16 max_h);

#endif // IMAGE_JPEG_H
