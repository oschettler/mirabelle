/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Das Listen-Widget aus M8.
 *
 * Panel und Layout gibt es noch nicht (M8 baut sie parallel); die Tests hier
 * setzen frame also von Hand, statt es errechnen zu lassen - wie in
 * test_widget.c.
 */
#include "test.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "core/i18n.h"
#include "gfx/bitmap.h"
#include "gfx/draw.h"
#include "gfx/font.h"
#include "gfx/text.h"
#include "plat/plat.h"
#include "support/golden.h"
#include "ui/theme.h"
#include "ui/widget.h"

extern const font system12;

#ifndef PDA_DATA_DIR
#define PDA_DATA_DIR "data"
#endif

/* --- Laden von Thema und Katalog -------------------------------------------
 *
 * Ein Widget speichert von seinem Thema nur einen Zeiger (widget.h), keine
 * Kopie. Käme das Thema aus einer Hilfsfunktion zurück, die eine lokale
 * theme-Variable auf ihrem eigenen Stapel hält, wäre dieser Zeiger nach der
 * Rückkehr der Hilfsfunktion baumelnd. Deshalb liegt das Testthema hier
 * statisch: eine einzige, für den ganzen Testlauf gültige Adresse, egal aus
 * welcher Funktion heraus sie geholt wird. */
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

/* Vier echte Katalogschlüssel, wie sie eine Ansichtsliste zeigen würde. */
static const char *const FOUR_ITEMS[4] = {
    "menu.view.tasks", "menu.view.calendar", "menu.view.contacts", "menu.view.notes"
};

/* --- Neubefüllung -------------------------------------------------------------- */

TEST(set_items_selects_first_or_none)
{
    const theme *th  = load_test_theme();
    catalog     *cat = load_test_catalog();
    REQUIRE(cat);

    widget *w = list_create(th, cat);
    REQUIRE(w);

    list_set_items(w, FOUR_ITEMS, 4);
    CHECK_EQ(list_count(w), 4);
    CHECK_EQ(list_selected(w), 0);
    CHECK_EQ(list_top(w), 0);

    list_set_items(w, FOUR_ITEMS, 0);
    CHECK_EQ(list_count(w), 0);
    CHECK_EQ(list_selected(w), -1);

    widget_destroy(w);
    i18n_free(cat);
}

TEST(select_out_of_range_keeps_selection)
{
    const theme *th  = load_test_theme();
    catalog     *cat = load_test_catalog();
    REQUIRE(cat);

    widget *w = list_create(th, cat);
    REQUIRE(w);
    list_set_items(w, FOUR_ITEMS, 4);

    list_select(w, 2);
    CHECK_EQ(list_selected(w), 2);

    list_select(w, 99);
    CHECK_EQ(list_selected(w), 2);   /* unverändert */

    list_select(w, -5);
    CHECK_EQ(list_selected(w), 2);   /* unverändert */

    widget_destroy(w);
    i18n_free(cat);
}

/* --- Maus ------------------------------------------------------------------------ */

TEST(click_selects_hit_item)
{
    const theme *th  = load_test_theme();
    catalog     *cat = load_test_catalog();
    REQUIRE(cat);

    widget *w = list_create(th, cat);
    REQUIRE(w);
    list_set_items(w, FOUR_ITEMS, 4);
    /* Absichtlich mehr Platz als vier Zeilen, damit sich auch prüfen lässt,
     * dass ein Klick unterhalb des letzten Eintrags nichts ändert. */
    w->frame = rect_make(0, 0, 120, 6 * th->menu_item_h);

    event hit = { .kind = EV_MOUSE_DOWN, .button = 1, .clicks = 1,
                  .x = 10, .y = 2 * th->menu_item_h + 3 };
    CHECK(widget_event(w, &hit));
    CHECK_EQ(list_selected(w), 2);

    event below = { .kind = EV_MOUSE_DOWN, .button = 1, .clicks = 1,
                     .x = 10, .y = 5 * th->menu_item_h + 3 };
    CHECK(widget_event(w, &below));
    CHECK_EQ(list_selected(w), 2);   /* unverändert */

    widget_destroy(w);
    i18n_free(cat);
}

