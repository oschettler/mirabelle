/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "expand.h"

#include <string.h>

void expand_table_init(expand_table *t, uint16_t on, uint16_t off)
{
    for (int b = 0; b < 256; b++)
        for (int i = 0; i < 8; i++)
            t->px[b][i] = ((b >> (7 - i)) & 1) ? on : off;
}

void expand_rows(const expand_table *t, const bitmap *src,
                 int y0, int y1, uint16_t *dst, int dst_stride_px)
{
    if (y0 < 0) y0 = 0;
    if (y1 > src->h - 1) y1 = src->h - 1;

    int full_bytes = src->w / 8;
    int rest       = src->w % 8;

    for (int y = y0; y <= y1; y++) {
        const uint8_t *srow = src->bits + (size_t)y * (size_t)src->stride;
        uint16_t      *drow = dst + (size_t)y * (size_t)dst_stride_px;

        for (int bx = 0; bx < full_bytes; bx++)
            memcpy(drow + bx * 8, t->px[srow[bx]], 16);

        /* Restpixel des letzten, unvollständigen Bytes - dessen Füllbits
         * sind laut bitmap.h immer 0, dafür aber KEIN Pixel nach dst
         * schreiben, sonst würden fremde Bildpunkte überschrieben. */
        if (rest > 0) {
            const uint16_t *px = t->px[srow[full_bytes]];
            for (int i = 0; i < rest; i++)
                drow[full_bytes * 8 + i] = px[i];
        }
    }
}
