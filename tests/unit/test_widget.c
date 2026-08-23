/* Die einfachen Bedienelemente aus M8: label, button, checkbox.
 *
 * Panel und Layout gibt es noch nicht (M8 baut sie parallel); die Tests hier
 * setzen frame also von Hand, statt es errechnen zu lassen.
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

/* --- Beschriftung ------------------------------------------------------------ */

TEST(label_size_depends_on_text)
{
    const theme *th  = load_test_theme();
    catalog     *cat = load_test_catalog();
    REQUIRE(cat);

    widget *short_label = label_create(th, cat, "button.ok");      /* "OK" */
    widget *long_label  = label_create(th, cat, "button.cancel");  /* "Abbrechen" */
    REQUIRE(short_label);
    REQUIRE(long_label);

    int sw, sh, lw, lh;
    widget_measure(short_label, &sw, &sh);
    widget_measure(long_label, &lw, &lh);

    CHECK(lw > sw);
    CHECK_EQ(sh, system12.size);
    CHECK_EQ(lh, system12.size);

    widget_destroy(short_label);
    widget_destroy(long_label);
    i18n_free(cat);
}

TEST(label_does_not_want_focus)
{
    const theme *th  = load_test_theme();
    catalog     *cat = load_test_catalog();
    REQUIRE(cat);

    widget *l = label_create(th, cat, "button.ok");
    REQUIRE(l);

    CHECK(!l->wants_focus);

    widget_destroy(l);
    i18n_free(cat);
}

/* --- Knopf: Größe -------------------------------------------------------------- */

TEST(button_width_is_at_least_the_minimum)
{
    const theme *th  = load_test_theme();
    catalog     *cat = load_test_catalog();
    REQUIRE(cat);

    widget *b = button_create(th, cat, "button.ok", "ok");   /* kurzer Text */
    REQUIRE(b);

    int bw, bh;
    widget_measure(b, &bw, &bh);

    CHECK(bw >= th->button_min_w);
    CHECK_EQ(bh, th->button_h);

    widget_destroy(b);
    i18n_free(cat);
}

/* --- Knopf: Maus --------------------------------------------------------------- */

TEST(click_inside_frame_fires_button)
{
    const theme *th  = load_test_theme();
    catalog     *cat = load_test_catalog();
    REQUIRE(cat);

    widget *b = button_create(th, cat, "button.ok", "ok");
    REQUIRE(b);
    b->frame = rect_make(10, 10, 60, 18);

    event down = { .kind = EV_MOUSE_DOWN, .button = 1, .x = 30, .y = 15 };
    CHECK(widget_event(b, &down));
    CHECK(button_was_pressed(b));
    CHECK(!button_was_pressed(b));   /* einmal auslesen setzt zurück */

    widget_destroy(b);
    i18n_free(cat);
}

TEST(click_outside_frame_does_not_fire_button)
{
    const theme *th  = load_test_theme();
    catalog     *cat = load_test_catalog();
    REQUIRE(cat);

    widget *b = button_create(th, cat, "button.ok", "ok");
    REQUIRE(b);
    b->frame = rect_make(10, 10, 60, 18);

    event down = { .kind = EV_MOUSE_DOWN, .button = 1, .x = 200, .y = 200 };
    CHECK(!widget_event(b, &down));
    CHECK(!button_was_pressed(b));

    widget_destroy(b);
    i18n_free(cat);
}

/* --- Knopf: Tastatur ------------------------------------------------------------ */

TEST(return_and_space_fire_button_only_when_focused)
{
    const theme *th  = load_test_theme();
    catalog     *cat = load_test_catalog();
    REQUIRE(cat);

    widget *b = button_create(th, cat, "button.ok", "ok");
    REQUIRE(b);
    b->frame = rect_make(0, 0, 60, 18);

    event ret = { .kind = EV_KEY_DOWN, .key = KEY_RETURN };
    CHECK(!widget_event(b, &ret));
    CHECK(!button_was_pressed(b));

    b->focused = true;
    CHECK(widget_event(b, &ret));
    CHECK(button_was_pressed(b));

    event space = { .kind = EV_KEY_DOWN, .key = KEY_SPACE };
    CHECK(widget_event(b, &space));
    CHECK(button_was_pressed(b));

    widget_destroy(b);
    i18n_free(cat);
}

TEST(disabled_button_ignores_click_and_keys)
{
    const theme *th  = load_test_theme();
    catalog     *cat = load_test_catalog();
    REQUIRE(cat);

    widget *b = button_create(th, cat, "button.ok", "ok");
    REQUIRE(b);
    b->frame   = rect_make(0, 0, 60, 18);
    b->focused = true;
    b->enabled = false;

    event down = { .kind = EV_MOUSE_DOWN, .button = 1, .x = 5, .y = 5 };
    CHECK(!widget_event(b, &down));
    CHECK(!button_was_pressed(b));

    event ret = { .kind = EV_KEY_DOWN, .key = KEY_RETURN };
    CHECK(!widget_event(b, &ret));
    CHECK(!button_was_pressed(b));

    widget_destroy(b);
    i18n_free(cat);
}

/* --- Kontrollkästchen ------------------------------------------------------------ */

TEST(click_toggles_checkbox_back_and_forth)
{
    const theme *th  = load_test_theme();
    catalog     *cat = load_test_catalog();
    REQUIRE(cat);

    widget *c = checkbox_create(th, cat, "button.ok", false);
    REQUIRE(c);
    c->frame = rect_make(0, 0, 60, 18);

    event down = { .kind = EV_MOUSE_DOWN, .button = 1, .x = 5, .y = 5 };
    CHECK(widget_event(c, &down));
    CHECK(checkbox_value(c));

    CHECK(widget_event(c, &down));
    CHECK(!checkbox_value(c));

    widget_destroy(c);
    i18n_free(cat);
}

