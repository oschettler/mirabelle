/* Der Monatskalender, siehe app/monthview.h.
 *
 * Ein Monatsraster hat wenige, aber unangenehme Randfälle: der Monat, der am
 * Sonntag beginnt und über sechs Wochenzeilen reicht; der Wechsel in einen
 * kürzeren Monat; der Wochenbeginn, der aus dem Katalog kommt.
 */
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/monthview.h"
#include "core/date.h"
#include "core/i18n.h"
#include "gfx/bitmap.h"
#include "gfx/draw.h"
#include "support/golden.h"
#include "ui/theme.h"

#ifndef PDA_DATA_DIR
#define PDA_DATA_DIR "data"
#endif

static theme g_theme;

static const theme *test_theme(void)
{
    static bool loaded = false;
    if (loaded) return &g_theme;

    char path[512], err[256] = "";
    snprintf(path, sizeof path, "%s/themes/desktop.theme", PDA_DATA_DIR);
    if (!theme_load(&g_theme, path, err, sizeof err)) theme_defaults(&g_theme);
    loaded = true;
    return &g_theme;
}

static catalog *load_cat(void)
{
    char path[512], err[256] = "";
    snprintf(path, sizeof path, "%s/lang/de.strings", PDA_DATA_DIR);

    catalog *c = i18n_load(path, err, sizeof err);
    if (!c) printf("  Katalog nicht ladbar: %s\n", err);
    return c;
}

/* Ein Katalog mit eigenem Wochenbeginn, für den Test, dass er wirklich aus der
 * Datei kommt. */
static catalog *catalog_with(const char *lines)
{
    const char *path = "/tmp/pda_monthview.strings";
    FILE       *fp   = fopen(path, "wb");
    if (!fp) return NULL;
    fputs(lines, fp);
    fclose(fp);

    char     err[256] = "";
    catalog *c = i18n_load(path, err, sizeof err);
    if (!c) printf("  %s\n", err);
    return c;
}

static date D(int y, int m, int d)
{
    date x = { y, m, d };
    return x;
}

static bool same(date a, date b) { return date_compare(a, b) == 0; }

/* --- Anlegen und Blättern ------------------------------------------------------- */

TEST(a_calendar_shows_the_month_of_the_date_it_got)
{
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    widget *w = monthview_create(test_theme(), cat, D(2026, 3, 15));
    REQUIRE(w != NULL);

    date m = monthview_month(w);
    CHECK_EQ(m.year, 2026);
    CHECK_EQ(m.month, 3);
    CHECK_EQ(m.day, 1);                       /* der Monat, nicht der Tag */
    CHECK(same(monthview_selected(w), D(2026, 3, 15)));

    widget_destroy(w);
    i18n_free(cat);
}

TEST(an_impossible_date_is_refused)
{
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    CHECK(monthview_create(test_theme(), cat, D(2025, 2, 29)) == NULL);
    i18n_free(cat);
}

TEST(paging_carries_the_selection_along)
{
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    widget *w = monthview_create(test_theme(), cat, D(2026, 3, 15));
    REQUIRE(w != NULL);

    monthview_show_month(w, 1);
    CHECK(same(monthview_selected(w), D(2026, 4, 15)));
    CHECK_EQ(monthview_month(w).day, 1);       /* der Monat, nicht der Tag */

    monthview_show_month(w, -2);
    CHECK(same(monthview_selected(w), D(2026, 2, 15)));

    widget_destroy(w);
    i18n_free(cat);
}

TEST(paging_into_a_shorter_month_keeps_a_valid_day)
{
    /* Vom 31. Januar einen Monat weiter: der 31. Februar existiert nicht.
     * Ohne Kürzung stünde die Auswahl auf einem Tag, den das Raster gar nicht
     * zeichnet. */
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    widget *w = monthview_create(test_theme(), cat, D(2026, 1, 31));
    REQUIRE(w != NULL);

    monthview_show_month(w, 1);
    CHECK(same(monthview_selected(w), D(2026, 2, 28)));
    CHECK(date_valid(monthview_selected(w)));
    CHECK_EQ(monthview_month(w).day, 1);

    /* Und im Schaltjahr einen Tag weiter. */
    CHECK(monthview_select(w, D(2024, 1, 31)));
    monthview_show_month(w, 1);
    CHECK(same(monthview_selected(w), D(2024, 2, 29)));

    widget_destroy(w);
    i18n_free(cat);
}

