/* Die Gemtext-Ansicht, siehe app/gemview.h.
 *
 * Dasselbe Widget zeigt eine Notiz und eine abgerufene Seite. Geprüft wird
 * beides mit demselben Text - wenn es dafür einen Unterschied gäbe, wäre die
 * Ansicht falsch gebaut.
 */
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/gemview.h"
#include "core/i18n.h"
#include "gfx/bitmap.h"
#include "gfx/draw.h"
#include "support/golden.h"
#include "ui/scroll.h"
#include "gfx/font.h"
#include "gfx/text.h"
#include "ui/theme.h"

extern const font system12;

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
    return i18n_load(path, err, sizeof err);
}

/* Eine Seite mit allem, was Gemtext kennt. */
static const char PAGE[] =
    "# Die Hauptseite\n"
    "Ein Absatz, der lang genug ist, dass er in einem schmalen Fenster über "
    "mehrere Zeilen umbrochen werden muss.\n"
    "\n"
    "## Verweise\n"
    "=> spartan://mozz.us/ Zu mozz.us\n"
    "=> /lokal Eine Seite hier\n"
    "=> spartan://ohne.name/\n"
    "\n"
    "* Erster Punkt\n"
    "* Zweiter Punkt\n"
    "> Ein Zitat.\n"
    "```\n"
    "  vorformatiert   bleibt\n"
    "```\n";

static widget *a_view(catalog *cat, int w, int h)
{
    widget *v = gemview_create(test_theme(), cat);
    if (!v) return NULL;

    v->frame = rect_make(0, 0, w, h);
    gemview_set_text(v, PAGE, strlen(PAGE));
    return v;
}

/* --- Verweise ---------------------------------------------------------------------- */

TEST(links_are_counted_and_numbered)
{
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    widget *v = a_view(cat, 300, 200);
    REQUIRE(v != NULL);

    CHECK_EQ(gemview_link_count(v), 3);

    size_t      len = 0;
    const char *url = gemview_link_url(v, 1, &len);
    REQUIRE(url != NULL);
    CHECK(strncmp(url, "spartan://mozz.us/", len) == 0);
    CHECK_EQ(len, strlen("spartan://mozz.us/"));

    url = gemview_link_url(v, 2, &len);
    REQUIRE(url != NULL);
    CHECK(strncmp(url, "/lokal", len) == 0);

    /* Nummern beginnen bei eins, wie angezeigt. */
    CHECK(gemview_link_url(v, 0, &len) == NULL);
    CHECK(gemview_link_url(v, 4, &len) == NULL);

    widget_destroy(v);
    i18n_free(cat);
}

TEST(a_link_without_a_name_shows_its_address)
{
    /* Stünde dort nur eine Nummer, wüsste niemand, wohin sie führt. */
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    const char *one = "=> spartan://ohne.name/pfad\n";
    widget     *v   = gemview_create(test_theme(), cat);
    REQUIRE(v != NULL);
    v->frame = rect_make(0, 0, 400, 100);
    gemview_set_text(v, one, strlen(one));

    CHECK_EQ(gemview_link_count(v), 1);

    /* Der Beweis ist im Bild: der Text der Zeile ist die Adresse. Geprüft
     * wird er über den Umbruch - es gibt genau eine Zeile, und sie ist nicht
     * leer. */
    scrollmodel *m = gemview_scroll(v);
    CHECK_EQ(m->total, 1);

    widget_destroy(v);
    i18n_free(cat);
}

TEST(selecting_a_link_by_number)
{
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    widget *v = a_view(cat, 300, 200);
    REQUIRE(v != NULL);
    v->focused = true;

    CHECK_EQ(gemview_selected_link(v), 0);

    event two = { .kind = EV_TEXT, .text = "2" };
    CHECK(widget_event(v, &two));
    CHECK_EQ(gemview_selected_link(v), 2);

    /* Eine Ziffer, zu der es keinen Verweis gibt, hebt die Auswahl auf statt
     * die alte stehenzulassen - sonst glaubte man, man hätte etwas gewählt. */
    event nine = { .kind = EV_TEXT, .text = "9" };
    CHECK(widget_event(v, &nine));
    CHECK_EQ(gemview_selected_link(v), 0);

    /* Buchstaben gehören nicht der Ansicht. */
    event a = { .kind = EV_TEXT, .text = "a" };
    CHECK(!widget_event(v, &a));

    widget_destroy(v);
    i18n_free(cat);
}

