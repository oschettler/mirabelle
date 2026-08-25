/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Die Textfelder aus M8, Abschluss des Meilensteins.
 *
 * Panel und Layout gibt es noch nicht flächendeckend; wie in test_list.c wird
 * frame hier von Hand gesetzt statt errechnet.
 *
 * Für den Umbruchtest gibt es keine öffentliche Abfrage der Anzeigezeilen -
 * mit Absicht, widget.h gibt nur text_widget_top_line() heraus. Der Umweg
 * über KEY_END: die Schreibmarke landet damit auf der letzten Anzeigezeile,
 * und wenn top_line danach über 0 liegt, gab es mehr als eine.
 */
#include "test.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/i18n.h"
#include "core/utf8.h"
#include "gfx/bitmap.h"
#include "gfx/draw.h"
#include "gfx/font.h"
#include "gfx/text.h"
#include "plat/plat.h"
#include "support/golden.h"
#include "ui/caret.h"
#include "ui/theme.h"
#include "ui/widget.h"

extern const font system12;

#ifndef PDA_DATA_DIR
#define PDA_DATA_DIR "data"
#endif

/* --- Laden von Thema und Katalog -------------------------------------------
 *
 * Wie in test_list.c: das Thema lebt in einer statischen Variablen, damit
 * der Zeiger, den sich jedes Widget merkt, den Testlauf überlebt.
 */
static theme g_theme;

static const theme *load_test_theme(void)
{
    static bool loaded = false;
    if (loaded) return &g_theme;

    char path[512], err[256] = "";
    snprintf(path, sizeof path, "%s/themes/desktop.theme", PDA_DATA_DIR);
    if (!theme_load(&g_theme, path, err, sizeof err)) {
        printf("  Thema nicht ladbar: %s\n", err);
        theme_defaults(&g_theme);
    }
    loaded = true;
    return &g_theme;
}

static catalog *load_test_catalog(void)
{
    char path[512], err[256] = "";
    snprintf(path, sizeof path, "%s/lang/de.strings", PDA_DATA_DIR);

    catalog *c = i18n_load(path, err, sizeof err);
    if (!c) printf("  Katalog nicht ladbar: %s\n", err);
    return c;
}

/* --- Tippen und Rücktaste --------------------------------------------------------- */

TEST(typing_fills_value_and_round_trips_with_set_value)
{
    const theme *th  = load_test_theme();
    catalog     *cat = load_test_catalog();
    REQUIRE(cat);

    widget *w = text_field_create(th, cat);
    REQUIRE(w);
    int mw, mh;
    widget_measure(w, &mw, &mh);
    w->frame   = rect_make(0, 0, mw, mh);
    w->focused = true;

    CHECK_STR(text_widget_value(w), "");

    event h = { .kind = EV_TEXT, .text = "h" };
    event i = { .kind = EV_TEXT, .text = "i" };
    CHECK(widget_event(w, &h));
    CHECK(widget_event(w, &i));
    CHECK_STR(text_widget_value(w), "hi");

    CHECK(text_widget_set_value(w, "servus"));
    CHECK_STR(text_widget_value(w), "servus");

    widget_destroy(w);
    i18n_free(cat);
}

TEST(backspace_removes_one_character_not_one_byte)
{
    const theme *th  = load_test_theme();
    catalog     *cat = load_test_catalog();
    REQUIRE(cat);

    widget *w = text_field_create(th, cat);
    REQUIRE(w);
    int mw, mh;
    widget_measure(w, &mw, &mh);
    w->frame   = rect_make(0, 0, mw, mh);
    w->focused = true;

    text_widget_set_value(w, "äöü");
    CHECK_EQ(strlen(text_widget_value(w)), 6);   /* drei Zeichen, sechs Bytes */

    event bs = { .kind = EV_KEY_DOWN, .key = KEY_BACKSPACE };
    CHECK(widget_event(w, &bs));

    CHECK_EQ(strlen(text_widget_value(w)), 4);   /* ein Zeichen weg, nicht ein Byte */
    CHECK_STR(text_widget_value(w), "äö");

    widget_destroy(w);
    i18n_free(cat);
}