TEST(selecting_a_date_in_another_month_pages_there)
{
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    widget *w = monthview_create(test_theme(), cat, D(2026, 3, 15));
    REQUIRE(w != NULL);

    CHECK(monthview_select(w, D(2026, 12, 24)));
    CHECK_EQ(monthview_month(w).month, 12);
    CHECK(same(monthview_selected(w), D(2026, 12, 24)));

    CHECK(!monthview_select(w, D(2026, 2, 30)));
    CHECK(same(monthview_selected(w), D(2026, 12, 24)));   /* unverändert */

    widget_destroy(w);
    i18n_free(cat);
}

/* --- Markierungen ----------------------------------------------------------------- */

TEST(marks_apply_only_to_the_shown_month)
{
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    widget *w = monthview_create(test_theme(), cat, D(2026, 3, 1));
    REQUIRE(w != NULL);

    /* Erst nur fremde Monate. Der Aufrufer soll seine Terminliste nicht
     * vorher aussieben müssen - deshalb ist das kein Fehler, sondern wird
     * übergangen. Übergangen heißt aber: es darf nichts markiert werden. */
    monthview_mark(w, D(2026, 4, 5));    /* anderer Monat */
    monthview_mark(w, D(2025, 3, 7));    /* anderes Jahr */
    monthview_mark(w, D(2026, 2, 30));   /* den Tag gibt es nicht */

    for (int day = 1; day <= 31; day++)
        if (monthview_is_marked(w, day)) {
            printf("  Tag %d ist markiert, obwohl nichts im Monat liegt\n", day);
            CHECK(false);
            break;
        }

    monthview_mark(w, D(2026, 3, 5));
    CHECK(monthview_is_marked(w, 5));
    CHECK(!monthview_is_marked(w, 4));

    /* Außerhalb des Bereichs liegt im Speicher das nächste Feld der Struktur.
     * Ohne Grenzprüfung läse dieser Aufruf es aus - und der Sanitizer sähe
     * nichts, weil es dieselbe Zuteilung ist. */
    w->focused = true;
    event ret = { .kind = EV_KEY_DOWN, .key = KEY_RETURN };
    CHECK(widget_event(w, &ret));

    CHECK(!monthview_is_marked(w, 0));
    CHECK(!monthview_is_marked(w, 32));
    CHECK(!monthview_is_marked(w, 99));

    widget_destroy(w);
    i18n_free(cat);
}

TEST(paging_forgets_the_marks)
{
    /* Ein anderer Monat, andere Termine. Die alten stehen zu lassen hieße,
     * Tage zu belegen, an denen nichts ist. */
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    widget *w = monthview_create(test_theme(), cat, D(2026, 3, 1));
    REQUIRE(w != NULL);

    monthview_mark(w, D(2026, 3, 5));
    CHECK(monthview_is_marked(w, 5));

    monthview_show_month(w, 1);
    CHECK(!monthview_is_marked(w, 5));

    /* Auch das Springen über monthview_select vergisst sie. */
    monthview_mark(w, D(2026, 4, 5));
    CHECK(monthview_is_marked(w, 5));
    CHECK(monthview_select(w, D(2026, 9, 1)));
    CHECK(!monthview_is_marked(w, 5));

    widget_destroy(w);
    i18n_free(cat);
}

/* --- Der Wochenbeginn kommt aus dem Katalog ---------------------------------------- */

