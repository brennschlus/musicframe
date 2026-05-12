#include "image_jpeg.h"
#include "stb_image.h"
#include <stdlib.h>

ImageBuffer *image_jpeg_load(const char *path, u16 max_w, u16 max_h)
{
    int src_w, src_h, channels;
    unsigned char *raw = stbi_load(path, &src_w, &src_h, &channels, 4);
    if (!raw) return NULL;

    // Compute scale to fit within max_w x max_h
    u16 dst_w = (u16)src_w;
    u16 dst_h = (u16)src_h;

    if (src_w > max_w || src_h > max_h) {
        // Integer fixed-point scale: multiply by 256 to avoid float
        int scale_x = ((int)max_w << 8) / src_w;
        int scale_y = ((int)max_h << 8) / src_h;
        int scale   = scale_x < scale_y ? scale_x : scale_y;

        dst_w = (u16)((src_w * scale) >> 8);
        dst_h = (u16)((src_h * scale) >> 8);
        if (dst_w < 1) dst_w = 1;
        if (dst_h < 1) dst_h = 1;
    }

    ImageBuffer *buf = image_buffer_create(dst_w, dst_h);
    if (!buf) {
        stbi_image_free(raw);
        return NULL;
    }

    if (dst_w == (u16)src_w && dst_h == (u16)src_h) {
        // No scaling — copy with Y-flip: 3DS DCIM JPEGs are stored upside down
        // (camera sensor orientation). The texture uploader expects row 0 = visual
        // bottom so that its own Y-flip produces the correct on-screen orientation,
        // matching the camera-capture pipeline.
        for (u16 dy = 0; dy < dst_h; dy++) {
            u16 sy = dst_h - 1 - dy;
            for (u16 dx = 0; dx < dst_w; dx++) {
                const unsigned char *p = raw + ((u32)sy * dst_w + dx) * 4;
                buf->pixels[(u32)dy * dst_w + dx] =
                    (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
            }
        }
    } else {
        // Nearest-neighbor downsample with Y-flip (same reasoning as above)
        for (u16 dy = 0; dy < dst_h; dy++) {
            int sy = (dst_h - 1 - dy) * src_h / dst_h;
            for (u16 dx = 0; dx < dst_w; dx++) {
                int sx = dx * src_w / dst_w;
                const unsigned char *p = raw + (sy * src_w + sx) * 4;
                buf->pixels[(u32)dy * dst_w + dx] =
                    (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
            }
        }
    }

    stbi_image_free(raw);
    return buf;
}
