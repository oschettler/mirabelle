/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Textfelder, siehe widget.h für den Vertrag.
 *
 * Zwei Klassen, ein gemeinsamer Zustand: beide setzen auf textbuf auf und
 * teilen sich Zeichnen und Ereignisse fast vollständig, nur measure und ein
 * paar Weichen für KEY_UP/KEY_DOWN/KEY_RETURN unterscheiden sich - siehe
 * tw->multiline. Genau wie bei widget_list.c steht struct widget als erstes
 * Feld, und außer dem eigenen Speicher gehört der Klasse nur der textbuf.
 *
 * Zwei "ensure_visible"-Funktionen wie bei der Liste, eine je Achse:
 * text_field_scroll_to_cursor schiebt waagerecht, text_area_scroll_to_cursor
 * senkrecht. Beide werden über text_after_move()/text_after_change() an
 * jeder Stelle gerufen, an der sich die Schreibmarke bewegt oder der Text
 * sich ändert - genau wie list_ensure_visible() nicht an jeder Aufrufstelle
 * neu geschrieben wird.
 *
 * Der Umbruch im mehrzeiligen Feld ist ein Zwischenspeicher (tw->lines), der
 * verfällt, sobald sich Text oder Feldbreite ändern (tw->wrap_valid).
 * text_area_ensure_wrap() zieht ihn bei Bedarf nach - auch aus draw(), das
 * das Widget nur lesend bekommt. Der Wurf von const weg dort ist bewusst:
 * das Widget selbst liegt auf dem Haufen und ist nie wirklich const, nur der
 * Zeiger in der Vertragssignatur ist es.
 */
#include "ui/widget.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "core/utf8.h"
#include "ui/caret.h"
#include "gfx/draw.h"
#include "gfx/font.h"
#include "gfx/pattern.h"
#include "gfx/text.h"
#include "plat/plat.h"
#include "ui/theme.h"

extern const font system12;

/* Eine Anzeigezeile: Byteversatz und -länge im Text des textbuf. Reine
 * Anzeigesache, siehe widget.h - der Text selbst bleibt unverändert. */
typedef struct {
    size_t start;
    size_t len;
} display_line;

typedef struct {
    widget   base;
    textbuf *buf;
    bool     multiline;

    bool     dragging;   /* Maustaste hält die Auswahl beim Ziehen offen */

    int      scroll_x;   /* nur einzeilig: Versatz in Pixeln */

    /* nur mehrzeilig: die Anzeigezeilen und ihre Sichtbarkeit */
    display_line *lines;
    int            line_count;
    int            line_cap;
    scrollmodel    sc;          /* value ist die erste sichtbare Anzeigezeile */
    bool           wrap_valid;
    int            wrap_w;      /* Breite, für die zuletzt umgebrochen wurde */
} text_widget;

/* --- Geometrie -------------------------------------------------------------- */

static rect text_content_rect(const widget *w)
{
    int inset = w->th->border + w->th->menu_pad;
    return rect_make(w->frame.x + inset, w->frame.y + inset,
                     w->frame.w - 2 * inset, w->frame.h - 2 * inset);
}

static int text_area_visible_rows(const widget *w)
{
    rect content = text_content_rect(w);
    if (content.h <= 0 || system12.size <= 0) return 0;
    return content.h / system12.size;
}

/* --- Breiten und Trefferpunkte -----------------------------------------------
 *
 * Beide messen zeichenweise über utf8_next/font_find, statt eine Kopie des
 * Ausschnitts anzulegen, nur um sie an text_width() zu verfüttern.
 */

static int width_of_range(const font *f, const char *text, size_t from, size_t to)
{
    const char *p   = text + from;
    const char *end = text + to;
    int         w   = 0;

    while (p < end) {
        uint32_t     cp = utf8_next(&p);
        const glyph *gl = font_find(f, cp);
        if (gl) w += gl->width;
    }

    return w;
}

/* Byteversatz in [from,to), dessen Zeichen dem Ziel target (Pixel ab from)
 * am nächsten liegt - auf die Mitte jedes Zeichens gerundet. */
/* Der Codepunkt an Stelle pos, ohne den Zeiger zu verschieben. */
static uint32_t peek_cp(const char *text, size_t pos)
{
    const char *q = text + pos;
    return utf8_next(&q);
}

static bool is_space_cp(uint32_t cp)
{
    return cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r';
}

