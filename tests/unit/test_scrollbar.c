/* SPDX-License-Identifier: GPL-3.0-or-later */
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
#include "gfx/font.h"
#include "gfx/pattern.h"
#include "plat/plat.h"
#include "support/golden.h"
#include "ui/scroll.h"
#include "ui/theme.h"
#include "ui/widget.h"

extern const font system12;

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

static catalog *load_test_catalog(void)
{
    char path[512], err[256] = "";
    snprintf(path, sizeof path, "%s/lang/de.strings", PDA_DATA_DIR);

    catalog *c = i18n_load(path, err, sizeof err);
    if (!c) printf("  Katalog nicht ladbar: %s\n", err);
    return c;
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

/* --- Zusammenspiel mit einer Liste --------------------------------------------
 *
 * Der Balken hängt am Modell der Liste. Zu prüfen ist deshalb nur eines: dass
 * es wirklich dasselbe Modell ist und nicht zwei, die abgeglichen werden
 * müssten. Beide Richtungen zählen - die Liste bewegt den Balken, und der
 * Balken bewegt die Liste.
 */

static const char *const items[] = {
    "ok", "cancel", "save", "open", "close", "quit",
    "cut", "copy", "paste", "undo", "redo", "new"
};

TEST(a_list_and_its_bar_share_one_position)
{
    catalog *cat = load_test_catalog();
    REQUIRE(cat != NULL);

    widget *lst = list_create(load_test_theme(), cat);
    REQUIRE(lst != NULL);
    list_set_items(lst, items, 12);
    lst->frame = rect_make(0, 0, 120, 4 * g_theme.menu_item_h);   /* vier Zeilen */

    widget *bar = scrollbar_create(load_test_theme(), cat, SCROLLBAR_VERTICAL,
                                   list_scroll(lst));
    REQUIRE(bar != NULL);
    bar->frame = rect_make(120, 0, 16, 4 * g_theme.menu_item_h);

    /* Die Liste kennt jetzt zwölf Einträge und zeigt vier davon. */
    scrollmodel *m = scrollbar_model(bar);
    CHECK_EQ(m->total, 12);
    CHECK_EQ(m->page, 4);

    /* Mit der Tastatur ans Ende: der Balken folgt, ohne dass jemand etwas
     * abgleicht. */
    lst->focused = true;
    CHECK(widget_event(lst, &(event){ .kind = EV_KEY_DOWN, .key = KEY_END }));
    CHECK_EQ(list_top(lst), 8);
    CHECK_EQ(m->value, 8);

    /* Und zurück über das Pfeilfeld des Balkens. */
    event up = click_at(128, 4);
    CHECK(widget_event(bar, &up));
    CHECK_EQ(list_top(lst), 7);

    widget_destroy(bar);
    widget_destroy(lst);
    i18n_free(cat);
}

TEST(drawing_the_list_brings_its_model_up_to_date)
{
    /* Die Höhe setzt das Layout, und die Liste erfährt sie erst beim
     * Zeichnen. Ein Balken daneben wird im selben Durchgang gezeichnet und
     * muss die neue Seitengröße schon sehen - sonst zeigt er beim ersten Bild
     * nach jeder Größenänderung einen Schieber falscher Länge. */
    catalog *cat = load_test_catalog();
    REQUIRE(cat != NULL);

    widget *lst = list_create(load_test_theme(), cat);
    REQUIRE(lst != NULL);
    list_set_items(lst, items, 12);

    scrollmodel *m = list_scroll(lst);

    bitmap bm;
    REQUIRE(bitmap_init(&bm, 160, 200));
    gc g;
    gc_init(&g, &bm);

    lst->frame = rect_make(0, 0, 120, 6 * g_theme.menu_item_h);
    widget_draw(lst, &g);
    CHECK_EQ(m->page, 6);

    /* Und noch einmal, ohne dass ein Ereignis dazwischenkommt. */
    lst->frame = rect_make(0, 0, 120, 3 * g_theme.menu_item_h);
    widget_draw(lst, &g);
    CHECK_EQ(m->page, 3);

    bitmap_free(&bm);
    widget_destroy(lst);
    i18n_free(cat);
}

TEST(the_list_brings_its_model_up_to_date_before_an_event)
{
    /* Dieselbe Frage von der anderen Seite: ein Rad-Ereignis darf nicht auf
     * einer veralteten Seitengröße rechnen, sonst scrollt die Liste über ihr
     * Ende hinaus und zeigt Leerraum. */
    catalog *cat = load_test_catalog();
    REQUIRE(cat != NULL);

    widget *lst = list_create(load_test_theme(), cat);
    REQUIRE(lst != NULL);
    list_set_items(lst, items, 12);
    lst->frame = rect_make(0, 0, 120, 4 * g_theme.menu_item_h);

    /* Ohne vorher zu zeichnen kräftig nach unten drehen. */
    CHECK(widget_event(lst, &(event){ .kind = EV_WHEEL, .x = 10, .y = 10, .wheel = -20 }));
    CHECK_EQ(list_top(lst), 8);      /* 12 Einträge, vier sichtbar */

    widget_destroy(lst);
    i18n_free(cat);
}

TEST(an_event_leaves_the_shared_model_consistent)
{
    /* Der Balken liest das Modell roh - er kommt nicht über einen Zugriff der
     * Liste, der es nebenbei nachziehen würde. Was ein Ereignis im Modell
     * hinterlässt, muss deshalb schon stimmen, nicht erst beim nächsten
     * Zugriff geradegerückt werden. */
    catalog *cat = load_test_catalog();
    REQUIRE(cat != NULL);

    widget *lst = list_create(load_test_theme(), cat);
    REQUIRE(lst != NULL);
    list_set_items(lst, items, 12);
    lst->frame = rect_make(0, 0, 120, 4 * g_theme.menu_item_h);

    widget *bar = scrollbar_create(load_test_theme(), cat, SCROLLBAR_VERTICAL,
                                   list_scroll(lst));
    REQUIRE(bar != NULL);

    /* Jetzt wird das Fenster höher gezogen - sechs Zeilen statt vier - und
     * ohne dazwischenliegendes Zeichnen gescrollt. */
    lst->frame = rect_make(0, 0, 120, 6 * g_theme.menu_item_h);
    CHECK(widget_event(lst, &(event){ .kind = EV_WHEEL, .x = 10, .y = 10, .wheel = -20 }));

    CHECK_EQ(scrollbar_model(bar)->value, 6);   /* 12 Einträge, sechs sichtbar */

    widget_destroy(bar);
    widget_destroy(lst);
    i18n_free(cat);
}

TEST(refilling_the_list_goes_back_to_the_top)
{
    catalog *cat = load_test_catalog();
    REQUIRE(cat != NULL);

    widget *lst = list_create(load_test_theme(), cat);
    REQUIRE(lst != NULL);
    lst->frame = rect_make(0, 0, 120, 4 * g_theme.menu_item_h);

    list_set_items(lst, items, 12);
    CHECK(widget_event(lst, &(event){ .kind = EV_WHEEL, .x = 10, .y = 10, .wheel = -5 }));
    CHECK(list_top(lst) > 0);

    list_set_items(lst, items, 12);
    CHECK_EQ(list_top(lst), 0);

    widget_destroy(lst);
    i18n_free(cat);
}

TEST(a_single_line_field_has_no_scroll_model)
{
    catalog *cat = load_test_catalog();
    REQUIRE(cat != NULL);

    widget *field = text_field_create(load_test_theme(), cat);
    REQUIRE(field != NULL);

    /* Es scrollt waagerecht und folgt dabei der Schreibmarke - da gibt es
     * nichts, was ein Balken anzeigen könnte. */
    CHECK(text_widget_scroll(field) == NULL);

    widget_destroy(field);
    i18n_free(cat);
}

TEST(the_wheel_scrolls_a_text_area)
{
    catalog *cat = load_test_catalog();
    REQUIRE(cat != NULL);

    widget *area = text_area_create(load_test_theme(), cat);
    REQUIRE(area != NULL);
    area->frame = rect_make(0, 0, 200, 4 * system12.size);
    text_widget_set_value(area, "eins\nzwei\ndrei\nvier\nfuenf\nsechs");

    scrollmodel *m = text_widget_scroll(area);
    REQUIRE(m != NULL);
    CHECK_EQ(m->total, 6);

    /* Nach dem Setzen steht die Schreibmarke am Ende, und die Ansicht folgt
     * ihr - das Feld steht also unten. Von dort mit dem Rad nach oben. */
    REQUIRE(scroll_max(m) >= 2);
    int before = m->value;

    CHECK(widget_event(area, &(event){ .kind = EV_WHEEL, .x = 10, .y = 10, .wheel = 2 }));
    CHECK_EQ(text_widget_top_line(area), before - 2);

    widget_destroy(area);
    i18n_free(cat);
}

TEST(a_text_area_notices_a_new_height_alone)
{
    /* Wird ein Feld nur niedriger gezogen, bleibt der Zeilenumbruch gültig -
     * er hängt an der Breite. Die Seitengröße hängt aber an der Höhe, und ein
     * Balken daneben zeigt sonst einen Schieber falscher Länge. */
    catalog *cat = load_test_catalog();
    REQUIRE(cat != NULL);

    widget *area = text_area_create(load_test_theme(), cat);
    REQUIRE(area != NULL);
    area->frame = rect_make(0, 0, 200, 6 * system12.size);
    text_widget_set_value(area, "eins\nzwei\ndrei\nvier\nfuenf\nsechs");

    widget *bar = scrollbar_create(load_test_theme(), cat, SCROLLBAR_VERTICAL,
                                   text_widget_scroll(area));
    REQUIRE(bar != NULL);
    int before = scrollbar_model(bar)->page;

    bitmap bm;
    REQUIRE(bitmap_init(&bm, 220, 220));
    gc g;
    gc_init(&g, &bm);

    /* Nur die Höhe ändern, die Breite bleibt. */
    area->frame = rect_make(0, 0, 200, 2 * system12.size);
    widget_draw(area, &g);

    int raw = scrollbar_model(bar)->page;
    CHECK(raw != before);                             /* sonst prüft der Test nichts */
    CHECK_EQ(raw, text_widget_scroll(area)->page);    /* und zwar dasselbe */

    bitmap_free(&bm);
    widget_destroy(bar);
    widget_destroy(area);
    i18n_free(cat);
}

TEST(the_wheel_leaves_a_single_line_field_alone)
{
    /* Das einzeilige Feld scrollt waagerecht und folgt dabei der
     * Schreibmarke. Ein Rad-Ereignis gehört ihm nicht - es muss zu dem
     * durchfallen, was darunter liegt. */
    catalog *cat = load_test_catalog();
    REQUIRE(cat != NULL);

    widget *field = text_field_create(load_test_theme(), cat);
    REQUIRE(field != NULL);
    field->frame = rect_make(0, 0, 200, 2 * system12.size);

    CHECK(!widget_event(field, &(event){ .kind = EV_WHEEL, .x = 10, .y = 10, .wheel = 2 }));

    widget_destroy(field);
    i18n_free(cat);
}

TEST(the_wheel_outside_a_text_area_is_left_alone)
{
    catalog *cat = load_test_catalog();
    REQUIRE(cat != NULL);

    widget *area = text_area_create(load_test_theme(), cat);
    REQUIRE(area != NULL);
    area->frame = rect_make(0, 0, 200, 4 * system12.size);
    text_widget_set_value(area, "eins\nzwei\ndrei\nvier\nfuenf\nsechs");

    int before = text_widget_top_line(area);
    CHECK(!widget_event(area, &(event){ .kind = EV_WHEEL, .x = 500, .y = 500, .wheel = 2 }));
    CHECK_EQ(text_widget_top_line(area), before);

    widget_destroy(area);
    i18n_free(cat);
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

TEST(a_list_with_a_bar_beside_it)
{
    /* So sieht das Ergebnis aus, wenn beide zusammenarbeiten: die Liste zeigt
     * vier von zwölf Einträgen, der Schieber ist entsprechend ein Drittel der
     * Rinne lang und steht dort, wo die Liste steht. */
    catalog *cat = load_test_catalog();
    REQUIRE(cat != NULL);

    int h = 6 * g_theme.menu_item_h;

    widget *lst = list_create(load_test_theme(), cat);
    REQUIRE(lst != NULL);
    list_set_items(lst, items, 12);
    lst->frame = rect_make(8, 8, 140, h);
    list_select(lst, 7);

    widget *bar = scrollbar_create(load_test_theme(), cat, SCROLLBAR_VERTICAL,
                                   list_scroll(lst));
    REQUIRE(bar != NULL);
    bar->frame = rect_make(8 + 140 - 1, 8, g_theme.scrollbar_w, h);

    bitmap bm;
    REQUIRE(bitmap_init(&bm, 180, h + 16));
    gc g;
    gc_init(&g, &bm);
    g.pat = PAT_WHITE;
    gfx_fill_rect(&g, rect_make(0, 0, 180, h + 16));

    widget_draw(lst, &g);
    widget_draw(bar, &g);

    CHECK(golden_check("list_with_scrollbar", &bm));

    bitmap_free(&bm);
    widget_destroy(bar);
    widget_destroy(lst);
    i18n_free(cat);
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

    RUN(a_list_and_its_bar_share_one_position);
    RUN(drawing_the_list_brings_its_model_up_to_date);
    RUN(the_list_brings_its_model_up_to_date_before_an_event);
    RUN(an_event_leaves_the_shared_model_consistent);
    RUN(refilling_the_list_goes_back_to_the_top);
    RUN(a_single_line_field_has_no_scroll_model);
    RUN(the_wheel_scrolls_a_text_area);
    RUN(a_text_area_notices_a_new_height_alone);
    RUN(the_wheel_leaves_a_single_line_field_alone);
    RUN(the_wheel_outside_a_text_area_is_left_alone);

    RUN(a_scrollbar_looks_like_a_scrollbar);
    RUN(a_list_with_a_bar_beside_it);

    return test_summary();
}
