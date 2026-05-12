#include "image_filters.h"

// image_filter_apply_zig is implemented in zig/filters.zig → zig/libfilters.a
extern void image_filter_apply_zig(int filter_id,
                                   const u32 *src, u32 *dst,
                                   u32 pixel_count);

static const char *s_filter_names[FILTER_COUNT] = {
    "None",
    "Grayscale",
    "Sepia",
    "Warm",
    "Faded"
};

const char *image_filter_name(int filter_id)
{
    if (filter_id < 0 || filter_id >= FILTER_COUNT) return "Unknown";
    return s_filter_names[filter_id];
}

void image_filter_apply(int filter_id, const ImageBuffer *src, ImageBuffer *dst)
{
    if (filter_id < 0 || filter_id >= FILTER_COUNT) filter_id = FILTER_NONE;
    image_filter_apply_zig(filter_id, src->pixels, dst->pixels,
                           (u32)src->width * src->height);
}