static size_t pos_at_width(const font *f, const char *text, size_t from, size_t to, int target)
{
    const char *p   = text + from;
    const char *end = text + to;
    int         sum = 0;

    while (p < end) {
        const char  *next = p;
        uint32_t     cp   = utf8_next(&next);
        const glyph *gl   = font_find(f, cp);
        int          cw   = gl ? gl->width : 0;

        if (sum + cw / 2 >= target) return (size_t)(p - text);

        sum += cw;
        p = next;
    }

    return to;
}

/* Zeichnet einen Byte-Ausschnitt des Texts. gfx_text() braucht eine
 * nullterminierte Zeichenkette, ein Ausschnitt mitten aus dem Puffer des
 * textbuf braucht deshalb eine eigene, kurzlebige Kopie. */
static void draw_text_span(gc *g, const font *f, int x, int y,
                           const char *text, size_t from, size_t len)
{
    char  stackbuf[256];
    char *buf  = stackbuf;
    bool  heap = false;

    if (len + 1 > sizeof stackbuf) {
        buf = malloc(len + 1);
        if (!buf) return;
        heap = true;
    }

    memcpy(buf, text + from, len);
    buf[len] = '\0';
    gfx_text(g, f, x, y, buf);

    if (heap) free(buf);
}

/* --- Umbruch im mehrzeiligen Feld --------------------------------------------
 *
 * Bricht eine ECHTE Zeile - im Sinne von textbuf, also durch \n begrenzt und
 * ohne das \n selbst - in eine oder mehrere Anzeigezeilen für wrap_w Pixel.
 * Gebrochen wird am letzten Leerzeichen, das noch hineinpasst; das
 * Leerzeichen selbst wird dabei nicht angezeigt. Passt nicht einmal ein
 * einziges Wort, wird hart getrennt, aber nur an einer Zeichengrenze, die
 * utf8_next liefert - das erste Zeichen eines Segments wird deshalb immer
 * angenommen, egal wie breit es ist, sonst käme man nie vom Fleck.
 */
static bool push_display_line(text_widget *tw, size_t start, size_t len)
{
    if (tw->line_count == tw->line_cap) {
        int            newcap = tw->line_cap ? tw->line_cap * 2 : 8;
        display_line *lines  = realloc(tw->lines, (size_t)newcap * sizeof *lines);
        if (!lines) return false;
        tw->lines    = lines;
        tw->line_cap = newcap;
    }

    tw->lines[tw->line_count].start = start;
    tw->lines[tw->line_count].len   = len;
    tw->line_count++;
    return true;
}

static void wrap_real_line(text_widget *tw, const char *text,
                           size_t line_start, size_t line_end, int wrap_w)
{
    if (line_start == line_end) {
        push_display_line(tw, line_start, 0);
        return;
    }

    size_t pos = line_start;
    while (pos < line_end) {
        size_t      seg_start   = pos;
        int         width_sum   = 0;
        bool        have_space  = false;
        size_t      after_space = 0;
        const char *p           = text + pos;

        while ((size_t)(p - text) < line_end) {
            const char  *next = p;
            uint32_t     cp   = utf8_next(&next);
            const glyph *gl   = font_find(&system12, cp);
            int          cw   = gl ? gl->width : 0;

            if (width_sum + cw > wrap_w && (size_t)(p - text) > seg_start) break;

            width_sum += cw;
            if (cp == ' ') { have_space = true; after_space = (size_t)(next - text); }
            p = next;
        }

        size_t seg_end = (size_t)(p - text);

        if (seg_end >= line_end) {
            push_display_line(tw, seg_start, seg_end - seg_start);
            pos = line_end;
        } else if (have_space) {
            push_display_line(tw, seg_start, (after_space - 1) - seg_start);
            pos = after_space;
        } else {
            push_display_line(tw, seg_start, seg_end - seg_start);
            pos = seg_end;
        }
    }
}

static void text_area_rebuild_lines(text_widget *tw, int wrap_w)
{
    tw->line_count = 0;   /* Kapazität bleibt, nur die Belegung wird verworfen */

    const char *text       = textbuf_text(tw->buf);
    int         real_lines = textbuf_line_count(tw->buf);

    for (int i = 0; i < real_lines; i++) {
        size_t start = textbuf_line_start(tw->buf, i);
        size_t end   = (i + 1 < real_lines) ? textbuf_line_start(tw->buf, i + 1) - 1
                                             : textbuf_len(tw->buf);
        wrap_real_line(tw, text, start, end, wrap_w);
    }

    if (tw->line_count == 0) push_display_line(tw, 0, 0);
}