TEST(double_click_selects_and_opens)
{
    const theme *th  = load_test_theme();
    catalog     *cat = load_test_catalog();
    REQUIRE(cat);

    widget *w = list_create(th, cat);
    REQUIRE(w);
    list_set_items(w, FOUR_ITEMS, 4);
    w->frame = rect_make(0, 0, 120, 4 * th->menu_item_h);

    event single = { .kind = EV_MOUSE_DOWN, .button = 1, .clicks = 1,
                      .x = 10, .y = 1 * th->menu_item_h + 3 };
    CHECK(widget_event(w, &single));
    CHECK_EQ(list_selected(w), 1);
    CHECK(!list_was_opened(w));

    event dbl = { .kind = EV_MOUSE_DOWN, .button = 1, .clicks = 2,
                  .x = 10, .y = 3 * th->menu_item_h + 3 };
    CHECK(widget_event(w, &dbl));
    CHECK_EQ(list_selected(w), 3);
    CHECK(list_was_opened(w));
    CHECK(!list_was_opened(w));   /* einmal auslesen setzt zurück */

    widget_destroy(w);
    i18n_free(cat);
}

TEST(wheel_scrolls_without_changing_selection)
{
    const theme *th  = load_test_theme();
    catalog     *cat = load_test_catalog();
    REQUIRE(cat);

    const char *many[20];
    for (int i = 0; i < 20; i++) many[i] = FOUR_ITEMS[i % 4];

    widget *w = list_create(th, cat);
    REQUIRE(w);
    list_set_items(w, many, 20);
    w->frame = rect_make(0, 0, 120, 4 * th->menu_item_h);   /* vier Zeilen sichtbar */

    list_select(w, 10);
    CHECK_EQ(list_selected(w), 10);
    int top_after_select = list_top(w);

    event up2 = { .kind = EV_WHEEL, .x = 10, .y = 10, .wheel = 2 };
    CHECK(widget_event(w, &up2));
    CHECK_EQ(list_top(w), top_after_select - 2);
    CHECK_EQ(list_selected(w), 10);

    event down3 = { .kind = EV_WHEEL, .x = 10, .y = 10, .wheel = -3 };
    CHECK(widget_event(w, &down3));
    CHECK_EQ(list_top(w), top_after_select + 1);
    CHECK_EQ(list_selected(w), 10);

    event far_up = { .kind = EV_WHEEL, .x = 10, .y = 10, .wheel = 100 };
    CHECK(widget_event(w, &far_up));
    CHECK_EQ(list_top(w), 0);
    CHECK_EQ(list_selected(w), 10);

    event far_down = { .kind = EV_WHEEL, .x = 10, .y = 10, .wheel = -100 };
    CHECK(widget_event(w, &far_down));
    CHECK_EQ(list_top(w), 20 - 4);   /* Maximum: Anzahl minus sichtbare Zeilen */
    CHECK_EQ(list_selected(w), 10);

    widget_destroy(w);
    i18n_free(cat);
}

/* --- Tastatur ---------------------------------------------------------------------- */

TEST(return_opens_only_when_focused)
{
    const theme *th  = load_test_theme();
    catalog     *cat = load_test_catalog();
    REQUIRE(cat);

    widget *w = list_create(th, cat);
    REQUIRE(w);
    list_set_items(w, FOUR_ITEMS, 4);
    w->frame = rect_make(0, 0, 120, 4 * th->menu_item_h);

    event ret = { .kind = EV_KEY_DOWN, .key = KEY_RETURN };
    CHECK(!widget_event(w, &ret));
    CHECK(!list_was_opened(w));

    w->focused = true;
    CHECK(widget_event(w, &ret));
    CHECK(list_was_opened(w));
    CHECK(!list_was_opened(w));

    widget_destroy(w);
    i18n_free(cat);
}

