/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Modale Dialoge aus M7: Öffnen, Zeichnen, Tastatur- und Mausbedienung.
 *
 * Trefferpunkte für die Knöpfe werden aus den Themamaßen und der im
 * Meilensteintext festgelegten Regel errechnet (rechtsbündig, von links nach
 * rechts, theme.button_gap dazwischen) - genau wie test_wm.c es für die
 * Fensterfelder tut. So bleibt der Test unabhängig von konkreten Pixelzahlen
 * im Zeichencode, prüft aber denselben Vertrag.
 */
#include "test.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "core/i18n.h"
#include "core/utf8.h"
#include "gfx/bitmap.h"
#include "gfx/draw.h"
#include "gfx/font.h"
#include "gfx/text.h"
#include "plat/plat.h"
#include "support/golden.h"
#include "ui/dialog.h"
#include "ui/theme.h"
#include "ui/window.h"
#include "ui/wm.h"

extern const font system12;

#ifndef PDA_DATA_DIR
#define PDA_DATA_DIR "data"
#endif

/* Die logische Bildschirmgröße, für die dialog_open zentriert - D-9 zufolge
 * überall dieselbe. Für die Tests reicht das als wm-Bildschirmgröße. */
#define TEST_SCREEN_W 800
#define TEST_SCREEN_H 480

/* --- Laden von Thema und Katalog, wie in test_wm.c und test_i18n.c -------- */

static theme load_test_theme(void)
{
    theme th;
    char  path[512], err[256] = "";

    snprintf(path, sizeof path, "%s/themes/desktop.theme", PDA_DATA_DIR);
    if (!theme_load(&th, path, err, sizeof err)) {
        printf("  Thema nicht ladbar: %s\n", err);
        theme_defaults(&th);
    }
    return th;
}

static catalog *load_test_catalog(void)
{
    char path[512], err[256] = "";
    snprintf(path, sizeof path, "%s/lang/de.strings", PDA_DATA_DIR);

    catalog *c = i18n_load(path, err, sizeof err);
    if (!c) printf("  Katalog nicht ladbar: %s\n", err);
    return c;
}

/* --- Einen Verwerfen-Dialog mit echten Katalogschlüsseln öffnen ----------- */

static dialog *open_discard_dialog(wm *m, const catalog *cat, const char *name)
{
    const char *args[1]    = { name };
    const char *buttons[2] = { "button.cancel", "button.discard" };
    return dialog_open(m, cat, "dialog.discard.body", args, 1, buttons, 2);
}

/* --- Knopfmittelpunkte aus den Themamaßen errechnen ----------------------- */

static void button_centers(const theme *th, const catalog *cat, window *win,
                           const char *const *button_keys, int button_count,
                           int *cx, int *cy)
{
    rect cr = window_content_rect(win);

    int bw[DIALOG_MAX_BUTTONS];
    int row_w = 0;
    for (int i = 0; i < button_count; i++) {
        int tw = text_width(&system12, T(cat, button_keys[i]));
        bw[i] = tw + 2 * th->menu_pad;
        if (bw[i] < th->button_min_w) bw[i] = th->button_min_w;
        row_w += bw[i];
        if (i > 0) row_w += th->button_gap;
    }

    int bx = cr.x + cr.w - th->dialog_pad - row_w;
    int by = cr.y + cr.h - th->dialog_pad - th->button_h;

    for (int i = 0; i < button_count; i++) {
        cx[i] = bx + bw[i] / 2;
        cy[i] = by + th->button_h / 2;
        bx += bw[i] + th->button_gap;
    }
}

/* --- Sucht das Muster des Ersatzzeichens U+FFFD irgendwo im Bild ---------- *
 *
 * Ein Fund hieße: der Umbruch hat mitten in einem Mehrbytezeichen
 * geschnitten, und gfx_text hat dafür das Ersatzzeichen gezeichnet. */
static bool bitmap_has_replacement_glyph(const bitmap *bm)
{
    const glyph *gl = font_find(&system12, UTF8_REPLACEMENT);
    if (!gl) return false;   /* Schrift ohne Ersatzzeichen - hier nicht der Fall */

    for (int y = 0; y + system12.size <= bm->h; y++) {
        for (int x = 0; x + gl->width <= bm->w; x++) {
            bool match = true;
            for (int gy = 0; gy < system12.size && match; gy++) {
                const uint8_t *row = system12.bits + gl->offset + (size_t)gy * gl->stride;
                for (int gx = 0; gx < gl->width; gx++) {
                    int bit = (row[gx / 8] >> (7 - (gx & 7))) & 1;
                    if (bitmap_get(bm, x + gx, y + gy) != bit) { match = false; break; }
                }
            }
            if (match) return true;
        }
    }
    return false;
}

