/* Das Rollbalken-Widget.
 *
 * Die Rechnung dahinter ist in test_scroll.c geprüft; hier geht es nur noch um
 * das, was der Balken selbst beisteuert: wo seine Teile liegen, welcher Klick
 * welche Bewegung auslöst, und wie er aussieht.
 *
 * Wie in test_widget.c und test_list.c setzen die Tests frame von Hand - das
 * täte sonst das Layout, und das ist hier nicht der Prüfgegenstand.
 */
#include "test.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "core/i18n.h"
#include "gfx/bitmap.h"
#include "gfx/draw.h"
#include "plat/plat.h"
#include "support/golden.h"
#include "ui/scroll.h"
#include "ui/theme.h"
#include "ui/widget.h"

#ifndef PDA_DATA_DIR
#define PDA_DATA_DIR "data"
#endif

/* Statisch aus demselben Grund wie in test_list.c: ein Widget hält vom Thema
 * nur einen Zeiger, und der darf nicht auf einen fremden Stapel zeigen. */
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

/* Ein senkrechter Balken über einem Modell, das der Aufrufer hält. Der Rahmen
 * ist so hoch, dass beide Pfeilfelder und eine ordentliche Rinne hineinpassen. */
static widget *make_bar(scrollmodel *m, scrollbar_dir dir, rect frame)
{
    widget *w = scrollbar_create(load_test_theme(), NULL, dir, m);
    if (w) w->frame = frame;
    return w;
}

static event click_at(int x, int y)
{
    event e = { 0 };
    e.kind = EV_MOUSE_DOWN;
    e.x = x;
    e.y = y;
    e.button = 1;
    e.clicks = 1;
    return e;
}

/* --- Anlegen ------------------------------------------------------------------ */

TEST(a_bar_without_a_model_is_refused)
{
    /* Ohne Modell hätte der Balken nichts anzuzeigen und würde beim ersten
     * Zeichnen in einen Nullzeiger greifen. Lieber gleich hier nein sagen. */
    CHECK(scrollbar_create(load_test_theme(), NULL, SCROLLBAR_VERTICAL, NULL) == NULL);
}

TEST(the_bar_shows_the_model_it_was_given)
{
    scrollmodel m = { 0, 0, 0 };
    widget     *w = make_bar(&m, SCROLLBAR_VERTICAL, rect_make(0, 0, 16, 200));
    REQUIRE(w != NULL);

    CHECK(scrollbar_model(w) == &m);
    widget_destroy(w);
}

TEST(the_bar_is_not_a_tab_stop)
{
    scrollmodel m = { 0, 0, 0 };
    widget     *w = make_bar(&m, SCROLLBAR_VERTICAL, rect_make(0, 0, 16, 200));
    REQUIRE(w != NULL);

    CHECK(!w->wants_focus);
    widget_destroy(w);
}

TEST(measured_size_is_the_theme_width_across_the_axis)
{
    scrollmodel m = { 0, 0, 0 };
    const theme *th = load_test_theme();

    widget *v = make_bar(&m, SCROLLBAR_VERTICAL, rect_make(0, 0, 0, 0));
    widget *h = make_bar(&m, SCROLLBAR_HORIZONTAL, rect_make(0, 0, 0, 0));
    REQUIRE(v != NULL);
    REQUIRE(h != NULL);

    int vw, vh, hw, hh;
    widget_measure(v, &vw, &vh);
    widget_measure(h, &hw, &hh);

    CHECK_EQ(vw, th->scrollbar_w);
    CHECK_EQ(vh, 3 * th->scrollbar_w);   /* zwei Pfeilfelder und etwas Rinne */
    CHECK_EQ(hh, th->scrollbar_w);
    CHECK_EQ(hw, 3 * th->scrollbar_w);

    widget_destroy(v);
    widget_destroy(h);
}

/* --- Pfeilfelder ------------------------------------------------------------- */

