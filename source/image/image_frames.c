#include "image_frames.h"

// ---------------------------------------------------------------------------
static const char* s_frame_names[FRAME_COUNT] = {
    "None",
    "Classic Gold",
    "Polaroid",
    "Vintage Wood",
    "Film Strip"
};

const char* image_frame_name(int frame_id)
{
    if (frame_id < 0 || frame_id >= FRAME_COUNT) return "Unknown";
    return s_frame_names[frame_id];
}

// ---------------------------------------------------------------------------
// Frame renderers — draw decorative elements around the photo
// Photo occupies rect (x, y, w, h). Frame extends outward.
// ---------------------------------------------------------------------------

static void frame_classic_gold(float x, float y, float w, float h, float z)
{
    float pad = 10.0f;
    float mat = 4.0f;

    u32 clrGold   = C2D_Color32(0xD4, 0xA5, 0x37, 0xFF);
    u32 clrInner  = C2D_Color32(0xB8, 0x8A, 0x2E, 0xFF);
    u32 clrMat    = C2D_Color32(0xF5, 0xF0, 0xE8, 0xFF);

    // Outer gold border
    C2D_DrawRectSolid(x - pad, y - pad, z, w + pad*2, pad, clrGold);           // top
    C2D_DrawRectSolid(x - pad, y + h,  z, w + pad*2, pad, clrGold);           // bottom
    C2D_DrawRectSolid(x - pad, y,      z, pad, h, clrGold);                    // left
    C2D_DrawRectSolid(x + w,   y,      z, pad, h, clrGold);                    // right

    // Inner gold trim
    C2D_DrawRectSolid(x - mat, y - mat, z, w + mat*2, mat, clrInner);
    C2D_DrawRectSolid(x - mat, y + h,   z, w + mat*2, mat, clrInner);
    C2D_DrawRectSolid(x - mat, y,       z, mat, h, clrInner);
    C2D_DrawRectSolid(x + w,   y,       z, mat, h, clrInner);

    // Corner accents (small squares)
    float cs = 6.0f;
    C2D_DrawRectSolid(x - pad,         y - pad,         z, cs, cs, clrMat);
    C2D_DrawRectSolid(x + w + pad - cs, y - pad,        z, cs, cs, clrMat);
    C2D_DrawRectSolid(x - pad,         y + h + pad - cs, z, cs, cs, clrMat);
    C2D_DrawRectSolid(x + w + pad - cs, y + h + pad - cs, z, cs, cs, clrMat);
}

static void frame_polaroid(float x, float y, float w, float h, float z)
{
    float side   = 8.0f;
    float top    = 8.0f;
    float bottom = 28.0f;  // Polaroid-style thick bottom

    u32 clrWhite  = C2D_Color32(0xFA, 0xFA, 0xFA, 0xFF);
    u32 clrShadow = C2D_Color32(0xE0, 0xE0, 0xE0, 0xFF);

    // White background rectangle
    C2D_DrawRectSolid(x - side, y - top, z,
                      w + side*2, h + top + bottom, clrWhite);

    // Subtle shadow on right and bottom edges
    float sh = 3.0f;
    C2D_DrawRectSolid(x + w + side, y - top + sh, z,
                      sh, h + top + bottom, clrShadow);
    C2D_DrawRectSolid(x - side + sh, y + h + bottom, z,
                      w + side*2, sh, clrShadow);
}

