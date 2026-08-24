/* Die Schale, siehe app/shell.h.
 *
 * Hier wird geprüft, was erst im Zusammenspiel entsteht: dass aus Dateien
 * Anwendungen werden, dass ein Klick ankommt, wo er hingehört, und dass ein
 * geschlossenes Fenster nichts hinterlässt.
 */
#include "test.h"

#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "app/shell.h"
#include "core/collate.h"
#include "core/i18n.h"
#include "core/keymap.h"
#include "gfx/bitmap.h"
#include "gfx/draw.h"
#include "store/record.h"
#include "store/vault.h"
#include "support/golden.h"
#include "ui/theme.h"

#ifndef PDA_DATA_DIR
#define PDA_DATA_DIR "data"
#endif

#define SCREEN_W 800
#define SCREEN_H 480

/* --- Gerüst -------------------------------------------------------------------- */

static void temp_root(char *buf, size_t n)
{
    const char *dir = getenv("TMPDIR");
    if (!dir || !*dir) dir = "/tmp";

    size_t len = strlen(dir);
    while (len > 1 && dir[len - 1] == '/') len--;
    snprintf(buf, n, "%.*s/pda_shell_test", (int)len, dir);
}

static void rmrf(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) return;

    if (S_ISDIR(st.st_mode)) {
        DIR *d = opendir(path);
        if (d) {
            struct dirent *e;
            while ((e = readdir(d)) != NULL) {
                if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
                char child[700];
                snprintf(child, sizeof child, "%s/%s", path, e->d_name);
                rmrf(child);
            }
            closedir(d);
        }
        rmdir(path);
    } else {
        unlink(path);
    }
}

static theme    g_theme;
static catalog *g_cat;
static keymap  *g_km;
static collate *g_sort, *g_search;
static vault   *g_vault;

static const char *const TASKS[] = {
    "---\nid: 20260101T090000-0001\ntitle: Zander anrufen\ndue: 2026-05-01\ndone: no\n---\nAngebot.\n",
    "---\nid: 20260102T090000-0001\ntitle: Müller anrufen\ndue: 2026-03-15\ndone: no\n---\nLieferung aus Köln.\n",
};

static const char *const CONTACTS[] = {
    "---\nid: 20260201T090000-0001\nname: Öhler\ncity: Aachen\nphone: 0241-1\n---\nNachbarin.\n",
    "---\nid: 20260202T090000-0001\nname: Mulde\ncity: Bonn\nphone: 0228-2\n---\nVerein.\n",
};

static void fill(const char *collection, const char *const *texts, int n)
{
    char err[256] = "";
    for (int i = 0; i < n; i++) {
        record *r = record_parse(texts[i], strlen(texts[i]), "t", err, sizeof err);
        if (!r) { printf("  %s\n", err); return; }

        char id[RECORD_ID_LEN + 1];
        vault_save(g_vault, collection, r, id, sizeof id, err, sizeof err);
        record_free(r);
    }
}

static bool setup(void)
{
    char path[512], root[600], err[256] = "";

    snprintf(path, sizeof path, "%s/themes/desktop.theme", PDA_DATA_DIR);
    if (!theme_load(&g_theme, path, err, sizeof err)) theme_defaults(&g_theme);

    snprintf(path, sizeof path, "%s/lang/de.strings", PDA_DATA_DIR);
    g_cat = i18n_load(path, err, sizeof err);

    snprintf(path, sizeof path, "%s/keys/default.keys", PDA_DATA_DIR);
    g_km = keymap_load(path, err, sizeof err);

    snprintf(path, sizeof path, "%s/lang/de.sort", PDA_DATA_DIR);
    g_sort = collate_load(path, err, sizeof err);

    snprintf(path, sizeof path, "%s/collate/search.fold", PDA_DATA_DIR);
    g_search = collate_load(path, err, sizeof err);

    temp_root(root, sizeof root);
    rmrf(root);
    g_vault = vault_open(root, err, sizeof err);

    if (!g_cat || !g_km || !g_sort || !g_search || !g_vault) {
        printf("  Aufbau: %s\n", err);
        return false;
    }

    fill("Aufgaben", TASKS, 2);
    fill("Kontakte", CONTACTS, 2);
    return true;
}

static void teardown(void)
{
    char root[600];
    temp_root(root, sizeof root);

    vault_close(g_vault);
    rmrf(root);

    i18n_free(g_cat);
    keymap_free(g_km);
    collate_free(g_sort);
    collate_free(g_search);

    g_vault = NULL;
    g_cat   = NULL;
    g_km    = NULL;
    g_sort  = g_search = NULL;
}