TEST(the_arrows_move_by_one_unit)
{
    scrollmodel m = { 0, 0, 0 };
    scroll_set(&m, 100, 10);
    scroll_to(&m, 50);

    widget *w = make_bar(&m, SCROLLBAR_VERTICAL, rect_make(0, 0, 16, 200));
    REQUIRE(w != NULL);

    event up = click_at(8, 4);            /* im oberen Pfeilfeld */
    CHECK(widget_event(w, &up));
    CHECK_EQ(m.value, 49);

    event down = click_at(8, 196);        /* im unteren Pfeilfeld */
    CHECK(widget_event(w, &down));
    CHECK_EQ(m.value, 50);

    widget_destroy(w);
}

TEST(a_horizontal_bar_uses_the_left_and_right_ends)
{
    scrollmodel m = { 0, 0, 0 };
    scroll_set(&m, 100, 10);
    scroll_to(&m, 50);

    widget *w = make_bar(&m, SCROLLBAR_HORIZONTAL, rect_make(0, 0, 200, 16));
    REQUIRE(w != NULL);

    CHECK(widget_event(w, &(event){ .kind = EV_MOUSE_DOWN, .x = 4, .y = 8 }));
    CHECK_EQ(m.value, 49);

    CHECK(widget_event(w, &(event){ .kind = EV_MOUSE_DOWN, .x = 196, .y = 8 }));
    CHECK_EQ(m.value, 50);

    widget_destroy(w);
}

/* --- Rinne --------------------------------------------------------------------- */

TEST(clicking_the_trough_moves_a_whole_page)
{
    scrollmodel m = { 0, 0, 0 };
    scroll_set(&m, 100, 10);
    scroll_to(&m, 50);

    widget *w = make_bar(&m, SCROLLBAR_VERTICAL, rect_make(0, 0, 16, 200));
    REQUIRE(w != NULL);

    /* Dicht unter dem oberen Pfeilfeld liegt bei value = 50 die Rinne über
     * dem Schieber. */
    event above = click_at(8, 20);
    CHECK(widget_event(w, &above));
    CHECK_EQ(m.value, 40);

    /* Und dicht über dem unteren Pfeilfeld die Rinne darunter. */
    event below = click_at(8, 180);
    CHECK(widget_event(w, &below));
    CHECK_EQ(m.value, 50);

    widget_destroy(w);
}

TEST(clicking_the_thumb_itself_does_not_move_it)
{
    scrollmodel m = { 0, 0, 0 };
    scroll_set(&m, 100, 10);
    scroll_to(&m, 45);

    widget *w = make_bar(&m, SCROLLBAR_VERTICAL, rect_make(0, 0, 16, 200));
    REQUIRE(w != NULL);

    rect  thumb = scrollbar_thumb(w);
    event grab  = click_at(8, thumb.y + thumb.h / 2);

    CHECK(widget_event(w, &grab));
    CHECK_EQ(m.value, 45);          /* nur gefasst, nicht bewegt */
    CHECK(scrollbar_is_dragging(w));

    widget_destroy(w);
}

/* --- Ziehen --------------------------------------------------------------------- */

TEST(dragging_the_thumb_follows_the_pointer)
{
    scrollmodel m = { 0, 0, 0 };
    scroll_set(&m, 100, 10);

    widget *w = make_bar(&m, SCROLLBAR_VERTICAL, rect_make(0, 0, 16, 200));
    REQUIRE(w != NULL);

    rect thumb  = scrollbar_thumb(w);
    int  grab_y = thumb.y + thumb.h / 2;

    CHECK(widget_event(w, &(event){ .kind = EV_MOUSE_DOWN, .x = 8, .y = grab_y }));

    /* Bis ganz nach unten ziehen. */
    CHECK(widget_event(w, &(event){ .kind = EV_MOUSE_MOVE, .x = 8, .y = 400 }));
    CHECK_EQ(m.value, scroll_max(&m));

    /* Und wieder ganz nach oben. */
    CHECK(widget_event(w, &(event){ .kind = EV_MOUSE_MOVE, .x = 8, .y = -400 }));
    CHECK_EQ(m.value, 0);

    CHECK(widget_event(w, &(event){ .kind = EV_MOUSE_UP, .x = 8, .y = 0 }));
    CHECK(!scrollbar_is_dragging(w));

    widget_destroy(w);
}

