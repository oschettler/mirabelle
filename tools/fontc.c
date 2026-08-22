/* Siehe fontc.h und docs/fnt-format.md für das Format.
 *
 * Der Übersetzer ist das Sicherheitsnetz für handgeschriebene Zeichensätze:
 * jeder Fehler soll sofort und mit Zeilenbezug zeigen, was falsch ist, statt
 * still eine falsche Glyphe zu erzeugen.
 */
#include "fontc.h"

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Eine im Bau befindliche oder fertige Glyphe. bits zeigt auf stride*size
 * Bytes, die die Zeile für Zeile gesetzten Pixel enthalten (siehe
 * gfx/bitmap.h: MSB links, Füllbits am Zeilenende immer 0). */
typedef struct {
    uint32_t codepoint;
    int      width;
    int      stride;
    int      line;      /* Zeile der "glyph"-Deklaration, für Meldungen */
    uint8_t *bits;
    uint32_t offset;     /* wird erst beim Schreiben gesetzt */
} glyph_data;

typedef struct {
    char *name;
    bool  have_name, have_size, have_ascent;
    int   size;
    int   ascent;
    int   ascent_line;

    glyph_data *glyphs;
    size_t      glyph_count;
    size_t      glyph_cap;
} fontc_ctx;

/* Baut die Meldung "datei:zeile: meldung" in errbuf und liefert immer 1,
 * damit Aufrufer direkt "return fail(...);" schreiben können. */
static int fail(char *errbuf, size_t errbuf_size, const char *path, int line,
                 const char *fmt, ...)
{
    char msg[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);

    if (errbuf && errbuf_size > 0)
        snprintf(errbuf, errbuf_size, "%s:%d: %s", path, line, msg);

    return 1;
}

/* Liest eine Zeile ohne Zeilenende in einen wachsenden Puffer. false am
 * echten Dateiende (keine weitere Zeile). Die letzte Zeile ohne
 * abschließendes '\n' zählt noch als Zeile. */
static bool read_line(FILE *f, char **buf, size_t *cap, int *line_no)
{
    size_t len = 0;
    bool   got_any = false;
    int    c;

    for (;;) {
        c = fgetc(f);
        if (c == EOF) {
            if (!got_any) return false;
            break;
        }
        got_any = true;
        if (c == '\n') break;

        if (len + 2 > *cap) {
            size_t newcap = (*cap == 0) ? 128 : (*cap * 2);
            char  *p = realloc(*buf, newcap);
            if (!p) return false;
            *buf = p;
            *cap = newcap;
        }
        (*buf)[len++] = (char)c;
    }

    (*buf)[len] = '\0';
    if (len > 0 && (*buf)[len - 1] == '\r') (*buf)[len - 1] = '\0';
    (*line_no)++;
    return true;
}

/* Liest ein optionales '-' und dann nur Ziffern, ohne Anhängsel. */
static bool parse_int(const char *s, int *out)
{
    if (*s == '\0') return false;

    char *end;
    errno = 0;
    long v = strtol(s, &end, 10);
    if (*end != '\0') return false;
    if (errno == ERANGE || v < INT_MIN || v > INT_MAX) return false;

    *out = (int)v;
    return true;
}

