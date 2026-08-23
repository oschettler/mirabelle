/* Siehe dialog.h für den Vertrag.
 *
 * Thema und Bildschirmgröße holt der Dialog über wm_theme() und
 * wm_screen_size() von der Fensterverwaltung, in deren Welt er ja auftaucht.
 * Er lädt also keine Datei und rät keine Größe. Eine frühere Fassung tat
 * beides und band den Dialog damit an einen Pfad und an eine Zahl, die auf
 * einem zweiten Gerät falsch wäre.
 */
#include "ui/dialog.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "gfx/draw.h"
#include "gfx/pattern.h"
#include "gfx/text.h"
#include "ui/theme.h"
#include "ui/window.h"

extern const font system12;

/* Vorgaben aus dem Meilensteintext, keine Thema-Werte. */
#define DIALOG_WRAP_MAX_W 280
#define DIALOG_MAX_LINES  6
#define DIALOG_TEXT_MAX   600   /* reicht für die längsten Katalogtexte plus Argumente */
#define DIALOG_LABEL_MAX  64

/* D-9: die logische Bildschirmgröße ist auf Arbeitsplatz und Gerät immer
 * dieselbe - wm.h liefert sie nirgends öffentlich, deshalb steht sie hier. */

struct dialog {
    wm     *m;
    window *win;

    int  line_count;
    char line[DIALOG_MAX_LINES][DIALOG_TEXT_MAX];
    int  dialog_pad;   /* für die Textposition beim Zeichnen gemerkt */

    int  button_count;
    char button_label[DIALOG_MAX_BUTTONS][DIALOG_LABEL_MAX];
    rect button_rect[DIALOG_MAX_BUTTONS];   /* im Koordinatensystem des Inhalts */

    int focus;
    int result;
};

/* Bricht text an ASCII-Leerzeichen auf höchstens max_w Pixel breite Zeilen um,
 * höchstens DIALOG_MAX_LINES Stück. Passt der Rest danach nicht mehr, wird er
 * stillschweigend abgeschnitten. Geschnitten wird ausschließlich an
 * Bytegrenzen mit dem Wert 0x20 - ein Mehrbytezeichen steht nie an einer
 * Wortgrenze und wird deshalb nie zerteilt. */
static void wrap_body(const char *text, char lines[DIALOG_MAX_LINES][DIALOG_TEXT_MAX],
                      int *line_count, int max_w)
{
    *line_count = 0;

    char        current[DIALOG_TEXT_MAX] = "";
    bool        have_word = false;
    const char *p = text;

    for (;;) {
        while (*p == ' ') p++;
        const char *start = p;
        while (*p != ' ' && *p != '\0') p++;
        size_t wlen = (size_t)(p - start);
        if (wlen == 0) break;   /* kein weiteres Wort */

        char candidate[DIALOG_TEXT_MAX];
        if (have_word)
            snprintf(candidate, sizeof candidate, "%s %.*s", current, (int)wlen, start);
        else
            snprintf(candidate, sizeof candidate, "%.*s", (int)wlen, start);

        if (have_word && text_width(&system12, candidate) > max_w) {
            if (*line_count >= DIALOG_MAX_LINES) break;   /* keine Zeile mehr frei */
            snprintf(lines[*line_count], DIALOG_TEXT_MAX, "%s", current);
            (*line_count)++;
            snprintf(current, sizeof current, "%.*s", (int)wlen, start);
            have_word = true;
        } else {
            snprintf(current, sizeof current, "%s", candidate);
            have_word = true;
        }
    }

    if (have_word && *line_count < DIALOG_MAX_LINES) {
        snprintf(lines[*line_count], DIALOG_TEXT_MAX, "%s", current);
        (*line_count)++;
    }
}

