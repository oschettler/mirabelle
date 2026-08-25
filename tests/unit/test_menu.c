/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Die Menüleiste aus M7: Geometrie, Maus- und Tastaturbedienung, ein Sollbild.
 *
 * Die Testmenüs bestehen aus echten Schlüsseln aus data/lang/de.strings und
 * echten Aktionen aus data/keys/default.keys - damit prüft golden_check auch
 * wirklich das, was am Ende auf dem Schirm steht, und keymap_describe liefert
 * echte Kürzel. Positionen werden wie in test_wm.c aus den Themamaßen
 * errechnet statt fest verdrahtet, nur dass hier zusätzlich Textbreiten
 * gebraucht werden - dieselbe Formel wie in menu.c, denn die Geometrie ist
 * Teil des in der Aufgabe festgelegten Vertrags, keine Implementierungslaune.
 */

#include "test.h"

#include <stdio.h>
#include <string.h>

#include "core/i18n.h"
#include "core/keymap.h"
#include "gfx/bitmap.h"
#include "gfx/draw.h"
#include "gfx/text.h"
#include "plat/plat.h"
#include "support/golden.h"
#include "ui/menu.h"
#include "ui/theme.h"

#ifndef PDA_DATA_DIR
#define PDA_DATA_DIR "data"
#endif

extern const font system12;

#define SCREEN_W 400

/* --- Testmenüs ---------------------------------------------------------- */

static const menu_item file_items[] = {
    { "menu.file.new",  "record.new"  },
    { "menu.file.open", "record.open" },
    { "menu.file.save", "record.save" },
    { NULL,             NULL          },   /* Trennlinie */
    { "menu.file.quit", "app.quit"    },
};

static const menu_item edit_items[] = {
    { "menu.edit.undo", "edit.undo" },
    { "menu.edit.redo", "edit.redo" },
};

static const menu test_menus[] = {
    { "menu.file", file_items, 5 },
    { "menu.edit", edit_items, 2 },
};

/* record.save ist in diesem Test gesperrt - für die Golden- und die
 * Rasterungsprüfung. */
static bool disable_save(const char *action, void *user)
{
    (void)user;
    return !(action && strcmp(action, "record.save") == 0);
}

/* Sperrt alles - für den Test, dass ein Menü ohne wählbaren Eintrag nicht
 * hängt. */
static bool disable_all(const char *action, void *user)
{
    (void)action;
    (void)user;
    return false;
}

/* --- Geometrie, dieselbe Formel wie in menu.c ---------------------------- */

static int title_width(const theme *th, const catalog *cat, const menu *m, int index)
{
    return text_width(&system12, T(cat, m[index].key)) + 2 * th->menu_pad;
}

static int title_x(const theme *th, const catalog *cat, const menu *m, int index)
{
    /* Vor dem ersten Titel steht die Luft, wo im Original das Apfelmenü sitzt. */
    int x = th->menubar_left;
    for (int i = 0; i < index; i++)
        x += title_width(th, cat, m, i);
    return x;
}

static int dropdown_width(const theme *th, const catalog *cat, const keymap *km,
                          const menu_item *items, int count)
{
    int  max_text = 0, max_short = 0;
    char shortcut[64];

    for (int i = 0; i < count; i++) {
        if (!items[i].key) continue;

        int tw = text_width(&system12, T(cat, items[i].key));
        if (tw > max_text) max_text = tw;

        if (items[i].action && keymap_describe(km, items[i].action, shortcut, sizeof shortcut)) {
            int sw = text_width(&system12, shortcut);
            if (sw > max_short) max_short = sw;
        }
    }

    return th->menu_text_pad + max_text + th->menu_gap + max_short +
           th->menu_pad;
}

static int item_center_y(const theme *th, int index)
{
    return th->menubar_h + index * th->menu_item_h + th->menu_item_h / 2;
}

/* --- Aufbau, wie in test_wm.c / test_i18n.c ------------------------------ */

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
    return i18n_load(path, err, sizeof err);
}