static int column_of(widget *w, int day, const theme *th)
{
    /* Sucht die Spalte, in der die Tageszahl steht, indem der Klickpunkt
     * durchprobiert wird - derselbe Weg, den auch ein Nutzer nimmt.
     *
     * Vor jedem Versuch wird die Auswahl woandershin gesetzt. Sonst zählte
     * schon die Ausgangslage als Treffer, und die erste leere Zelle, die
     * nichts ändert, gälte als die gesuchte Spalte. */
    int other = day == 20 ? 21 : 20;

    for (int col = 0; col < 7; col++) {
        int x = w->frame.x + w->frame.w * col / 7 + 2;
        int y = w->frame.y + th->menu_item_h + 2;

        for (int row = 0; row < 6; row++) {
            date d = monthview_month(w);
            d.day  = other;
            monthview_select(w, d);

            event e = { .kind = EV_MOUSE_DOWN, .x = x,
                        .y = y + row * ((w->frame.h - th->menu_item_h) / 6) };
            widget_event(w, &e);
            if (monthview_selected(w).day == day) return col;
        }
    }
    return -1;
}

TEST(the_week_starts_where_the_catalog_says)
{
    /* Der 1. März 2026 ist ein Sonntag. Beginnt die Woche am Montag, steht er
     * ganz rechts; beginnt sie am Sonntag, ganz links. Im Code steht dazu
     * nichts - nur eine Zeile im Katalog. */
    catalog *mo = catalog_with("week.start = 1\nweekday.short = Mo Di Mi Do Fr Sa So\n");
    catalog *so = catalog_with("week.start = 7\nweekday.short = Mo Di Mi Do Fr Sa So\n");
    REQUIRE(mo && so);

    const theme *th = test_theme();

    widget *a = monthview_create(th, mo, D(2026, 3, 1));
    widget *b = monthview_create(th, so, D(2026, 3, 1));
    REQUIRE(a && b);

    a->frame = rect_make(0, 0, 210, th->menu_item_h * 7);
    b->frame = rect_make(0, 0, 210, th->menu_item_h * 7);

    CHECK_EQ(date_weekday(D(2026, 3, 1)), 6);   /* Sonntag */
    CHECK_EQ(column_of(a, 1, th), 6);           /* Montag zuerst: ganz rechts */
    CHECK_EQ(column_of(b, 1, th), 0);           /* Sonntag zuerst: ganz links */

    /* Und ein Monat, dessen Erster nicht auf den Wochenbeginn fällt. Bei
     * Sonntagsbeginn wird der Versatz dann rechnerisch negativ - wer ihn nicht
     * umbricht, verliert die erste Woche ganz. */
    CHECK(monthview_select(a, D(2026, 4, 1)));
    CHECK(monthview_select(b, D(2026, 4, 1)));
    CHECK_EQ(date_weekday(D(2026, 4, 1)), 2);   /* Mittwoch */
    CHECK_EQ(column_of(a, 1, th), 2);
    CHECK_EQ(column_of(b, 1, th), 3);

    widget_destroy(a);
    widget_destroy(b);
    i18n_free(mo);
    i18n_free(so);
}

/* --- Bedienung -------------------------------------------------------------------- */

static bool key(widget *w, int k)
{
    event e = { .kind = EV_KEY_DOWN, .key = k };
    return widget_event(w, &e);
}

TEST(a_short_month_has_no_days_past_its_end)
{
    /* Februar hat 28 Tage. Ein Raster, das bis 31 zeichnet, setzte dort drei
     * Zahlen hin, die es nicht gibt - und nur in kurzen Monaten. */
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    const theme *th = test_theme();
    widget      *w  = monthview_create(th, cat, D(2026, 2, 10));
    REQUIRE(w != NULL);
    w->frame = rect_make(0, 0, 210, th->menu_item_h * 7);

    /* Der 1. Februar 2026 ist ein Sonntag und steht damit in der letzten
     * Spalte der ersten Zeile. Danach füllen sich die Zeilen von Montag an:
     * 2 bis 8, 9 bis 15, 16 bis 22, 23 bis 28. Der 28. ist also der Samstag
     * der fünften Zeile, und die Zelle rechts daneben - dort stünde der 29. -
     * muss leer bleiben. 2026 ist kein Schaltjahr. */
    CHECK_EQ(date_weekday(D(2026, 2, 1)), 6);
    CHECK_EQ(column_of(w, 28, th), 5);

    int   cell_h = (w->frame.h - th->menu_item_h) / 6;
    event e = { .kind = EV_MOUSE_DOWN, .clicks = 1,
                .x = w->frame.w * 6 / 7 + 2,
                .y = th->menu_item_h + 4 * cell_h + cell_h / 2 };

    CHECK(monthview_select(w, D(2026, 2, 10)));
    CHECK(widget_event(w, &e));
    CHECK_EQ(monthview_selected(w).day, 10);   /* nichts ausgewählt, also unverändert */

    widget_destroy(w);
    i18n_free(cat);
}

