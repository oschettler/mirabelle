/* Siehe theme.h für den Vertrag. Der Parser folgt dem Vorbild aus
 * src/core/keymap.c: "datei:zeile: meldung", niemals ein halbfertiges
 * Ergebnis.
 */
#include "ui/theme.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void theme_defaults(theme *th)
{
    th->titlebar_h      = 20;
    th->border          = 1;
    th->close_box       = 12;
    th->grow_box        = 16;
    th->close_box_top   = 4;
    th->close_box_left  = 7;
    th->box_margin      = 0;
    th->stripe_gap      = 2;
    th->title_pad       = 6;
    th->hit_slop        = 0;
    th->min_w           = 96;
    th->min_h           = 48;
    th->menubar_h       = 20;
    th->menubar_left    = 16;
    th->menu_item_h     = 16;
    th->menu_pad        = 8;
    th->menu_text_pad   = 16;
    th->menu_gap        = 24;
    th->menu_shadow     = 2;
    th->dialog_pad      = 24;
    th->dialog_btn_pad  = 16;
    th->button_h        = 20;
    th->button_min_w    = 62;
    th->button_gap      = 10;
    th->button_radius   = 4;
    th->button_pad      = 6;
    th->default_ring    = 3;
    th->default_gap     = 1;
    th->check_gap       = 6;
    th->scrollbar_w     = 16;
    snprintf(th->font, sizeof th->font, "%s", "system12");
}

/* Wie fail() in keymap.c, liefert hier aber gleich false: der Aufrufer kann
 * dann "return fail(...);" schreiben. */
static bool fail(char *err, size_t err_size, const char *path, int line,
                  const char *fmt, ...)
{
    char msg[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);

    if (err && err_size > 0)
        snprintf(err, err_size, "%s:%d: %s", path, line, msg);

    return false;
}

/* Nächstes durch Leerraum getrenntes Feld ab *cursor, wie in keymap.c. */
static const char *next_token(const char **cursor, size_t *len)
{
    const char *s = *cursor + strspn(*cursor, " \t");
    size_t      l = strcspn(s, " \t\r\n");
    if (l == 0) { *len = 0; return NULL; }

    *len    = l;
    *cursor = s + l;
    return s;
}

typedef struct {
    const char *name;
    int        *field;
} int_field;

bool theme_load(theme *th, const char *path, char *err, size_t err_size)
{
    if (err && err_size > 0) err[0] = '\0';

    /* Beginne mit den Voreinstellungen und überschreibe, was in der Datei steht. */
    theme_defaults(th);

    FILE *f = fopen(path, "rb");
    if (!f)
        return fail(err, err_size, path, 0,
                    "Datei kann nicht geöffnet werden: %s", strerror(errno));

    int_field fields[] = {
        { "titlebar_h", &th->titlebar_h },
        { "border", &th->border },
        { "close_box", &th->close_box },
        { "grow_box", &th->grow_box },
        { "close_box_top", &th->close_box_top },
        { "close_box_left", &th->close_box_left },
        { "box_margin", &th->box_margin },
        { "stripe_gap", &th->stripe_gap },
        { "title_pad", &th->title_pad },
        { "hit_slop", &th->hit_slop },
        { "min_w", &th->min_w },
        { "min_h", &th->min_h },
        { "menubar_h", &th->menubar_h },
        { "menubar_left", &th->menubar_left },
        { "menu_item_h", &th->menu_item_h },
        { "menu_pad", &th->menu_pad },
        { "menu_text_pad", &th->menu_text_pad },
        { "menu_gap", &th->menu_gap },
        { "menu_shadow", &th->menu_shadow },
        { "dialog_pad", &th->dialog_pad },
        { "dialog_btn_pad", &th->dialog_btn_pad },
        { "button_h", &th->button_h },
        { "button_min_w", &th->button_min_w },
        { "button_gap", &th->button_gap },
        { "button_radius", &th->button_radius },
        { "button_pad", &th->button_pad },
        { "default_ring", &th->default_ring },
        { "default_gap", &th->default_gap },
        { "check_gap", &th->check_gap },
        { "scrollbar_w", &th->scrollbar_w },
    };
    size_t field_count = sizeof fields / sizeof fields[0];

    char line[256];
    int  line_no = 0;

    while (fgets(line, sizeof line, f)) {
        line_no++;

        char *hash = strchr(line, '#');
        if (hash) *hash = '\0';

        const char *cur = line;
        size_t      nlen, vlen;
        const char *name_tok = next_token(&cur, &nlen);
        if (!name_tok) continue;   /* Leerzeile oder reiner Kommentar */

        const char *value_tok = next_token(&cur, &vlen);
        if (!value_tok) {
            fclose(f);
            return fail(err, err_size, path, line_no,
                        "fehlender Wert für '%.*s'", (int)nlen, name_tok);
        }

        char name[32];
        if (nlen >= sizeof name) {
            fclose(f);
            return fail(err, err_size, path, line_no,
                        "unbekannter Name: '%.*s'", (int)nlen, name_tok);
        }
        memcpy(name, name_tok, nlen);
        name[nlen] = '\0';

        if (strcmp(name, "font") == 0) {
            if (vlen >= sizeof th->font) {
                fclose(f);
                return fail(err, err_size, path, line_no,
                            "Schriftname zu lang: '%.*s'", (int)vlen, value_tok);
            }
            memcpy(th->font, value_tok, vlen);
            th->font[vlen] = '\0';
            continue;
        }

        char value[64];
        if (vlen >= sizeof value) {
            fclose(f);
            return fail(err, err_size, path, line_no,
                        "Wert zu lang: '%.*s'", (int)vlen, value_tok);
        }
        memcpy(value, value_tok, vlen);
        value[vlen] = '\0';

        int_field *matched = NULL;
        for (size_t i = 0; i < field_count; i++) {
            if (strcmp(name, fields[i].name) == 0) { matched = &fields[i]; break; }
        }
        if (!matched) {
            fclose(f);
            return fail(err, err_size, path, line_no, "unbekannter Name: '%s'", name);
        }

        char *end;
        errno = 0;
        long v = strtol(value, &end, 10);
        if (*end != '\0' || errno == ERANGE) {
            fclose(f);
            return fail(err, err_size, path, line_no, "unlesbare Zahl: '%s'", value);
        }
        *matched->field = (int)v;
    }

    bool had_error = ferror(f) != 0;
    fclose(f);

    if (had_error)
        return fail(err, err_size, path, 0, "Datei nicht lesbar: Lesefehler");

    return true;
}
