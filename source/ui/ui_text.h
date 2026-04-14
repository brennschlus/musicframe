#ifndef UI_TEXT_H
#define UI_TEXT_H

#include <citro2d.h>

// ---------------------------------------------------------------------------
// ui_text — small wrapper over citro2d text rendering using the 3DS system
// shared font. One dynamic C2D_TextBuf is shared for all purely transient
// per-frame strings; callers are expected to call ui_text_frame_begin() once
// per rendered frame so that stale glyphs from the previous frame are reset.
// ---------------------------------------------------------------------------

void ui_text_init(void);
void ui_text_shutdown(void);

// Clear the shared text buffer at the start of a frame.
void ui_text_frame_begin(void);

// Draw formatted text with a given color at (x, y) using the shared buffer.
// Uses the system font. scale is applied on both axes.
void ui_draw(float x, float y, float z, float scale, u32 color,
             const char* fmt, ...) __attribute__((format(printf, 6, 7)));

// Draw horizontally-centered formatted text around cx.
void ui_draw_centered(float cx, float y, float z, float scale, u32 color,
                      const char* fmt, ...) __attribute__((format(printf, 6, 7)));

// Measure a formatted string (for layout decisions) using the shared buffer.
// Returns width in pixels at the given scale. Height written to *out_h if
// non-NULL. The produced text is left in the shared buffer — safe to draw
// in subsequent ui_draw calls within the same frame.
float ui_text_measure(float scale, float* out_h, const char* fmt, ...)
    __attribute__((format(printf, 3, 4)));

#endif // UI_TEXT_H