/* --- Öffnen und Grundzustand ----------------------------------------------- */

TEST(dialog_starts_open)
{
    theme th = load_test_theme();
    wm   *m  = wm_create(&th, TEST_SCREEN_W, TEST_SCREEN_H);
    REQUIRE(m);
    catalog *cat = load_test_catalog();
    REQUIRE(cat);

    dialog *d = open_discard_dialog(m, cat, "Adressbuch");
    REQUIRE(d);

    CHECK_EQ(dialog_result(d), DIALOG_OPEN);

    dialog_close(d);
    i18n_free(cat);
    wm_destroy(m);
}

TEST(return_selects_the_focused_default_button)
{
    theme th = load_test_theme();
    wm   *m  = wm_create(&th, TEST_SCREEN_W, TEST_SCREEN_H);
    REQUIRE(m);
    catalog *cat = load_test_catalog();
    REQUIRE(cat);

    dialog *d = open_discard_dialog(m, cat, "Adressbuch");
    REQUIRE(d);

    event ret = { .kind = EV_KEY_DOWN, .key = KEY_RETURN };
    CHECK(dialog_event(d, &ret));
    CHECK_EQ(dialog_result(d), 1);   /* letzter Knopf: Verwerfen */

    dialog_close(d);
    i18n_free(cat);
    wm_destroy(m);
}

/* --- Maus -------------------------------------------------------------------- */

TEST(click_selects_left_and_right_button)
{
    theme th = load_test_theme();
    wm   *m  = wm_create(&th, TEST_SCREEN_W, TEST_SCREEN_H);
    REQUIRE(m);
    catalog *cat = load_test_catalog();
    REQUIRE(cat);

    const char *buttons[2] = { "button.cancel", "button.discard" };
    int         cx[DIALOG_MAX_BUTTONS], cy[DIALOG_MAX_BUTTONS];

    dialog *left = open_discard_dialog(m, cat, "Adressbuch");
    REQUIRE(left);
    button_centers(&th, cat, dialog_window(left), buttons, 2, cx, cy);

    event down_left = { .kind = EV_MOUSE_DOWN, .button = 1, .x = cx[0], .y = cy[0] };
    CHECK(dialog_event(left, &down_left));
    CHECK_EQ(dialog_result(left), 0);
    dialog_close(left);

    dialog *right = open_discard_dialog(m, cat, "Adressbuch");
    REQUIRE(right);
    button_centers(&th, cat, dialog_window(right), buttons, 2, cx, cy);

    event down_right = { .kind = EV_MOUSE_DOWN, .button = 1, .x = cx[1], .y = cy[1] };
    CHECK(dialog_event(right, &down_right));
    CHECK_EQ(dialog_result(right), 1);
    dialog_close(right);

    i18n_free(cat);
    wm_destroy(m);
}

/* --- Tastatur ------------------------------------------------------------- */

TEST(escape_selects_first_button)
{
    theme th = load_test_theme();
    wm   *m  = wm_create(&th, TEST_SCREEN_W, TEST_SCREEN_H);
    REQUIRE(m);
    catalog *cat = load_test_catalog();
    REQUIRE(cat);

    dialog *d = open_discard_dialog(m, cat, "Adressbuch");
    REQUIRE(d);

    event esc = { .kind = EV_KEY_DOWN, .key = KEY_ESCAPE };
    CHECK(dialog_event(d, &esc));
    CHECK_EQ(dialog_result(d), 0);

    dialog_close(d);
    i18n_free(cat);
    wm_destroy(m);
}

TEST(tab_wraps_around_with_two_buttons)
{
    theme th = load_test_theme();
    wm   *m  = wm_create(&th, TEST_SCREEN_W, TEST_SCREEN_H);
    REQUIRE(m);
    catalog *cat = load_test_catalog();
    REQUIRE(cat);

    dialog *d = open_discard_dialog(m, cat, "Adressbuch");
    REQUIRE(d);

    /* Nach EINEM Tab prüfen, nicht erst nach zweien. Bei zwei Knöpfen landet
     * man sonst mit und ohne Umbruch am selben Ort, und der Test kann gar
     * nichts unterscheiden. */
    event tab = { .kind = EV_KEY_DOWN, .key = KEY_TAB };
    CHECK(dialog_event(d, &tab));   /* Fokus 1 -> 0, also mit Umbruch */

    event ret = { .kind = EV_KEY_DOWN, .key = KEY_RETURN };
    CHECK(dialog_event(d, &ret));
    CHECK_EQ(dialog_result(d), 0);

    dialog_close(d);

    /* Und zurück: zwei Tabs führen wieder zum Ausgangspunkt. */
    d = open_discard_dialog(m, cat, "Adressbuch");
    REQUIRE(d);
    CHECK(dialog_event(d, &tab));
    CHECK(dialog_event(d, &tab));
    CHECK(dialog_event(d, &ret));
    CHECK_EQ(dialog_result(d), 1);

    dialog_close(d);
    i18n_free(cat);
    wm_destroy(m);
}

