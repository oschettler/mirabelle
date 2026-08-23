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
    demo_init(&st, NULL);

    type_text(&st, "G");
    type_text(&st, "r");
    type_text(&st, "ü");
    type_text(&st, "ß");
    type_text(&st, "e");

    CHECK_STR(st.typed, "Grüße");
    CHECK(st.running);
}

/* Die Rücktaste entfernt ein Zeichen, nicht ein Byte. Ohne das bliebe von
 * einem ü ein halbes übrig, und die Anzeige zerfiele. */
TEST(backspace_removes_a_codepoint_not_a_byte)
{
    demo_state st;
    demo_init(&st, NULL);

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
}

TEST(escape_stops_the_loop)
{
    demo_state st;
    demo_init(&st, NULL);

    CHECK(st.running);
    press(&st, KEY_ESCAPE, 0);
    CHECK(!st.running);
}

TEST(quit_event_stops_the_loop)
{
    demo_state st;
    demo_init(&st, NULL);

    event e = { .kind = EV_QUIT };
    demo_event(&st, &e);
    CHECK(!st.running);
}

TEST(shortcuts_come_from_the_keymap)
{
    keymap *km = load_keys();
    REQUIRE(km != NULL);

    demo_state st;
    demo_init(&st, km);

    press(&st, 'n', MOD_CMD);
    CHECK_STR(st.last_action, "record.new");

    press(&st, 's', MOD_CMD);
    CHECK_STR(st.last_action, "record.save");

    press(&st, 'z', MOD_CMD);
    CHECK_STR(st.last_action, "edit.undo");

    /* Cmd+Shift+Z ist ein anderes Kürzel als Cmd+Z. */
    press(&st, 'z', MOD_CMD | MOD_SHIFT);
    CHECK_STR(st.last_action, "edit.redo");

    CHECK(st.running);
    press(&st, 'q', MOD_CMD);
    CHECK_STR(st.last_action, "app.quit");
    CHECK(!st.running);

    keymap_free(km);
}

TEST(unknown_shortcut_leaves_last_action_alone)
{
    keymap *km = load_keys();
    REQUIRE(km != NULL);

    demo_state st;
    demo_init(&st, km);

    press(&st, 'n', MOD_CMD);
    CHECK_STR(st.last_action, "record.new");

    press(&st, 'j', MOD_CMD | MOD_ALT);   /* nicht belegt */
    CHECK_STR(st.last_action, "record.new");

    keymap_free(km);
}

/* Ein Buchstabe ohne Befehlstaste ist Text, kein Befehl. */
TEST(plain_letter_is_not_a_shortcut)
{
    keymap *km = load_keys();
    REQUIRE(km != NULL);

    demo_state st;
    demo_init(&st, km);

    press(&st, 'n', 0);
    CHECK_STR(st.last_action, "");

    keymap_free(km);
}

TEST(mouse_click_is_recorded_with_count)
{
    demo_state st;
    demo_init(&st, NULL);

    event e = { .kind = EV_MOUSE_DOWN, .x = 137, .y = 42,
                .button = 1, .clicks = 2 };
    demo_event(&st, &e);

    CHECK_EQ(st.click_x, 137);
    CHECK_EQ(st.click_y, 42);
    CHECK_EQ(st.click_count, 2);
}

TEST(golden_demo_after_input)
{
    keymap *km = load_keys();
    REQUIRE(km != NULL);

    demo_state st;
    demo_init(&st, km);

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
    demo_draw(&st, &g, 800, 480);

    /* Nur das Fenster, nicht der ganze Schreibtisch: als Ausschnitt bleibt das
     * Sollbild klein und im Diff lesbar. */
    bitmap win;
    REQUIRE(bitmap_copy_rect(&win, &fb, rect_make(210, 150, 250, 190)));
    CHECK(golden_check("demo_after_input", &win));

    bitmap_free(&win);
    bitmap_free(&fb);
    keymap_free(km);
}

int main(void)
{
    RUN(typing_accumulates_text);
    RUN(backspace_removes_a_codepoint_not_a_byte);
    RUN(escape_stops_the_loop);
    RUN(quit_event_stops_the_loop);
    RUN(shortcuts_come_from_the_keymap);
    RUN(unknown_shortcut_leaves_last_action_alone);
    RUN(plain_letter_is_not_a_shortcut);
    RUN(mouse_click_is_recorded_with_count);
    RUN(golden_demo_after_input);
    return test_summary();
}