TEST(the_thumb_keeps_its_distance_to_the_pointer)
{
    /* Am unteren Rand des Schiebers anfassen und um genau so viele Pixel
     * ziehen, wie eine Einheit auf der Rinne breit ist. Springt der Schieber
     * beim Anfassen unter den Zeiger, kommt hier ein anderer Wert heraus. */
    scrollmodel m = { 0, 0, 0 };
    scroll_set(&m, 50, 10);

    widget *w = make_bar(&m, SCROLLBAR_VERTICAL, rect_make(0, 0, 16, 216));
    REQUIRE(w != NULL);

    rect thumb = scrollbar_thumb(w);
    int  track = 216 - 2 * 16 + 2;      /* die Rinne teilt sich beide Randlinien */
    int  edge  = thumb.y + thumb.h - 1;

    CHECK(widget_event(w, &(event){ .kind = EV_MOUSE_DOWN, .x = 8, .y = edge }));

    /* Ein Schritt entspricht (Rinne - Schieber) / scroll_max Pixeln. */
    int step = (track - thumb.h) / scroll_max(&m);
    CHECK(widget_event(w, &(event){ .kind = EV_MOUSE_MOVE, .x = 8, .y = edge + step }));
    CHECK_EQ(m.value, 1);

    widget_destroy(w);
}

TEST(moving_without_grabbing_changes_nothing)
{
    scrollmodel m = { 0, 0, 0 };
    scroll_set(&m, 100, 10);
    scroll_to(&m, 30);

    widget *w = make_bar(&m, SCROLLBAR_VERTICAL, rect_make(0, 0, 16, 200));
    REQUIRE(w != NULL);

    CHECK(!widget_event(w, &(event){ .kind = EV_MOUSE_MOVE, .x = 8, .y = 100 }));
    CHECK_EQ(m.value, 30);

    widget_destroy(w);
}

/* --- Mausrad und Grenzen --------------------------------------------------------- */

TEST(the_wheel_scrolls_in_the_reading_direction)
{
    scrollmodel m = { 0, 0, 0 };
    scroll_set(&m, 100, 10);
    scroll_to(&m, 30);

    widget *w = make_bar(&m, SCROLLBAR_VERTICAL, rect_make(0, 0, 16, 200));
    REQUIRE(w != NULL);

    /* wheel ist positiv nach oben (plat.h), und nach oben heißt kleinere
     * Werte. */
    CHECK(widget_event(w, &(event){ .kind = EV_WHEEL, .x = 8, .y = 100, .wheel = 3 }));
    CHECK_EQ(m.value, 27);

    widget_destroy(w);
}

TEST(events_outside_the_bar_are_left_alone)
{
    scrollmodel m = { 0, 0, 0 };
    scroll_set(&m, 100, 10);
    scroll_to(&m, 30);

    widget *w = make_bar(&m, SCROLLBAR_VERTICAL, rect_make(0, 0, 16, 200));
    REQUIRE(w != NULL);

    event far = click_at(100, 100);
    CHECK(!widget_event(w, &far));
    CHECK(!widget_event(w, &(event){ .kind = EV_WHEEL, .x = 100, .y = 100, .wheel = 1 }));
    CHECK_EQ(m.value, 30);

    widget_destroy(w);
}

TEST(a_bar_with_nothing_to_scroll_swallows_the_click)
{
    /* Es gibt nichts zu bedienen, aber der Klick gehört trotzdem dem Balken -
     * er darf nicht auf das durchfallen, was hinter ihm liegt. */
    scrollmodel m = { 0, 0, 0 };
    scroll_set(&m, 5, 10);

    widget *w = make_bar(&m, SCROLLBAR_VERTICAL, rect_make(0, 0, 16, 200));
    REQUIRE(w != NULL);

    event c = click_at(8, 4);
    CHECK(widget_event(w, &c));
    CHECK_EQ(m.value, 0);

    widget_destroy(w);
}