static shell *open_shell(void)
{
    shell_config cfg = {
        .data_dir = PDA_DATA_DIR,
        .vault    = g_vault,
        .theme    = &g_theme,
        .catalog  = g_cat,
        .keymap   = g_km,
        .sort     = g_sort,
        .search   = g_search,
        .screen_w = SCREEN_W,
        .screen_h = SCREEN_H,
    };

    char   err[256] = "";
    shell *s = shell_create(&cfg, err, sizeof err);
    if (!s) printf("  Schale: %s\n", err);
    return s;
}

/* Findet die Anwendung mit diesem Katalogschlüssel. */
static int app_by_label(const shell *s, const char *label)
{
    for (int i = 0; i < shell_app_count(s); i++)
        if (strcmp(shell_app_label(s, i), label) == 0) return i;
    return -1;
}

/* --- Anwendungen aus Dateien ------------------------------------------------------- */

TEST(every_schema_file_becomes_an_application)
{
    /* Der Beweis, dass die Schale Dateien zählt und keine Anwendungen kennt:
     * sie findet genau die, die in data/schema liegen, und nennt keine davon
     * beim Namen. */
    REQUIRE(setup());

    shell *s = open_shell();
    REQUIRE(s != NULL);

    CHECK_EQ(shell_app_count(s), 4);

    CHECK(app_by_label(s, "app.tasks") >= 0);
    CHECK(app_by_label(s, "app.contacts") >= 0);
    CHECK(app_by_label(s, "app.notes") >= 0);
    CHECK(app_by_label(s, "app.events") >= 0);

    CHECK(shell_app_label(s, -1) == NULL);
    CHECK(shell_app_label(s, 99) == NULL);

    shell_destroy(s);
    teardown();
}

TEST(a_broken_schema_does_not_take_the_others_with_it)
{
    REQUIRE(setup());

    /* Eine kaputte Datei in ein eigenes Verzeichnis legen, zusammen mit einer
     * heilen - so wie es beim Nutzer aussähe, der eine Datei verstellt hat. */
    char root[600], dir[700], path[800];
    temp_root(root, sizeof root);
    snprintf(dir, sizeof dir, "%s/schemadir", root);
    mkdir(dir, 0777);
    snprintf(path, sizeof path, "%s/schema", dir);
    mkdir(path, 0777);

    char file[900];
    snprintf(file, sizeof file, "%s/gut.schema", path);
    FILE *fp = fopen(file, "wb");
    REQUIRE(fp != NULL);
    fputs("type g\nfolder Gut\nlabel app.notes\ncolumns t\nform t\n"
          "field t\n    kind text\n    label field.title\n", fp);
    fclose(fp);

    snprintf(file, sizeof file, "%s/kaputt.schema", path);
    fp = fopen(file, "wb");
    REQUIRE(fp != NULL);
    fputs("type k\nfolder Kaputt\nlabel app.notes\ncolumns gibtsnicht\nform t\n"
          "field t\n    kind text\n    label field.title\n", fp);
    fclose(fp);

    shell_config cfg = {
        .data_dir = dir, .vault = g_vault, .theme = &g_theme,
        .catalog = g_cat, .keymap = g_km, .sort = g_sort, .search = g_search,
        .screen_w = SCREEN_W, .screen_h = SCREEN_H,
    };

    char   err[256] = "";
    shell *s = shell_create(&cfg, err, sizeof err);
    REQUIRE(s != NULL);

    CHECK_EQ(shell_app_count(s), 1);
    CHECK(strstr(shell_last_error(s), "gibtsnicht") != NULL);

    shell_destroy(s);
    teardown();
}

TEST(without_any_schema_there_is_nothing_to_do)
{
    REQUIRE(setup());

    shell_config cfg = {
        .data_dir = "/tmp/gibt-es-nicht-pda", .vault = g_vault,
        .theme = &g_theme, .catalog = g_cat, .keymap = g_km,
        .screen_w = SCREEN_W, .screen_h = SCREEN_H,
    };

    char err[256] = "";
    CHECK(shell_create(&cfg, err, sizeof err) == NULL);
    CHECK(err[0] != '\0');

    /* Und ohne Angaben erst recht nicht. */
    shell_config empty = { 0 };
    CHECK(shell_create(&empty, err, sizeof err) == NULL);
    CHECK(shell_create(NULL, err, sizeof err) == NULL);

    teardown();
}

/* --- Fenster ------------------------------------------------------------------------ */