TEST(arrow_keys_move_without_wrapping)
{
    const theme *th  = load_test_theme();
    catalog     *cat = load_test_catalog();
    REQUIRE(cat);

    widget *w = list_create(th, cat);
    REQUIRE(w);
    list_set_items(w, FOUR_ITEMS, 4);
    w->frame   = rect_make(0, 0, 120, 4 * th->menu_item_h);
    w->focused = true;

    event up   = { .kind = EV_KEY_DOWN, .key = KEY_UP };
    event down = { .kind = EV_KEY_DOWN, .key = KEY_DOWN };

    /* Am Anfang bleibt Pfeil-auf ohne Wirkung - kein Umbruch ans Ende. */
    CHECK(widget_event(w, &up));
    CHECK_EQ(list_selected(w), 0);

    CHECK(widget_event(w, &down));
    CHECK_EQ(list_selected(w), 1);
    CHECK(widget_event(w, &down));
    CHECK_EQ(list_selected(w), 2);
    CHECK(widget_event(w, &down));
    CHECK_EQ(list_selected(w), 3);

    /* Am Ende bleibt Pfeil-ab ohne Wirkung - kein Umbruch an den Anfang. */
    CHECK(widget_event(w, &down));
    CHECK_EQ(list_selected(w), 3);

    CHECK(widget_event(w, &up));
    CHECK_EQ(list_selected(w), 2);

    widget_destroy(w);
    i18n_free(cat);
}

/* Ein SPRUNG ans Ende ist der Fall, in dem sich ein Rechenfehler in der
 * Sichtbarkeitsprüfung zeigt. Beim schrittweisen Wandern fällt er nicht auf,
 * weil die Bedingung dort am Listenende gar nicht mehr auslöst - der letzte
 * Eintrag ist dann längst im Bild. Nur wer springt, sieht die leere Zeile. */
TEST(jumping_to_the_end_leaves_no_blank_row)
{
    const theme *th  = load_test_theme();
    catalog     *cat = load_test_catalog();
    REQUIRE(cat);

    const char *many[20];
    for (int i = 0; i < 20; i++) many[i] = FOUR_ITEMS[i % 4];

    widget *w = list_create(th, cat);
    REQUIRE(w);
    list_set_items(w, many, 20);
    w->frame   = rect_make(0, 0, 120, 4 * th->menu_item_h);
    w->focused = true;

    int rows = w->frame.h / th->menu_item_h;

    event end = { .kind = EV_KEY_DOWN, .key = KEY_END };
    CHECK(widget_event(w, &end));
    CHECK_EQ(list_selected(w), 19);
    CHECK_EQ(list_top(w), 20 - rows);

    /* Auch der direkte Weg über list_select, nicht nur die Taste. */
    list_select(w, 0);
    list_select(w, 19);
    CHECK_EQ(list_top(w), 20 - rows);

    /* Und eine Bildseite abwärts vom Anfang darf ebenfalls nicht überschießen. */
    list_select(w, 0);
    event pgdn = { .kind = EV_KEY_DOWN, .key = KEY_PAGE_DOWN };
    CHECK(widget_event(w, &pgdn));
    CHECK(list_top(w) <= 20 - rows);

    widget_destroy(w);
    i18n_free(cat);
}

TEST(home_and_end_jump_to_the_ends)
{
    const theme *th  = load_test_theme();
    catalog     *cat = load_test_catalog();
    REQUIRE(cat);

    widget *w = list_create(th, cat);
    REQUIRE(w);
    list_set_items(w, FOUR_ITEMS, 4);
    w->frame   = rect_make(0, 0, 120, 4 * th->menu_item_h);
    w->focused = true;

    list_select(w, 1);

    event end = { .kind = EV_KEY_DOWN, .key = KEY_END };
    CHECK(widget_event(w, &end));
    CHECK_EQ(list_selected(w), 3);

    event home = { .kind = EV_KEY_DOWN, .key = KEY_HOME };
    CHECK(widget_event(w, &home));
    CHECK_EQ(list_selected(w), 0);

    widget_destroy(w);
    i18n_free(cat);
}