/* "U+" gefolgt von genau 4 bis 6 Hexziffern, sonst ungültig. */
static bool parse_codepoint(const char *s, uint32_t *out)
{
    if (strncmp(s, "U+", 2) != 0) return false;

    const char *digits = s + 2;
    size_t      len     = strlen(digits);
    if (len < 4 || len > 6) return false;

    uint32_t value = 0;
    for (size_t i = 0; i < len; i++) {
        char c = digits[i];
        int  d;
        if      (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else return false;
        value = value * 16 + (uint32_t)d;
    }

    *out = value;
    return true;
}

/* Nächstes durch Leerraum getrenntes Wort ab *cursor. Setzt *cursor hinter
 * das Wort (nicht hinter folgenden Leerraum). NULL, wenn nichts mehr da ist. */
static const char *next_token(const char **cursor, size_t *len)
{
    const char *s = *cursor + strspn(*cursor, " \t");
    size_t      l = strcspn(s, " \t");
    if (l == 0) { *len = 0; return NULL; }

    *len     = l;
    *cursor  = s + l;
    return s;
}

static bool glyphs_push(fontc_ctx *ctx, glyph_data g)
{
    if (ctx->glyph_count == ctx->glyph_cap) {
        size_t      newcap = ctx->glyph_cap ? ctx->glyph_cap * 2 : 16;
        glyph_data *p       = realloc(ctx->glyphs, newcap * sizeof *p);
        if (!p) return false;
        ctx->glyphs   = p;
        ctx->glyph_cap = newcap;
    }
    ctx->glyphs[ctx->glyph_count++] = g;
    return true;
}

static int cmp_codepoint(const void *a, const void *b)
{
    const glyph_data *ga = a, *gb = b;
    if (ga->codepoint < gb->codepoint) return -1;
    if (ga->codepoint > gb->codepoint) return 1;
    return 0;
}

/* name, size und ascent müssen vor der ersten Glyphe vollständig und gültig
 * sein. Wird sowohl beim Erreichen der ersten Glyphe als auch - falls die
 * Datei gar keine Glyphe enthält - am Dateiende aufgerufen. */
static int check_header(const fontc_ctx *ctx, const char *in_path, int line_no,
                         char *errbuf, size_t errbuf_size)
{
    if (!ctx->have_name)
        return fail(errbuf, errbuf_size, in_path, line_no,
                    "name fehlt oder steht nach der ersten Glyphe");
    if (!ctx->have_size)
        return fail(errbuf, errbuf_size, in_path, line_no,
                    "size fehlt oder steht nach der ersten Glyphe");
    if (!ctx->have_ascent)
        return fail(errbuf, errbuf_size, in_path, line_no,
                    "ascent fehlt oder steht nach der ersten Glyphe");
    if (ctx->ascent > ctx->size)
        return fail(errbuf, errbuf_size, in_path, ctx->ascent_line,
                    "ascent (%d) darf nicht größer als size (%d) sein",
                    ctx->ascent, ctx->size);
    return 0;
}

static void write_escaped(FILE *out, const char *s)
{
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (*p == '"' || *p == '\\') fputc('\\', out);
        fputc((char)*p, out);
    }
}

static bool write_output(FILE *out, const fontc_ctx *ctx, const char *in_path,
                          const char *symbol)
{
    fprintf(out, "/* Erzeugt von tools/fontc aus %s.\n", in_path);
    fprintf(out, " * Nicht von Hand ändern - Änderungen gehen beim nächsten Lauf verloren. */\n\n");
    fprintf(out, "#include \"gfx/font.h\"\n\n");

    fprintf(out, "static const uint8_t %s_bits[] = {\n", symbol);
    int col = 0;
    for (size_t i = 0; i < ctx->glyph_count; i++) {
        const glyph_data *g = &ctx->glyphs[i];
        size_t n = (size_t)g->stride * (size_t)ctx->size;
        for (size_t b = 0; b < n; b++) {
            if (col == 0) fprintf(out, "    ");
            fprintf(out, "0x%02X,", (unsigned)g->bits[b]);
            col++;
            if (col == 12) { fputc('\n', out); col = 0; }
            else            fputc(' ', out);
        }
    }
    if (col != 0) fputc('\n', out);
    fprintf(out, "};\n\n");

    fprintf(out, "/* codepoint, width, stride, offset */\n");
    fprintf(out, "static const glyph %s_glyphs[] = {\n", symbol);
    for (size_t i = 0; i < ctx->glyph_count; i++) {
        const glyph_data *g = &ctx->glyphs[i];
        fprintf(out, "    { 0x%04X, %d, %d, %u },  /* U+%04X */\n",
                (unsigned)g->codepoint, g->width, g->stride, (unsigned)g->offset,
                (unsigned)g->codepoint);
    }
    fprintf(out, "};\n\n");

    fprintf(out, "const font %s = {\n", symbol);
    fprintf(out, "    .name   = \"");
    write_escaped(out, ctx->name);
    fprintf(out, "\",\n");
    fprintf(out, "    .size   = %d,\n", ctx->size);
    fprintf(out, "    .ascent = %d,\n", ctx->ascent);
    fprintf(out, "    .count  = %d,\n", (int)ctx->glyph_count);
    fprintf(out, "    .glyphs = %s_glyphs,\n", symbol);
    fprintf(out, "    .bits   = %s_bits,\n", symbol);
    fprintf(out, "};\n");

    return !ferror(out);
}