/* --- Auswahl und Widerruf ---------------------------------------------------------- */

TEST(cmd_a_then_typing_replaces_everything)
{
    const theme *th  = load_test_theme();
    catalog     *cat = load_test_catalog();
    REQUIRE(cat);

    widget *w = text_field_create(th, cat);
    REQUIRE(w);
    int mw, mh;
    widget_measure(w, &mw, &mh);
    w->frame   = rect_make(0, 0, mw, mh);
    w->focused = true;

    text_widget_set_value(w, "altes wort");

    event select_all = { .kind = EV_KEY_DOWN, .key = 'a', .mods = MOD_CMD };
    CHECK(widget_event(w, &select_all));
    CHECK(textbuf_has_selection(text_widget_buf(w)));

    event x = { .kind = EV_TEXT, .text = "x" };
    CHECK(widget_event(w, &x));

    CHECK_STR(text_widget_value(w), "x");

    widget_destroy(w);
    i18n_free(cat);
}

TEST(cmd_z_undoes_several_typed_letters_together)
{
    const theme *th  = load_test_theme();
    catalog     *cat = load_test_catalog();
    REQUIRE(cat);

    widget *w = text_field_create(th, cat);
    REQUIRE(w);
    int mw, mh;
    widget_measure(w, &mw, &mh);
    w->frame   = rect_make(0, 0, mw, mh);
    w->focused = true;

    event a = { .kind = EV_TEXT, .text = "a" };
    event b = { .kind = EV_TEXT, .text = "b" };
    event c = { .kind = EV_TEXT, .text = "c" };
    CHECK(widget_event(w, &a));
    CHECK(widget_event(w, &b));
    CHECK(widget_event(w, &c));
    CHECK_STR(text_widget_value(w), "abc");

    event undo = { .kind = EV_KEY_DOWN, .key = 'z', .mods = MOD_CMD };
    CHECK(widget_event(w, &undo));
    CHECK_STR(text_widget_value(w), "");   /* alle drei auf einmal zurück */

    event redo = { .kind = EV_KEY_DOWN, .key = 'z', .mods = MOD_CMD | MOD_SHIFT };
    CHECK(widget_event(w, &redo));
    CHECK_STR(text_widget_value(w), "abc");

    widget_destroy(w);
    i18n_free(cat);
}

/* --- Pfeiltasten -------------------------------------------------------------------- */

TEST(arrow_keys_move_and_shift_extends_selection)
{
    const theme *th  = load_test_theme();
    catalog     *cat = load_test_catalog();
    REQUIRE(cat);

    widget *w = text_field_create(th, cat);
    REQUIRE(w);
    int mw, mh;
    widget_measure(w, &mw, &mh);
    w->frame   = rect_make(0, 0, mw, mh);
    w->focused = true;

    text_widget_set_value(w, "abcde");   /* Marke jetzt am Ende, Position 5 */
    textbuf *buf = text_widget_buf(w);

    event left = { .kind = EV_KEY_DOWN, .key = KEY_LEFT };
    CHECK(widget_event(w, &left));
    CHECK_EQ(textbuf_cursor(buf), 4);
    CHECK(!textbuf_has_selection(buf));

    event left_shift = { .kind = EV_KEY_DOWN, .key = KEY_LEFT, .mods = MOD_SHIFT };
    CHECK(widget_event(w, &left_shift));
    CHECK(textbuf_has_selection(buf));

    size_t from, to;
    textbuf_selection(buf, &from, &to);
    CHECK_EQ(from, 3);
    CHECK_EQ(to, 4);

    widget_destroy(w);
    i18n_free(cat);
}