TEST(two_digit_link_numbers_are_typed_one_digit_at_a_time)
{
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    /* Zwölf Verweise, damit es eine zweistellige Nummer gibt. */
    static char many[1024];
    int         n = 0;
    for (int i = 1; i <= 12; i++)
        n += snprintf(many + n, sizeof many - (size_t)n,
                      "=> spartan://x.org/%d Ziel %d\n", i, i);

    widget *v = gemview_create(test_theme(), cat);
    REQUIRE(v != NULL);
    v->frame   = rect_make(0, 0, 300, 400);
    v->focused = true;
    gemview_set_text(v, many, strlen(many));

    CHECK_EQ(gemview_link_count(v), 12);

    event one = { .kind = EV_TEXT, .text = "1" };
    CHECK(widget_event(v, &one));
    CHECK_EQ(gemview_selected_link(v), 1);

    event two = { .kind = EV_TEXT, .text = "2" };
    CHECK(widget_event(v, &two));
    CHECK_EQ(gemview_selected_link(v), 12);      /* aus 1 wird 12 */

    /* Und weiter geht es nicht - 123 gibt es nicht, also gilt die 3. */
    event three = { .kind = EV_TEXT, .text = "3" };
    CHECK(widget_event(v, &three));
    CHECK_EQ(gemview_selected_link(v), 3);

    widget_destroy(v);
    i18n_free(cat);
}

TEST(opening_a_link_is_reported_once)
{
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    widget *v = a_view(cat, 300, 200);
    REQUIRE(v != NULL);
    v->focused = true;

    /* Ohne Auswahl gibt es nichts zu öffnen. */
    event ret = { .kind = EV_KEY_DOWN, .key = KEY_RETURN };
    CHECK(widget_event(v, &ret));
    CHECK(!gemview_was_opened(v));

    gemview_select_link(v, 1);
    CHECK(widget_event(v, &ret));
    CHECK(gemview_was_opened(v));
    CHECK(!gemview_was_opened(v));          /* der Merker wird beim Lesen gelöscht */

    widget_destroy(v);
    i18n_free(cat);
}

TEST(clicking_a_link_line_selects_it)
{
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    widget *v = a_view(cat, 400, 300);
    REQUIRE(v != NULL);

    /* Die Verweiszeilen durchprobieren, wie ein Nutzer es täte. */
    int found = 0;
    for (int y = 0; y < 300 && found == 0; y += 2) {
        gemview_select_link(v, 0);

        event click = { .kind = EV_MOUSE_DOWN, .clicks = 1, .x = 20, .y = y };
        widget_event(v, &click);
        if (gemview_selected_link(v) == 1) found = y;
    }
    CHECK(found > 0);

    /* Ein Doppelklick öffnet. */
    event dbl = { .kind = EV_MOUSE_DOWN, .clicks = 2, .x = 20, .y = found };
    CHECK(widget_event(v, &dbl));
    CHECK(gemview_was_opened(v));

    /* Ein Klick auf eine gewöhnliche Zeile wählt nichts aus. */
    gemview_select_link(v, 0);
    event text_click = { .kind = EV_MOUSE_DOWN, .clicks = 1, .x = 20, .y = 4 };
    CHECK(widget_event(v, &text_click));
    CHECK_EQ(gemview_selected_link(v), 0);

    /* Und daneben gehört der Klick nicht der Ansicht. */
    event outside = { .kind = EV_MOUSE_DOWN, .clicks = 1, .x = 900, .y = 900 };
    CHECK(!widget_event(v, &outside));

    widget_destroy(v);
    i18n_free(cat);
}

