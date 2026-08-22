#include "pbm.h"

#include <stdio.h>

bool pbm_write_p1(const char *path, const bitmap *bm)
{
    FILE *f = fopen(path, "wb");
    if (!f) return false;

    fprintf(f, "P1\n%d %d\n", bm->w, bm->h);
    for (int y = 0; y < bm->h; y++) {
        for (int x = 0; x < bm->w; x++)
            fputc(bitmap_get(bm, x, y) ? '1' : '0', f);
        fputc('\n', f);
    }

    return fclose(f) == 0;
}

bool pbm_write_p4(const char *path, const bitmap *bm)
{
    FILE *f = fopen(path, "wb");
    if (!f) return false;

    fprintf(f, "P4\n%d %d\n", bm->w, bm->h);
    size_t n = bitmap_bytes(bm);
    bool   ok = fwrite(bm->bits, 1, n, f) == n;

    return fclose(f) == 0 && ok;
}

/* Liest die nächste Zahl des Kopfes und verbraucht genau ein Trennzeichen
 * dahinter. Genau das verlangt P4: nach der Höhe folgt ein einziges
 * Leerzeichen, danach beginnen sofort die Bilddaten. */
static bool read_int(FILE *f, int *out)
{
    int c;

    for (;;) {
        c = fgetc(f);
        if (c == '#') {                       /* Kommentar bis Zeilenende */
            while (c != '\n' && c != EOF) c = fgetc(f);
        } else if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            continue;
        } else {
            break;
        }
    }

    if (c < '0' || c > '9') return false;

    int value = 0;
    while (c >= '0' && c <= '9') {
        value = value * 10 + (c - '0');
        c = fgetc(f);
    }

    *out = value;
    return true;
}

bool pbm_read(const char *path, bitmap *out)
{
    FILE *f = fopen(path, "rb");
    if (!f) return false;

    bool ok = false;
    int  w = 0, h = 0;

    int magic0 = fgetc(f);
    int magic1 = fgetc(f);
    if (magic0 != 'P' || (magic1 != '1' && magic1 != '4')) goto done;
    if (!read_int(f, &w) || !read_int(f, &h))              goto done;
    if (!bitmap_init(out, w, h))                           goto done;

    if (magic1 == '4') {
        size_t n = bitmap_bytes(out);
        ok = fread(out->bits, 1, n, f) == n;
    } else {
        ok = true;
        for (int y = 0; y < h && ok; y++) {
            for (int x = 0; x < w; x++) {
                int c;
                do { c = fgetc(f); } while (c == ' ' || c == '\t' || c == '\r' || c == '\n');
                if (c != '0' && c != '1') { ok = false; break; }
                bitmap_set(out, x, y, c == '1');
            }
        }
    }

    if (!ok) bitmap_free(out);

done:
    fclose(f);
    return ok;
}