/* Bringt das Bildlaufmodell auf den Stand von Umbruch und Rahmen. Wie
 * list_sync() in widget_list.c, und aus demselben Grund: die Höhe setzt das
 * Layout, und die Zahl der Anzeigezeilen folgt aus dem Umbruch. */
static void text_area_sync_scroll(text_widget *tw)
{
    scroll_set(&tw->sc, tw->line_count, text_area_visible_rows(&tw->base));
}

/* Zieht den Umbruch nach, wenn Text oder Breite sich seit dem letzten Mal
 * geändert haben. tw_const ist bewusst const - siehe Dateikopf. */
static void text_area_ensure_wrap(const text_widget *tw_const)
{
    text_widget *tw = (text_widget *)tw_const;

    rect content = text_content_rect(&tw->base);
    int  w       = content.w;
    if (w < 1) w = 1;

    /* Auch wenn der Umbruch noch stimmt, wird das Bildlaufmodell nachgezogen:
     * die Höhe kann sich geändert haben, ohne dass die Breite es tat, und dann
     * stimmt die Seitengröße nicht mehr. Damit ist diese Funktion die eine
     * Stelle, die das Modell aktuell hält - jede Aufrufstelle darf sich darauf
     * verlassen, statt es sicherheitshalber noch einmal zu tun. */
    if (!tw->wrap_valid || tw->wrap_w != w) {
        text_area_rebuild_lines(tw, w);
        tw->wrap_valid = true;
        tw->wrap_w     = w;
    }

    text_area_sync_scroll(tw);
}


/* Anzeigezeile, die pos enthält. Setzt einen frischen Umbruch voraus - jede
 * Aufrufstelle ruft vorher text_area_ensure_wrap(). */
static int text_area_line_at(const text_widget *tw, size_t pos)
{
    for (int i = tw->line_count - 1; i >= 0; i--)
        if (tw->lines[i].start <= pos) return i;
    return 0;
}

/* --- Sichtbarkeit nach einer Bewegung der Schreibmarke ----------------------- */

static void text_field_scroll_to_cursor(text_widget *tw)
{
    rect content = text_content_rect(&tw->base);
    if (content.w <= 0) { tw->scroll_x = 0; return; }

    const char *text     = textbuf_text(tw->buf);
    int         cursor_x = width_of_range(&system12, text, 0, textbuf_cursor(tw->buf));

    if (cursor_x < tw->scroll_x) tw->scroll_x = cursor_x;
    if (cursor_x > tw->scroll_x + content.w) tw->scroll_x = cursor_x - content.w;
    if (tw->scroll_x < 0) tw->scroll_x = 0;
}

static void text_area_scroll_to_cursor(text_widget *tw)
{
    text_area_ensure_wrap(tw);
    scroll_reveal(&tw->sc, text_area_line_at(tw, textbuf_cursor(tw->buf)));
}

/* Nach jeder Bewegung der Schreibmarke gerufen, ob durch Tastatur, Maus oder
 * von außen über text_widget_set_value() - genau wie list_ensure_visible(). */
static void text_after_move(text_widget *tw)
{
    /* Wer gerade tippt, will sehen, wo er ist: der Takt fängt von vorn an,
     * sichtbar. Sonst landet ein Anschlag in einer dunklen Phase und die
     * Schreibmarke scheint zu fehlen. */
    caret_wake();

    if (tw->multiline) text_area_scroll_to_cursor(tw);
    else                text_field_scroll_to_cursor(tw);
}

/* Wie text_after_move(), aber der Text selbst hat sich auch geändert - der
 * Umbruch im mehrzeiligen Feld muss deshalb neu berechnet werden. */
static void text_after_change(text_widget *tw)
{
    if (tw->multiline) tw->wrap_valid = false;
    text_after_move(tw);
}

/* --- Trefferpunkte für die Maus ----------------------------------------------- */

static size_t text_field_pos_at(const text_widget *tw, int x)
{
    rect        content = text_content_rect(&tw->base);
    const char *text    = textbuf_text(tw->buf);
    int         target  = x - content.x + tw->scroll_x;
    if (target < 0) target = 0;

    return pos_at_width(&system12, text, 0, textbuf_len(tw->buf), target);
}

