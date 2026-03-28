#ifndef IMAGE_BUFFER_H
#define IMAGE_BUFFER_H

#include <3ds/types.h>
#include <stdbool.h>

// ---------------------------------------------------------------------------
// ImageBuffer — CPU-side RGBA8 pixel storage
// Pixel format: u32 = R | (G<<8) | (B<<16) | (A<<24) (little-endian ARM)
// ---------------------------------------------------------------------------

typedef struct {
    u32* pixels;
    u16  width;
    u16  height;
} ImageBuffer;

#define RGBA8(r, g, b, a) ((u32)(r) | ((u32)(g) << 8) | ((u32)(b) << 16) | ((u32)(a) << 24))

static inline void rgba8_unpack(u32 px, u8* r, u8* g, u8* b, u8* a) {
    *r = px & 0xFF;
    *g = (px >> 8) & 0xFF;
    *b = (px >> 16) & 0xFF;
    *a = (px >> 24) & 0xFF;
}

static inline u8 clamp_u8(int v) {
    return (u8)(v < 0 ? 0 : (v > 255 ? 255 : v));
}

ImageBuffer* image_buffer_create(u16 width, u16 height);
void         image_buffer_destroy(ImageBuffer* buf);
void         image_buffer_copy(const ImageBuffer* src, ImageBuffer* dst);


#endif // IMAGE_BUFFER_H