TEST(the_arrow_keys_move_by_day_and_week)
{
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    widget *w = monthview_create(test_theme(), cat, D(2026, 3, 15));
    REQUIRE(w != NULL);
    w->focused = true;

    CHECK(key(w, KEY_RIGHT));
    CHECK_EQ(monthview_selected(w).day, 16);
    CHECK(key(w, KEY_LEFT));
    CHECK_EQ(monthview_selected(w).day, 15);

    CHECK(key(w, KEY_DOWN));
    CHECK_EQ(monthview_selected(w).day, 22);
    CHECK(key(w, KEY_UP));
    CHECK_EQ(monthview_selected(w).day, 15);

    widget_destroy(w);
    i18n_free(cat);
}

TEST(the_arrows_stop_at_the_edges_of_the_month)
{
    /* Geklemmt statt in den Nachbarmonat gesprungen: das Blättern ist eine
     * eigene Geste, und wer mit den Pfeilen wandert, will nicht unversehens
     * einen anderen Monat vor sich haben. */
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    widget *w = monthview_create(test_theme(), cat, D(2026, 3, 1));
    REQUIRE(w != NULL);
    w->focused = true;

    CHECK(key(w, KEY_LEFT));
    CHECK_EQ(monthview_selected(w).day, 1);
    CHECK_EQ(monthview_month(w).month, 3);

    CHECK(key(w, KEY_UP));
    CHECK_EQ(monthview_selected(w).day, 1);

    CHECK(key(w, KEY_END));
    CHECK_EQ(monthview_selected(w).day, 31);
    CHECK(key(w, KEY_RIGHT));
    CHECK_EQ(monthview_selected(w).day, 31);
    CHECK(key(w, KEY_DOWN));
    CHECK_EQ(monthview_selected(w).day, 31);

    CHECK(key(w, KEY_HOME));
    CHECK_EQ(monthview_selected(w).day, 1);

    widget_destroy(w);
    i18n_free(cat);
}

TEST(page_keys_and_the_wheel_change_the_month)
{
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    widget *w = monthview_create(test_theme(), cat, D(2026, 3, 15));
    REQUIRE(w != NULL);
    w->focused = true;
    w->frame   = rect_make(0, 0, 210, 140);

    CHECK(key(w, KEY_PAGE_DOWN));
    CHECK_EQ(monthview_month(w).month, 4);
    CHECK(key(w, KEY_PAGE_UP));
    CHECK_EQ(monthview_month(w).month, 3);

    event wheel = { .kind = EV_WHEEL, .x = 10, .y = 10, .wheel = 1 };
    CHECK(widget_event(w, &wheel));
    CHECK_EQ(monthview_month(w).month, 2);      /* nach oben heißt zurück */

    wheel.x = 500;
    CHECK(!widget_event(w, &wheel));
    CHECK_EQ(monthview_month(w).month, 2);

    widget_destroy(w);
    i18n_free(cat);
}

TEST(clicking_a_day_selects_it_and_double_click_opens)
{
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    const theme *th = test_theme();
    widget      *w  = monthview_create(th, cat, D(2026, 3, 1));
    REQUIRE(w != NULL);
    w->frame = rect_make(0, 0, 210, th->menu_item_h * 7);

    /* Der 1. März 2026 ist ein Sonntag, steht bei Wochenbeginn Montag also in
     * der letzten Spalte der ersten Zeile. */
    int  cell_h = (w->frame.h - th->menu_item_h) / 6;
    event e = { .kind = EV_MOUSE_DOWN, .clicks = 1,
                .x = w->frame.w * 6 / 7 + 2,
                .y = th->menu_item_h + cell_h / 2 };

    CHECK(widget_event(w, &e));
    CHECK_EQ(monthview_selected(w).day, 1);
    CHECK(!monthview_was_opened(w));

    e.clicks = 2;
    CHECK(widget_event(w, &e));
    CHECK(monthview_was_opened(w));
    CHECK(!monthview_was_opened(w));      /* der Merker wird beim Lesen gelöscht */

    /* Eine leere Zelle wählt nichts aus. */
    e.clicks = 1;
    e.x      = 2;                          /* Montag der ersten Zeile: leer */
    CHECK(widget_event(w, &e));
    CHECK_EQ(monthview_selected(w).day, 1);

    /* Und ein Klick weit daneben gehört dem Kalender nicht. */
    e.x = 900;
    e.y = 900;
    CHECK(!widget_event(w, &e));

    widget_destroy(w);
    i18n_free(cat);
}

