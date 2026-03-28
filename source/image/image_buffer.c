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
