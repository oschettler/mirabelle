/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Siehe gemview.h für den Vertrag.
 *
 * Der Umbruch entsteht in einer eigenen Liste von Anzeigezeilen, genau wie im
 * mehrzeiligen Textfeld (widget_text.c) und aus demselben Grund: er hängt an
 * der Breite, und die setzt das Layout. Ein Umbruch, der beim Zeichnen
 * entsteht, müsste bei jedem Bild neu gerechnet werden; einer, der beim
 * Setzen entsteht, wäre falsch, sobald jemand das Fenster zieht.
 */
#include "app/gemview.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/i18n.h"
#include "gfx/draw.h"
#include "gfx/font.h"
#include "gfx/pattern.h"
#include "gfx/text.h"
#include "plat/plat.h"
#include "store/gemtext.h"

extern const font system12;

#define LINES_MAX 1024
#define LINKS_MAX 128

typedef struct {
    const char *text;      /* zeigt in den Quelltext, nicht nullterminiert */
    size_t      len;
    gem_kind    kind;
    int         level;     /* bei Überschriften */
    int         link;      /* Nummer des Verweises, oder 0 */
} display_line;

typedef struct {
    const char *url;
    size_t      url_len;
} link_entry;

typedef struct {
    widget base;

    const char *src;
    size_t      src_len;

    display_line lines[LINES_MAX];
    int          line_count;

    link_entry links[LINKS_MAX];
    int        link_count;

    int  selected;         /* Verweisnummer, oder 0 */
    bool opened;

    scrollmodel sc;
    bool        wrap_valid;
    int         wrap_w;
} gemview;

/* --- Maße ------------------------------------------------------------------------
 *
 * Eine Anzeigezeile ist so hoch wie die Schrift plus etwas Luft. Überschriften
 * bekommen keine eigene Größe - es gibt nur einen Schnitt -, sondern werden
 * durch einen Balken darunter hervorgehoben. Bei einem Bit je Pixel ist das
 * der Unterschied, den man sehen kann.
 */

/* Der Einzug einer Aufzählungszeile: das Zeichen aus dem Katalog plus ein
 * Leerzeichen. Aus dem Katalog, weil das Aufzählungszeichen eine
 * Darstellungsentscheidung ist - manche Umsetzungen nehmen einen Strich, und
 * wer eine Schrift mit einem echten Punkt hat, will den. */
static int item_indent(const widget *w)
{
    char buf[16];
    snprintf(buf, sizeof buf, "%s ", T(w->cat, "gemtext.bullet"));
    return text_width(&system12, buf);
}

static int line_h(const widget *w)
{
    (void)w;
    return system12.size + 2;
}

static rect content_rect(const widget *w)
{
    int pad = w->th->menu_pad;
    return rect_make(w->frame.x + pad, w->frame.y + pad / 2,
                     w->frame.w - 2 * pad, w->frame.h - pad);
}

static int visible_lines(const widget *w)
{
    rect c = content_rect(w);
    if (c.h <= 0) return 0;
    return c.h / line_h(w);
}

/* --- Umbruch --------------------------------------------------------------------- */

static void push_line(gemview *gv, const char *text, size_t len,
                      gem_kind kind, int level, int link)
{
    if (gv->line_count >= LINES_MAX) return;

    display_line *l = &gv->lines[gv->line_count++];
    l->text  = text;
    l->len   = len;
    l->kind  = kind;
    l->level = level;
    l->link  = link;
}

/* Bricht einen Textabschnitt auf width Pixel um.
 *
 * Umbrochen wird an Leerzeichen. Passt ein einzelnes Wort nicht, wird es hart
 * getrennt - sonst liefe eine sehr lange Adresse aus dem Fenster heraus, und
 * der Nutzer sähe nicht, dass da noch etwas ist. */
