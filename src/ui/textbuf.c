/* Siehe textbuf.h für den Vertrag.
 *
 * Der Puffer ist ein einziges char-Feld, das per realloc wächst und per
 * memmove verschoben wird - kein Lückenpuffer. Jede Bewegung um ein Zeichen
 * geht über utf8_next/utf8_prev, damit die Schreibmarke nie mitten in einem
 * Mehrbytezeichen landet.
 *
 * Ein Widerruf-Schritt merkt sich, was an welcher Stelle eingefügt oder
 * gelöscht wurde, dazu Schreibmarke und Anker davor. Aufeinanderfolgende
 * Schritte werden zusammengefasst, solange sie unmittelbar aneinander
 * anschließen - das erspart, jeden getippten Buchstaben einzeln zurücknehmen
 * zu müssen.
 */
#include "ui/textbuf.h"

#include "core/utf8.h"

#include <stdlib.h>
#include <string.h>

/* --- Widerruf-Schritte ----------------------------------------------------- */

typedef struct {
    bool   is_insert;   /* sonst eine Löschung */
    size_t pos;
    char  *text;        /* eingefügter bzw. gelöschter Text, malloc'iert */
    size_t len;
    size_t cursor_before;
    size_t anchor_before;

    /* Dieser Schritt gehört mit dem darunterliegenden zu EINER Handlung und
     * wird zusammen mit ihm zurückgenommen. Gebraucht wird das beim Ersetzen
     * einer Auswahl: das ist für den Nutzer ein Vorgang, intern aber ein
     * Löschen und ein Einfügen. Ohne die Marke bräuchte es zwei Widerrufe,
     * und dazwischen stünde ein Text, den nie jemand gesehen hat. */
    bool joined;
} undo_step;

typedef struct {
    undo_step *items;
    size_t     count;
    size_t     cap;
} step_stack;

struct textbuf {
    char  *data;
    size_t len;    /* Bytes ohne Nullterminierung */
    size_t cap;

    size_t cursor;
    size_t anchor;

    int want_col;   /* Wunschspalte in Codepunkten für MOVE_UP/DOWN, -1 = keine */

    step_stack undo_stack;
    step_stack redo_stack;
    bool       undo_break;   /* nächste Änderung fängt einen neuen Schritt an */
};

static void stack_free_all(step_stack *s)
{
    for (size_t i = 0; i < s->count; i++) free(s->items[i].text);
    s->count = 0;
}

static void stack_destroy(step_stack *s)
{
    stack_free_all(s);
    free(s->items);
    s->items = NULL;
    s->cap   = 0;
}

static bool stack_push(step_stack *s, undo_step step)
{
    if (s->count == s->cap) {
        size_t     newcap = s->cap ? s->cap * 2 : 8;
        undo_step *items  = realloc(s->items, newcap * sizeof *items);
        if (!items) return false;
        s->items = items;
        s->cap   = newcap;
    }

    s->items[s->count++] = step;
    return true;
}

/* --- Der Puffer selbst ------------------------------------------------------ */

static bool ensure_cap(textbuf *tb, size_t need)
{
    if (tb->cap >= need) return true;

    size_t newcap = tb->cap ? tb->cap : 64;
    while (newcap < need) newcap *= 2;

    char *p = realloc(tb->data, newcap);
    if (!p) return false;

    tb->data = p;
    tb->cap  = newcap;
    return true;
}

/* Entfernt [from,to) aus dem Puffer. */
static void buf_erase(textbuf *tb, size_t from, size_t to)
{
    memmove(tb->data + from, tb->data + to, tb->len - to);
    tb->len -= (to - from);
    tb->data[tb->len] = '\0';
}

/* Fügt text (tlen Bytes) an Stelle at ein. */
static bool buf_splice(textbuf *tb, size_t at, const char *text, size_t tlen)
{
    if (!ensure_cap(tb, tb->len + tlen + 1)) return false;

    memmove(tb->data + at + tlen, tb->data + at, tb->len - at);
    memcpy(tb->data + at, text, tlen);
    tb->len += tlen;
    tb->data[tb->len] = '\0';
    return true;
}

static void reset_want_col(textbuf *tb)
{
    tb->want_col = -1;
}