TEST(a_bar_too_short_for_a_trough_is_safe)
{
    /* Vor dem ersten Layout ist der Rahmen leer, und auch danach kann er
     * kleiner sein als die beiden Pfeilfelder zusammen. Nichts davon darf in
     * eine negative Länge oder eine Division durch null laufen. */
    scrollmodel m = { 0, 0, 0 };
    scroll_set(&m, 100, 10);

    widget *w = make_bar(&m, SCROLLBAR_VERTICAL, rect_make(0, 0, 16, 20));
    REQUIRE(w != NULL);

    bitmap bm;
    REQUIRE(bitmap_init(&bm, 16, 20));
    gc g;
    gc_init(&g, &bm);
    widget_draw(w, &g);

    event c = click_at(8, 10);
    CHECK(widget_event(w, &c));
    bitmap_free(&bm);

    widget *empty = make_bar(&m, SCROLLBAR_VERTICAL, rect_make(0, 0, 0, 0));
    REQUIRE(empty != NULL);
    event c0 = click_at(0, 0);
    CHECK(!widget_event(empty, &c0));

    widget_destroy(empty);
    widget_destroy(w);
}

/* --- Aussehen -------------------------------------------------------------------- */

TEST(a_scrollbar_looks_like_a_scrollbar)
{
    /* Drei Zustände nebeneinander: oben, in der Mitte, und einer ohne etwas zu
     * scrollen. Dazu ein waagerechter Balken. */
    scrollmodel top    = { 0, 0, 0 };
    scrollmodel middle = { 0, 0, 0 };
    scrollmodel idle   = { 0, 0, 0 };
    scrollmodel wide   = { 0, 0, 0 };

    scroll_set(&top, 100, 10);
    scroll_set(&middle, 100, 10);
    scroll_to(&middle, 45);
    scroll_set(&idle, 5, 10);
    scroll_set(&wide, 40, 10);
    scroll_to(&wide, 12);

    widget *bars[4];
    bars[0] = make_bar(&top,    SCROLLBAR_VERTICAL,   rect_make(4,   4, 16, 120));
    bars[1] = make_bar(&middle, SCROLLBAR_VERTICAL,   rect_make(28,  4, 16, 120));
    bars[2] = make_bar(&idle,   SCROLLBAR_VERTICAL,   rect_make(52,  4, 16, 120));
    bars[3] = make_bar(&wide,   SCROLLBAR_HORIZONTAL, rect_make(76,  4, 120, 16));
    for (int i = 0; i < 4; i++) REQUIRE(bars[i] != NULL);

    bitmap bm;
    REQUIRE(bitmap_init(&bm, 200, 128));
    gc g;
    gc_init(&g, &bm);
    g.pat = PAT_WHITE;
    gfx_fill_rect(&g, rect_make(0, 0, 200, 128));

    for (int i = 0; i < 4; i++) widget_draw(bars[i], &g);

    CHECK(golden_check("scrollbars", &bm));

    bitmap_free(&bm);
    for (int i = 0; i < 4; i++) widget_destroy(bars[i]);
}

int main(void)
{
    RUN(a_bar_without_a_model_is_refused);
    RUN(the_bar_shows_the_model_it_was_given);
    RUN(the_bar_is_not_a_tab_stop);
    RUN(measured_size_is_the_theme_width_across_the_axis);

    RUN(the_arrows_move_by_one_unit);
    RUN(a_horizontal_bar_uses_the_left_and_right_ends);

    RUN(clicking_the_trough_moves_a_whole_page);
    RUN(clicking_the_thumb_itself_does_not_move_it);

    RUN(dragging_the_thumb_follows_the_pointer);
    RUN(the_thumb_keeps_its_distance_to_the_pointer);
    RUN(moving_without_grabbing_changes_nothing);

    RUN(the_wheel_scrolls_in_the_reading_direction);
    RUN(events_outside_the_bar_are_left_alone);
    RUN(a_bar_with_nothing_to_scroll_swallows_the_click);
    RUN(a_bar_too_short_for_a_trough_is_safe);

    RUN(a_scrollbar_looks_like_a_scrollbar);

    return test_summary();
}