static int parse_and_generate(fontc_ctx *ctx, const char *in_path,
                               const char *out_path, const char *symbol,
                               char *errbuf, size_t errbuf_size)
{
    FILE *f = fopen(in_path, "rb");
    if (!f)
        return fail(errbuf, errbuf_size, in_path, 0,
                    "Datei kann nicht geöffnet werden: %s", strerror(errno));

    int  line_no = 0;
    bool seen_first_glyph = false;
    bool seen_fffd        = false;

    int rows_needed    = 0;
    int rows_collected  = 0;
    glyph_data pending  = {0};
    bool pending_excess_check = false;

    char  *line     = NULL;
    size_t line_cap = 0;

    while (read_line(f, &line, &line_cap, &line_no)) {
        char *p = line + strspn(line, " \t");

        if (rows_needed > 0) {
            if (*p == '\0') continue;   /* Leerzeile, zählt nicht */

            size_t wl = strcspn(p, " \t");
            bool looks_like_keyword =
                (wl == 5 && strncmp(p, "glyph",  5) == 0) ||
                (wl == 4 && strncmp(p, "name",   4) == 0) ||
                (wl == 4 && strncmp(p, "size",   4) == 0) ||
                (wl == 6 && strncmp(p, "ascent", 6) == 0);
            if (looks_like_keyword) {
                int rc = fail(errbuf, errbuf_size, in_path, line_no,
                               "zu wenige Pixelzeilen in Glyphe U+%04X: erwartet %d, %d gefunden",
                               (unsigned)pending.codepoint, ctx->size, rows_collected);
                free(pending.bits);
                free(line);
                fclose(f);
                return rc;
            }

            size_t plen = strlen(p);
            if (plen != (size_t)pending.width) {
                int rc = fail(errbuf, errbuf_size, in_path, line_no,
                               "Glyphe U+%04X: Pixelzeile hat falsche Länge: erwartet %d, %zu gefunden",
                               (unsigned)pending.codepoint, pending.width, plen);
                free(pending.bits);
                free(line);
                fclose(f);
                return rc;
            }

            for (size_t i = 0; i < plen; i++) {
                if (p[i] != '#' && p[i] != '.') {
                    int rc = fail(errbuf, errbuf_size, in_path, line_no,
                                   "Glyphe U+%04X: ungültiges Zeichen '%c' in Pixelzeile, Spalte %zu",
                                   (unsigned)pending.codepoint, p[i], i + 1);
                    free(pending.bits);
                    free(line);
                    fclose(f);
                    return rc;
                }
            }

            for (size_t x = 0; x < plen; x++) {
                if (p[x] == '#') {
                    size_t idx = (size_t)rows_collected * (size_t)pending.stride + x / 8;
                    pending.bits[idx] |= (uint8_t)(0x80u >> (x % 8));
                }
            }

            rows_collected++;
            if (rows_collected == rows_needed) {
                if (!glyphs_push(ctx, pending)) {
                    int rc = fail(errbuf, errbuf_size, in_path, line_no, "Speicher reicht nicht");
                    free(pending.bits);
                    free(line);
                    fclose(f);
                    return rc;
                }
                rows_needed = 0;
                pending_excess_check = true;
            }
            continue;
        }

        /* Zustand: außerhalb einer Glyphe, Schlüsselwortzeile erwartet. */
        if (pending_excess_check) {
            pending_excess_check = false;
            if (*p != '\0') {
                bool all_pixel_chars = true;
                for (const char *q = p; *q; q++)
                    if (*q != '#' && *q != '.') { all_pixel_chars = false; break; }
                if (all_pixel_chars) {
                    const glyph_data *lastg = &ctx->glyphs[ctx->glyph_count - 1];
                    int rc = fail(errbuf, errbuf_size, in_path, line_no,
                                   "zu viele Pixelzeilen in Glyphe U+%04X: erwartet %d, mindestens %d gefunden",
                                   (unsigned)lastg->codepoint, ctx->size, ctx->size + 1);
                    free(line);
                    fclose(f);
                    return rc;
                }
            }
        }

        char *hash = strchr(p, '#');
        if (hash) *hash = '\0';
        size_t tlen = strlen(p);
        while (tlen > 0 && (p[tlen - 1] == ' ' || p[tlen - 1] == '\t')) p[--tlen] = '\0';
        if (tlen == 0) continue;   /* Leerzeile oder reiner Kommentar */

        const char *cur = p;
        size_t      l1;
        const char *t1 = next_token(&cur, &l1);
        const char *rest = cur + strspn(cur, " \t");

        if (l1 == 4 && strncmp(t1, "name", 4) == 0) {
            if (seen_first_glyph) {
                int rc = fail(errbuf, errbuf_size, in_path, line_no,
                               "name darf nicht nach der ersten Glyphe stehen");
                free(line); fclose(f); return rc;
            }
            if (*rest == '\0') {
                int rc = fail(errbuf, errbuf_size, in_path, line_no, "name ohne Wert");
                free(line); fclose(f); return rc;
            }
            char *copy = malloc(strlen(rest) + 1);
            if (!copy) {
                int rc = fail(errbuf, errbuf_size, in_path, line_no, "Speicher reicht nicht");
                free(line); fclose(f); return rc;
            }
            strcpy(copy, rest);
            free(ctx->name);
            ctx->name      = copy;
            ctx->have_name = true;

        } else if (l1 == 4 && strncmp(t1, "size", 4) == 0) {
            if (seen_first_glyph) {
                int rc = fail(errbuf, errbuf_size, in_path, line_no,
                               "size darf nicht nach der ersten Glyphe stehen");
                free(line); fclose(f); return rc;
            }
            int val;
            if (!parse_int(rest, &val)) {
                int rc = fail(errbuf, errbuf_size, in_path, line_no,
                               "ungültige Zahl bei size: '%s'", rest);
                free(line); fclose(f); return rc;
            }
            if (val <= 0) {
                int rc = fail(errbuf, errbuf_size, in_path, line_no,
                               "size muss größer als 0 sein, ist %d", val);
                free(line); fclose(f); return rc;
            }
            ctx->size      = val;
            ctx->have_size = true;

        } else if (l1 == 6 && strncmp(t1, "ascent", 6) == 0) {
            if (seen_first_glyph) {
                int rc = fail(errbuf, errbuf_size, in_path, line_no,
                               "ascent darf nicht nach der ersten Glyphe stehen");
                free(line); fclose(f); return rc;
            }
            int val;
            if (!parse_int(rest, &val)) {
                int rc = fail(errbuf, errbuf_size, in_path, line_no,
                               "ungültige Zahl bei ascent: '%s'", rest);
                free(line); fclose(f); return rc;
            }
            if (val < 0) {
                int rc = fail(errbuf, errbuf_size, in_path, line_no,
                               "ascent darf nicht negativ sein, ist %d", val);
                free(line); fclose(f); return rc;
            }
            ctx->ascent      = val;
            ctx->have_ascent = true;
            ctx->ascent_line = line_no;

        } else if (l1 == 5 && strncmp(t1, "glyph", 5) == 0) {
            if (!seen_first_glyph) {
                int rc = check_header(ctx, in_path, line_no, errbuf, errbuf_size);
                if (rc) { free(line); fclose(f); return rc; }
            }
            seen_first_glyph = true;

            size_t      lc, lw, lv, lext;
            const char *cp_tok    = next_token(&cur, &lc);
            const char *width_kw  = next_token(&cur, &lw);
            const char *width_val = next_token(&cur, &lv);
            const char *extra     = next_token(&cur, &lext);

            if (!cp_tok || !width_kw || !width_val || extra ||
                lw != 5 || strncmp(width_kw, "width", 5) != 0) {
                int rc = fail(errbuf, errbuf_size, in_path, line_no,
                               "ungültige glyph-Zeile (erwartet 'glyph U+XXXX width N'): '%s'", rest);
                free(line); fclose(f); return rc;
            }

            char cp_buf[16];
            if (lc >= sizeof cp_buf) {
                int rc = fail(errbuf, errbuf_size, in_path, line_no,
                               "kaputte Codepunktschreibweise: '%.*s'", (int)lc, cp_tok);
                free(line); fclose(f); return rc;
            }
            memcpy(cp_buf, cp_tok, lc);
            cp_buf[lc] = '\0';

            uint32_t codepoint;
            if (!parse_codepoint(cp_buf, &codepoint)) {
                int rc = fail(errbuf, errbuf_size, in_path, line_no,
                               "kaputte Codepunktschreibweise: '%s' (erwartet U+ gefolgt von 4 bis 6 Hexziffern)",
                               cp_buf);
                free(line); fclose(f); return rc;
            }
            if (codepoint > 0x10FFFF) {
                int rc = fail(errbuf, errbuf_size, in_path, line_no,
                               "Codepunkt U+%04X liegt über U+10FFFF", (unsigned)codepoint);
                free(line); fclose(f); return rc;
            }
            if (codepoint >= 0xD800 && codepoint <= 0xDFFF) {
                int rc = fail(errbuf, errbuf_size, in_path, line_no,
                               "Codepunkt U+%04X liegt im Surrogatbereich U+D800..U+DFFF", (unsigned)codepoint);
                free(line); fclose(f); return rc;
            }
            for (size_t i = 0; i < ctx->glyph_count; i++) {
                if (ctx->glyphs[i].codepoint == codepoint) {
                    int rc = fail(errbuf, errbuf_size, in_path, line_no,
                                   "doppelter Codepunkt U+%04X (zuerst in Zeile %d)",
                                   (unsigned)codepoint, ctx->glyphs[i].line);
                    free(line); fclose(f); return rc;
                }
            }

            char wv_buf[16];
            if (lv >= sizeof wv_buf) {
                int rc = fail(errbuf, errbuf_size, in_path, line_no,
                               "ungültige Zahl bei width: '%.*s'", (int)lv, width_val);
                free(line); fclose(f); return rc;
            }
            memcpy(wv_buf, width_val, lv);
            wv_buf[lv] = '\0';

            int width;
            if (!parse_int(wv_buf, &width)) {
                int rc = fail(errbuf, errbuf_size, in_path, line_no,
                               "ungültige Zahl bei width: '%s'", wv_buf);
                free(line); fclose(f); return rc;
            }
            if (width <= 0) {
                int rc = fail(errbuf, errbuf_size, in_path, line_no,
                               "width muss größer als 0 sein, ist %d", width);
                free(line); fclose(f); return rc;
            }
            if (width > 64) {
                int rc = fail(errbuf, errbuf_size, in_path, line_no,
                               "width darf höchstens 64 sein, ist %d", width);
                free(line); fclose(f); return rc;
            }

            if (codepoint == 0xFFFD) seen_fffd = true;

            pending.codepoint = codepoint;
            pending.width     = width;
            pending.stride    = (width + 7) / 8;
            pending.line      = line_no;
            pending.bits      = calloc((size_t)pending.stride * (size_t)ctx->size, 1);
            if (!pending.bits) {
                int rc = fail(errbuf, errbuf_size, in_path, line_no, "Speicher reicht nicht");
                free(line); fclose(f); return rc;
            }
            rows_needed    = ctx->size;
            rows_collected = 0;

        } else {
            int rc = fail(errbuf, errbuf_size, in_path, line_no,
                           "unbekanntes Schlüsselwort: '%.*s'", (int)l1, t1);
            free(line); fclose(f); return rc;
        }
    }

    if (ferror(f)) {
        free(line);
        fclose(f);
        return fail(errbuf, errbuf_size, in_path, 0, "Datei nicht lesbar: Lesefehler");
    }
    fclose(f);
    free(line);

    if (rows_needed > 0) {
        int rc = fail(errbuf, errbuf_size, in_path, line_no,
                       "zu wenige Pixelzeilen in Glyphe U+%04X: erwartet %d, %d gefunden",
                       (unsigned)pending.codepoint, ctx->size, rows_collected);
        free(pending.bits);
        return rc;
    }

    if (!seen_first_glyph) {
        int rc = check_header(ctx, in_path, line_no > 0 ? line_no : 1, errbuf, errbuf_size);
        if (rc) return rc;
    }

    if (!seen_fffd)
        return fail(errbuf, errbuf_size, in_path, line_no > 0 ? line_no : 1,
                     "Zeichensatz muss U+FFFD (Ersatzzeichen) enthalten");

    qsort(ctx->glyphs, ctx->glyph_count, sizeof(glyph_data), cmp_codepoint);

    uint32_t offset = 0;
    for (size_t i = 0; i < ctx->glyph_count; i++) {
        ctx->glyphs[i].offset = offset;
        offset += (uint32_t)ctx->glyphs[i].stride * (uint32_t)ctx->size;
    }

    FILE *out = fopen(out_path, "wb");
    if (!out)
        return fail(errbuf, errbuf_size, out_path, 0,
                     "Ausgabedatei kann nicht geschrieben werden: %s", strerror(errno));

    bool ok        = write_output(out, ctx, in_path, symbol);
    bool closed_ok = (fclose(out) == 0);
    if (!ok || !closed_ok) {
        remove(out_path);
        return fail(errbuf, errbuf_size, out_path, 0, "Fehler beim Schreiben der Ausgabedatei");
    }

    return 0;
}

int fontc_run(const char *in_path, const char *out_path, const char *symbol,
              char *errbuf, size_t errbuf_size)
{
    if (errbuf && errbuf_size > 0) errbuf[0] = '\0';

    fontc_ctx ctx = {0};
    int       rc  = parse_and_generate(&ctx, in_path, out_path, symbol, errbuf, errbuf_size);

    free(ctx.name);
    for (size_t i = 0; i < ctx.glyph_count; i++) free(ctx.glyphs[i].bits);
    free(ctx.glyphs);

    return rc;
}

/* main() entfällt, wenn diese Quelle als Bibliothek in den Test gelinkt wird -
 * der bringt seine eigene mit. */
#ifndef PDA_FONTC_NO_MAIN

int main(int argc, char **argv)
{
    if (argc != 4) {
        fprintf(stderr, "Aufruf: %s <eingabe.fnt> <ausgabe.c> <symbolname>\n", argv[0]);
        return 1;
    }

    char errbuf[512];
    if (fontc_run(argv[1], argv[2], argv[3], errbuf, sizeof errbuf) != 0) {
        fprintf(stderr, "%s\n", errbuf);
        return 1;
    }
    return 0;
}

#endif /* PDA_FONTC_NO_MAIN */
