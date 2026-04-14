#include "ui_text.h"

#include <3ds.h>
#include <stdarg.h>
#include <stdio.h>

// Shared transient text buffer. Sized for a reasonable amount of text per
// frame (bottom-screen UI is typically < 256 glyphs).
#define UI_TEXT_BUF_GLYPHS 4096

static C2D_TextBuf s_buf = NULL;
static char s_fmt_buf[512];

// ---------------------------------------------------------------------------
void ui_text_init(void)
{
    if (s_buf) return;
    s_buf = C2D_TextBufNew(UI_TEXT_BUF_GLYPHS);
    // Make sure the system shared font is mapped for citro2d's default path.
    fontEnsureMapped();
}

// ---------------------------------------------------------------------------
void ui_text_shutdown(void)
{
    if (s_buf) {
        C2D_TextBufDelete(s_buf);
        s_buf = NULL;
    }
}

// ---------------------------------------------------------------------------
void ui_text_frame_begin(void)
{
    if (s_buf) C2D_TextBufClear(s_buf);
}

// ---------------------------------------------------------------------------
static void parse_formatted(C2D_Text* out, const char* fmt, va_list ap)
{
    vsnprintf(s_fmt_buf, sizeof(s_fmt_buf), fmt, ap);
    C2D_TextParse(out, s_buf, s_fmt_buf);
    C2D_TextOptimize(out);
}

// ---------------------------------------------------------------------------
void ui_draw(float x, float y, float z, float scale, u32 color,
             const char* fmt, ...)
{
    if (!s_buf) return;
    C2D_Text t;
    va_list ap;
    va_start(ap, fmt);
    parse_formatted(&t, fmt, ap);
    va_end(ap);
    C2D_DrawText(&t, C2D_WithColor, x, y, z, scale, scale, color);
}

// ---------------------------------------------------------------------------
void ui_draw_centered(float cx, float y, float z, float scale, u32 color,
                      const char* fmt, ...)
{
    if (!s_buf) return;
    C2D_Text t;
    va_list ap;
    va_start(ap, fmt);
    parse_formatted(&t, fmt, ap);
    va_end(ap);

    float w = 0.0f, h = 0.0f;
    C2D_TextGetDimensions(&t, scale, scale, &w, &h);
    C2D_DrawText(&t, C2D_WithColor, cx - w * 0.5f, y, z, scale, scale, color);
}

// ---------------------------------------------------------------------------
float ui_text_measure(float scale, float* out_h, const char* fmt, ...)
{
    if (!s_buf) { if (out_h) *out_h = 0.0f; return 0.0f; }
    C2D_Text t;
    va_list ap;
    va_start(ap, fmt);
    parse_formatted(&t, fmt, ap);
    va_end(ap);

    float w = 0.0f, h = 0.0f;
    C2D_TextGetDimensions(&t, scale, scale, &w, &h);
    if (out_h) *out_h = h;
    return w;
}