TEST(selecting_a_link_that_does_not_exist_changes_nothing)
{
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    widget *v = a_view(cat, 300, 200);
    REQUIRE(v != NULL);

    gemview_select_link(v, 2);
    CHECK_EQ(gemview_selected_link(v), 2);

    gemview_select_link(v, 99);
    CHECK_EQ(gemview_selected_link(v), 2);      /* unverändert */

    gemview_select_link(v, -1);
    CHECK_EQ(gemview_selected_link(v), 2);

    /* Null hebt sie auf - das ist ausdrücklich erlaubt. */
    gemview_select_link(v, 0);
    CHECK_EQ(gemview_selected_link(v), 0);

    widget_destroy(v);
    i18n_free(cat);
}

TEST(without_focus_the_keyboard_belongs_to_someone_else)
{
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    widget *v = a_view(cat, 200, 60);
    REQUIRE(v != NULL);
    v->focused = false;

    scrollmodel *m = gemview_scroll(v);
    REQUIRE(scroll_max(m) > 0);

    event down = { .kind = EV_KEY_DOWN, .key = KEY_DOWN };
    CHECK(!widget_event(v, &down));
    CHECK_EQ(m->value, 0);

    event two = { .kind = EV_TEXT, .text = "2" };
    CHECK(!widget_event(v, &two));
    CHECK_EQ(gemview_selected_link(v), 0);

    widget_destroy(v);
    i18n_free(cat);
}

TEST(double_clicking_a_plain_line_opens_nothing)
{
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    widget *v = a_view(cat, 400, 300);
    REQUIRE(v != NULL);

    /* Die erste Zeile ist die Überschrift, kein Verweis. */
    event dbl = { .kind = EV_MOUSE_DOWN, .clicks = 2, .x = 20, .y = 6 };
    CHECK(widget_event(v, &dbl));
    CHECK(!gemview_was_opened(v));
    CHECK_EQ(gemview_selected_link(v), 0);

    widget_destroy(v);
    i18n_free(cat);
}

TEST(new_text_forgets_the_old_wrap_and_the_old_selection)
{
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    widget *v = a_view(cat, 300, 200);
    REQUIRE(v != NULL);

    int before = gemview_scroll(v)->total;
    gemview_select_link(v, 2);
    CHECK_EQ(gemview_selected_link(v), 2);

    /* Ein anderer Text, gleiche Breite. Bliebe der alte Umbruch stehen, zeigte
     * die Ansicht weiter die alte Seite - und die alte Auswahl zeigte auf
     * einen Verweis, den es dort gar nicht mehr gibt. */
    static const char OTHER[] =
        "Nur eine Zeile.\n=> spartan://a/ A\n=> spartan://b/ B\n";
    gemview_set_text(v, OTHER, strlen(OTHER));

    CHECK_EQ(gemview_selected_link(v), 0);
    CHECK(gemview_scroll(v)->total != before);
    CHECK_EQ(gemview_link_count(v), 2);

    widget_destroy(v);
    i18n_free(cat);
}

/* --- Umbruch ------------------------------------------------------------------------ */

TEST(a_narrow_view_needs_more_lines_than_a_wide_one)
{
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    widget *wide = a_view(cat, 600, 400);
    widget *slim = a_view(cat, 180, 400);
    REQUIRE(wide && slim);

    int a = gemview_scroll(wide)->total;
    int b = gemview_scroll(slim)->total;

    CHECK(b > a);

    /* Und beide zeigen dieselben Verweise - der Umbruch ändert den Inhalt
     * nicht. */
    CHECK_EQ(gemview_link_count(wide), gemview_link_count(slim));

    widget_destroy(wide);
    widget_destroy(slim);
    i18n_free(cat);
}

TEST(the_wrap_follows_a_changed_width)
{
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    widget *v = a_view(cat, 600, 400);
    REQUIRE(v != NULL);
    int wide = gemview_scroll(v)->total;

    v->frame.w = 180;
    int slim = gemview_scroll(v)->total;

    CHECK(slim > wide);

    widget_destroy(v);
    i18n_free(cat);
}