TEST(cmd_arrow_moves_by_word)
{
    const theme *th  = load_test_theme();
    catalog     *cat = load_test_catalog();
    REQUIRE(cat);

    widget *w = text_field_create(th, cat);
    REQUIRE(w);
    int mw, mh;
    widget_measure(w, &mw, &mh);
    w->frame   = rect_make(0, 0, mw, mh);
    w->focused = true;

    text_widget_set_value(w, "hello world");   /* Marke am Ende, Position 11 */
    textbuf *buf = text_widget_buf(w);

    event cmd_left = { .kind = EV_KEY_DOWN, .key = KEY_LEFT, .mods = MOD_CMD };
    CHECK(widget_event(w, &cmd_left));
    CHECK_EQ(textbuf_cursor(buf), 6);   /* Anfang von "world" */

    event cmd_left2 = { .kind = EV_KEY_DOWN, .key = KEY_LEFT, .mods = MOD_CMD };
    CHECK(widget_event(w, &cmd_left2));
    CHECK_EQ(textbuf_cursor(buf), 0);   /* Anfang von "hello" */

    widget_destroy(w);
    i18n_free(cat);
}

/* --- Maus -------------------------------------------------------------------------- */

TEST(double_click_selects_word_under_pointer)
{
    const theme *th  = load_test_theme();
    catalog     *cat = load_test_catalog();
    REQUIRE(cat);

    widget *w = text_field_create(th, cat);
    REQUIRE(w);
    w->frame = rect_make(0, 0, 300, 40);   /* breit genug, kein Scrollen nötig */
    text_widget_set_value(w, "hello world foo");

    int inset      = th->border + th->menu_pad;
    int content_x  = w->frame.x + inset;
    int prefix_w   = text_width(&system12, "hello ");

    event dbl = { .kind = EV_MOUSE_DOWN, .button = 1, .clicks = 2,
                  .x = content_x + prefix_w + 3, .y = w->frame.y + 20 };
    CHECK(widget_event(w, &dbl));

    textbuf *buf = text_widget_buf(w);
    CHECK(textbuf_has_selection(buf));

    size_t from, to;
    textbuf_selection(buf, &from, &to);
    CHECK_EQ(from, 6);    /* Anfang von "world" */
    CHECK_EQ(to, 11);     /* Ende von "world" */

    widget_destroy(w);
    i18n_free(cat);
}

TEST(dragging_mouse_extends_selection)
{
    const theme *th  = load_test_theme();
    catalog     *cat = load_test_catalog();
    REQUIRE(cat);

    widget *w = text_field_create(th, cat);
    REQUIRE(w);
    w->frame = rect_make(0, 0, 300, 40);
    text_widget_set_value(w, "hello world foo");

    int inset     = th->border + th->menu_pad;
    int content_x = w->frame.x + inset;
    int x_start   = content_x + 2;
    int x_end     = content_x + text_width(&system12, "hello world");

    event down = { .kind = EV_MOUSE_DOWN, .button = 1, .clicks = 1,
                   .x = x_start, .y = w->frame.y + 20 };
    CHECK(widget_event(w, &down));

    event move = { .kind = EV_MOUSE_MOVE, .x = x_end, .y = w->frame.y + 20 };
    CHECK(widget_event(w, &move));

    textbuf *buf = text_widget_buf(w);
    CHECK(textbuf_has_selection(buf));
    size_t from, to;
    textbuf_selection(buf, &from, &to);
    CHECK_EQ(from, 0);
    CHECK_EQ(to, 11);

    event up = { .kind = EV_MOUSE_UP };
    CHECK(widget_event(w, &up));   /* hat gezogen, meldet also true */

    /* Nach dem Loslassen zieht eine weitere Bewegung nichts mehr auf. */
    event move2 = { .kind = EV_MOUSE_MOVE, .x = content_x, .y = w->frame.y + 20 };
    CHECK(!widget_event(w, &move2));
    textbuf_selection(buf, &from, &to);
    CHECK_EQ(to, 11);

    widget_destroy(w);
    i18n_free(cat);
}

/* --- Der Unterschied zwischen Feld und Fläche -------------------------------------- */

