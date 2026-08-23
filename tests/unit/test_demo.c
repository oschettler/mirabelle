/* Die Vorführung als Ganzes: Ereignisse hinein, Zustand und Bild heraus.
 *
 * Das ist der erste Test, der die Schichten zusammen prüft - Belegung, Text,
 * Zeichensatz und Grafik. Er kommt ohne Fenster aus, weil demo.c von der
 * Schleife getrennt ist.
 */

#include "test.h"

#include <stdio.h>
#include <string.h>

#include "core/keymap.h"
#include "demo.h"
#include "core/i18n.h"
#include "ui/theme.h"
#include "gfx/bitmap.h"
#include "gfx/draw.h"
#include "support/golden.h"

#ifndef PDA_DATA_DIR
#define PDA_DATA_DIR "data"
#endif

static keymap *load_keys(void)
{
    char path[512], err[512] = "";
    snprintf(path, sizeof path, "%s/keys/default.keys", PDA_DATA_DIR);

    keymap *km = keymap_load(path, err, sizeof err);
    if (!km) printf("  Belegung nicht ladbar: %s\n", err);
    return km;
}

/* Die Vorführung öffnet ab M6 Fenster, braucht also einen Schirm und ein
 * Thema. Die Maße kommen aus den Voreinstellungen, damit der Test nicht von
 * der Themendatei abhängt.
 *
 * Das Thema liegt hier bewusst auf dem Stapel: wm_create legt eine Kopie an,
 * der Aufrufer muss es also nicht überleben. Diese Zusage gilt seit dem Fehler,
 * den genau diese Funktion einmal ausgelöst hat. */
/* Ein Katalog für alle Tests. Ohne ihn erschienen statt der Texte deren
 * Schlüssel - das Sollbild wäre dann nicht das, was der Nutzer sieht. */
static catalog *g_cat;

static bool start(demo_state *st, const keymap *km)
{
    theme th;
    theme_defaults(&th);
    return demo_init(st, km, g_cat, &th, 800, 480);
}

static void type_text(demo_state *st, const char *utf8)
{
    event e = { .kind = EV_TEXT };
    snprintf(e.text, sizeof e.text, "%s", utf8);
    demo_event(st, &e);
}

static void press(demo_state *st, int key, uint8_t mods)
{
    event e = { .kind = EV_KEY_DOWN, .key = key, .mods = mods };
    demo_event(st, &e);
}

TEST(typing_accumulates_text)
{
    demo_state st;
    REQUIRE(start(&st, NULL));

    type_text(&st, "G");
    type_text(&st, "r");
    type_text(&st, "ü");
    type_text(&st, "ß");
    type_text(&st, "e");

    CHECK_STR(st.typed, "Grüße");
    CHECK(st.running);

    demo_free(&st);
}

/* Die Rücktaste entfernt ein Zeichen, nicht ein Byte. Ohne das bliebe von
 * einem ü ein halbes übrig, und die Anzeige zerfiele. */
TEST(backspace_removes_a_codepoint_not_a_byte)
{
    demo_state st;
    REQUIRE(start(&st, NULL));

    type_text(&st, "ä");
    type_text(&st, "ö");
    type_text(&st, "ü");
    CHECK_EQ(strlen(st.typed), 6u);

    press(&st, KEY_BACKSPACE, 0);
    CHECK_STR(st.typed, "äö");
    CHECK_EQ(strlen(st.typed), 4u);

    press(&st, KEY_BACKSPACE, 0);
    press(&st, KEY_BACKSPACE, 0);
    CHECK_STR(st.typed, "");

    press(&st, KEY_BACKSPACE, 0);   /* auf leerem Text darf nichts passieren */
    CHECK_STR(st.typed, "");

    demo_free(&st);
}

TEST(escape_stops_the_loop)
{
    demo_state st;
    REQUIRE(start(&st, NULL));

    CHECK(st.running);
    press(&st, KEY_ESCAPE, 0);
    CHECK(!st.running);

    demo_free(&st);
}

TEST(quit_event_stops_the_loop)
{
    demo_state st;
    REQUIRE(start(&st, NULL));

    event e = { .kind = EV_QUIT };
    demo_event(&st, &e);
    CHECK(!st.running);

    demo_free(&st);
}