static void wrap_into(gemview *gv, const char *text, size_t len,
                      gem_kind kind, int level, int link, int width, int indent)
{
    int avail = width - indent;
    if (avail < 16) avail = 16;

    if (len == 0) {
        push_line(gv, text, 0, kind, level, link);
        return;
    }

    size_t start = 0;
    while (start < len) {
        size_t take = 0, last_space = 0;
        int    used = 0;

        while (start + take < len) {
            /* Ein Zeichen kann mehrere Bytes haben; text_width misst den
             * ganzen Abschnitt, also wird er stückweise verlängert. */
            size_t next = take + 1;
            while (start + next < len &&
                   ((unsigned char)text[start + next] & 0xC0) == 0x80) next++;

            char    tmp[512];
            size_t  n = next < sizeof tmp ? next : sizeof tmp - 1;
            memcpy(tmp, text + start, n);
            tmp[n] = '\0';

            used = text_width(&system12, tmp);
            if (used > avail && take > 0) break;

            if (text[start + take] == ' ') last_space = take;
            take = next;
        }

        size_t cut = take;
        if (start + take < len && last_space > 0) cut = last_space;

        push_line(gv, text + start, cut, kind, level, link);

        start += cut;
        while (start < len && text[start] == ' ') start++;
    }
}

typedef struct { gemview *gv; int width; } wrap_ctx;

static void on_gem_line(const gem_line *line, void *user)
{
    wrap_ctx *ctx = user;
    gemview  *gv  = ctx->gv;

    int link   = 0;
    int indent = 0;

    switch (line->kind) {
    case GEM_LINK:
        if (gv->link_count < LINKS_MAX) {
            gv->links[gv->link_count].url     = line->url;
            gv->links[gv->link_count].url_len = line->url_len;
            gv->link_count++;
            link = gv->link_count;
        }
        indent = text_width(&system12, "[88] ");
        break;

    case GEM_ITEM:
        indent = item_indent(&gv->base);
        break;

    case GEM_QUOTE:
        indent = text_width(&system12, "| ");
        break;

    default:
        break;
    }

    /* Ein Verweis ohne Namen wird durch seine Adresse vertreten - sonst stünde
     * dort nur eine Nummer, und niemand wüsste, wohin sie führt. */
    const char *text = line->text;
    size_t      len  = line->text_len;
    if (line->kind == GEM_LINK && len == 0) {
        text = line->url;
        len  = line->url_len;
    }

    /* Vorformatiertes bricht nicht um: dort bedeutet die Zeilenlage etwas. */
    if (line->kind == GEM_PRE) {
        push_line(gv, text, len, GEM_PRE, 0, 0);
        return;
    }

    wrap_into(gv, text, len, line->kind, line->level, link, ctx->width, indent);
}

static void ensure_wrap(const gemview *gv_const)
{
    gemview *gv = (gemview *)gv_const;

    rect c = content_rect(&gv->base);
    int  w = c.w > 16 ? c.w : 16;

    if (!gv->wrap_valid || gv->wrap_w != w) {
        gv->line_count = 0;
        gv->link_count = 0;

        if (gv->src && gv->src_len) {
            wrap_ctx ctx = { gv, w };
            gemtext_parse(gv->src, gv->src_len, on_gem_line, &ctx);
        }

        gv->wrap_valid = true;
        gv->wrap_w     = w;

        if (gv->selected > gv->link_count) gv->selected = 0;
    }

    scroll_set(&gv->sc, gv->line_count, visible_lines(&gv->base));
}

/* --- Zugriffe -------------------------------------------------------------------- */

void gemview_set_text(widget *w, const char *gemtext, size_t len)
{
    gemview *gv = (gemview *)w;

    gv->src        = gemtext;
    gv->src_len    = gemtext ? len : 0;
    gv->wrap_valid = false;
    gv->selected   = 0;
    gv->sc.value   = 0;
}

scrollmodel *gemview_scroll(widget *w)
{
    gemview *gv = (gemview *)w;
    ensure_wrap(gv);
    return &gv->sc;
}

int gemview_link_count(const widget *w)
{
    ensure_wrap((const gemview *)w);
    return ((const gemview *)w)->link_count;
}

