#ifndef IMAGE_BUFFER_H
#define IMAGE_BUFFER_H

#include <3ds/types.h>
#include <stdbool.h>

// ---------------------------------------------------------------------------
// ImageBuffer — CPU-side RGBA8 pixel storage
// Pixel format: u32 = R | (G<<8) | (B<<16) | (A<<24) (little-endian ARM)
// ---------------------------------------------------------------------------

typedef struct {
  u32 *pixels;
  u16 width;
  u16 height;
} ImageBuffer;

ImageBuffer *image_buffer_create(u16 width, u16 height);
void image_buffer_destroy(ImageBuffer *buf);
void image_buffer_copy(const ImageBuffer *src, ImageBuffer *dst);

#endif // IMAGE_BUFFER_H
