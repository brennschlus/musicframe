#include "image_buffer.h"
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
ImageBuffer* image_buffer_create(u16 width, u16 height)
{
    ImageBuffer* buf = (ImageBuffer*)malloc(sizeof(ImageBuffer));
    if (!buf) return NULL;

    buf->width  = width;
    buf->height = height;
    buf->pixels = (u32*)malloc(width * height * sizeof(u32));
    if (!buf->pixels) {
        free(buf);
        return NULL;
    }
    memset(buf->pixels, 0, width * height * sizeof(u32));
    return buf;
}

// ---------------------------------------------------------------------------
void image_buffer_destroy(ImageBuffer* buf)
{
    if (!buf) return;
    free(buf->pixels);
    free(buf);
}

// ---------------------------------------------------------------------------
void image_buffer_copy(const ImageBuffer* src, ImageBuffer* dst)
{
    if (!src || !dst) return;
    u32 count = src->width * src->height;
    if (count > (u32)(dst->width * dst->height)) {
        count = dst->width * dst->height;
    }
    memcpy(dst->pixels, src->pixels, count * sizeof(u32));
}

// ---------------------------------------------------------------------------
// Test pattern: 240x160 landscape with gradient sky, ground, and sun
// ---------------------------------------------------------------------------
#define TEST_W 240
#define TEST_H 160

ImageBuffer* image_buffer_create_test_pattern(void)
{
    ImageBuffer* buf = image_buffer_create(TEST_W, TEST_H);
    if (!buf) return NULL;

    int horizon = (int)(TEST_H * 0.55f);

    for (int y = 0; y < TEST_H; y++) {
        for (int x = 0; x < TEST_W; x++) {
            u8 r, g, b;

            if (y < horizon) {
                // Sky gradient: warm sunset tones at horizon, blue at top
                float t = (float)y / horizon;
                r = (u8)(70  + t * 140);   // blue-ish at top, warm at horizon
                g = (u8)(130 + t * 80);
                b = (u8)(220 - t * 60);
            } else {
                // Ground: dark green to brown
                float t = (float)(y - horizon) / (TEST_H - horizon);
                r = (u8)(50  + t * 40);
                g = (u8)(100 - t * 30);
                b = (u8)(30  + t * 15);
            }

            // Sun: warm circle in upper-right area
            float dx = (float)x - TEST_W * 0.72f;
            float dy = (float)y - TEST_H * 0.22f;
            float dist2 = dx * dx + dy * dy;
            float sun_r = 18.0f;
            if (dist2 < sun_r * sun_r) {
                r = 255; g = 230; b = 80;
            } else if (dist2 < (sun_r + 8) * (sun_r + 8)) {
                // Sun glow
                float glow = 1.0f - (dist2 - sun_r * sun_r) / ((sun_r + 8) * (sun_r + 8) - sun_r * sun_r);
                r = clamp_u8(r + (int)(glow * 80));
                g = clamp_u8(g + (int)(glow * 50));
                b = clamp_u8(b + (int)(glow * 10));
            }

            buf->pixels[y * TEST_W + x] = RGBA8(r, g, b, 0xFF);
        }
    }

    return buf;
}