static void frame_vintage_wood(float x, float y, float w, float h, float z)
{
    float pad = 12.0f;

    u32 clrDark   = C2D_Color32(0x3E, 0x27, 0x1A, 0xFF);  // Dark brown
    u32 clrMid    = C2D_Color32(0x5C, 0x3A, 0x24, 0xFF);  // Medium brown
    u32 clrLight  = C2D_Color32(0x8B, 0x6B, 0x4A, 0xFF);  // Light wood
    u32 clrInset  = C2D_Color32(0x1A, 0x10, 0x0A, 0xFF);  // Inner shadow

    // Outer frame
    C2D_DrawRectSolid(x - pad, y - pad, z, w + pad*2, pad, clrDark);
    C2D_DrawRectSolid(x - pad, y + h,  z, w + pad*2, pad, clrDark);
    C2D_DrawRectSolid(x - pad, y,      z, pad, h, clrDark);
    C2D_DrawRectSolid(x + w,   y,      z, pad, h, clrDark);

    // Mid-tone wood grain stripe
    float gs = 3.0f;
    float gi = 4.0f; // inset from outer edge
    C2D_DrawRectSolid(x - pad + gi, y - pad + gi, z, w + (pad-gi)*2, gs, clrMid);
    C2D_DrawRectSolid(x - pad + gi, y + h + pad - gi - gs, z, w + (pad-gi)*2, gs, clrMid);
    C2D_DrawRectSolid(x - pad + gi, y - pad + gi, z, gs, h + (pad-gi)*2, clrMid);
    C2D_DrawRectSolid(x + w + pad - gi - gs, y - pad + gi, z, gs, h + (pad-gi)*2, clrMid);

    // Light highlight on top-left edges
    C2D_DrawRectSolid(x - pad, y - pad, z, w + pad*2, 2, clrLight);
    C2D_DrawRectSolid(x - pad, y - pad, z, 2, h + pad*2, clrLight);

    // Inner shadow
    float is = 2.0f;
    C2D_DrawRectSolid(x - is, y - is, z, w + is*2, is, clrInset);
    C2D_DrawRectSolid(x - is, y + h,  z, w + is*2, is, clrInset);
    C2D_DrawRectSolid(x - is, y,      z, is, h, clrInset);
    C2D_DrawRectSolid(x + w,  y,      z, is, h, clrInset);
}

static void frame_film_strip(float x, float y, float w, float h, float z)
{
    float bar_h = 16.0f;
    float side  = 6.0f;

    u32 clrBlack  = C2D_Color32(0x1A, 0x1A, 0x1A, 0xFF);
    u32 clrHole   = C2D_Color32(0x40, 0x40, 0x40, 0xFF);

    // Side bars
    C2D_DrawRectSolid(x - side, y - bar_h, z, side, h + bar_h*2, clrBlack);
    C2D_DrawRectSolid(x + w,    y - bar_h, z, side, h + bar_h*2, clrBlack);

    // Top and bottom bars
    C2D_DrawRectSolid(x - side, y - bar_h, z, w + side*2, bar_h, clrBlack);
    C2D_DrawRectSolid(x - side, y + h,     z, w + side*2, bar_h, clrBlack);

    // Sprocket holes (top bar)
    float hole_w = 8.0f, hole_h = 6.0f;
    float spacing = 20.0f;
    float start_x = x + 10.0f;
    for (float hx = start_x; hx + hole_w < x + w; hx += spacing) {
        C2D_DrawRectSolid(hx, y - bar_h + (bar_h - hole_h)/2, z, hole_w, hole_h, clrHole);
        C2D_DrawRectSolid(hx, y + h + (bar_h - hole_h)/2,     z, hole_w, hole_h, clrHole);
    }
}

// ---------------------------------------------------------------------------
// Dispatch
// ---------------------------------------------------------------------------

typedef void (*FrameDrawFunc)(float x, float y, float w, float h, float z);

static FrameDrawFunc s_frame_funcs[FRAME_COUNT] = {
    NULL,                // FRAME_NONE
    frame_classic_gold,
    frame_polaroid,
    frame_vintage_wood,
    frame_film_strip,
};

void image_frame_draw(int frame_id, float x, float y, float w, float h, float depth)
{
    if (frame_id <= FRAME_NONE || frame_id >= FRAME_COUNT) return;
    if (s_frame_funcs[frame_id]) {
        s_frame_funcs[frame_id](x, y, w, h, depth);
    }
}