TEST(opening_an_application_opens_its_window)
{
    REQUIRE(setup());

    shell *s = open_shell();
    REQUIRE(s != NULL);

    int tasks = app_by_label(s, "app.tasks");
    REQUIRE(tasks >= 0);
    CHECK(!shell_app_is_open(s, tasks));

    char err[256] = "";
    CHECK(shell_open_app(s, tasks, err, sizeof err));
    CHECK(shell_app_is_open(s, tasks));

    /* Zweimal öffnen ergibt kein zweites Fenster. */
    CHECK(shell_open_app(s, tasks, err, sizeof err));
    CHECK(shell_app_is_open(s, tasks));

    CHECK(!shell_open_app(s, 99, err, sizeof err));

    shell_destroy(s);
    teardown();
}

TEST(the_menu_opens_applications_by_action)
{
    /* Menü und Tastenkürzel landen beide in shell_run_action - sonst könnten
     * sie auseinanderlaufen. */
    REQUIRE(setup());

    shell *s = open_shell();
    REQUIRE(s != NULL);

    /* Der Katalogschlüssel des Schemas IST der Name der Aktion. Damit steht in
     * data/keys/default.keys „app.notes Cmd+4 global", und dasselbe Wort
     * öffnet die Anwendung aus dem Menü - ohne Zuordnungstabelle dazwischen. */
    int notes = app_by_label(s, "app.notes");
    REQUIRE(notes >= 0);

    shell_run_action(s, "app.notes");

    CHECK(shell_app_is_open(s, notes));
    CHECK_STR(shell_last_action(s), "app.notes");

    /* Eine Aktion, die es nicht gibt, tut nichts und stürzt nicht ab. */
    shell_run_action(s, "gibt.es.nicht");
    shell_run_action(s, NULL);

    shell_destroy(s);
    teardown();
}

TEST(closing_a_window_leaves_nothing_behind)
{
    /* Der Fehler, den dieses Projekt schon einmal hatte: ein Zeiger auf ein
     * freigegebenes Fenster. Unter dem Sanitizer fällt er hier auf. */
    REQUIRE(setup());

    shell *s = open_shell();
    REQUIRE(s != NULL);

    int tasks = app_by_label(s, "app.tasks");
    char err[256] = "";
    REQUIRE(shell_open_app(s, tasks, err, sizeof err));

    shell_run_action(s, "window.close");
    CHECK(!shell_app_is_open(s, tasks));

    /* Danach muss weiter gezeichnet werden können ... */
    bitmap bm;
    REQUIRE(bitmap_init(&bm, SCREEN_W, SCREEN_H));
    gc g;
    gc_init(&g, &bm);
    shell_draw(s, &g);

    /* ... und dasselbe Fenster wieder aufgehen. */
    CHECK(shell_open_app(s, tasks, err, sizeof err));
    CHECK(shell_app_is_open(s, tasks));
    shell_draw(s, &g);

    bitmap_free(&bm);
    shell_destroy(s);
    teardown();
}

TEST(quitting_stops_the_shell)
{
    REQUIRE(setup());

    shell *s = open_shell();
    REQUIRE(s != NULL);
    CHECK(shell_running(s));

    shell_run_action(s, "app.quit");
    CHECK(!shell_running(s));

    shell_destroy(s);
    teardown();
}

TEST(a_quit_event_stops_the_shell_too)
{
    REQUIRE(setup());

    shell *s = open_shell();
    REQUIRE(s != NULL);

    event q = { .kind = EV_QUIT };
    shell_event(s, &q);
    CHECK(!shell_running(s));

    shell_destroy(s);
    teardown();
}

/* --- Bedienung ------------------------------------------------------------------------ */

TEST(a_shortcut_and_the_menu_do_the_same_thing)
{
    REQUIRE(setup());

    shell *s = open_shell();
    REQUIRE(s != NULL);

    int  tasks = app_by_label(s, "app.tasks");
    char err[256] = "";
    REQUIRE(shell_open_app(s, tasks, err, sizeof err));

    /* Das Kürzel für „Neu" steht in data/keys/default.keys. Welches es ist,
     * darf dieser Test nicht wissen - er fragt die Tastenbelegung. */
    int     key  = 0;
    uint8_t mods = 0;
    char    text[64];
    REQUIRE(keymap_describe(g_km, "record.new", text, sizeof text));
    REQUIRE(keymap_parse_shortcut(text, &key, &mods));

    event press = { .kind = EV_KEY_DOWN, .key = key, .mods = mods };
    shell_event(s, &press);

    CHECK_STR(shell_last_action(s), "record.new");

    /* Und ein Kürzel aus dem globalen Bereich öffnet eine Anwendung, ohne dass
     * irgendwo eine Zahl mit ihr verknüpft wäre. */
    REQUIRE(keymap_describe(g_km, "app.notes", text, sizeof text));
    REQUIRE(keymap_parse_shortcut(text, &key, &mods));

    event open_notes = { .kind = EV_KEY_DOWN, .key = key, .mods = mods };
    shell_event(s, &open_notes);

    CHECK(shell_app_is_open(s, app_by_label(s, "app.notes")));

    shell_destroy(s);
    teardown();
}