static size_t text_area_pos_at(const text_widget *tw, int x, int y)
{
    text_area_ensure_wrap(tw);
    if (tw->line_count == 0) return 0;

    rect content = text_content_rect(&tw->base);
    int  line_h  = system12.size;
    int  row     = line_h > 0 ? (y - content.y) / line_h : 0;
    int  line    = tw->sc.value + row;
    if (line < 0) line = 0;
    if (line >= tw->line_count) line = tw->line_count - 1;

    const display_line *dl     = &tw->lines[line];
    int                  target = x - content.x;
    if (target < 0) target = 0;

    const char *text = textbuf_text(tw->buf);
    return pos_at_width(&system12, text, dl->start, dl->start + dl->len, target);
}

static size_t text_pos_at(const text_widget *tw, int x, int y)
{
    return tw->multiline ? text_area_pos_at(tw, x, y) : text_field_pos_at(tw, x);
}

/* --- Wunschgröße -------------------------------------------------------------- */

static void text_field_measure(widget *w, int *pw, int *ph)
{
    if (pw) *pw = 120;
    if (ph) *ph = system12.size + 2 * w->th->menu_pad + 2 * w->th->border;
}

static void text_area_measure(widget *w, int *pw, int *ph)
{
    if (pw) *pw = 200;
    if (ph) *ph = 6 * system12.size + 2 * w->th->menu_pad + 2 * w->th->border;
}

/* --- Zeichnen ------------------------------------------------------------------ */

static void text_field_draw_content(const text_widget *tw, gc *g, rect content)
{
    const textbuf *buf  = tw->buf;
    const char    *text = textbuf_text(buf);
    int             ty  = content.y + (content.h - system12.size) / 2 + system12.ascent;

    g->pat  = PAT_BLACK;
    g->mode = GFX_COPY;
    gfx_text(g, &system12, content.x - tw->scroll_x, ty, text);

    if (textbuf_has_selection(buf)) {
        size_t from, to;
        textbuf_selection(buf, &from, &to);
        int x0 = content.x - tw->scroll_x + width_of_range(&system12, text, 0, from);
        int x1 = content.x - tw->scroll_x + width_of_range(&system12, text, 0, to);
        gfx_invert_rect(g, rect_make(x0, content.y, x1 - x0, content.h));
    } else if (tw->base.focused && caret_on()) {
        int cx = content.x - tw->scroll_x + width_of_range(&system12, text, 0, textbuf_cursor(buf));
        int cy = content.y + (content.h - system12.size) / 2;
        g->mode = GFX_XOR;
        g->pat  = PAT_BLACK;
        gfx_vline(g, cx, cy, system12.size);
        g->mode = GFX_COPY;
    }
}

static void text_area_draw_content(const text_widget *tw, gc *g, rect content)
{
    text_area_ensure_wrap(tw);

    const char *text   = textbuf_text(tw->buf);
    int         line_h = system12.size > 0 ? system12.size : 1;
    int         rows   = content.h / line_h;

    bool   has_sel  = textbuf_has_selection(tw->buf);
    size_t sel_from = 0, sel_to = 0;
    if (has_sel) textbuf_selection(tw->buf, &sel_from, &sel_to);

    g->pat  = PAT_BLACK;
    g->mode = GFX_COPY;

    for (int i = 0; i < rows; i++) {
        int line = tw->sc.value + i;
        if (line >= tw->line_count) break;

        const display_line *dl    = &tw->lines[line];
        int                  row_y = content.y + i * line_h;

        draw_text_span(g, &system12, content.x, row_y + system12.ascent, text, dl->start, dl->len);

        if (has_sel) {
            size_t line_end = dl->start + dl->len;
            size_t s = sel_from > dl->start ? sel_from : dl->start;
            size_t e = sel_to   < line_end  ? sel_to   : line_end;
            if (s < e) {
                int x0 = content.x + width_of_range(&system12, text, dl->start, s);
                int x1 = content.x + width_of_range(&system12, text, dl->start, e);
                gfx_invert_rect(g, rect_make(x0, row_y, x1 - x0, line_h));
            }
        }
    }

    if (!has_sel && tw->base.focused && caret_on()) {
        int cursor_line = text_area_line_at(tw, textbuf_cursor(tw->buf));
        int row          = cursor_line - tw->sc.value;
        if (row >= 0 && row < rows) {
            const display_line *dl = &tw->lines[cursor_line];
            int cx = content.x + width_of_range(&system12, text, dl->start, textbuf_cursor(tw->buf));
            int cy = content.y + row * line_h;
            g->mode = GFX_XOR;
            g->pat  = PAT_BLACK;
            gfx_vline(g, cx, cy, system12.size);
            g->mode = GFX_COPY;
        }
    }
}