TEST(return_opens_the_selected_day)
{
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    widget *w = monthview_create(test_theme(), cat, D(2026, 3, 15));
    REQUIRE(w != NULL);
    w->focused = true;

    CHECK(key(w, KEY_RETURN));
    CHECK(monthview_was_opened(w));

    /* Ohne Fokus gehört die Tastatur nicht dem Kalender. */
    w->focused = false;
    CHECK(!key(w, KEY_RETURN));
    CHECK(!monthview_was_opened(w));

    widget_destroy(w);
    i18n_free(cat);
}

TEST(a_frame_too_small_for_a_row_is_safe)
{
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    widget *w = monthview_create(test_theme(), cat, D(2026, 3, 15));
    REQUIRE(w != NULL);
    w->frame = rect_make(0, 0, 40, 8);

    bitmap bm;
    REQUIRE(bitmap_init(&bm, 60, 20));
    gc g;
    gc_init(&g, &bm);
    widget_draw(w, &g);

    event e = { .kind = EV_MOUSE_DOWN, .clicks = 1, .x = 5, .y = 5 };
    CHECK(widget_event(w, &e));

    bitmap_free(&bm);
    widget_destroy(w);
    i18n_free(cat);
}

/* --- Aussehen ----------------------------------------------------------------------- */

TEST(a_month_that_needs_six_rows_still_fits)
{
    /* März 2026 beginnt an einem Sonntag und hat 31 Tage - bei Wochenbeginn
     * Montag reicht er über sechs Wochenzeilen. Ein Raster mit fünf Zeilen
     * verlöre hier die letzten Tage, und zwar nur in solchen Monaten. */
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    const theme *th = test_theme();
    widget      *w  = monthview_create(th, cat, D(2026, 3, 29));
    REQUIRE(w != NULL);

    monthview_mark(w, D(2026, 3, 5));
    monthview_mark(w, D(2026, 3, 17));
    monthview_mark(w, D(2026, 3, 31));

    w->frame = rect_make(4, 4, 217, th->menu_item_h * 7);

    bitmap bm;
    REQUIRE(bitmap_init(&bm, 225, th->menu_item_h * 7 + 8));
    gc g;
    gc_init(&g, &bm);
    g.pat = PAT_WHITE;
    gfx_fill_rect(&g, rect_make(0, 0, bm.w, bm.h));

    widget_draw(w, &g);
    CHECK(golden_check("monthview_march_2026", &bm));

    bitmap_free(&bm);
    widget_destroy(w);
    i18n_free(cat);
}

int main(void)
{
    RUN(a_calendar_shows_the_month_of_the_date_it_got);
    RUN(an_impossible_date_is_refused);
    RUN(paging_carries_the_selection_along);
    RUN(paging_into_a_shorter_month_keeps_a_valid_day);
    RUN(selecting_a_date_in_another_month_pages_there);

    RUN(marks_apply_only_to_the_shown_month);
    RUN(paging_forgets_the_marks);

    RUN(the_week_starts_where_the_catalog_says);

    RUN(a_short_month_has_no_days_past_its_end);
    RUN(the_arrow_keys_move_by_day_and_week);
    RUN(the_arrows_stop_at_the_edges_of_the_month);
    RUN(page_keys_and_the_wheel_change_the_month);
    RUN(clicking_a_day_selects_it_and_double_click_opens);
    RUN(return_opens_the_selected_day);
    RUN(a_frame_too_small_for_a_row_is_safe);

    RUN(a_month_that_needs_six_rows_still_fits);

    return test_summary();
}