static keymap *load_test_keymap(void)
{
    char path[512], err[256] = "";
    snprintf(path, sizeof path, "%s/keys/default.keys", PDA_DATA_DIR);
    return keymap_load(path, err, sizeof err);
}

typedef struct {
    theme    th;
    catalog *cat;
    keymap  *km;
    menubar *mb;
} fixture;

static bool fixture_init(fixture *f, int menu_count)
{
    f->th  = load_test_theme();
    f->cat = load_test_catalog();
    f->km  = load_test_keymap();
    if (!f->cat || !f->km) return false;

    f->mb = menubar_create(test_menus, menu_count, f->cat, f->km, &f->th);
    return f->mb != NULL;
}

static void fixture_free(fixture *f)
{
    menubar_free(f->mb);
    i18n_free(f->cat);
    keymap_free(f->km);
}

/* --- kleine Hilfen für Ereignisse ----------------------------------------- */

static bool send_mouse(menubar *mb, event_kind kind, int x, int y, const char **action)
{
    event e = { .kind = kind, .button = 1, .x = x, .y = y };
    return menubar_event(mb, &e, SCREEN_W, action);
}

static bool send_key(menubar *mb, int key, const char **action)
{
    event e = { .kind = EV_KEY_DOWN, .key = key };
    return menubar_event(mb, &e, SCREEN_W, action);
}

/* --- Geometrie ------------------------------------------------------------- */

TEST(menubar_height_matches_theme)
{
    fixture f;
    REQUIRE(fixture_init(&f, 2));

    CHECK_EQ(menubar_height(f.mb), f.th.menubar_h);

    fixture_free(&f);
}

/* --- Maus ------------------------------------------------------------------- */

TEST(click_on_title_opens_menu)
{
    fixture f;
    REQUIRE(fixture_init(&f, 2));
    CHECK(!menubar_is_open(f.mb));

    int x = title_x(&f.th, f.cat, test_menus, 0) + 5;

    const char *action;
    CHECK(send_mouse(f.mb, EV_MOUSE_DOWN, x, 5, &action));
    CHECK(menubar_is_open(f.mb));
    CHECK(action == NULL);

    fixture_free(&f);
}

TEST(click_elsewhere_closes_without_action)
{
    fixture f;
    REQUIRE(fixture_init(&f, 2));

    int x = title_x(&f.th, f.cat, test_menus, 0) + 5;
    const char *action;
    REQUIRE(send_mouse(f.mb, EV_MOUSE_DOWN, x, 5, &action));
    REQUIRE(menubar_is_open(f.mb));

    /* weit weg von Leiste und aufgeklapptem Menü */
    CHECK(send_mouse(f.mb, EV_MOUSE_DOWN, SCREEN_W - 5, 300, &action));
    CHECK(!menubar_is_open(f.mb));
    CHECK(action == NULL);

    fixture_free(&f);
}

/* Kurzer Klick auf den Titel: das Menü bleibt offen, damit der Nutzer in Ruhe
 * hineinfahren kann. Erst ein Klick auf einen Eintrag löst aus. */
TEST(short_click_keeps_menu_open)
{
    fixture f;
    REQUIRE(fixture_init(&f, 2));

    int tx = title_x(&f.th, f.cat, test_menus, 0) + 5;

    const char *action;
    REQUIRE(send_mouse(f.mb, EV_MOUSE_DOWN, tx, 5, &action));
    CHECK(send_mouse(f.mb, EV_MOUSE_UP, tx, 5, &action));

    CHECK(menubar_is_open(f.mb));
    CHECK(action == NULL);

    fixture_free(&f);
}