TEST(text_field_leaves_return_and_vertical_arrows_unhandled)
{
    const theme *th  = load_test_theme();
    catalog     *cat = load_test_catalog();
    REQUIRE(cat);

    widget *w = text_field_create(th, cat);
    REQUIRE(w);
    int mw, mh;
    widget_measure(w, &mw, &mh);
    w->frame   = rect_make(0, 0, mw, mh);
    w->focused = true;

    event ret  = { .kind = EV_KEY_DOWN, .key = KEY_RETURN };
    event up   = { .kind = EV_KEY_DOWN, .key = KEY_UP };
    event down = { .kind = EV_KEY_DOWN, .key = KEY_DOWN };

    CHECK(!widget_event(w, &ret));    /* gehört dem Formular */
    CHECK(!widget_event(w, &up));
    CHECK(!widget_event(w, &down));

    widget_destroy(w);
    i18n_free(cat);
}

TEST(text_area_return_inserts_newline_and_is_handled)
{
    const theme *th  = load_test_theme();
    catalog     *cat = load_test_catalog();
    REQUIRE(cat);

    widget *w = text_area_create(th, cat);
    REQUIRE(w);
    int mw, mh;
    widget_measure(w, &mw, &mh);
    w->frame   = rect_make(0, 0, mw, mh);
    w->focused = true;

    text_widget_set_value(w, "ab");

    event ret = { .kind = EV_KEY_DOWN, .key = KEY_RETURN };
    CHECK(widget_event(w, &ret));
    CHECK_STR(text_widget_value(w), "ab\n");

    widget_destroy(w);
    i18n_free(cat);
}

/* --- Umbruch im mehrzeiligen Feld --------------------------------------------------- */

TEST(long_text_wraps_into_several_display_lines)
{
    const theme *th  = load_test_theme();
    catalog     *cat = load_test_catalog();
    REQUIRE(cat);

    widget *w = text_area_create(th, cat);
    REQUIRE(w);
    int inset = th->border + th->menu_pad;
    w->frame  = rect_make(0, 0, 100, 6 * system12.size + 2 * inset);
    w->focused = true;

    text_widget_set_value(w,
        "eins zwei drei vier fuenf sechs sieben acht neun zehn elf zwoelf "
        "dreizehn vierzehn fuenfzehn");

    event end = { .kind = EV_KEY_DOWN, .key = KEY_END };
    CHECK(widget_event(w, &end));   /* Marke ans Ende, auf die letzte Anzeigezeile */

    CHECK(text_widget_top_line(w) > 0);   /* wäre 0 geblieben, gäbe es nur eine */

    widget_destroy(w);
    i18n_free(cat);
}

TEST(wrapping_does_not_change_the_underlying_text)
{
    const theme *th  = load_test_theme();
    catalog     *cat = load_test_catalog();
    REQUIRE(cat);

    widget *w = text_area_create(th, cat);
    REQUIRE(w);
    int inset = th->border + th->menu_pad;
    w->frame  = rect_make(0, 0, 90, 6 * system12.size + 2 * inset);
    w->focused = true;

    const char *original = "ein etwas längerer Text zum Umbrechen im schmalen Feld";
    text_widget_set_value(w, original);

    event end = { .kind = EV_KEY_DOWN, .key = KEY_END };
    CHECK(widget_event(w, &end));   /* erzwingt den Umbruch als Nebeneffekt */

    CHECK_STR(text_widget_value(w), original);   /* byteweise unverändert */

    widget_destroy(w);
    i18n_free(cat);
}

TEST(unbreakable_long_word_is_split_hard_and_stays_valid_utf8)
{
    const theme *th  = load_test_theme();
    catalog     *cat = load_test_catalog();
    REQUIRE(cat);

    widget *w = text_area_create(th, cat);
    REQUIRE(w);
    int inset = th->border + th->menu_pad;
    w->frame  = rect_make(0, 0, 50, 6 * system12.size + 2 * inset);
    w->focused = true;

    /* Ein einziges, sehr langes "Wort" ohne Leerzeichen, mit Umlauten - keine
     * Stelle zum Umbrechen an einem Leerzeichen, nur das harte Trennen bleibt. */
    const char *long_word =
        "ÄÖÜäöüÄÖÜäöüÄÖÜäöüÄÖÜäöüÄÖÜäöüÄÖÜäöüÄÖÜäöüÄÖÜäöüÄÖÜäöüÄÖÜäöü";
    CHECK(text_widget_set_value(w, long_word));

    event end = { .kind = EV_KEY_DOWN, .key = KEY_END };
    CHECK(widget_event(w, &end));

    CHECK(text_widget_top_line(w) > 0);
    CHECK_STR(text_widget_value(w), long_word);
    CHECK(utf8_valid(text_widget_value(w)));

    widget_destroy(w);
    i18n_free(cat);
}

