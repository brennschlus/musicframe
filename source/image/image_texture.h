#ifndef IMAGE_TEXTURE_H
#define IMAGE_TEXTURE_H

#include <citro2d.h>
#include <stdbool.h>
#include "image_buffer.h"

// ---------------------------------------------------------------------------
// ImageTexture — GPU texture with its UV sub-region bundled together.
//
// Wraps the three fields that always travel together: the C3D_Tex, the
// Tex3DS_SubTexture that describes the valid (non-padding) region inside the
// pow2 texture, and an initialisation flag.
// ---------------------------------------------------------------------------

typedef struct {
    C3D_Tex           tex;
    Tex3DS_SubTexture subtex;
    bool              initialized;
} ImageTexture;

// Upload ImageBuffer pixels to a GPU texture (Morton tiling + Y-flip + RGBA→ABGR).
// Allocates the texture on the first call (initialized == false).
// On subsequent calls, re-uploads in-place.
bool image_texture_upload(const ImageBuffer* buf, ImageTexture* tex);

// Free GPU texture if allocated.
void image_texture_cleanup(ImageTexture* tex);

#endif // IMAGE_TEXTURE_H