const char *gemview_link_url(const widget *w, int number, size_t *len_out)
{
    const gemview *gv = (const gemview *)w;
    ensure_wrap(gv);

    /* Nummern beginnen bei eins, wie angezeigt.
     *
     * Ein Test kann diese Grenze nicht von ihrem Fehlen unterscheiden: vor
     * links liegt im Speicher das nie benutzte Ende von lines, und calloc hat
     * es genullt - ein Zugriff auf links[-1] liefert deshalb zufällig NULL.
     * Die Prüfung bleibt trotzdem stehen; sich darauf zu verlassen, was
     * neben einem Feld liegt, wäre keine Programmierung. */
    if (number < 1 || number > gv->link_count) return NULL;
    if (len_out) *len_out = gv->links[number - 1].url_len;
    return gv->links[number - 1].url;
}

int gemview_selected_link(const widget *w)
{
    return ((const gemview *)w)->selected;
}

void gemview_select_link(widget *w, int number)
{
    gemview *gv = (gemview *)w;
    ensure_wrap(gv);

    if (number < 0 || number > gv->link_count) return;
    gv->selected = number;

    /* Den gewählten Verweis ins Bild holen. Ihn auszuwählen und nicht zu
     * zeigen wäre die halbe Handlung. */
    for (int i = 0; i < gv->line_count; i++)
        if (gv->lines[i].link == number) {
            scroll_reveal(&gv->sc, i);
            break;
        }
}

bool gemview_was_opened(widget *w)
{
    gemview *gv = (gemview *)w;
    bool     o  = gv->opened;
    gv->opened = false;
    return o;
}

/* --- Zeichnen ---------------------------------------------------------------------- */

/* Die Breite eines nicht nullterminierten Abschnitts. Über einen Puffer, weil
 * text_width eine Zeichenkette will - sehr lange Zeilen werden dabei
 * abgeschnitten, aber sie stehen ohnehin nicht ganz im Fenster. */
static int run_width(const char *text, size_t len)
{
    char buf[512];
    if (len >= sizeof buf) len = sizeof buf - 1;

    memcpy(buf, text, len);
    buf[len] = '\0';
    return text_width(&system12, buf);
}

static void draw_run(gc *g, int x, int y, const char *text, size_t len)
{
    char buf[512];
    while (len > 0) {
        size_t n = len < sizeof buf - 1 ? len : sizeof buf - 1;
        memcpy(buf, text, n);
        buf[n] = '\0';

        gfx_text(g, &system12, x, y, buf);
        x += text_width(&system12, buf);

        text += n;
        len  -= n;
    }
}

static void gemview_draw(const widget *w, gc *g)
{
    const gemview *gv = (const gemview *)w;
    ensure_wrap(gv);

    rect r = w->frame;
    g->pat  = PAT_WHITE;
    g->mode = GFX_COPY;
    gfx_fill_rect(g, r);
    g->pat = PAT_BLACK;
    gfx_frame_rect(g, r);

    rect c    = content_rect(w);
    int  rows = visible_lines(w);
    if (rows <= 0) return;

    for (int i = 0; i < rows; i++) {
        int idx = gv->sc.value + i;
        if (idx >= gv->line_count) break;

        const display_line *l = &gv->lines[idx];
        int y  = c.y + i * line_h(w);
        int ty = y + system12.ascent;
        int x  = c.x;

        /* Das Präfix trägt die Bedeutung: eine Nummer beim Verweis, ein Stern
         * bei der Aufzählung, ein Strich beim Zitat. Nur bei der ersten Zeile
         * eines umbrochenen Absatzes - danach wird eingerückt. */
        bool first = (idx == 0) || gv->lines[idx - 1].text != l->text ||
                     gv->lines[idx - 1].kind != l->kind;

        switch (l->kind) {
        case GEM_LINK: {
            char num[16];
            snprintf(num, sizeof num, "[%d] ", l->link);
            if (first) gfx_text(g, &system12, x, ty, num);
            x += text_width(&system12, "[88] ");
            break;
        }
        case GEM_ITEM:
            if (first) gfx_text(g, &system12, x, ty, T(w->cat, "gemtext.bullet"));
            x += item_indent(w);
            break;
        case GEM_QUOTE:
            gfx_vline(g, x + 2, y, line_h(w));
            x += text_width(&system12, "| ");
            break;
        default:
            break;
        }

        draw_run(g, x, ty, l->text, l->len);

        /* Überschriften bekommen einen Balken darunter, erste Ebene einen
         * dickeren. Es gibt nur einen Schnitt, also muss die Hervorhebung aus
         * der Geometrie kommen. */
        if (l->kind == GEM_HEADING && l->len > 0) {
            /* Der wirklich gezeichnete Text, nicht eine Schätzung über die
             * Breite eines M: der Balken soll unter der Überschrift enden und
             * nicht irgendwo dahinter. */
            int width = run_width(l->text, l->len);
            if (width > c.w) width = c.w;

            for (int t = 0; t < (l->level == 1 ? 2 : 1); t++)
                gfx_hline(g, c.x, y + line_h(w) - 1 - t, width);
        }

        if (l->link != 0 && l->link == gv->selected)
            gfx_invert_rect(g, rect_make(c.x, y, c.w, line_h(w)));
    }

    if (w->focused) {
        rect outer = rect_make(r.x - 2, r.y - 2, r.w + 4, r.h + 4);
        gfx_frame_rect(g, outer);
    }
}