TEST(cursor_stays_visible_while_moving_through_lines)
{
    const theme *th  = load_test_theme();
    catalog     *cat = load_test_catalog();
    REQUIRE(cat);

    widget *w = text_area_create(th, cat);
    REQUIRE(w);
    int mw, mh;
    widget_measure(w, &mw, &mh);
    w->frame   = rect_make(0, 0, 200, mh);
    w->focused = true;

    int  inset = th->border + th->menu_pad;
    int  rows  = (mh - 2 * inset) / system12.size;

    /* Zwanzig kurze, durch \n getrennte Zeilen - jede echte Zeile ist damit
     * genau eine Anzeigezeile, ohne dass der Umbruch selbst mitspielt. */
    char text[512] = "";
    for (int i = 0; i < 20; i++) {
        char line[16];
        snprintf(line, sizeof line, "z%d\n", i);
        strcat(text, line);
    }
    text_widget_set_value(w, text);
    textbuf_move_cursor(text_widget_buf(w), MOVE_TEXT_START, false);

    for (int i = 0; i < 19; i++) {
        event down = { .kind = EV_KEY_DOWN, .key = KEY_DOWN };
        CHECK(widget_event(w, &down));

        int top = text_widget_top_line(w);
        CHECK(top <= i + 1);
        CHECK(i + 1 < top + rows);
    }

    widget_destroy(w);
    i18n_free(cat);
}

/* --- Randfälle, die nicht abstürzen dürfen ----------------------------------------- */

TEST(text_area_without_layout_does_not_crash)
{
    const theme *th  = load_test_theme();
    catalog     *cat = load_test_catalog();
    REQUIRE(cat);

    widget *w = text_area_create(th, cat);
    REQUIRE(w);
    w->focused = true;
    /* frame bleibt (0,0,0,0) - so wie vor dem ersten Layoutlauf. */

    text_widget_set_value(w, "hallo welt");

    event txt    = { .kind = EV_TEXT, .key = 0, .text = "!" };
    event bs     = { .kind = EV_KEY_DOWN, .key = KEY_BACKSPACE };
    event del    = { .kind = EV_KEY_DOWN, .key = KEY_DELETE };
    event left   = { .kind = EV_KEY_DOWN, .key = KEY_LEFT, .mods = MOD_SHIFT };
    event right  = { .kind = EV_KEY_DOWN, .key = KEY_RIGHT, .mods = MOD_CMD };
    event up     = { .kind = EV_KEY_DOWN, .key = KEY_UP };
    event down   = { .kind = EV_KEY_DOWN, .key = KEY_DOWN };
    event home   = { .kind = EV_KEY_DOWN, .key = KEY_HOME };
    event endk   = { .kind = EV_KEY_DOWN, .key = KEY_END };
    event ret    = { .kind = EV_KEY_DOWN, .key = KEY_RETURN };
    event selall = { .kind = EV_KEY_DOWN, .key = 'a', .mods = MOD_CMD };
    event undo   = { .kind = EV_KEY_DOWN, .key = 'z', .mods = MOD_CMD };
    event redo   = { .kind = EV_KEY_DOWN, .key = 'z', .mods = MOD_CMD | MOD_SHIFT };
    event click  = { .kind = EV_MOUSE_DOWN, .button = 1, .clicks = 1, .x = 10, .y = 10 };
    event dbl    = { .kind = EV_MOUSE_DOWN, .button = 1, .clicks = 2, .x = 10, .y = 10 };
    event move   = { .kind = EV_MOUSE_MOVE, .x = 20, .y = 20 };
    event mup    = { .kind = EV_MOUSE_UP };

    widget_event(w, &txt);
    widget_event(w, &bs);
    widget_event(w, &del);
    widget_event(w, &left);
    widget_event(w, &right);
    widget_event(w, &up);
    widget_event(w, &down);
    widget_event(w, &home);
    widget_event(w, &endk);
    widget_event(w, &ret);
    widget_event(w, &selall);
    widget_event(w, &undo);
    widget_event(w, &redo);
    widget_event(w, &click);
    widget_event(w, &dbl);
    widget_event(w, &move);
    widget_event(w, &mup);

    bitmap bm;
    REQUIRE(bitmap_init(&bm, 4, 4));
    gc g;
    gc_init(&g, &bm);
    widget_draw(w, &g);   /* auch das Zeichnen ohne Layout darf nicht abstürzen */
    bitmap_free(&bm);

    CHECK(text_widget_value(w) != NULL);

    widget_destroy(w);
    i18n_free(cat);
}