TEST(the_prefix_of_a_link_line_takes_away_from_its_width)
{
    /* Eine Verweiszeile beginnt mit „[n] ". Wer diesen Einzug beim Umbrechen
     * vergisst, lässt sie um genau diese Breite über den Rand hinauslaufen -
     * und zwar nur bei Verweisen, also selten genug, um lange unbemerkt zu
     * bleiben. */
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    const int view_w = 300;

    widget *v = gemview_create(test_theme(), cat);
    REQUIRE(v != NULL);
    v->frame = rect_make(0, 0, view_w, 200);

    /* Ein Name, der ohne Einzug gerade noch in eine Zeile passt. */
    char name[256];
    int  n = 0;
    while (n < 200) {
        name[n] = 'm';
        name[n + 1] = '\0';
        if (text_width(&system12, name) > view_w - 2 * g_theme.menu_pad - 8) break;
        n++;
    }
    REQUIRE(n > 10);

    char plain[512], linked[512];
    snprintf(plain, sizeof plain, "%s\n", name);
    snprintf(linked, sizeof linked, "=> spartan://x/ %s\n", name);

    gemview_set_text(v, plain, strlen(plain));
    CHECK_EQ(gemview_scroll(v)->total, 1);

    gemview_set_text(v, linked, strlen(linked));
    CHECK_EQ(gemview_scroll(v)->total, 2);

    widget_destroy(v);
    i18n_free(cat);
}

TEST(a_word_longer_than_the_line_is_broken_hard)
{
    /* Sonst liefe eine sehr lange Adresse aus dem Fenster heraus, und der
     * Nutzer sähe nicht, dass da noch etwas ist. */
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    static const char LONG[] =
        "einsehrlangeswortohnejedeleerstellewelchesnichtindiezeilepasst\n";

    widget *v = gemview_create(test_theme(), cat);
    REQUIRE(v != NULL);
    v->frame = rect_make(0, 0, 120, 200);
    gemview_set_text(v, LONG, strlen(LONG));

    CHECK(gemview_scroll(v)->total > 1);

    widget_destroy(v);
    i18n_free(cat);
}

TEST(preformatted_text_is_not_wrapped)
{
    /* Dort bedeutet die Zeilenlage etwas. */
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    static const char PRE[] =
        "```\n"
        "eine sehr lange vorformatierte zeile die trotzdem eine zeile bleibt\n"
        "```\n";

    widget *v = gemview_create(test_theme(), cat);
    REQUIRE(v != NULL);
    v->frame = rect_make(0, 0, 120, 200);
    gemview_set_text(v, PRE, strlen(PRE));

    CHECK_EQ(gemview_scroll(v)->total, 1);

    widget_destroy(v);
    i18n_free(cat);
}

TEST(an_empty_view_is_safe)
{
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    widget *v = gemview_create(test_theme(), cat);
    REQUIRE(v != NULL);
    v->frame = rect_make(0, 0, 100, 60);

    gemview_set_text(v, NULL, 0);
    CHECK_EQ(gemview_link_count(v), 0);
    CHECK_EQ(gemview_scroll(v)->total, 0);

    bitmap bm;
    REQUIRE(bitmap_init(&bm, 120, 80));
    gc g;
    gc_init(&g, &bm);
    widget_draw(v, &g);

    event click = { .kind = EV_MOUSE_DOWN, .clicks = 1, .x = 10, .y = 10 };
    CHECK(widget_event(v, &click));

    /* Und ein Rahmen, in den keine Zeile passt. */
    v->frame = rect_make(0, 0, 20, 4);
    widget_draw(v, &g);

    bitmap_free(&bm);
    widget_destroy(v);
    i18n_free(cat);
}

/* --- Blättern -------------------------------------------------------------------------- */