TEST(shift_tab_moves_focus_backward)
{
    theme th = load_test_theme();
    wm   *m  = wm_create(&th, TEST_SCREEN_W, TEST_SCREEN_H);
    REQUIRE(m);
    catalog *cat = load_test_catalog();
    REQUIRE(cat);

    dialog *d = open_discard_dialog(m, cat, "Adressbuch");
    REQUIRE(d);

    event shift_tab = { .kind = EV_KEY_DOWN, .key = KEY_TAB, .mods = MOD_SHIFT };
    CHECK(dialog_event(d, &shift_tab));   /* Fokus 1 -> 0, rückwärts */

    event ret = { .kind = EV_KEY_DOWN, .key = KEY_RETURN };
    CHECK(dialog_event(d, &ret));
    CHECK_EQ(dialog_result(d), 0);

    dialog_close(d);
    i18n_free(cat);
    wm_destroy(m);
}

/* Mit nur zwei Knöpfen führen ein Schritt vorwärts und ein Schritt rückwärts
 * zufällig zum selben Ziel (1 -> 0 in beiden Richtungen). Drei Knöpfe zeigen
 * deshalb wirklich, dass Pfeil rechts vorwärts und Pfeil links rückwärts
 * zählt - mit echten Katalogschlüsseln, auch wenn sie inhaltlich nicht zu
 * dialog.discard.body passen; das prüft hier nur die Mechanik. */
TEST(arrow_keys_match_tab_and_shift_tab)
{
    theme th = load_test_theme();
    wm   *m  = wm_create(&th, TEST_SCREEN_W, TEST_SCREEN_H);
    REQUIRE(m);
    catalog *cat = load_test_catalog();
    REQUIRE(cat);

    const char *buttons[3] = { "button.cancel", "button.save", "button.discard" };

    dialog *right = dialog_open(m, cat, "dialog.discard.body",
                                (const char *[1]){ "Adressbuch" }, 1, buttons, 3);
    REQUIRE(right);
    /* Fokus beginnt bei 2. Rechts, rechts: 2 -> 0 -> 1. */
    event key_right = { .kind = EV_KEY_DOWN, .key = KEY_RIGHT };
    CHECK(dialog_event(right, &key_right));
    CHECK(dialog_event(right, &key_right));
    event ret = { .kind = EV_KEY_DOWN, .key = KEY_RETURN };
    CHECK(dialog_event(right, &ret));
    CHECK_EQ(dialog_result(right), 1);
    dialog_close(right);

    dialog *left = dialog_open(m, cat, "dialog.discard.body",
                               (const char *[1]){ "Adressbuch" }, 1, buttons, 3);
    REQUIRE(left);
    /* Fokus beginnt bei 2. Links, links: 2 -> 1 -> 0. */
    event key_left = { .kind = EV_KEY_DOWN, .key = KEY_LEFT };
    CHECK(dialog_event(left, &key_left));
    CHECK(dialog_event(left, &key_left));
    CHECK(dialog_event(left, &ret));
    CHECK_EQ(dialog_result(left), 0);
    dialog_close(left);

    i18n_free(cat);
    wm_destroy(m);
}

/* --- Modalität und Fensterart ----------------------------------------------- */

TEST(modal_dialog_blocks_clicks_outside)
{
    theme th = load_test_theme();
    wm   *m  = wm_create(&th, TEST_SCREEN_W, TEST_SCREEN_H);
    REQUIRE(m);
    catalog *cat = load_test_catalog();
    REQUIRE(cat);

    rect    fa = rect_make(10, 10, 150, 100);
    window *a  = wm_open(m, fa, "A", WIN_NORMAL);
    REQUIRE(a);

    dialog *d = open_discard_dialog(m, cat, "Adressbuch");
    REQUIRE(d);

    int cx = fa.x + fa.w / 2;
    int cy = fa.y + th.titlebar_h + 10;

    hit_part part;
    CHECK(wm_hit(m, cx, cy, &part) == NULL);
    CHECK_EQ(part, HIT_NONE);

    dialog_close(d);
    i18n_free(cat);
    wm_destroy(m);
}