/* Und danach auswählen: hinfahren, drücken, loslassen. */
TEST(click_on_item_after_short_click_triggers)
{
    fixture f;
    REQUIRE(fixture_init(&f, 2));

    int tx = title_x(&f.th, f.cat, test_menus, 0) + 5;
    int iy = item_center_y(&f.th, 1);   /* "Öffnen" */

    const char *action;
    REQUIRE(send_mouse(f.mb, EV_MOUSE_DOWN, tx, 5, &action));
    REQUIRE(send_mouse(f.mb, EV_MOUSE_UP, tx, 5, &action));
    REQUIRE(menubar_is_open(f.mb));

    REQUIRE(send_mouse(f.mb, EV_MOUSE_MOVE, tx, iy, &action));
    REQUIRE(send_mouse(f.mb, EV_MOUSE_DOWN, tx, iy, &action));
    CHECK(send_mouse(f.mb, EV_MOUSE_UP, tx, iy, &action));

    CHECK_STR(action, "record.open");
    CHECK(!menubar_is_open(f.mb));

    fixture_free(&f);
}

/* Noch einmal auf denselben Titel klappt wieder zu - sonst wäre ein
 * offengehaltenes Menü nur über einen Klick daneben loszuwerden. */
TEST(second_click_on_same_title_closes)
{
    fixture f;
    REQUIRE(fixture_init(&f, 2));

    int tx = title_x(&f.th, f.cat, test_menus, 0) + 5;

    const char *action;
    REQUIRE(send_mouse(f.mb, EV_MOUSE_DOWN, tx, 5, &action));
    REQUIRE(send_mouse(f.mb, EV_MOUSE_UP, tx, 5, &action));
    REQUIRE(menubar_is_open(f.mb));

    CHECK(send_mouse(f.mb, EV_MOUSE_DOWN, tx, 5, &action));
    CHECK(!menubar_is_open(f.mb));
    CHECK(action == NULL);

    fixture_free(&f);
}

TEST(drag_over_item_and_release_returns_its_action)
{
    fixture f;
    REQUIRE(fixture_init(&f, 2));

    int tx = title_x(&f.th, f.cat, test_menus, 0) + 5;
    int iy = item_center_y(&f.th, 0);   /* "Neu" */

    const char *action;
    REQUIRE(send_mouse(f.mb, EV_MOUSE_DOWN, tx, 5, &action));
    REQUIRE(send_mouse(f.mb, EV_MOUSE_MOVE, tx, iy, &action));

    CHECK(send_mouse(f.mb, EV_MOUSE_UP, tx, iy, &action));
    CHECK_STR(action, "record.new");
    CHECK(!menubar_is_open(f.mb));

    fixture_free(&f);
}

TEST(release_over_separator_returns_no_action)
{
    fixture f;
    REQUIRE(fixture_init(&f, 2));

    int tx = title_x(&f.th, f.cat, test_menus, 0) + 5;
    int sy = item_center_y(&f.th, 3);   /* die Trennlinie */

    const char *action;
    REQUIRE(send_mouse(f.mb, EV_MOUSE_DOWN, tx, 5, &action));
    REQUIRE(send_mouse(f.mb, EV_MOUSE_MOVE, tx, sy, &action));

    CHECK(send_mouse(f.mb, EV_MOUSE_UP, tx, sy, &action));
    CHECK(action == NULL);
    CHECK(!menubar_is_open(f.mb));

    fixture_free(&f);
}

TEST(release_over_disabled_item_returns_no_action)
{
    fixture f;
    REQUIRE(fixture_init(&f, 2));
    menubar_set_enabled_fn(f.mb, disable_save, NULL);

    int tx = title_x(&f.th, f.cat, test_menus, 0) + 5;
    int sy = item_center_y(&f.th, 2);   /* "Sichern", gesperrt */

    const char *action;
    REQUIRE(send_mouse(f.mb, EV_MOUSE_DOWN, tx, 5, &action));
    REQUIRE(send_mouse(f.mb, EV_MOUSE_MOVE, tx, sy, &action));

    CHECK(send_mouse(f.mb, EV_MOUSE_UP, tx, sy, &action));
    CHECK(action == NULL);
    CHECK(!menubar_is_open(f.mb));

    fixture_free(&f);
}

