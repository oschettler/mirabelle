/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "text.h"

#include "core/utf8.h"

int text_width(const font *f, const char *s)
{
    int width = 0;

    while (*s) {
        uint32_t cp = utf8_next(&s);
        const glyph *gl = font_find(f, cp);
        if (gl) width += gl->width;
    }

    return width;
}

int gfx_text(gc *g, const font *f, int x, int y, const char *s)
{
    int pen = x;
    int top = y - f->ascent;

    while (*s) {
        uint32_t cp = utf8_next(&s);
        const glyph *gl = font_find(f, cp);
        if (!gl) continue;

        gfx_blit_bits(g, f->bits + gl->offset, gl->stride,
                      gl->width, f->size, pen, top);
        pen += gl->width;
    }

    return pen - x;
}