/* --- Widerruf-Buchführung ---------------------------------------------------
 *
 * record_insert/record_delete werden aufgerufen, NACHDEM der Aufrufer weiß,
 * was passiert ist, aber bei record_delete BEVOR der Text aus dem Puffer
 * verschwindet - text zeigt in diesem Fall noch auf die Originalstelle im
 * Puffer und wird hier hinein kopiert.
 */

static bool make_step_text(undo_step *step, const char *text, size_t len)
{
    char *copy = malloc(len);
    if (!copy) return false;
    memcpy(copy, text, len);
    step->text = copy;
    step->len  = len;
    return true;
}

static void record_insert(textbuf *tb, size_t pos, const char *text, size_t len,
                           size_t cur_before, size_t anc_before, bool joined)
{
    if (len == 0) return;

    stack_free_all(&tb->redo_stack);

    if (!joined && !tb->undo_break && tb->undo_stack.count > 0) {
        undo_step *top = &tb->undo_stack.items[tb->undo_stack.count - 1];
        if (top->is_insert && top->pos + top->len == pos &&
            memchr(text, '\n', len) == NULL &&
            memchr(top->text, '\n', top->len) == NULL) {
            char *grown = realloc(top->text, top->len + len);
            if (grown) {
                memcpy(grown + top->len, text, len);
                top->text = grown;
                top->len += len;
                tb->undo_break = false;
                return;
            }
            /* realloc gescheitert: als eigenständigen Schritt weitermachen */
        }
    }

    undo_step step = {0};
    step.is_insert     = true;
    step.pos           = pos;
    step.cursor_before = cur_before;
    step.anchor_before = anc_before;
    step.joined        = joined;
    if (make_step_text(&step, text, len)) {
        if (!stack_push(&tb->undo_stack, step)) free(step.text);
    }

    tb->undo_break = false;
}

static void record_delete(textbuf *tb, size_t pos, const char *text, size_t len,
                           size_t cur_before, size_t anc_before)
{
    if (len == 0) return;

    stack_free_all(&tb->redo_stack);

    if (!tb->undo_break && tb->undo_stack.count > 0) {
        undo_step *top = &tb->undo_stack.items[tb->undo_stack.count - 1];
        if (!top->is_insert) {
            if (top->pos == pos) {
                /* Entf mehrfach an derselben Stelle: hinten anfügen */
                char *grown = realloc(top->text, top->len + len);
                if (grown) {
                    memcpy(grown + top->len, text, len);
                    top->text = grown;
                    top->len += len;
                    tb->undo_break = false;
                    return;
                }
            } else if (pos + len == top->pos) {
                /* Rücktaste mehrfach: die neue Löschung endet, wo die vorige
                 * begann - vorn anfügen */
                char *grown = malloc(top->len + len);
                if (grown) {
                    memcpy(grown, text, len);
                    memcpy(grown + len, top->text, top->len);
                    free(top->text);
                    top->text = grown;
                    top->len += len;
                    top->pos  = pos;
                    tb->undo_break = false;
                    return;
                }
            }
        }
    }

    undo_step step = {0};
    step.is_insert     = false;
    step.pos           = pos;
    step.cursor_before = cur_before;
    step.anchor_before = anc_before;
    if (make_step_text(&step, text, len)) {
        if (!stack_push(&tb->undo_stack, step)) free(step.text);
    }

    tb->undo_break = false;
}

/* Löscht [from,to), merkt sich den Schritt, zieht Schreibmarke und Anker auf
 * from zusammen. Für Auswahl-Löschungen wie für Rücktaste/Entf gleich. */
static void erase_range_recorded(textbuf *tb, size_t from, size_t to)
{
    size_t cur_before = tb->cursor;
    size_t anc_before  = tb->anchor;

    record_delete(tb, from, tb->data + from, to - from, cur_before, anc_before);
    buf_erase(tb, from, to);
    tb->cursor = tb->anchor = from;
}

/* --- Wörter und Leerraum ----------------------------------------------------- */

static bool is_space(uint32_t cp)
{
    return cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r' ||
           cp == '\v' || cp == '\f';
}