/* Die entscheidende Eigenschaft: die neue Position liegt nur im aufgeklappten
 * Bearbeiten-Menü, nicht im Ablage-Menü. Kommt "edit.undo" zurück, ist
 * tatsächlich umgeschaltet worden - sonst hätte das Loslassen daneben
 * gegriffen und nichts ausgelöst. */
TEST(move_over_another_title_switches_open_menu)
{
    fixture f;
    REQUIRE(fixture_init(&f, 2));

    int file_tx = title_x(&f.th, f.cat, test_menus, 0) + 5;
    int edit_tx = title_x(&f.th, f.cat, test_menus, 1) + 5;
    int iy      = item_center_y(&f.th, 0);

    const char *action;
    REQUIRE(send_mouse(f.mb, EV_MOUSE_DOWN, file_tx, 5, &action));
    REQUIRE(send_mouse(f.mb, EV_MOUSE_MOVE, edit_tx, 5, &action));
    REQUIRE(send_mouse(f.mb, EV_MOUSE_MOVE, edit_tx, iy, &action));

    CHECK(send_mouse(f.mb, EV_MOUSE_UP, edit_tx, iy, &action));
    CHECK_STR(action, "edit.undo");

    fixture_free(&f);
}

/* --- Tastatur ---------------------------------------------------------------- */

TEST(keyboard_enter_navigate_return_selects_action)
{
    fixture f;
    REQUIRE(fixture_init(&f, 2));

    menubar_enter(f.mb);   /* öffnet Ablage, hebt "Neu" hervor */
    CHECK(menubar_is_open(f.mb));

    const char *action;
    CHECK(send_key(f.mb, KEY_DOWN, &action));   /* Neu -> Öffnen */
    CHECK(send_key(f.mb, KEY_DOWN, &action));   /* Öffnen -> Sichern */

    CHECK(send_key(f.mb, KEY_RETURN, &action));
    CHECK_STR(action, "record.save");
    CHECK(!menubar_is_open(f.mb));

    fixture_free(&f);
}

TEST(arrow_keys_skip_separators_and_disabled_items)
{
    fixture f;
    REQUIRE(fixture_init(&f, 2));
    menubar_set_enabled_fn(f.mb, disable_save, NULL);

    menubar_enter(f.mb);   /* hebt "Neu" hervor, "Sichern" ist gesperrt */

    const char *action;
    CHECK(send_key(f.mb, KEY_DOWN, &action));   /* Neu -> Öffnen */
    CHECK(send_key(f.mb, KEY_DOWN, &action));   /* Öffnen -> Sichern (gesperrt) und
                                                  * Trennlinie überspringen -> Beenden */
    CHECK(send_key(f.mb, KEY_RETURN, &action));
    CHECK_STR(action, "app.quit");

    fixture_free(&f);
}

TEST(up_from_first_item_wraps_to_last_selectable)
{
    fixture f;
    REQUIRE(fixture_init(&f, 2));

    menubar_enter(f.mb);   /* hebt "Neu" hervor, Index 0 */

    const char *action;
    CHECK(send_key(f.mb, KEY_UP, &action));   /* umgebrochen zu "Beenden" */
    CHECK(send_key(f.mb, KEY_RETURN, &action));
    CHECK_STR(action, "app.quit");

    fixture_free(&f);
}

TEST(left_and_right_wrap_between_menus)
{
    fixture f;
    REQUIRE(fixture_init(&f, 2));

    menubar_enter(f.mb);   /* Ablage, Index 0 */

    const char *action;
    CHECK(send_key(f.mb, KEY_LEFT, &action));    /* umgebrochen zu Bearbeiten */
    CHECK(send_key(f.mb, KEY_RETURN, &action));
    CHECK_STR(action, "edit.undo");

    fixture_free(&f);
}

