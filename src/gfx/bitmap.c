#include "bitmap.h"

#include <stdlib.h>
#include <string.h>

size_t bitmap_bytes(const bitmap *bm)
{
    return (size_t)bm->stride * (size_t)bm->h;
}

/* Setzt die ungenutzten Bits am Ende jeder Zeile auf 0. Siehe Zusicherung im Kopf. */
static void mask_padding(bitmap *bm)
{
    int used = bm->w & 7;
    if (used == 0) return;               /* Zeile endet genau auf Bytegrenze */

    uint8_t keep = (uint8_t)(0xFFu << (8 - used));
    for (int y = 0; y < bm->h; y++)
        bm->bits[(size_t)y * bm->stride + bm->stride - 1] &= keep;
}

bool bitmap_init(bitmap *bm, int w, int h)
{
    if (w <= 0 || h <= 0) return false;

    bm->w      = w;
    bm->h      = h;
    bm->stride = (w + 7) / 8;
    bm->bits   = calloc(bitmap_bytes(bm), 1);
    bm->owned  = true;

    return bm->bits != NULL;
}

void bitmap_free(bitmap *bm)
{
    if (bm->owned) free(bm->bits);
    bm->bits  = NULL;
    bm->owned = false;
    bm->w = bm->h = bm->stride = 0;
}

void bitmap_clear(bitmap *bm, int value)
{
    memset(bm->bits, value ? 0xFF : 0x00, bitmap_bytes(bm));
    if (value) mask_padding(bm);
}

int bitmap_get(const bitmap *bm, int x, int y)
{
    if (x < 0 || y < 0 || x >= bm->w || y >= bm->h) return 0;

    uint8_t byte = bm->bits[(size_t)y * bm->stride + x / 8];
    return (byte >> (7 - (x & 7))) & 1;
}

void bitmap_set(bitmap *bm, int x, int y, int value)
{
    if (x < 0 || y < 0 || x >= bm->w || y >= bm->h) return;

    uint8_t *byte = &bm->bits[(size_t)y * bm->stride + x / 8];
    uint8_t  mask = (uint8_t)(0x80u >> (x & 7));

    if (value) *byte |= mask;
    else       *byte &= (uint8_t)~mask;
}

bool bitmap_equal(const bitmap *a, const bitmap *b)
{
    if (a->w != b->w || a->h != b->h) return false;
    return memcmp(a->bits, b->bits, bitmap_bytes(a)) == 0;
}

bool bitmap_copy_rect(bitmap *dst, const bitmap *src, rect r)
{
    if (!bitmap_init(dst, r.w, r.h)) return false;

    for (int y = 0; y < r.h; y++)
        for (int x = 0; x < r.w; x++)
            bitmap_set(dst, x, y, bitmap_get(src, r.x + x, r.y + y));

    return true;
}
