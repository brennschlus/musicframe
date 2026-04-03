#include "image_filters.h"
#include <string.h>

// ---------------------------------------------------------------------------
// Pixel helpers
// ---------------------------------------------------------------------------

// Pixel format in ImageBuffer: u32 = R | (G<<8) | (B<<16) | (A<<24)
static inline void rgba8_unpack(u32 px, u8* r, u8* g, u8* b, u8* a)
{
    *r = (u8)(px);
    *g = (u8)(px >> 8);
    *b = (u8)(px >> 16);
    *a = (u8)(px >> 24);
}

static inline u32 RGBA8(u8 r, u8 g, u8 b, u8 a)
{
    return (u32)r | ((u32)g << 8) | ((u32)b << 16) | ((u32)a << 24);
}

static inline u8 clamp_u8(int v)
{
    return (v < 0) ? 0 : (v > 255) ? 255 : (u8)v;
}

// ---------------------------------------------------------------------------
static const char* s_filter_names[FILTER_COUNT] = {
    "None",
    "Grayscale",
    "Sepia",
    "Warm",
    "Faded"
};

const char* image_filter_name(int filter_id)
{
    if (filter_id < 0 || filter_id >= FILTER_COUNT) return "Unknown";
    return s_filter_names[filter_id];
}

// ---------------------------------------------------------------------------
// Individual filter implementations (integer arithmetic for ARM11 speed)
// ---------------------------------------------------------------------------

static void filter_none(const ImageBuffer* src, ImageBuffer* dst)
{
    memcpy(dst->pixels, src->pixels, src->width * src->height * sizeof(u32));
}

static void filter_grayscale(const ImageBuffer* src, ImageBuffer* dst)
{
    u32 count = src->width * src->height;
    for (u32 i = 0; i < count; i++) {
        u8 r, g, b, a;
        rgba8_unpack(src->pixels[i], &r, &g, &b, &a);
        // ITU-R BT.601 luminance approximation
        u8 gray = (u8)((77 * r + 150 * g + 29 * b) >> 8);
        dst->pixels[i] = RGBA8(gray, gray, gray, a);
    }
}

static void filter_sepia(const ImageBuffer* src, ImageBuffer* dst)
{
    u32 count = src->width * src->height;
    for (u32 i = 0; i < count; i++) {
        u8 r, g, b, a;
        rgba8_unpack(src->pixels[i], &r, &g, &b, &a);
        u8 gray = (u8)((77 * r + 150 * g + 29 * b) >> 8);
        dst->pixels[i] = RGBA8(
            clamp_u8(gray + 40),
            clamp_u8(gray + 20),
            gray,
            a
        );
    }
}

static void filter_warm(const ImageBuffer* src, ImageBuffer* dst)
{
    u32 count = src->width * src->height;
    for (u32 i = 0; i < count; i++) {
        u8 r, g, b, a;
        rgba8_unpack(src->pixels[i], &r, &g, &b, &a);
        dst->pixels[i] = RGBA8(
            clamp_u8(r + 25),
            g,
            clamp_u8(b - 20),
            a
        );
    }
}

static void filter_faded(const ImageBuffer* src, ImageBuffer* dst)
{
    u32 count = src->width * src->height;
    for (u32 i = 0; i < count; i++) {
        u8 r, g, b, a;
        rgba8_unpack(src->pixels[i], &r, &g, &b, &a);
        // Reduce contrast + warm shift (vintage look)
        dst->pixels[i] = RGBA8(
            clamp_u8(50 + (r * 180 >> 8)),
            clamp_u8(50 + (g * 170 >> 8)),
            clamp_u8(50 + (b * 160 >> 8)),
            a
        );
    }
}

// ---------------------------------------------------------------------------
// Dispatch
// ---------------------------------------------------------------------------
static void (*s_filter_funcs[FILTER_COUNT])(const ImageBuffer*, ImageBuffer*) = {
    filter_none,
    filter_grayscale,
    filter_sepia,
    filter_warm,
    filter_faded,
};

void image_filter_apply(int filter_id, const ImageBuffer* src, ImageBuffer* dst)
{
    if (filter_id < 0 || filter_id >= FILTER_COUNT) filter_id = FILTER_NONE;
    s_filter_funcs[filter_id](src, dst);
}
