#ifndef IMAGE_TEXTURE_H
#define IMAGE_TEXTURE_H

#include <citro2d.h>
#include <stdbool.h>
#include "image_buffer.h"

// Upload ImageBuffer pixels to a C3D_Tex with Morton tiling + Y-flip + RGBA→ABGR.
// On first call (*initialized == false), allocates the texture via C3D_TexInit.
// On subsequent calls, re-uploads in-place.
bool image_texture_upload(const ImageBuffer* buf, C3D_Tex* tex,
                          Tex3DS_SubTexture* subtex, bool* initialized);

// Free GPU texture if initialized.
void image_texture_cleanup(C3D_Tex* tex, bool* initialized);

#endif // IMAGE_TEXTURE_H