static size_t word_left(const textbuf *tb, size_t pos)
{
    const char *start = tb->data;
    const char *p = start + pos;

    while (p > start) {
        const char *q = p;
        if (!is_space(utf8_prev(start, &q))) break;
        p = q;
    }
    while (p > start) {
        const char *q = p;
        if (is_space(utf8_prev(start, &q))) break;
        p = q;
    }

    return (size_t)(p - start);
}

static size_t word_right(const textbuf *tb, size_t pos)
{
    const char *end = tb->data + tb->len;
    const char *p   = tb->data + pos;

    while (p < end) {
        const char *q = p;
        if (is_space(utf8_next(&q))) break;
        p = q;
    }
    while (p < end) {
        const char *q = p;
        if (!is_space(utf8_next(&q))) break;
        p = q;
    }

    return (size_t)(p - tb->data);
}

static size_t line_end(const textbuf *tb, size_t pos)
{
    const char *nl = memchr(tb->data + pos, '\n', tb->len - pos);
    return nl ? (size_t)(nl - tb->data) : tb->len;
}

/* --- Spalten für MOVE_UP/MOVE_DOWN ------------------------------------------- */

/* Spalte in Codepunkten von line_start bis pos. */
static int pos_to_col(const textbuf *tb, size_t line_start, size_t pos)
{
    const char *p      = tb->data + line_start;
    const char *target = tb->data + pos;
    int col = 0;

    while (p < target) {
        utf8_next(&p);
        col++;
    }

    return col;
}

/* Position col Codepunkte hinter line_start, höchstens bis zum Zeilenende. */
static size_t col_to_pos(const textbuf *tb, size_t line_start, int col)
{
    const char *p   = tb->data + line_start;
    const char *end = tb->data + tb->len;

    for (int i = 0; i < col; i++) {
        if (p >= end || *p == '\n') break;
        utf8_next(&p);
    }

    return (size_t)(p - tb->data);
}

static size_t move_vertical(textbuf *tb, textbuf_move how, size_t pos)
{
    int cur_line   = textbuf_line_at(tb, pos);
    int line_count = textbuf_line_count(tb);

    if (how == MOVE_UP && cur_line == 0) return pos;
    if (how == MOVE_DOWN && cur_line == line_count - 1) return pos;

    size_t cur_line_start = textbuf_line_start(tb, cur_line);

    /* Die Wunschspalte nur setzen, wenn noch keine gemerkt ist - sonst würde
     * man sie beim Wandern über eine kurze Zeile bei jedem Schritt verlieren. */
    if (tb->want_col < 0)
        tb->want_col = pos_to_col(tb, cur_line_start, pos);

    int    target_line  = cur_line + (how == MOVE_UP ? -1 : 1);
    size_t target_start = textbuf_line_start(tb, target_line);

    return col_to_pos(tb, target_start, tb->want_col);
}

/* --- Inhalt ------------------------------------------------------------------ */

textbuf *textbuf_create(void)
{
    textbuf *tb = calloc(1, sizeof *tb);
    if (!tb) return NULL;

    tb->data = malloc(64);
    if (!tb->data) {
        free(tb);
        return NULL;
    }
    tb->data[0] = '\0';
    tb->cap     = 64;
    tb->want_col = -1;

    return tb;
}

void textbuf_destroy(textbuf *tb)
{
    if (!tb) return;
    stack_destroy(&tb->undo_stack);
    stack_destroy(&tb->redo_stack);
    free(tb->data);
    free(tb);
}

const char *textbuf_text(const textbuf *tb)
{
    return tb->data;
}

size_t textbuf_len(const textbuf *tb)
{
    return tb->len;
}

bool textbuf_set(textbuf *tb, const char *utf8)
{
    if (!utf8) return false;

    size_t n = strlen(utf8);
    if (!ensure_cap(tb, n + 1)) return false;

    memcpy(tb->data, utf8, n);
    tb->len = n;
    tb->data[n] = '\0';

    tb->cursor = tb->anchor = n;
    tb->want_col = -1;

    stack_free_all(&tb->undo_stack);
    stack_free_all(&tb->redo_stack);
    tb->undo_break = false;

    return true;
}

/* --- Schreibmarke und Auswahl ------------------------------------------------ */

size_t textbuf_cursor(const textbuf *tb)
{
    return tb->cursor;
}