TEST(disabled_widget_processes_nothing)
{
    const theme *th  = load_test_theme();
    catalog     *cat = load_test_catalog();
    REQUIRE(cat);

    widget *w = text_field_create(th, cat);
    REQUIRE(w);
    int mw, mh;
    widget_measure(w, &mw, &mh);
    w->frame   = rect_make(0, 0, mw, mh);
    w->focused = true;
    w->enabled = false;

    event txt   = { .kind = EV_TEXT, .text = "x" };
    event bs    = { .kind = EV_KEY_DOWN, .key = KEY_BACKSPACE };
    event click = { .kind = EV_MOUSE_DOWN, .button = 1, .clicks = 1,
                     .x = w->frame.x + 2, .y = w->frame.y + 2 };

    CHECK(!widget_event(w, &txt));
    CHECK(!widget_event(w, &bs));
    CHECK(!widget_event(w, &click));
    CHECK_STR(text_widget_value(w), "");

    widget_destroy(w);
    i18n_free(cat);
}

/* --- Sollbilder ---------------------------------------------------------------------- */

TEST(golden_text_field_focused)
{
    const theme *th  = load_test_theme();
    catalog     *cat = load_test_catalog();
    REQUIRE(cat);

    widget *w = text_field_create(th, cat);
    REQUIRE(w);
    text_widget_set_value(w, "grün Käse");

    int fw, fh;
    widget_measure(w, &fw, &fh);

    int margin    = 4;
    int content_w = fw + 2 * margin;
    int content_h = fh + 2 * margin;
    CHECK(content_w * content_h <= GOLDEN_MAX_P1_PIXELS);

    w->frame   = rect_make(margin, margin, fw, fh);
    w->focused = true;

    bitmap bm;
    REQUIRE(bitmap_init(&bm, content_w, content_h));

    gc g;
    gc_init(&g, &bm);
    g.pat  = PAT_WHITE;
    g.mode = GFX_COPY;
    gfx_clear(&g);

    widget_draw(w, &g);

    CHECK(golden_check("text_field_focused", &bm));

    bitmap_free(&bm);
    widget_destroy(w);
    i18n_free(cat);
}

TEST(golden_text_area_wrapped)
{
    const theme *th  = load_test_theme();
    catalog     *cat = load_test_catalog();
    REQUIRE(cat);

    widget *w = text_area_create(th, cat);
    REQUIRE(w);

    int fw, fh;
    widget_measure(w, &fw, &fh);
    fw = 100;   /* schmaler als die Wunschbreite, damit sichtbar umgebrochen wird */

    text_widget_set_value(w,
        "Dies ist ein längerer Text, der in diesem schmalen Feld über "
        "mehrere Zeilen umbrechen sollte.");

    textbuf *buf = text_widget_buf(w);
    textbuf_set_cursor(buf, 5, false);
    textbuf_set_cursor(buf, 12, true);   /* eine Auswahl fürs Sollbild */

    int margin    = 4;
    int content_w = fw + 2 * margin;
    int content_h = fh + 2 * margin;
    CHECK(content_w * content_h <= GOLDEN_MAX_P1_PIXELS);

    w->frame   = rect_make(margin, margin, fw, fh);
    w->focused = true;

    bitmap bm;
    REQUIRE(bitmap_init(&bm, content_w, content_h));

    gc g;
    gc_init(&g, &bm);
    g.pat  = PAT_WHITE;
    g.mode = GFX_COPY;
    gfx_clear(&g);

    widget_draw(w, &g);

    CHECK(golden_check("text_area_wrapped", &bm));

    bitmap_free(&bm);
    widget_destroy(w);
    i18n_free(cat);
}

