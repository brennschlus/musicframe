#include "image_frames.h"

// ---------------------------------------------------------------------------
static const char *s_frame_names[FRAME_COUNT] = {
    "None", "Classic Gold", "Polaroid", "Vintage Wood", "Film Strip"};

const char *image_frame_name(int frame_id) {
  if (frame_id < 0 || frame_id >= FRAME_COUNT)
    return "Unknown";
  return s_frame_names[frame_id];
}

// ---------------------------------------------------------------------------
// Frame renderers — draw decorative elements on the photo's edges.
// Photo occupies rect (x, y, w, h). Frame extends inside.
// ---------------------------------------------------------------------------

static void frame_classic_gold(float x, float y, float w, float h, float z) {
  float outer = 10.0f;
  float inner = 4.0f;

  u32 clrGold = C2D_Color32(0xD4, 0xA5, 0x37, 0xFF);
  u32 clrInner = C2D_Color32(0xB8, 0x8A, 0x2E, 0xFF);
  u32 clrMat = C2D_Color32(0xF5, 0xF0, 0xE8, 0xFF);

  // Outer border inward
  C2D_DrawRectSolid(x, y, z, w, outer, clrGold);
  C2D_DrawRectSolid(x, y + h - outer, z, w, outer, clrGold);
  C2D_DrawRectSolid(x, y, z, outer, h, clrGold);
  C2D_DrawRectSolid(x + w - outer, y, z, outer, h, clrGold);

  // Inner trim
  C2D_DrawRectSolid(x + outer, y + outer, z, w - outer * 2, inner, clrInner);
  C2D_DrawRectSolid(x + outer, y + h - outer - inner, z, w - outer * 2, inner,
                    clrInner);
  C2D_DrawRectSolid(x + outer, y + outer, z, inner, h - outer * 2, clrInner);
  C2D_DrawRectSolid(x + w - outer - inner, y + outer, z, inner, h - outer * 2,
                    clrInner);

  // Corner accents
  float cs = 6.0f;
  C2D_DrawRectSolid(x + outer, y + outer, z, cs, cs, clrMat);
  C2D_DrawRectSolid(x + w - outer - cs, y + outer, z, cs, cs, clrMat);
  C2D_DrawRectSolid(x + outer, y + h - outer - cs, z, cs, cs, clrMat);
  C2D_DrawRectSolid(x + w - outer - cs, y + h - outer - cs, z, cs, cs, clrMat);
}

static void frame_polaroid(float x, float y, float w, float h, float z) {
  float side = 8.0f;
  float top = 8.0f;
  float bottom = 28.0f;

  u32 clrWhite = C2D_Color32(0xFA, 0xFA, 0xFA, 0xFF);
  u32 clrShadow = C2D_Color32(0xE0, 0xE0, 0xE0, 0xFF);

  // White borders inward, not full fill
  C2D_DrawRectSolid(x, y, z, w, top, clrWhite);                       // top
  C2D_DrawRectSolid(x, y + h - bottom, z, w, bottom, clrWhite);       // bottom
  C2D_DrawRectSolid(x, y + top, z, side, h - top - bottom, clrWhite); // left
  C2D_DrawRectSolid(x + w - side, y + top, z, side, h - top - bottom,
                    clrWhite); // right

  // Shadow
  float sh = 3.0f;
  C2D_DrawRectSolid(x + w - sh, y + top + sh, z, sh, h - top - bottom,
                    clrShadow);
  C2D_DrawRectSolid(x + side + sh, y + h - sh, z, w - side * 2, sh, clrShadow);
}