dialog *dialog_open(wm *m, const catalog *cat,
                    const char *body_key,
                    const char *const *args, int argc,
                    const char *const *button_keys, int button_count)
{
    if (button_count < 1) return NULL;
    if (button_count > DIALOG_MAX_BUTTONS) button_count = DIALOG_MAX_BUTTONS;

    dialog *d = calloc(1, sizeof *d);
    if (!d) return NULL;

    d->m            = m;
    d->button_count = button_count;
    d->focus        = button_count - 1;   /* der Voreinstellungsknopf */
    d->result       = DIALOG_OPEN;

    char body[DIALOG_TEXT_MAX];
    if (!Tf(cat, body_key, body, sizeof body, args, argc))
        snprintf(body, sizeof body, "%s", T(cat, body_key));
    wrap_body(body, d->line, &d->line_count, DIALOG_WRAP_MAX_W);

    for (int i = 0; i < button_count; i++)
        snprintf(d->button_label[i], DIALOG_LABEL_MAX, "%s", T(cat, button_keys[i]));

    theme th;
    th = *wm_theme(m);
    d->dialog_pad = th.dialog_pad;

    int max_line_w = 0;
    for (int i = 0; i < d->line_count; i++) {
        int lw = text_width(&system12, d->line[i]);
        if (lw > max_line_w) max_line_w = lw;
    }

    int bw[DIALOG_MAX_BUTTONS];
    int row_w = 0;
    for (int i = 0; i < button_count; i++) {
        int tw = text_width(&system12, d->button_label[i]);
        bw[i] = tw + 2 * th.menu_pad;
        if (bw[i] < th.button_min_w) bw[i] = th.button_min_w;
        row_w += bw[i];
        if (i > 0) row_w += th.button_gap;
    }

    int content_w = (max_line_w > row_w ? max_line_w : row_w) + 2 * th.dialog_pad;
    int text_h    = d->line_count * system12.size;
    int content_h = text_h + th.dialog_pad + th.button_h + 2 * th.dialog_pad;

    int bx = content_w - th.dialog_pad - row_w;
    int by = content_h - th.dialog_pad - th.button_h;
    for (int i = 0; i < button_count; i++) {
        d->button_rect[i] = rect_make(bx, by, bw[i], th.button_h);
        bx += bw[i] + th.button_gap;
    }

    int frame_w = content_w + 2 * th.border;
    int frame_h = content_h + th.titlebar_h + th.border;
    int screen_w = 0, screen_h = 0;
    wm_screen_size(m, &screen_w, &screen_h);
    int frame_x = (screen_w - frame_w) / 2;
    int frame_y = (screen_h - frame_h) / 2;

    d->win = wm_open(m, rect_make(frame_x, frame_y, frame_w, frame_h), "", WIN_MODAL);
    if (!d->win) {
        free(d);
        return NULL;
    }

    return d;
}

void dialog_close(dialog *d)
{
    if (!d) return;
    wm_close(d->m, d->win);
    free(d);
}

int dialog_result(const dialog *d)
{
    return d->result;
}

window *dialog_window(const dialog *d)
{
    return d->win;
}

void dialog_draw(dialog *d)
{
    gc g;
    window_gc(d->win, &g);

    g.pat  = PAT_WHITE;
    g.mode = GFX_COPY;
    gfx_clear(&g);
    g.pat = PAT_BLACK;

    for (int i = 0; i < d->line_count; i++) {
        int y = d->dialog_pad + i * system12.size + system12.ascent;
        gfx_text(&g, &system12, d->dialog_pad, y, d->line[i]);
    }

    for (int i = 0; i < d->button_count; i++) {
        rect r = d->button_rect[i];

        g.pat = PAT_WHITE;
        gfx_fill_rect(&g, r);
        g.pat = PAT_BLACK;
        gfx_frame_rect(&g, r);

        if (i == d->button_count - 1) {   /* der Voreinstellungsknopf */
            rect outer = rect_make(r.x - 2, r.y - 2, r.w + 4, r.h + 4);
            gfx_frame_rect(&g, outer);
        }

        int tw = text_width(&system12, d->button_label[i]);
        int tx = r.x + (r.w - tw) / 2;
        int ty = r.y + (r.h - system12.size) / 2 + system12.ascent;
        gfx_text(&g, &system12, tx, ty, d->button_label[i]);

        if (i == d->focus) gfx_invert_rect(&g, r);
    }
}

bool dialog_event(dialog *d, const event *e)
{
    if (d->result != DIALOG_OPEN) return false;

    switch (e->kind) {
    case EV_MOUSE_DOWN: {
        rect cr = window_content_rect(d->win);
        int  lx = e->x - cr.x;
        int  ly = e->y - cr.y;

        for (int i = 0; i < d->button_count; i++) {
            if (rect_contains(d->button_rect[i], lx, ly)) {
                d->focus  = i;
                d->result = i;
                return true;
            }
        }
        return false;
    }

    case EV_KEY_DOWN:
        switch (e->key) {
        case KEY_RETURN:
            d->result = d->focus;
            return true;

        case KEY_ESCAPE:
            d->result = 0;   /* der erste Knopf ist immer der abbrechende */
            return true;

        case KEY_TAB:
            if (e->mods & MOD_SHIFT)
                d->focus = (d->focus + d->button_count - 1) % d->button_count;
            else
                d->focus = (d->focus + 1) % d->button_count;
            return true;

        case KEY_RIGHT:
            d->focus = (d->focus + 1) % d->button_count;
            return true;

        case KEY_LEFT:
            d->focus = (d->focus + d->button_count - 1) % d->button_count;
            return true;

        default:
            return false;
        }

    default:
        return false;
    }
}