/* --- Die Schreibmarke blinkt ------------------------------------------------------ */

/* Zählt die gesetzten Pixel. Genau genug: die Schreibmarke ist der einzige
 * Unterschied zwischen den beiden Bildern in diesem Test. */
static int ink(const bitmap *bm)
{
    int n = 0;
    for (int y = 0; y < bm->h; y++)
        for (int x = 0; x < bm->w; x++)
            n += bitmap_get(bm, x, y) ? 1 : 0;
    return n;
}

static int draw_field_ink(widget *w)
{
    bitmap bm;
    if (!bitmap_init(&bm, w->frame.w, w->frame.h)) return -1;

    gc g;
    gc_init(&g, &bm);
    g.pat  = PAT_WHITE;
    g.mode = GFX_COPY;
    gfx_clear(&g);

    widget_draw(w, &g);

    int n = ink(&bm);
    bitmap_free(&bm);
    return n;
}

TEST(the_caret_disappears_in_the_dark_half_of_the_blink)
{
    const theme *th  = load_test_theme();
    catalog     *cat = load_test_catalog();
    REQUIRE(cat);

    widget *w = text_field_create(th, cat);
    REQUIRE(w);
    int mw, mh;
    widget_measure(w, &mw, &mh);
    w->frame   = rect_make(0, 0, mw, mh);
    w->focused = true;

    text_widget_set_value(w, "abc");

    /* Ohne Uhr steht die Schreibmarke und ist zu sehen - so entstehen alle
     * Sollbilder in diesem Verzeichnis. */
    caret_reset();
    int hell = draw_field_ink(w);
    REQUIRE(hell > 0);

    caret_tick(0);
    CHECK_EQ(draw_field_ink(w), hell);

    /* Eine halbe Periode weiter ist sie aus, und das Bild hat weniger Pixel. */
    caret_tick(CARET_BLINK_MS);
    int dunkel = draw_field_ink(w);
    CHECK(dunkel < hell);

    /* Ein Anschlag holt sie sofort zurück: wer tippt, will sehen, wo er ist. */
    event x = { .kind = EV_TEXT, .text = "d" };
    CHECK(widget_event(w, &x));
    CHECK(draw_field_ink(w) > dunkel);

    caret_reset();
    widget_destroy(w);
    i18n_free(cat);
}

int main(void)
{
    RUN(typing_fills_value_and_round_trips_with_set_value);
    RUN(backspace_removes_one_character_not_one_byte);

    RUN(cmd_a_then_typing_replaces_everything);
    RUN(cmd_z_undoes_several_typed_letters_together);

    RUN(arrow_keys_move_and_shift_extends_selection);
    RUN(cmd_arrow_moves_by_word);

    RUN(double_click_selects_word_under_pointer);
    RUN(dragging_mouse_extends_selection);

    RUN(text_field_leaves_return_and_vertical_arrows_unhandled);
    RUN(text_area_return_inserts_newline_and_is_handled);

    RUN(long_text_wraps_into_several_display_lines);
    RUN(wrapping_does_not_change_the_underlying_text);
    RUN(unbreakable_long_word_is_split_hard_and_stays_valid_utf8);
    RUN(cursor_stays_visible_while_moving_through_lines);

    RUN(text_area_without_layout_does_not_crash);
    RUN(disabled_widget_processes_nothing);
    RUN(the_caret_disappears_in_the_dark_half_of_the_blink);

    RUN(golden_text_field_focused);
    RUN(golden_text_area_wrapped);

    return test_summary();
}