TEST(menu_without_selectable_item_does_not_hang_on_enter)
{
    static const menu_item items[] = {
        { NULL,             NULL         },   /* Trennlinie */
        { "menu.file.save", "record.save" },
    };
    static const menu menus[] = { { "menu.file", items, 2 } };

    theme    th  = load_test_theme();
    catalog *cat = load_test_catalog();
    keymap  *km  = load_test_keymap();
    REQUIRE(cat && km);

    menubar *mb = menubar_create(menus, 1, cat, km, &th);
    REQUIRE(mb);
    menubar_set_enabled_fn(mb, disable_all, NULL);

    menubar_enter(mb);   /* darf nicht endlos nach einem wählbaren Eintrag suchen */
    CHECK(menubar_is_open(mb));

    const char *action;
    CHECK(send_key(mb, KEY_DOWN, &action));   /* bleibt ohne Hervorhebung stehen */
    CHECK(send_key(mb, KEY_RETURN, &action));
    CHECK(action == NULL);
    CHECK(menubar_is_open(mb));   /* Return ohne Hervorhebung löst nichts aus */

    menubar_free(mb);
    i18n_free(cat);
    keymap_free(km);
}

TEST(escape_closes_without_action)
{
    fixture f;
    REQUIRE(fixture_init(&f, 2));

    menubar_enter(f.mb);
    CHECK(menubar_is_open(f.mb));

    const char *action;
    CHECK(send_key(f.mb, KEY_ESCAPE, &action));
    CHECK(action == NULL);
    CHECK(!menubar_is_open(f.mb));

    fixture_free(&f);
}

/* --- Verbrauch außerhalb der Leiste ------------------------------------------- */

TEST(events_outside_bar_are_not_consumed_when_closed)
{
    fixture f;
    REQUIRE(fixture_init(&f, 2));
    CHECK(!menubar_is_open(f.mb));

    const char *action;
    CHECK(!send_mouse(f.mb, EV_MOUSE_MOVE, 10, 200, &action));
    CHECK(!send_mouse(f.mb, EV_MOUSE_DOWN, 10, 200, &action));
    CHECK(!send_key(f.mb, KEY_DOWN, &action));

    fixture_free(&f);
}

/* --- Sollbild ------------------------------------------------------------------ */

TEST(golden_menu_file_open)
{
    fixture f;
    REQUIRE(fixture_init(&f, 2));
    menubar_set_enabled_fn(f.mb, disable_save, NULL);

    menubar_enter(f.mb);   /* öffnet Ablage, hebt "Neu" hervor, "Sichern" gerastert */

    int dd_w = dropdown_width(&f.th, f.cat, f.km, file_items, 5);
    int w    = title_x(&f.th, f.cat, test_menus, 0) + dd_w + 10;
    int h    = f.th.menubar_h + 5 * f.th.menu_item_h + 10;

    bitmap bm;
    REQUIRE(bitmap_init(&bm, w, h));
    gc g;
    gc_init(&g, &bm);
    menubar_draw(f.mb, &g, w);

    CHECK(golden_check("menu_file_open", &bm));

    bitmap_free(&bm);
    fixture_free(&f);
}

int main(void)
{
    RUN(menubar_height_matches_theme);

    RUN(click_on_title_opens_menu);
    RUN(click_elsewhere_closes_without_action);
    RUN(short_click_keeps_menu_open);
    RUN(click_on_item_after_short_click_triggers);
    RUN(second_click_on_same_title_closes);
    RUN(drag_over_item_and_release_returns_its_action);
    RUN(release_over_separator_returns_no_action);
    RUN(release_over_disabled_item_returns_no_action);
    RUN(move_over_another_title_switches_open_menu);

    RUN(keyboard_enter_navigate_return_selects_action);
    RUN(arrow_keys_skip_separators_and_disabled_items);
    RUN(up_from_first_item_wraps_to_last_selectable);
    RUN(left_and_right_wrap_between_menus);
    RUN(menu_without_selectable_item_does_not_hang_on_enter);
    RUN(escape_closes_without_action);

    RUN(events_outside_bar_are_not_consumed_when_closed);

    RUN(golden_menu_file_open);

    return test_summary();
}