TEST(space_toggles_checkbox_when_focused_return_does_not)
{
    const theme *th  = load_test_theme();
    catalog     *cat = load_test_catalog();
    REQUIRE(cat);

    widget *c = checkbox_create(th, cat, "button.ok", false);
    REQUIRE(c);
    c->frame = rect_make(0, 0, 60, 18);

    event space = { .kind = EV_KEY_DOWN, .key = KEY_SPACE };
    CHECK(!widget_event(c, &space));   /* ohne Fokus nichts */
    CHECK(!checkbox_value(c));

    c->focused = true;
    CHECK(widget_event(c, &space));
    CHECK(checkbox_value(c));

    event ret = { .kind = EV_KEY_DOWN, .key = KEY_RETURN };
    CHECK(!widget_event(c, &ret));
    CHECK(checkbox_value(c));   /* Return ändert nichts, bleibt an */

    widget_destroy(c);
    i18n_free(cat);
}

TEST(checkbox_set_value_and_value_agree)
{
    const theme *th  = load_test_theme();
    catalog     *cat = load_test_catalog();
    REQUIRE(cat);

    widget *c = checkbox_create(th, cat, "button.ok", false);
    REQUIRE(c);

    CHECK(!checkbox_value(c));
    checkbox_set_value(c, true);
    CHECK(checkbox_value(c));
    checkbox_set_value(c, false);
    CHECK(!checkbox_value(c));

    widget_destroy(c);
    i18n_free(cat);
}

/* --- Gesperrte Widgets allgemein -------------------------------------------------- */

TEST(disabled_widgets_return_false_from_widget_event)
{
    const theme *th  = load_test_theme();
    catalog     *cat = load_test_catalog();
    REQUIRE(cat);

    widget *b = button_create(th, cat, "button.ok", "ok");
    widget *c = checkbox_create(th, cat, "button.ok", false);
    REQUIRE(b);
    REQUIRE(c);

    b->frame   = rect_make(0, 0, 60, 18);
    c->frame   = rect_make(0, 0, 60, 18);
    b->enabled = false;
    c->enabled = false;

    event down = { .kind = EV_MOUSE_DOWN, .button = 1, .x = 5, .y = 5 };
    CHECK(!widget_event(b, &down));
    CHECK(!widget_event(c, &down));

    widget_destroy(b);
    widget_destroy(c);
    i18n_free(cat);
}

/* --- Sollbild ---------------------------------------------------------------------- */

TEST(golden_widgets_row)
{
    const theme *th  = load_test_theme();
    catalog     *cat = load_test_catalog();
    REQUIRE(cat);

    widget *label  = label_create(th, cat, "menu.view.tasks");
    widget *plain  = button_create(th, cat, "button.cancel", "cancel");
    widget *deflt  = button_create(th, cat, "button.ok", "ok");
    widget *on     = checkbox_create(th, cat, "menu.view.notes", true);
    widget *off    = checkbox_create(th, cat, "menu.view.calendar", false);
    widget *locked = button_create(th, cat, "button.save", "save");
    REQUIRE(label);
    REQUIRE(plain);
    REQUIRE(deflt);
    REQUIRE(on);
    REQUIRE(off);
    REQUIRE(locked);

    button_set_default(deflt, true);
    deflt->focused  = true;
    locked->enabled = false;

    widget *row[6] = { label, plain, deflt, on, off, locked };
    int     row_h  = th->button_h;
    int     pad    = th->dialog_pad;
    int     gap    = th->button_gap;

    int x = pad;
    for (int i = 0; i < 6; i++) {
        int mw, mh;
        widget_measure(row[i], &mw, &mh);
        (void)mh;   /* die Zeile richtet sich nach theme.button_h, nicht nach mh */
        row[i]->frame = rect_make(x, pad, mw, row_h);
        x += mw + gap;
    }

    int content_w = x - gap + pad;
    int content_h = row_h + 2 * pad;
    CHECK(content_w * content_h <= GOLDEN_MAX_P1_PIXELS);

    bitmap bm;
    REQUIRE(bitmap_init(&bm, content_w, content_h));

    gc g;
    gc_init(&g, &bm);
    g.pat  = PAT_WHITE;
    g.mode = GFX_COPY;
    gfx_clear(&g);

    for (int i = 0; i < 6; i++)
        widget_draw(row[i], &g);

    CHECK(golden_check("widgets_row", &bm));

    bitmap_free(&bm);
    for (int i = 0; i < 6; i++) widget_destroy(row[i]);
    i18n_free(cat);
}

int main(void)
{
    RUN(label_size_depends_on_text);
    RUN(label_does_not_want_focus);

    RUN(button_width_is_at_least_the_minimum);

    RUN(click_inside_frame_fires_button);
    RUN(click_outside_frame_does_not_fire_button);

    RUN(return_and_space_fire_button_only_when_focused);
    RUN(disabled_button_ignores_click_and_keys);

    RUN(click_toggles_checkbox_back_and_forth);
    RUN(space_toggles_checkbox_when_focused_return_does_not);
    RUN(checkbox_set_value_and_value_agree);

    RUN(disabled_widgets_return_false_from_widget_event);

    RUN(golden_widgets_row);

    return test_summary();
}
