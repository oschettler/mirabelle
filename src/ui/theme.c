/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Siehe theme.h für den Vertrag. Der Parser folgt dem Vorbild aus
 * src/core/keymap.c: "datei:zeile: meldung", niemals ein halbfertiges
 * Ergebnis.
 */
#include "ui/theme.h"

#include "core/lines.h"

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
    th->stripe_inset    = 3;
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
    th->check_box       = 12;
    th->check_gap       = 6;
    th->scrollbar_w     = 16;
    snprintf(th->font, sizeof th->font, "%s", "system12");
}

typedef struct {
    const char *name;
    int        *slot;
} int_field;

bool theme_load(theme *th, const char *path, char *err, size_t err_size)
{
    /* Mit den Voreinstellungen anfangen und überschreiben, was in der Datei
     * steht. Ein Thema, das eine Zeile vergisst, ist damit unvollständig, aber
     * nicht kaputt. */
    theme_defaults(th);

    linereader r;
    if (!lines_open(&r, path, err, err_size)) return false;

    int_field fields[] = {
        { "titlebar_h", &th->titlebar_h },
        { "border", &th->border },
        { "close_box", &th->close_box },
        { "grow_box", &th->grow_box },
        { "close_box_top", &th->close_box_top },
        { "close_box_left", &th->close_box_left },
        { "box_margin", &th->box_margin },
        { "stripe_gap", &th->stripe_gap },
        { "stripe_inset", &th->stripe_inset },
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
        { "check_box", &th->check_box },
        { "check_gap", &th->check_gap },
        { "scrollbar_w", &th->scrollbar_w },
    };
    size_t field_count = sizeof fields / sizeof fields[0];

    bool ok = true;
    while (ok && lines_next(&r)) {
        if (r.count != 2) {
            ok = lines_fail(&r, err, err_size,
                            r.count < 2 ? "der Wert fehlt"
                                        : "genau ein Name und ein Wert je Zeile");
            break;
        }

        const char *name  = r.word[0];
        const char *value = r.word[1];

        /* Der Zeichensatz ist der einzige Name unter lauter Zahlen. */
        if (strcmp(name, "font") == 0) {
            if (strlen(value) >= sizeof th->font) {
                ok = lines_fail(&r, err, err_size, "Name des Zeichensatzes zu lang");
                break;
            }
            snprintf(th->font, sizeof th->font, "%s", value);
            continue;
        }

        int *slot = NULL;
        for (size_t k = 0; k < field_count; k++)
            if (strcmp(fields[k].name, name) == 0) { slot = fields[k].slot; break; }

        if (!slot) {
            ok = lines_fail(&r, err, err_size, "unbekannter Name „%s“", name);
            break;
        }

        char *end = NULL;
        long  v   = strtol(value, &end, 10);

        if (end == value || (end && *end)) {
            ok = lines_fail(&r, err, err_size, "„%s“ ist keine Zahl", value);
            break;
        }
        if (v < 0 || v > 10000) {
            ok = lines_fail(&r, err, err_size, "%ld liegt ausserhalb des Bereichs", v);
            break;
        }

        *slot = (int)v;
    }

    lines_close(&r);
    return ok;
}