TEST(page_up_and_page_down_move_by_visible_page)
{
    const theme *th  = load_test_theme();
    catalog     *cat = load_test_catalog();
    REQUIRE(cat);

    const char *many[20];
    for (int i = 0; i < 20; i++) many[i] = FOUR_ITEMS[i % 4];

    widget *w = list_create(th, cat);
    REQUIRE(w);
    list_set_items(w, many, 20);
    w->frame   = rect_make(0, 0, 120, 3 * th->menu_item_h);   /* drei Zeilen sichtbar */
    w->focused = true;

    event page_down = { .kind = EV_KEY_DOWN, .key = KEY_PAGE_DOWN };
    event page_up   = { .kind = EV_KEY_DOWN, .key = KEY_PAGE_UP };

    CHECK(widget_event(w, &page_down));
    CHECK_EQ(list_selected(w), 3);

    CHECK(widget_event(w, &page_down));
    CHECK_EQ(list_selected(w), 6);

    CHECK(widget_event(w, &page_up));
    CHECK_EQ(list_selected(w), 3);

    /* Am Ende klemmt eine Seite genauso wie eine einzelne Zeile. */
    list_select(w, 19);
    CHECK(widget_event(w, &page_down));
    CHECK_EQ(list_selected(w), 19);

    widget_destroy(w);
    i18n_free(cat);
}

/* --- Der wichtigste Test: die Auswahl bleibt immer sichtbar ----------------------- */

TEST(selection_always_stays_within_the_visible_rows)
{
    const theme *th  = load_test_theme();
    catalog     *cat = load_test_catalog();
    REQUIRE(cat);

    const char *many[20];
    for (int i = 0; i < 20; i++) many[i] = FOUR_ITEMS[i % 4];

    widget *w = list_create(th, cat);
    REQUIRE(w);
    list_set_items(w, many, 20);
    w->frame   = rect_make(0, 0, 120, 4 * th->menu_item_h);   /* vier Zeilen sichtbar */
    w->focused = true;

    int rows = w->frame.h / th->menu_item_h;
    event down = { .kind = EV_KEY_DOWN, .key = KEY_DOWN };
    event up   = { .kind = EV_KEY_DOWN, .key = KEY_UP };

    for (int i = 0; i < 19; i++) {
        CHECK(widget_event(w, &down));
        int sel = list_selected(w);
        int top = list_top(w);
        CHECK_EQ(sel, i + 1);
        CHECK(sel >= top);
        CHECK(sel < top + rows);

        /* Und es wird kein Platz verschenkt: solange die Liste länger ist als
         * die Ansicht, darf unten keine leere Zeile stehen. Sichtbarkeit
         * allein genügt als Zusage nicht - eine Ansicht, die eine Zeile zu
         * weit scrollt, erfüllt sie und sieht trotzdem falsch aus. */
        CHECK(top <= 20 - rows);
    }

    /* Am Ende der Liste steht der letzte Eintrag in der letzten Zeile. */
    CHECK_EQ(list_top(w), 20 - rows);

    for (int i = 0; i < 19; i++) {
        CHECK(widget_event(w, &up));
        int sel = list_selected(w);
        int top = list_top(w);
        CHECK_EQ(sel, 18 - i);
        CHECK(sel >= top);
        CHECK(sel < top + rows);
    }

    widget_destroy(w);
    i18n_free(cat);
}

/* --- Randfälle, die nicht abstürzen dürfen ----------------------------------------- */

TEST(list_without_layout_does_not_crash)
{
    const theme *th  = load_test_theme();
    catalog     *cat = load_test_catalog();
    REQUIRE(cat);

    widget *w = list_create(th, cat);
    REQUIRE(w);
    list_set_items(w, FOUR_ITEMS, 4);
    w->focused = true;
    /* frame bleibt (0,0,0,0) - so wie vor dem ersten Layoutlauf. */

    event click = { .kind = EV_MOUSE_DOWN, .button = 1, .clicks = 1, .x = 10, .y = 10 };
    event wheel = { .kind = EV_WHEEL, .x = 10, .y = 10, .wheel = 3 };
    event down  = { .kind = EV_KEY_DOWN, .key = KEY_DOWN };
    event up    = { .kind = EV_KEY_DOWN, .key = KEY_UP };
    event pdown = { .kind = EV_KEY_DOWN, .key = KEY_PAGE_DOWN };
    event pup   = { .kind = EV_KEY_DOWN, .key = KEY_PAGE_UP };
    event home  = { .kind = EV_KEY_DOWN, .key = KEY_HOME };
    event end   = { .kind = EV_KEY_DOWN, .key = KEY_END };
    event ret   = { .kind = EV_KEY_DOWN, .key = KEY_RETURN };

    widget_event(w, &click);
    widget_event(w, &wheel);
    widget_event(w, &down);
    widget_event(w, &up);
    widget_event(w, &pdown);
    widget_event(w, &pup);
    widget_event(w, &home);
    widget_event(w, &end);
    widget_event(w, &ret);

    CHECK(list_selected(w) >= -1 && list_selected(w) < 4);

    widget_destroy(w);
    i18n_free(cat);
}