TEST(shortcuts_come_from_the_keymap)
{
    keymap *km = load_keys();
    REQUIRE(km != NULL);

    demo_state st;
    REQUIRE(start(&st, km));

    press(&st, 'n', MOD_CMD);
    CHECK_STR(st.last_action, "record.new");

    press(&st, 's', MOD_CMD);
    CHECK_STR(st.last_action, "record.save");

    press(&st, 'z', MOD_CMD);
    CHECK_STR(st.last_action, "edit.undo");

    /* Cmd+Shift+Z ist ein anderes Kürzel als Cmd+Z. */
    press(&st, 'z', MOD_CMD | MOD_SHIFT);
    CHECK_STR(st.last_action, "edit.redo");

    /* Beenden fragt seit M7 modal nach, statt sofort zu schließen. Erst die
     * Antwort auf den Dialog beendet wirklich. */
    CHECK(st.running);
    press(&st, 'q', MOD_CMD);
    CHECK_STR(st.last_action, "app.quit");
    CHECK(st.running);
    CHECK(st.dlg != NULL);

    press(&st, KEY_ESCAPE, 0);          /* abgebrochen */
    CHECK(st.dlg == NULL);
    CHECK(st.running);

    press(&st, 'q', MOD_CMD);
    REQUIRE(st.dlg != NULL);
    press(&st, KEY_RETURN, 0);          /* Voreinstellung: verwerfen */
    CHECK(st.dlg == NULL);
    CHECK(!st.running);

    demo_free(&st);
    keymap_free(km);
}

TEST(unknown_shortcut_leaves_last_action_alone)
{
    keymap *km = load_keys();
    REQUIRE(km != NULL);

    demo_state st;
    REQUIRE(start(&st, km));

    press(&st, 'n', MOD_CMD);
    CHECK_STR(st.last_action, "record.new");

    press(&st, 'j', MOD_CMD | MOD_ALT);   /* nicht belegt */
    CHECK_STR(st.last_action, "record.new");

    demo_free(&st);
    keymap_free(km);
}

/* Ein Buchstabe ohne Befehlstaste ist Text, kein Befehl. */
TEST(plain_letter_is_not_a_shortcut)
{
    keymap *km = load_keys();
    REQUIRE(km != NULL);

    demo_state st;
    REQUIRE(start(&st, km));

    press(&st, 'n', 0);
    CHECK_STR(st.last_action, "");

    demo_free(&st);
    keymap_free(km);
}

TEST(mouse_click_is_recorded_with_count)
{
    demo_state st;
    REQUIRE(start(&st, NULL));

    event e = { .kind = EV_MOUSE_DOWN, .x = 137, .y = 42,
                .button = 1, .clicks = 2 };
    demo_event(&st, &e);

    CHECK_EQ(st.click_x, 137);
    CHECK_EQ(st.click_y, 42);
    CHECK_EQ(st.click_count, 2);

    demo_free(&st);
}

TEST(golden_demo_after_input)
{
    keymap *km = load_keys();
    REQUIRE(km != NULL);

    demo_state st;
    REQUIRE(start(&st, km));

    press(&st, 'f', MOD_CMD);
    event click = { .kind = EV_MOUSE_DOWN, .x = 240, .y = 300,
                    .button = 1, .clicks = 2 };
    demo_event(&st, &click);
    type_text(&st, "M");
    type_text(&st, "ü");
    type_text(&st, "l");
    type_text(&st, "l");
    type_text(&st, "e");
    type_text(&st, "r");

    bitmap fb;
    REQUIRE(bitmap_init(&fb, 800, 480));
    gc g;
    gc_init(&g, &fb);
    demo_draw(&st, &g);

    /* Der Bereich, in dem sich beide Fenster überlappen. Als Ausschnitt bleibt
     * das Sollbild klein und im Diff lesbar; der ganze Schreibtisch wäre als
     * Text rund 750 kB. */
    bitmap win;
    REQUIRE(bitmap_copy_rect(&win, &fb, rect_make(300, 170, 250, 190)));
    CHECK(golden_check("demo_after_input", &win));

    bitmap_free(&win);
    bitmap_free(&fb);
    demo_free(&st);
    keymap_free(km);
}

int main(void)
{
    char path[512], err[512] = "";
    snprintf(path, sizeof path, "%s/lang/de.strings", PDA_DATA_DIR);
    g_cat = i18n_load(path, err, sizeof err);
    if (!g_cat) printf("Katalog nicht ladbar: %s\n", err);

    RUN(typing_accumulates_text);
    RUN(backspace_removes_a_codepoint_not_a_byte);
    RUN(escape_stops_the_loop);
    RUN(quit_event_stops_the_loop);
    RUN(shortcuts_come_from_the_keymap);
    RUN(unknown_shortcut_leaves_last_action_alone);
    RUN(plain_letter_is_not_a_shortcut);
    RUN(mouse_click_is_recorded_with_count);
    RUN(golden_demo_after_input);
    i18n_free(g_cat);
    return test_summary();
}