static void text_draw(const widget *w, gc *g)
{
    const text_widget *tw = (const text_widget *)w;
    rect                r  = w->frame;

    g->pat  = PAT_WHITE;
    g->mode = GFX_COPY;
    gfx_fill_rect(g, r);

    g->pat = PAT_BLACK;
    for (int i = 0; i < w->th->border; i++)
        gfx_frame_rect(g, rect_make(r.x + i, r.y + i, r.w - 2 * i, r.h - 2 * i));

    if (w->focused) {
        rect outer = rect_make(r.x - 2, r.y - 2, r.w + 4, r.h + 4);
        gfx_frame_rect(g, outer);
    }

    rect content = text_content_rect(w);
    if (!rect_empty(content)) {
        rect saved_clip = g->clip;
        gc_clip_intersect(g, content);

        if (tw->multiline) text_area_draw_content(tw, g, content);
        else                text_field_draw_content(tw, g, content);

        g->clip = saved_clip;
    }

    if (!w->enabled) {
        g->pat  = PAT_GRAY50;
        g->mode = GFX_CLEAR;
        gfx_fill_rect(g, r);
        g->mode = GFX_COPY;
    }
}

/* --- Ereignisse ------------------------------------------------------------------ */

/* Wählt das Wort an pos aus.
 *
 * Bewusst NICHT über MOVE_WORD_LEFT und MOVE_WORD_RIGHT: die überspringen für
 * die Cursorbewegung zuerst den Leerraum, und genau das ist hier falsch. Steht
 * die Marke am Anfang von "world", rutscht MOVE_WORD_LEFT über das Leerzeichen
 * hinweg bis zum Anfang von "hello" - ein Doppelklick auf ein Wort wählte dann
 * das davor.
 *
 * Hier wird stattdessen von pos aus nach beiden Seiten ausgedehnt, solange das
 * Nachbarzeichen zur selben Sorte gehört: Wort oder Leerraum. */
static void select_word_at(text_widget *tw, size_t pos)
{
    const char *text = textbuf_text(tw->buf);
    size_t      len  = textbuf_len(tw->buf);

    if (len == 0) {
        textbuf_set_cursor(tw->buf, 0, false);
        return;
    }
    if (pos > len) pos = len;

    /* Am Textende auf das letzte Zeichen zurückgehen, sonst gäbe es nichts,
     * woran sich die Sorte ablesen ließe. */
    size_t probe = pos;
    if (probe == len) {
        const char *q = text + probe;
        utf8_prev(text, &q);
        probe = (size_t)(q - text);
    }

    bool want_space = is_space_cp(peek_cp(text, probe));

    size_t from = probe;
    while (from > 0) {
        const char *q = text + from;
        utf8_prev(text, &q);
        size_t prev = (size_t)(q - text);
        if (is_space_cp(peek_cp(text, prev)) != want_space) break;
        from = prev;
    }

    size_t to = probe;
    while (to < len) {
        if (is_space_cp(peek_cp(text, to)) != want_space) break;
        const char *q = text + to;
        utf8_next(&q);
        to = (size_t)(q - text);
    }

    textbuf_set_cursor(tw->buf, from, false);
    textbuf_set_cursor(tw->buf, to, true);
}