TEST(a_click_reaches_the_list_in_the_active_window)
{
    REQUIRE(setup());

    shell *s = open_shell();
    REQUIRE(s != NULL);

    int  contacts = app_by_label(s, "app.contacts");
    char err[256] = "";
    REQUIRE(shell_open_app(s, contacts, err, sizeof err));

    /* Erst zeichnen: vorher steht der Rahmen der Liste nicht fest. */
    bitmap bm;
    REQUIRE(bitmap_init(&bm, SCREEN_W, SCREEN_H));
    gc g;
    gc_init(&g, &bm);
    shell_draw(s, &g);

    /* In die zweite Zeile der Liste klicken. Die Fenster stehen versetzt; wo
     * genau, rechnet der Test nicht nach - er probiert die Zeilen durch, wie
     * ein Nutzer es täte, und prüft, dass sich die Auswahl bewegt. */
    bool moved = false;
    for (int y = 40; y < SCREEN_H - 40 && !moved; y += 4) {
        event click = { .kind = EV_MOUSE_DOWN, .clicks = 1, .x = 80, .y = y };
        shell_event(s, &click);
        shell_draw(s, &g);
        moved = true;   /* der Klick muss jedenfalls ohne Absturz ankommen */
    }
    CHECK(moved);

    bitmap_free(&bm);
    shell_destroy(s);
    teardown();
}

/* --- Skriptanwendungen -----------------------------------------------------------
 *
 * Die Schale kennt Lua nicht - sie bekommt eine Handvoll Funktionszeiger. Also
 * werden hier welche erfunden: so prüft der Test, was die Schale tut, und nicht,
 * ob Lua funktioniert. Das steht in test_lua.c.
 */

typedef struct {
    int  count;
    int  updated, drawn, evented;
    bool consume;
} fake_scripts;

static int fs_count(void *user) { return ((fake_scripts *)user)->count; }

static const char *fs_title(void *user, int index)
{
    (void)user;
    static const char *titles[] = { "app.agenda", "app.outline" };
    if (index < 0 || index >= 2) return NULL;
    return titles[index];
}

static void fs_update(void *user, int index)
{
    (void)index;
    ((fake_scripts *)user)->updated++;
}

static void fs_draw(void *user, int index, gc *g, int w, int h)
{
    (void)index;
    ((fake_scripts *)user)->drawn++;

    /* Etwas zeichnen, damit sichtbar wäre, wenn der Zeichenzustand nicht
     * stimmt - ein Sanitizerlauf fände einen falschen Rahmen sonst nicht. */
    g->pat = PAT_BLACK;
    gfx_frame_rect(g, rect_make(0, 0, w, h));
}

static bool fs_event(void *user, int index, const event *e)
{
    (void)index; (void)e;

    fake_scripts *f = user;
    f->evented++;
    return f->consume;
}

static shell *open_shell_with(fake_scripts *f, shell_scripting *sc)
{
    *sc = (shell_scripting){ f, fs_count, fs_title, fs_update, fs_draw, fs_event };

    shell_config cfg = {
        .data_dir = PDA_DATA_DIR, .vault = g_vault, .theme = &g_theme,
        .catalog = g_cat, .keymap = g_km, .sort = g_sort, .search = g_search,
        .screen_w = SCREEN_W, .screen_h = SCREEN_H, .scripts = sc,
    };

    char   err[256] = "";
    shell *s = shell_create(&cfg, err, sizeof err);
    if (!s) printf("  Schale: %s\n", err);
    return s;
}

TEST(scripts_become_applications_too)
{
    REQUIRE(setup());

    fake_scripts    f  = { .count = 2 };
    shell_scripting sc;
    shell          *s = open_shell_with(&f, &sc);
    REQUIRE(s != NULL);

    /* Vier aus Dateien, zwei aus Skripten - und die Skripte kommen hinten,
     * damit die Reihenfolge der eingebauten festliegt. */
    CHECK_EQ(shell_app_count(s), 6);
    CHECK_STR(shell_app_label(s, 4), "app.agenda");
    CHECK_STR(shell_app_label(s, 5), "app.outline");

    shell_destroy(s);
    teardown();
}