static void gemview_measure(widget *w, int *pw, int *ph)
{
    if (pw) *pw = text_width(&system12, "M") * 40;
    if (ph) *ph = line_h(w) * 10;
}

/* --- Ereignisse ---------------------------------------------------------------------- */

static bool gemview_event(widget *w, const event *e)
{
    gemview *gv = (gemview *)w;
    ensure_wrap(gv);

    switch (e->kind) {
    case EV_MOUSE_DOWN: {
        if (!rect_contains(w->frame, e->x, e->y)) return false;

        rect c   = content_rect(w);
        int  row = (e->y - c.y) / line_h(w);
        int  idx = gv->sc.value + row;

        if (row >= 0 && idx < gv->line_count && gv->lines[idx].link) {
            gemview_select_link(w, gv->lines[idx].link);
            if (e->clicks >= 2) gv->opened = true;
        }
        return true;
    }

    case EV_WHEEL:
        if (!rect_contains(w->frame, e->x, e->y)) return false;
        scroll_by(&gv->sc, -e->wheel);
        return true;

    case EV_KEY_DOWN:
        if (!w->focused) return false;

        switch (e->key) {
        case KEY_UP:        scroll_by(&gv->sc, -1); return true;
        case KEY_DOWN:      scroll_by(&gv->sc, +1); return true;
        case KEY_PAGE_UP:   scroll_pages(&gv->sc, -1); return true;
        case KEY_PAGE_DOWN: scroll_pages(&gv->sc, +1); return true;
        case KEY_HOME:      scroll_to(&gv->sc, 0); return true;
        case KEY_END:       scroll_to(&gv->sc, gv->line_count); return true;

        case KEY_RETURN:
            if (gv->selected) gv->opened = true;
            return true;

        default:
            return false;
        }

    case EV_TEXT: {
        if (!w->focused) return false;

        /* Ziffern wählen einen Verweis - das ist der Grund, warum sie
         * angezeigt werden. Mehrstellige Nummern entstehen durch
         * Weitertippen: aus 1 wird 12, solange es einen Verweis 12 gibt. */
        char ch = e->text[0];
        if (ch < '0' || ch > '9' || e->text[1]) return false;

        int digit = ch - '0';
        int wide  = gv->selected * 10 + digit;

        if (wide >= 1 && wide <= gv->link_count) gemview_select_link(w, wide);
        else if (digit >= 1 && digit <= gv->link_count) gemview_select_link(w, digit);
        else gemview_select_link(w, 0);

        return true;
    }

    default:
        return false;
    }
}

static const widget_class gemview_class = {
    .name    = "gemview",
    .measure = gemview_measure,
    .draw    = gemview_draw,
    .event   = gemview_event,
};

widget *gemview_create(const theme *th, const catalog *cat)
{
    gemview *gv = calloc(1, sizeof *gv);
    if (!gv) return NULL;

    gv->base.cls         = &gemview_class;
    gv->base.th          = th;
    gv->base.cat         = cat;
    gv->base.enabled     = true;
    gv->base.wants_focus = true;

    return &gv->base;
}