size_t textbuf_anchor(const textbuf *tb)
{
    return tb->anchor;
}

bool textbuf_has_selection(const textbuf *tb)
{
    return tb->cursor != tb->anchor;
}

void textbuf_selection(const textbuf *tb, size_t *from, size_t *to)
{
    size_t a = tb->anchor, c = tb->cursor;
    if (from) *from = a < c ? a : c;
    if (to)   *to   = a < c ? c : a;
}

void textbuf_set_cursor(textbuf *tb, size_t pos, bool extend)
{
    if (pos > tb->len) pos = tb->len;

    /* Auf den Anfang des Zeichens zurückziehen, falls pos mittendrin liegt. */
    while (pos > 0 && ((unsigned char)tb->data[pos] & 0xC0) == 0x80) pos--;

    tb->cursor = pos;
    if (!extend) tb->anchor = pos;

    reset_want_col(tb);
}

void textbuf_select_all(textbuf *tb)
{
    tb->anchor = 0;
    tb->cursor = tb->len;
    reset_want_col(tb);
}

/* --- Bewegen ------------------------------------------------------------------ */

void textbuf_move_cursor(textbuf *tb, textbuf_move how, bool extend)
{
    size_t pos = tb->cursor;

    switch (how) {
    case MOVE_LEFT: {
        const char *p = tb->data + pos;
        utf8_prev(tb->data, &p);
        pos = (size_t)(p - tb->data);
        break;
    }
    case MOVE_RIGHT: {
        const char *p = tb->data + pos;
        utf8_next(&p);
        pos = (size_t)(p - tb->data);
        break;
    }
    case MOVE_WORD_LEFT:
        pos = word_left(tb, pos);
        break;
    case MOVE_WORD_RIGHT:
        pos = word_right(tb, pos);
        break;
    case MOVE_LINE_START:
        pos = textbuf_line_start(tb, textbuf_line_at(tb, pos));
        break;
    case MOVE_LINE_END:
        pos = line_end(tb, pos);
        break;
    case MOVE_UP:
    case MOVE_DOWN:
        pos = move_vertical(tb, how, pos);
        break;
    case MOVE_TEXT_START:
        pos = 0;
        break;
    case MOVE_TEXT_END:
        pos = tb->len;
        break;
    }

    tb->cursor = pos;
    if (!extend) tb->anchor = pos;

    /* Jede andere Bewegung als hoch/runter setzt die Wunschspalte neu -
     * sonst würde man beim Wandern nach links wieder in eine alte Spalte
     * zurückspringen. */
    if (how != MOVE_UP && how != MOVE_DOWN) reset_want_col(tb);
}

/* --- Ändern -------------------------------------------------------------------- */

bool textbuf_insert(textbuf *tb, const char *utf8)
{
    if (!utf8) return false;

    size_t ilen = strlen(utf8);

    size_t from, to;
    textbuf_selection(tb, &from, &to);
    bool replaced = (from != to);
    if (replaced) erase_range_recorded(tb, from, to);

    if (ilen == 0) {
        reset_want_col(tb);
        return true;
    }

    size_t cur_before = tb->cursor;
    size_t anc_before  = tb->anchor;

    if (!buf_splice(tb, tb->cursor, utf8, ilen)) return false;

    record_insert(tb, tb->cursor, utf8, ilen, cur_before, anc_before, replaced);

    tb->cursor += ilen;
    tb->anchor  = tb->cursor;

    reset_want_col(tb);
    return true;
}

bool textbuf_delete_back(textbuf *tb)
{
    size_t from, to;
    textbuf_selection(tb, &from, &to);
    if (from != to) {
        erase_range_recorded(tb, from, to);
        reset_want_col(tb);
        return true;
    }

    if (tb->cursor == 0) return false;

    const char *p = tb->data + tb->cursor;
    utf8_prev(tb->data, &p);
    size_t start = (size_t)(p - tb->data);

    erase_range_recorded(tb, start, tb->cursor);
    reset_want_col(tb);
    return true;
}