TEST(dialog_window_is_neither_movable_nor_resizable)
{
    theme th = load_test_theme();
    wm   *m  = wm_create(&th, TEST_SCREEN_W, TEST_SCREEN_H);
    REQUIRE(m);
    catalog *cat = load_test_catalog();
    REQUIRE(cat);

    dialog *d = open_discard_dialog(m, cat, "Adressbuch");
    REQUIRE(d);

    unsigned flags = window_flags(dialog_window(d));
    CHECK((flags & WIN_MODAL) != 0);
    CHECK((flags & WIN_MOVABLE) == 0);
    CHECK((flags & WIN_RESIZABLE) == 0);

    dialog_close(d);
    i18n_free(cat);
    wm_destroy(m);
}

/* --- Textumbruch ------------------------------------------------------------- */

TEST(long_text_wraps_to_more_lines_and_taller_window)
{
    theme th = load_test_theme();
    wm   *m  = wm_create(&th, TEST_SCREEN_W, TEST_SCREEN_H);
    REQUIRE(m);
    catalog *cat = load_test_catalog();
    REQUIRE(cat);

    dialog *short_d = open_discard_dialog(m, cat, "Adressbuch");
    REQUIRE(short_d);

    dialog *long_d = open_discard_dialog(m, cat,
        "Kontaktliste Aufgabenliste Notizen Kalender Adressbuch Projektplan "
        "Rechnungen Belege Archiv Sammlung Bericht Anhang Vorlage Muster");
    REQUIRE(long_d);

    rect short_frame = window_frame(dialog_window(short_d));
    rect long_frame  = window_frame(dialog_window(long_d));
    CHECK(long_frame.h > short_frame.h);

    dialog_close(short_d);
    dialog_close(long_d);
    i18n_free(cat);
    wm_destroy(m);
}

TEST(wrapped_umlaut_text_has_no_replacement_glyphs)
{
    theme th = load_test_theme();
    wm   *m  = wm_create(&th, TEST_SCREEN_W, TEST_SCREEN_H);
    REQUIRE(m);
    catalog *cat = load_test_catalog();
    REQUIRE(cat);

    /* dialog.discard.body enthält bereits „ “ ä Ä - genau der Fall, den der
     * Umbruch nicht zerschneiden darf. */
    dialog *d = open_discard_dialog(m, cat, "Straße Müller Übersicht");
    REQUIRE(d);

    dialog_draw(d);

    gc g;
    window_gc(dialog_window(d), &g);
    CHECK(!bitmap_has_replacement_glyph(g.dst));

    dialog_close(d);
    i18n_free(cat);
    wm_destroy(m);
}

/* --- Sollbild ------------------------------------------------------------------ */

TEST(golden_dialog_discard)
{
    theme th = load_test_theme();
    wm   *m  = wm_create(&th, TEST_SCREEN_W, TEST_SCREEN_H);
    REQUIRE(m);
    catalog *cat = load_test_catalog();
    REQUIRE(cat);

    dialog *d = open_discard_dialog(m, cat, "Adressbuch");
    REQUIRE(d);

    dialog_draw(d);

    gc g;
    window_gc(dialog_window(d), &g);

    int cw = g.dst->w < 250 ? g.dst->w : 250;
    int ch = g.dst->h < 250 ? g.dst->h : 250;

    bitmap crop;
    REQUIRE(bitmap_copy_rect(&crop, g.dst, rect_make(0, 0, cw, ch)));
    CHECK(golden_check("dialog_discard", &crop));
    bitmap_free(&crop);

    dialog_close(d);
    i18n_free(cat);
    wm_destroy(m);
}

int main(void)
{
    RUN(dialog_starts_open);
    RUN(return_selects_the_focused_default_button);

    RUN(click_selects_left_and_right_button);

    RUN(escape_selects_first_button);
    RUN(tab_wraps_around_with_two_buttons);
    RUN(shift_tab_moves_focus_backward);
    RUN(arrow_keys_match_tab_and_shift_tab);

    RUN(modal_dialog_blocks_clicks_outside);
    RUN(dialog_window_is_neither_movable_nor_resizable);

    RUN(long_text_wraps_to_more_lines_and_taller_window);
    RUN(wrapped_umlaut_text_has_no_replacement_glyphs);

    RUN(golden_dialog_discard);

    return test_summary();
}