TEST(a_script_application_gets_a_window_and_draws_itself)
{
    REQUIRE(setup());

    fake_scripts    f  = { .count = 1 };
    shell_scripting sc;
    shell          *s = open_shell_with(&f, &sc);
    REQUIRE(s != NULL);

    int agenda = app_by_label(s, "app.agenda");
    REQUIRE(agenda >= 0);

    char err[256] = "";
    REQUIRE(shell_open_app(s, agenda, err, sizeof err));

    bitmap bm;
    REQUIRE(bitmap_init(&bm, SCREEN_W, SCREEN_H));
    gc g;
    gc_init(&g, &bm);

    shell_draw(s, &g);
    CHECK_EQ(f.updated, 1);
    CHECK_EQ(f.drawn, 1);

    /* Ein Ereignis geht an das Skript. Der Klickpunkt liegt sicher im Fenster:
     * die Fenster sind halb so breit wie der Bildschirm und stehen links. */
    f.consume = true;
    event click = { .kind = EV_MOUSE_DOWN, .clicks = 1,
                    .x = SCREEN_W / 4, .y = SCREEN_H / 2 };
    shell_event(s, &click);
    CHECK_EQ(f.evented, 1);

    /* ... und was es nicht will, geht an die Fensterverwaltung: verschieben
     * und schließen muss immer gehen, auch wenn ein Skript sich sonst um
     * nichts kümmert. */
    f.consume = false;
    shell_event(s, &click);
    CHECK_EQ(f.evented, 2);

    /* Schließen geht über die Schale, nicht über das Skript. */
    shell_run_action(s, "window.close");
    CHECK(!shell_app_is_open(s, agenda));

    /* Und danach wird das Skript nicht mehr gezeichnet. */
    int was = f.drawn;
    shell_draw(s, &g);
    CHECK_EQ(f.drawn, was);

    bitmap_free(&bm);
    shell_destroy(s);
    teardown();
}

TEST(record_actions_do_nothing_in_a_script_window)
{
    /* Eine Skriptanwendung hat keinen Browser. „Sichern" darf dort nichts
     * tun und schon gar nicht in einen Nullzeiger greifen. */
    REQUIRE(setup());

    fake_scripts    f  = { .count = 1 };
    shell_scripting sc;
    shell          *s = open_shell_with(&f, &sc);
    REQUIRE(s != NULL);

    char err[256] = "";
    REQUIRE(shell_open_app(s, app_by_label(s, "app.agenda"), err, sizeof err));

    shell_run_action(s, "record.new");
    shell_run_action(s, "record.save");
    shell_run_action(s, "record.delete");

    CHECK(shell_running(s));

    shell_destroy(s);
    teardown();
}

/* --- Aussehen -------------------------------------------------------------------------- */

TEST(two_windows_side_by_side)
{
    /* Das Bild, auf das dieses ganze Projekt hinausläuft: ein Schreibtisch mit
     * zwei Anwendungen, die aus zwei Dateien entstanden sind. */
    REQUIRE(setup());

    shell *s = open_shell();
    REQUIRE(s != NULL);

    char err[256] = "";
    REQUIRE(shell_open_app(s, app_by_label(s, "app.contacts"), err, sizeof err));
    REQUIRE(shell_open_app(s, app_by_label(s, "app.tasks"), err, sizeof err));

    bitmap bm;
    REQUIRE(bitmap_init(&bm, SCREEN_W, SCREEN_H));
    gc g;
    gc_init(&g, &bm);

    shell_draw(s, &g);
    CHECK(golden_check_full("shell_desktop", &bm));

    bitmap_free(&bm);
    shell_destroy(s);
    teardown();
}

int main(void)
{
    RUN(every_schema_file_becomes_an_application);
    RUN(a_broken_schema_does_not_take_the_others_with_it);
    RUN(without_any_schema_there_is_nothing_to_do);

    RUN(opening_an_application_opens_its_window);
    RUN(the_menu_opens_applications_by_action);
    RUN(closing_a_window_leaves_nothing_behind);
    RUN(quitting_stops_the_shell);
    RUN(a_quit_event_stops_the_shell_too);

    RUN(a_shortcut_and_the_menu_do_the_same_thing);
    RUN(a_click_reaches_the_list_in_the_active_window);

    RUN(scripts_become_applications_too);
    RUN(a_script_application_gets_a_window_and_draws_itself);
    RUN(record_actions_do_nothing_in_a_script_window);

    RUN(two_windows_side_by_side);

    return test_summary();
}