static bool text_event(widget *w, const event *e)
{
    text_widget *tw    = (text_widget *)w;
    bool         shift = (e->mods & MOD_SHIFT) != 0;
    bool         cmd   = (e->mods & MOD_CMD) != 0;

    switch (e->kind) {
    case EV_MOUSE_DOWN: {
        if (!rect_contains(w->frame, e->x, e->y)) return false;

        size_t pos = text_pos_at(tw, e->x, e->y);

        if (e->clicks >= 2) {
            select_word_at(tw, pos);
        } else {
            textbuf_set_cursor(tw->buf, pos, false);
        }

        tw->dragging = true;
        text_after_move(tw);
        return true;
    }

    case EV_MOUSE_MOVE:
        if (!tw->dragging) return false;
        textbuf_set_cursor(tw->buf, text_pos_at(tw, e->x, e->y), true);
        text_after_move(tw);
        return true;

    case EV_MOUSE_UP: {
        bool was = tw->dragging;
        tw->dragging = false;
        return was;
    }

    case EV_WHEEL:
        /* Nur das mehrzeilige Feld. Das einzeilige scrollt waagerecht und
         * folgt dabei der Schreibmarke - dort hätte das Rad nichts zu
         * verschieben. */
        if (!tw->multiline) return false;
        if (!rect_contains(w->frame, e->x, e->y)) return false;

        text_area_ensure_wrap(tw);
        scroll_by(&tw->sc, -e->wheel);
        return true;

    case EV_TEXT:
        if (!w->focused) return false;
        textbuf_insert(tw->buf, e->text);
        text_after_change(tw);
        return true;

    case EV_KEY_DOWN:
        if (!w->focused) return false;

        switch (e->key) {
        case KEY_BACKSPACE:
            textbuf_delete_back(tw->buf);
            text_after_change(tw);
            return true;

        case KEY_DELETE:
            textbuf_delete_forward(tw->buf);
            text_after_change(tw);
            return true;

        case KEY_LEFT:
            textbuf_move_cursor(tw->buf, cmd ? MOVE_WORD_LEFT : MOVE_LEFT, shift);
            text_after_move(tw);
            return true;

        case KEY_RIGHT:
            textbuf_move_cursor(tw->buf, cmd ? MOVE_WORD_RIGHT : MOVE_RIGHT, shift);
            text_after_move(tw);
            return true;

        case KEY_HOME:
            textbuf_move_cursor(tw->buf, MOVE_LINE_START, shift);
            text_after_move(tw);
            return true;

        case KEY_END:
            textbuf_move_cursor(tw->buf, MOVE_LINE_END, shift);
            text_after_move(tw);
            return true;

        case KEY_UP:
            if (!tw->multiline) return false;
            textbuf_move_cursor(tw->buf, MOVE_UP, shift);
            text_after_move(tw);
            return true;

        case KEY_DOWN:
            if (!tw->multiline) return false;
            textbuf_move_cursor(tw->buf, MOVE_DOWN, shift);
            text_after_move(tw);
            return true;

        case KEY_RETURN:
            if (!tw->multiline) return false;
            textbuf_insert(tw->buf, "\n");
            text_after_change(tw);
            return true;

        case 'a':
            if (!cmd) return false;
            textbuf_select_all(tw->buf);
            text_after_move(tw);
            return true;

        case 'z':
            if (!cmd) return false;
            if (shift) textbuf_redo(tw->buf);
            else        textbuf_undo(tw->buf);
            text_after_change(tw);
            return true;

        default:
            return false;
        }

    default:
        return false;
    }
}

/* --- Verwaltung -------------------------------------------------------------- */

static void text_destroy(widget *w)
{
    text_widget *tw = (text_widget *)w;
    textbuf_destroy(tw->buf);
    free(tw->lines);
}

static const widget_class text_field_class = {
    .name    = "text_field",
    .measure = text_field_measure,
    .draw    = text_draw,
    .event   = text_event,
    .destroy = text_destroy,
};

static const widget_class text_area_class = {
    .name    = "text_area",
    .measure = text_area_measure,
    .draw    = text_draw,
    .event   = text_event,
    .destroy = text_destroy,
};

static widget *text_create(const theme *th, const catalog *cat,
                           const widget_class *cls, bool multiline)
{
    text_widget *tw = calloc(1, sizeof *tw);
    if (!tw) return NULL;

    tw->buf = textbuf_create();
    if (!tw->buf) { free(tw); return NULL; }

    tw->base.cls         = cls;
    tw->base.th          = th;
    tw->base.cat         = cat;
    tw->base.enabled     = true;
    tw->base.wants_focus = true;
    tw->multiline        = multiline;

    return &tw->base;
}

widget *text_field_create(const theme *th, const catalog *cat)
{
    return text_create(th, cat, &text_field_class, false);
}

widget *text_area_create(const theme *th, const catalog *cat)
{
    return text_create(th, cat, &text_area_class, true);
}

const char *text_widget_value(const widget *w)
{
    const text_widget *tw = (const text_widget *)w;
    return textbuf_text(tw->buf);
}

bool text_widget_set_value(widget *w, const char *utf8)
{
    text_widget *tw = (text_widget *)w;
    bool          ok = textbuf_set(tw->buf, utf8);
    text_after_change(tw);
    return ok;
}

textbuf *text_widget_buf(widget *w)
{
    return ((text_widget *)w)->buf;
}

scrollmodel *text_widget_scroll(widget *w)
{
    text_widget *tw = (text_widget *)w;
    if (!tw->multiline) return NULL;

    text_area_ensure_wrap(tw);
    return &tw->sc;
}

int text_widget_top_line(const widget *w)
{
    const text_widget *tw = (const text_widget *)w;
    return tw->multiline ? tw->sc.value : 0;
}