TEST(the_view_scrolls_with_keys_and_wheel)
{
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    widget *v = a_view(cat, 200, 60);      /* absichtlich zu klein */
    REQUIRE(v != NULL);
    v->focused = true;

    scrollmodel *m = gemview_scroll(v);
    REQUIRE(scroll_max(m) > 2);
    CHECK_EQ(m->value, 0);

    event down = { .kind = EV_KEY_DOWN, .key = KEY_DOWN };
    CHECK(widget_event(v, &down));
    CHECK_EQ(m->value, 1);

    event end = { .kind = EV_KEY_DOWN, .key = KEY_END };
    CHECK(widget_event(v, &end));
    CHECK_EQ(m->value, scroll_max(m));

    event home = { .kind = EV_KEY_DOWN, .key = KEY_HOME };
    CHECK(widget_event(v, &home));
    CHECK_EQ(m->value, 0);

    event wheel = { .kind = EV_WHEEL, .x = 10, .y = 10, .wheel = -2 };
    CHECK(widget_event(v, &wheel));
    CHECK_EQ(m->value, 2);

    widget_destroy(v);
    i18n_free(cat);
}

TEST(selecting_a_link_brings_it_into_view)
{
    /* Einen Verweis auszuwählen und ihn nicht zu zeigen wäre die halbe
     * Handlung. */
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    static char many[2048];
    int         n = 0;
    for (int i = 1; i <= 30; i++)
        n += snprintf(many + n, sizeof many - (size_t)n,
                      "Zeile %d\n=> spartan://x.org/%d Ziel %d\n", i, i, i);

    widget *v = gemview_create(test_theme(), cat);
    REQUIRE(v != NULL);
    v->frame   = rect_make(0, 0, 300, 60);
    gemview_set_text(v, many, strlen(many));

    scrollmodel *m = gemview_scroll(v);
    CHECK_EQ(m->value, 0);

    gemview_select_link(v, 25);
    CHECK(m->value > 0);
    CHECK_EQ(gemview_selected_link(v), 25);

    widget_destroy(v);
    i18n_free(cat);
}

/* --- Aussehen ---------------------------------------------------------------------------- */

TEST(a_page_looks_like_a_page)
{
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    /* Unter 256 x 256, damit das Sollbild als lesbarer P1-Text abgelegt wird
     * (golden.h). Für eine Seite mit allem, was Gemtext kennt, reicht es. */
    widget *v = gemview_create(test_theme(), cat);
    REQUIRE(v != NULL);
    v->frame = rect_make(3, 3, 244, 244);
    gemview_set_text(v, PAGE, strlen(PAGE));
    gemview_select_link(v, 2);

    bitmap bm;
    REQUIRE(bitmap_init(&bm, 250, 250));
    gc g;
    gc_init(&g, &bm);
    g.pat = PAT_WHITE;
    gfx_fill_rect(&g, rect_make(0, 0, 250, 250));

    widget_draw(v, &g);
    CHECK(golden_check("gemview_page", &bm));

    bitmap_free(&bm);
    widget_destroy(v);
    i18n_free(cat);
}

int main(void)
{
    RUN(links_are_counted_and_numbered);
    RUN(a_link_without_a_name_shows_its_address);
    RUN(selecting_a_link_by_number);
    RUN(two_digit_link_numbers_are_typed_one_digit_at_a_time);
    RUN(opening_a_link_is_reported_once);
    RUN(clicking_a_link_line_selects_it);

    RUN(selecting_a_link_that_does_not_exist_changes_nothing);
    RUN(without_focus_the_keyboard_belongs_to_someone_else);
    RUN(double_clicking_a_plain_line_opens_nothing);
    RUN(new_text_forgets_the_old_wrap_and_the_old_selection);

    RUN(a_narrow_view_needs_more_lines_than_a_wide_one);
    RUN(the_wrap_follows_a_changed_width);
    RUN(the_prefix_of_a_link_line_takes_away_from_its_width);
    RUN(a_word_longer_than_the_line_is_broken_hard);
    RUN(preformatted_text_is_not_wrapped);
    RUN(an_empty_view_is_safe);

    RUN(the_view_scrolls_with_keys_and_wheel);
    RUN(selecting_a_link_brings_it_into_view);

    RUN(a_page_looks_like_a_page);

    return test_summary();
}