TEST(empty_list_does_not_crash)
{
    const theme *th  = load_test_theme();
    catalog     *cat = load_test_catalog();
    REQUIRE(cat);

    widget *w = list_create(th, cat);
    REQUIRE(w);
    w->frame   = rect_make(0, 0, 120, 4 * th->menu_item_h);
    w->focused = true;
    /* list_set_items nie aufgerufen - genau wie eine gerade erst angelegte,
     * noch nicht befüllte Liste. */

    event click = { .kind = EV_MOUSE_DOWN, .button = 1, .clicks = 1, .x = 10, .y = 10 };
    event wheel = { .kind = EV_WHEEL, .x = 10, .y = 10, .wheel = 3 };
    event down  = { .kind = EV_KEY_DOWN, .key = KEY_DOWN };
    event pdown = { .kind = EV_KEY_DOWN, .key = KEY_PAGE_DOWN };
    event home  = { .kind = EV_KEY_DOWN, .key = KEY_HOME };
    event end   = { .kind = EV_KEY_DOWN, .key = KEY_END };
    event ret   = { .kind = EV_KEY_DOWN, .key = KEY_RETURN };

    widget_event(w, &click);
    widget_event(w, &wheel);
    widget_event(w, &down);
    widget_event(w, &pdown);
    widget_event(w, &home);
    widget_event(w, &end);
    widget_event(w, &ret);

    CHECK_EQ(list_count(w), 0);
    CHECK_EQ(list_selected(w), -1);
    CHECK(!list_was_opened(w));

    widget_destroy(w);
    i18n_free(cat);
}

/* --- Sollbild ---------------------------------------------------------------------- */

TEST(golden_list_four_items)
{
    const theme *th  = load_test_theme();
    catalog     *cat = load_test_catalog();
    REQUIRE(cat);

    widget *w = list_create(th, cat);
    REQUIRE(w);
    list_set_items(w, FOUR_ITEMS, 4);

    int lw, lh;
    widget_measure(w, &lw, &lh);

    /* Rand um die Liste, damit auch der zweite, zwei Pixel weiter außen
     * liegende Rahmen des Fokus vollständig im Bild landet. */
    int margin    = 4;
    int content_w = lw + 2 * margin;
    int content_h = lh + 2 * margin;
    CHECK(content_w * content_h <= GOLDEN_MAX_P1_PIXELS);

    w->frame = rect_make(margin, margin, lw, lh);
    list_select(w, 1);
    w->focused = true;

    bitmap bm;
    REQUIRE(bitmap_init(&bm, content_w, content_h));

    gc g;
    gc_init(&g, &bm);
    g.pat  = PAT_WHITE;
    g.mode = GFX_COPY;
    gfx_clear(&g);

    widget_draw(w, &g);

    CHECK(golden_check("list_four_items", &bm));

    bitmap_free(&bm);
    widget_destroy(w);
    i18n_free(cat);
}

int main(void)
{
    RUN(set_items_selects_first_or_none);
    RUN(select_out_of_range_keeps_selection);

    RUN(click_selects_hit_item);
    RUN(double_click_selects_and_opens);
    RUN(wheel_scrolls_without_changing_selection);

    RUN(return_opens_only_when_focused);
    RUN(arrow_keys_move_without_wrapping);
    RUN(jumping_to_the_end_leaves_no_blank_row);
    RUN(home_and_end_jump_to_the_ends);
    RUN(page_up_and_page_down_move_by_visible_page);

    RUN(selection_always_stays_within_the_visible_rows);

    RUN(list_without_layout_does_not_crash);
    RUN(empty_list_does_not_crash);

    RUN(golden_list_four_items);

    return test_summary();
}
