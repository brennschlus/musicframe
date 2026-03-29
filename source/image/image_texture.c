#include "image_texture.h"
#include <string.h>

// ---------------------------------------------------------------------------
// Morton interleave tables for 8x8 tile encoding (3DS GPU texture format)
// ---------------------------------------------------------------------------
static const u32 s_morton_x[] = {0x00,0x01,0x04,0x05,0x10,0x11,0x14,0x15};
static const u32 s_morton_y[] = {0x00,0x02,0x08,0x0a,0x20,0x22,0x28,0x2a};

static inline u32 get_tiled_offset(u32 x, u32 y, u32 tex_w)
{
    u32 coarse_x = x & ~7u;
    u32 coarse_y = y & ~7u;
    u32 fine = s_morton_x[x & 7] + s_morton_y[y & 7];
    return coarse_y * tex_w + coarse_x * 8 + fine;
}

static u32 next_pow2(u32 v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    return v + 1;
}

// ---------------------------------------------------------------------------
bool image_texture_upload(const ImageBuffer* buf, C3D_Tex* tex,
                          Tex3DS_SubTexture* subtex, bool* initialized)
{
    if (!buf || !buf->pixels || !tex || !subtex || !initialized)
        return false;

    u32 tex_w = next_pow2(buf->width);
    u32 tex_h = next_pow2(buf->height);

    // Ensure minimum size
    if (tex_w < 8) tex_w = 8;
    if (tex_h < 8) tex_h = 8;

    // Allocate texture on first call
    if (!*initialized) {
        if (!C3D_TexInit(tex, (u16)tex_w, (u16)tex_h, GPU_RGBA8)) {
            return false;
        }
        C3D_TexSetFilter(tex, GPU_LINEAR, GPU_LINEAR);
        C3D_TexSetWrap(tex, GPU_CLAMP_TO_BORDER, GPU_CLAMP_TO_BORDER);
        *initialized = true;
    }

    // Fill subtexture UV region
    subtex->width  = buf->width;
    subtex->height = buf->height;
    subtex->left   = 0.0f;
    subtex->top    = 1.0f;
    subtex->right  = (float)buf->width  / tex_w;
    subtex->bottom = 1.0f - (float)buf->height / tex_h;

    // Clear texture data (padding area)
    memset(tex->data, 0, tex->size);

    // Upload: linear RGBA → tiled ABGR with Y-flip
    const u32* src = buf->pixels;
    u32*       dst = (u32*)tex->data;

    for (u32 y = 0; y < buf->height; y++) {
        u32 flipped_y = buf->height - 1 - y;
        for (u32 x = 0; x < buf->width; x++) {
            u32 si = y * buf->width + x;
            u32 di = get_tiled_offset(x, flipped_y, tex_w);
            // RGBA (LE: 0xAABBGGRR) → ABGR (LE: 0xRRGGBBAA) via byte swap
            dst[di] = __builtin_bswap32(src[si]);
        }
    }

    C3D_TexFlush(tex);
    return true;
}

// ---------------------------------------------------------------------------
void image_texture_cleanup(C3D_Tex* tex, bool* initialized)
{
    if (initialized && *initialized) {
        C3D_TexDelete(tex);
        *initialized = false;
    }
}