static void frame_vintage_wood(float x, float y, float w, float h, float z) {
  float outer = 12.0f;
  float mid = 3.0f;
  float hi = 2.0f;
  float inset = 2.0f;

  u32 clrDark = C2D_Color32(0x3E, 0x27, 0x1A, 0xFF);  // dark brown
  u32 clrMid = C2D_Color32(0x5C, 0x3A, 0x24, 0xFF);   // medium brown
  u32 clrLight = C2D_Color32(0x8B, 0x6B, 0x4A, 0xFF); // highlight
  u32 clrInset = C2D_Color32(0x1A, 0x10, 0x0A, 0xFF); // inner shadow

  // Outer frame inward
  C2D_DrawRectSolid(x, y, z, w, outer, clrDark);             // top
  C2D_DrawRectSolid(x, y + h - outer, z, w, outer, clrDark); // bottom
  C2D_DrawRectSolid(x, y, z, outer, h, clrDark);             // left
  C2D_DrawRectSolid(x + w - outer, y, z, outer, h, clrDark); // right

  // Mid-tone wood stripe a bit inside the border
  float m = 4.0f;
  C2D_DrawRectSolid(x + m, y + m, z, w - m * 2, mid, clrMid);
  C2D_DrawRectSolid(x + m, y + h - m - mid, z, w - m * 2, mid, clrMid);
  C2D_DrawRectSolid(x + m, y + m, z, mid, h - m * 2, clrMid);
  C2D_DrawRectSolid(x + w - m - mid, y + m, z, mid, h - m * 2, clrMid);

  // Top-left highlight
  C2D_DrawRectSolid(x, y, z, w, hi, clrLight);
  C2D_DrawRectSolid(x, y, z, hi, h, clrLight);

  // Inner shadow near photo area
  C2D_DrawRectSolid(x + outer - inset, y + outer - inset, z,
                    w - (outer - inset) * 2, inset, clrInset);
  C2D_DrawRectSolid(x + outer - inset, y + h - outer, z,
                    w - (outer - inset) * 2, inset, clrInset);
  C2D_DrawRectSolid(x + outer - inset, y + outer - inset, z, inset,
                    h - (outer - inset) * 2, clrInset);
  C2D_DrawRectSolid(x + w - outer, y + outer - inset, z, inset,
                    h - (outer - inset) * 2, clrInset);
}

static void frame_film_strip(float x, float y, float w, float h, float z) {
  float bar_h = 16.0f;
  float side = 6.0f;

  u32 clrBlack = C2D_Color32(0x1A, 0x1A, 0x1A, 0xFF);
  u32 clrHole = C2D_Color32(0x40, 0x40, 0x40, 0xFF);

  // Frame inward
  C2D_DrawRectSolid(x, y, z, w, bar_h, clrBlack);             // top
  C2D_DrawRectSolid(x, y + h - bar_h, z, w, bar_h, clrBlack); // bottom
  C2D_DrawRectSolid(x, y, z, side, h, clrBlack);              // left
  C2D_DrawRectSolid(x + w - side, y, z, side, h, clrBlack);   // right

  // Sprocket holes on top and bottom
  float hole_w = 8.0f;
  float hole_h = 6.0f;
  float spacing = 20.0f;
  float start_x = x + side + 8.0f;
  float end_x = x + w - side - 8.0f;

  for (float hx = start_x; hx + hole_w <= end_x; hx += spacing) {
    C2D_DrawRectSolid(hx, y + (bar_h - hole_h) / 2.0f, z, hole_w, hole_h,
                      clrHole);
    C2D_DrawRectSolid(hx, y + h - bar_h + (bar_h - hole_h) / 2.0f, z, hole_w,
                      hole_h, clrHole);
  }
}

// ---------------------------------------------------------------------------
// Dispatch
// ---------------------------------------------------------------------------

typedef void (*FrameDrawFunc)(float x, float y, float w, float h, float z);

static FrameDrawFunc s_frame_funcs[FRAME_COUNT] = {
    NULL, // FRAME_NONE
    frame_classic_gold,
    frame_polaroid,
    frame_vintage_wood,
    frame_film_strip,
};

void image_frame_draw(int frame_id, float x, float y, float w, float h,
                      float depth) {
  if (frame_id <= FRAME_NONE || frame_id >= FRAME_COUNT)
    return;
  if (s_frame_funcs[frame_id]) {
    s_frame_funcs[frame_id](x, y, w, h, depth);
  }
}