bool textbuf_delete_forward(textbuf *tb)
{
    size_t from, to;
    textbuf_selection(tb, &from, &to);
    if (from != to) {
        erase_range_recorded(tb, from, to);
        reset_want_col(tb);
        return true;
    }

    if (tb->cursor >= tb->len) return false;

    const char *p = tb->data + tb->cursor;
    utf8_next(&p);
    size_t end = (size_t)(p - tb->data);

    erase_range_recorded(tb, tb->cursor, end);
    reset_want_col(tb);
    return true;
}

/* --- Widerrufen ----------------------------------------------------------------- */

/* Nimmt genau einen Schritt zurück. Ob noch einer dazugehört, entscheidet der
 * Aufrufer anhand von step.joined. */
static bool undo_one(textbuf *tb, bool *was_joined)
{
    if (tb->undo_stack.count == 0) return false;

    undo_step step = tb->undo_stack.items[tb->undo_stack.count - 1];

    /* Bei der Rücknahme einer Löschung muss der Text zurück in den Puffer -
     * das kann an Speicher scheitern. Dann lieber gar nichts tun, als einen
     * halb zurückgenommenen Schritt zu hinterlassen. */
    if (!step.is_insert) {
        if (!buf_splice(tb, step.pos, step.text, step.len)) return false;
    }

    tb->undo_stack.count--;

    if (step.is_insert) buf_erase(tb, step.pos, step.pos + step.len);

    tb->cursor = step.cursor_before;
    tb->anchor = step.anchor_before;

    if (was_joined) *was_joined = step.joined;
    if (!stack_push(&tb->redo_stack, step)) free(step.text);

    tb->undo_break = true;
    reset_want_col(tb);
    return true;
}

bool textbuf_undo(textbuf *tb)
{
    bool joined = false;
    if (!undo_one(tb, &joined)) return false;

    /* Zusammengehörige Schritte in einem Rutsch. Die Grenze schützt vor einer
     * beschädigten Kette. */
    int guard = 0;
    while (joined && guard++ < 64) {
        if (!undo_one(tb, &joined)) break;
    }

    return true;
}

static bool redo_one(textbuf *tb)
{
    if (tb->redo_stack.count == 0) return false;

    undo_step step = tb->redo_stack.items[tb->redo_stack.count - 1];

    if (step.is_insert) {
        if (!buf_splice(tb, step.pos, step.text, step.len)) return false;
    } else {
        buf_erase(tb, step.pos, step.pos + step.len);
    }

    tb->redo_stack.count--;

    tb->cursor = tb->anchor = step.is_insert ? step.pos + step.len : step.pos;

    if (!stack_push(&tb->undo_stack, step)) free(step.text);

    tb->undo_break = true;
    reset_want_col(tb);
    return true;
}

bool textbuf_redo(textbuf *tb)
{
    if (!redo_one(tb)) return false;

    /* Beim Wiederholen liegt die Marke auf dem Schritt, der als NÄCHSTER
     * dazugehört - er ist nach dem Abheben oben. */
    int guard = 0;
    while (tb->redo_stack.count > 0 && guard++ < 64 &&
           tb->redo_stack.items[tb->redo_stack.count - 1].joined) {
        if (!redo_one(tb)) break;
    }

    return true;
}

bool textbuf_can_undo(const textbuf *tb)
{
    return tb->undo_stack.count > 0;
}

bool textbuf_can_redo(const textbuf *tb)
{
    return tb->redo_stack.count > 0;
}

void textbuf_break_undo(textbuf *tb)
{
    tb->undo_break = true;
}

/* --- Zeilen ---------------------------------------------------------------------- */

int textbuf_line_count(const textbuf *tb)
{
    int n = 1;
    for (size_t i = 0; i < tb->len; i++)
        if (tb->data[i] == '\n') n++;
    return n;
}

size_t textbuf_line_start(const textbuf *tb, int line)
{
    if (line <= 0) return 0;

    size_t i = 0;
    int cur = 0;
    while (i < tb->len && cur < line) {
        if (tb->data[i] == '\n') cur++;
        i++;
    }

    return i;
}

int textbuf_line_at(const textbuf *tb, size_t pos)
{
    if (pos > tb->len) pos = tb->len;

    int line = 0;
    for (size_t i = 0; i < pos; i++)
        if (tb->data[i] == '\n') line++;

    return line;
}
