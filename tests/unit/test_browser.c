/* Der generische Browser, siehe app/browser.h.
 *
 * Die Beweisführung dieser Datei: dieselben Aufrufe, drei verschiedene
 * Anwendungen, und der Unterschied kommt allein aus der Schemadatei. Wenn das
 * hier durchläuft, ist D-7 eingelöst.
 */
#include "test.h"

#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "app/browser.h"
#include "app/fieldkind.h"
#include "app/schema.h"
#include "core/collate.h"
#include "core/i18n.h"
#include "gfx/bitmap.h"
#include "gfx/draw.h"
#include "store/record.h"
#include "store/vault.h"
#include "support/golden.h"
#include "ui/theme.h"
#include "ui/widget.h"

#ifndef PDA_DATA_DIR
#define PDA_DATA_DIR "data"
#endif

/* --- Gerüst -------------------------------------------------------------------- */

static void temp_root(char *buf, size_t n)
{
    const char *dir = getenv("TMPDIR");
    if (!dir || !*dir) dir = "/tmp";

    size_t len = strlen(dir);
    while (len > 1 && dir[len - 1] == '/') len--;
    snprintf(buf, n, "%.*s/pda_browser_test", (int)len, dir);
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
static collate *g_sort;
static collate *g_search;

static bool setup(void)
{
    char path[512], err[256] = "";

    snprintf(path, sizeof path, "%s/themes/desktop.theme", PDA_DATA_DIR);
    if (!theme_load(&g_theme, path, err, sizeof err)) theme_defaults(&g_theme);

    snprintf(path, sizeof path, "%s/lang/de.strings", PDA_DATA_DIR);
    g_cat = i18n_load(path, err, sizeof err);

    snprintf(path, sizeof path, "%s/lang/de.sort", PDA_DATA_DIR);
    g_sort = collate_load(path, err, sizeof err);

    snprintf(path, sizeof path, "%s/collate/search.fold", PDA_DATA_DIR);
    g_search = collate_load(path, err, sizeof err);

    if (!g_cat || !g_sort || !g_search) printf("  Aufbau: %s\n", err);
    return g_cat && g_sort && g_search;
}

static void teardown(void)
{
    i18n_free(g_cat);
    collate_free(g_sort);
    collate_free(g_search);
    g_cat = NULL;
    g_sort = g_search = NULL;
}

static bool load_schema(schema *s, const char *name)
{
    char path[512], err[256] = "";
    snprintf(path, sizeof path, "%s/schema/%s.schema", PDA_DATA_DIR, name);

    if (!schema_load(s, path, err, sizeof err)) { printf("  %s\n", err); return false; }
    return true;
}

/* Legt einen frischen Vault an und schreibt die Datensätze hinein. */
static vault *fresh_vault(const char *collection, const char *const *texts, int n)
{
    char root[600], err[256] = "";
    temp_root(root, sizeof root);
    rmrf(root);

    vault *v = vault_open(root, err, sizeof err);
    if (!v) { printf("  Vault: %s\n", err); return NULL; }

    for (int i = 0; i < n; i++) {
        record *r = record_parse(texts[i], strlen(texts[i]), "test", err, sizeof err);
        if (!r) { printf("  Datensatz %d: %s\n", i, err); vault_close(v); return NULL; }

        char id[RECORD_ID_LEN + 1];
        if (!vault_save(v, collection, r, id, sizeof id, err, sizeof err))
            printf("  Speichern: %s\n", err);
        record_free(r);
    }
    return v;
}

static void drop_vault(vault *v)
{
    char root[600];
    temp_root(root, sizeof root);
    vault_close(v);
    rmrf(root);
}

/* Drei Aufgaben. Absichtlich in einer Reihenfolge, die nicht die sortierte
 * ist - sonst prüfte die Sortierung nichts. */
static const char *const TASKS[] = {
    "---\nid: 20260101T090000-0001\ntitle: Zander anrufen\ndue: 2026-05-01\n"
    "priority: 3\ncategory: arbeit\ndone: no\n---\nWegen des Angebots.\n",

    "---\nid: 20260102T090000-0001\ntitle: Müller anrufen\ndue: 2026-03-15\n"
    "priority: 1\ncategory: arbeit\ndone: no\n---\nWegen der Lieferung aus Köln.\n",

    "---\nid: 20260103T090000-0001\ntitle: Milch kaufen\ndue: 2026-02-01\n"
    "priority: 5\ncategory: einkauf\ndone: yes\n---\nUnd Brot.\n",
};

static const char *const CONTACTS[] = {
    "---\nid: 20260201T090000-0001\nname: Öhler\ncity: Aachen\nphone: 0241-1\n---\nNachbarin.\n",
    "---\nid: 20260202T090000-0001\nname: Mulde\ncity: Bonn\nphone: 0228-2\n---\nVerein.\n",
};

static browser *open_browser(const schema *s, vault *v)
{
    browser *b = browser_create(s, v, &g_theme, g_cat, g_sort, g_search);
    if (!b) return NULL;

    char err[256] = "";
    if (!browser_reload(b, err, sizeof err)) {
        printf("  Laden: %s\n", err);
        browser_destroy(b);
        return NULL;
    }
    return b;
}

/* --- Dieselben Aufrufe, drei Anwendungen ----------------------------------------- */

TEST(the_same_calls_give_three_different_applications)
{
    REQUIRE(setup());

    /* Aufgaben. */
    schema task;
    REQUIRE(load_schema(&task, "task"));
    vault   *v = fresh_vault(task.folder, TASKS, 3);
    REQUIRE(v != NULL);
    browser *b = open_browser(&task, v);
    REQUIRE(b != NULL);

    CHECK_EQ(browser_count(b), 3);
    /* Sortiert nach „due" - Milch am 01.02. steht vorn, nicht als Erstes
     * eingelesen. */
    CHECK(strstr(browser_row_text(b, 0), "Milch") != NULL);
    CHECK(strstr(browser_row_text(b, 2), "Zander") != NULL);

    /* Die Spalten des Aufgabenschemas: done, title, due. Also steht in der
     * Zeile ein „Ja" und ein deutsches Datum. */
    CHECK(strstr(browser_row_text(b, 0), "Ja") != NULL);
    CHECK(strstr(browser_row_text(b, 0), "01.02.2026") != NULL);

    browser_destroy(b);
    drop_vault(v);

    /* Kontakte - dieselben Aufrufe, anderes Schema. */
    schema contact;
    REQUIRE(load_schema(&contact, "contact"));
    v = fresh_vault(contact.folder, CONTACTS, 2);
    REQUIRE(v != NULL);
    b = open_browser(&contact, v);
    REQUIRE(b != NULL);

    CHECK_EQ(browser_count(b), 2);
    /* Sortiert nach „name" mit deutscher Faltung: Mulde vor Öhler. */
    CHECK(strstr(browser_row_text(b, 0), "Mulde") != NULL);
    CHECK(strstr(browser_row_text(b, 1), "Öhler") != NULL);

    /* Und die Spalten sind die des Kontaktschemas: name, city, phone. */
    CHECK(strstr(browser_row_text(b, 0), "Bonn") != NULL);
    CHECK(strstr(browser_row_text(b, 0), "0228-2") != NULL);
    CHECK(strstr(browser_row_text(b, 0), "Verein") == NULL);   /* keine Spalte */

    browser_destroy(b);
    drop_vault(v);
    teardown();
}

TEST(sorting_follows_the_schema_and_the_collation)
{
    REQUIRE(setup());

    schema contact;
    REQUIRE(load_schema(&contact, "contact"));

    static const char *const names[] = {
        "---\nid: 20260201T090000-0001\nname: Zander\n---\n",
        "---\nid: 20260202T090000-0001\nname: Öhler\n---\n",
        "---\nid: 20260203T090000-0001\nname: Müller\n---\n",
        "---\nid: 20260204T090000-0001\nname: Mulde\n---\n",
    };
    vault *v = fresh_vault(contact.folder, names, 4);
    REQUIRE(v != NULL);

    browser *b = open_browser(&contact, v);
    REQUIRE(b != NULL);

    /* Mulde, Müller, Öhler, Zander - DIN 5007. Ohne die Faltung stünden
     * Müller und Öhler hinter Zander. */
    CHECK_EQ(browser_count(b), 4);
    CHECK(strstr(browser_row_text(b, 0), "Mulde") != NULL);
    CHECK(strstr(browser_row_text(b, 1), "Müller") != NULL);
    CHECK(strstr(browser_row_text(b, 2), "Öhler") != NULL);
    CHECK(strstr(browser_row_text(b, 3), "Zander") != NULL);

    browser_destroy(b);
    drop_vault(v);
    teardown();
}

TEST(the_filter_narrows_the_list)
{
    REQUIRE(setup());

    schema task;
    REQUIRE(load_schema(&task, "task"));
    vault *v = fresh_vault(task.folder, TASKS, 3);
    REQUIRE(v != NULL);

    browser *b = open_browser(&task, v);
    REQUIRE(b != NULL);

    char err[256] = "";

    /* Gefaltet gesucht: „koln" findet „Köln" im Körper. */
    CHECK(browser_set_filter(b, "koln"));
    CHECK(browser_reload(b, err, sizeof err));
    CHECK_EQ(browser_count(b), 1);
    CHECK(strstr(browser_row_text(b, 0), "Müller") != NULL);

    /* Filter weg, alles wieder da. */
    CHECK(browser_set_filter(b, NULL));
    CHECK(browser_reload(b, err, sizeof err));
    CHECK_EQ(browser_count(b), 3);

    browser_destroy(b);
    drop_vault(v);
    teardown();
}

/* --- Das Formular ------------------------------------------------------------------ */

TEST(the_form_shows_what_the_schema_lists)
{
    REQUIRE(setup());

    schema task;
    REQUIRE(load_schema(&task, "task"));
    vault *v = fresh_vault(task.folder, TASKS, 3);
    REQUIRE(v != NULL);

    browser *b = open_browser(&task, v);
    REQUIRE(b != NULL);

    CHECK_EQ(browser_view_of(b), BROWSE_LIST);

    char err[256] = "";
    CHECK(browser_open_selected(b, err, sizeof err));
    CHECK_EQ(browser_view_of(b), BROWSE_FORM);

    browser_cancel(b);
    CHECK_EQ(browser_view_of(b), BROWSE_LIST);

    browser_destroy(b);
    drop_vault(v);
    teardown();
}

TEST(editing_and_saving_keeps_the_same_record)
{
    REQUIRE(setup());

    schema task;
    REQUIRE(load_schema(&task, "task"));
    vault *v = fresh_vault(task.folder, TASKS, 3);
    REQUIRE(v != NULL);

    browser *b = open_browser(&task, v);
    REQUIRE(b != NULL);
    CHECK_EQ(browser_count(b), 3);

    char err[256] = "";
    CHECK(browser_open_selected(b, err, sizeof err));

    /* Der Datensatz behält seine Kennung - sonst würde aus dem Bearbeiten ein
     * zweiter Datensatz, und die Liste hätte danach vier Zeilen. */
    CHECK(browser_save(b, err, sizeof err));
    CHECK_EQ(browser_view_of(b), BROWSE_LIST);
    CHECK_EQ(browser_count(b), 3);

    browser_destroy(b);
    drop_vault(v);
    teardown();
}

TEST(a_new_record_appears_in_the_list)
{
    REQUIRE(setup());

    schema note;
    REQUIRE(load_schema(&note, "note"));

    static const char *const one[] = {
        "---\nid: 20260301T090000-0001\ntitle: Erste Notiz\n---\nInhalt.\n",
    };
    vault *v = fresh_vault(note.folder, one, 1);
    REQUIRE(v != NULL);

    browser *b = open_browser(&note, v);
    REQUIRE(b != NULL);
    CHECK_EQ(browser_count(b), 1);

    char err[256] = "";
    CHECK(browser_new(b, err, sizeof err));
    CHECK_EQ(browser_view_of(b), BROWSE_FORM);

    /* Ohne Titel geht es nicht - und das Formular bleibt offen, damit der
     * Nutzer sieht, was er getippt hat. */
    CHECK(!browser_save(b, err, sizeof err));
    CHECK_EQ(browser_view_of(b), BROWSE_FORM);
    CHECK(strstr(err, T(g_cat, "field.title")) != NULL);

    /* Jetzt mit Titel. Geschrieben wird über den Feldtyp, nicht an ihm vorbei. */
    const schema_field *title = schema_field_by_name(&note, "title");
    const schema_field *body  = schema_field_by_name(&note, "body");
    REQUIRE(title && body);

    widget *wt = browser_form_widget(b, "title");
    widget *wb = browser_form_widget(b, "body");
    REQUIRE(wt && wb);

    fieldkind_of(title)->write(title, g_cat, wt, "Zweite Notiz");
    fieldkind_of(body)->write(body, g_cat, wb, "Noch ein Inhalt.\n");

    CHECK(browser_save(b, err, sizeof err));
    CHECK_EQ(browser_view_of(b), BROWSE_LIST);
    CHECK_EQ(browser_count(b), 2);

    /* Und die Auswahl steht auf dem, was gerade entstanden ist. */
    const char *row = browser_row_text(b, browser_selected(b));
    REQUIRE(row != NULL);
    CHECK_STR(row, "Zweite Notiz");

    /* Der Körper ist im Datensatz gelandet, nicht als Feld im Front Matter -
     * ein Gemtext-Feld IST der Körper. */
    record *saved = vault_load(v, note.folder, browser_selected_id(b), err, sizeof err);
    REQUIRE(saved != NULL);
    CHECK_STR(record_body(saved), "Noch ein Inhalt.\n");
    CHECK(!frontmatter_has(record_fields(saved), "body"));
    record_free(saved);

    browser_destroy(b);
    drop_vault(v);
    teardown();
}

TEST(deleting_removes_it_from_the_vault)
{
    REQUIRE(setup());

    schema task;
    REQUIRE(load_schema(&task, "task"));
    vault *v = fresh_vault(task.folder, TASKS, 3);
    REQUIRE(v != NULL);

    browser *b = open_browser(&task, v);
    REQUIRE(b != NULL);

    char        err[256] = "";
    const char *id       = browser_selected_id(b);
    REQUIRE(id != NULL);

    char gone[RECORD_ID_LEN + 1];
    snprintf(gone, sizeof gone, "%s", id);

    CHECK(browser_delete_selected(b, err, sizeof err));
    CHECK_EQ(browser_count(b), 2);

    /* Und wirklich weg, nicht nur aus der Liste. */
    CHECK(vault_load(v, task.folder, gone, err, sizeof err) == NULL);

    browser_destroy(b);
    drop_vault(v);
    teardown();
}

TEST(an_empty_collection_is_not_an_error)
{
    REQUIRE(setup());

    schema note;
    REQUIRE(load_schema(&note, "note"));

    char root[600], err[256] = "";
    temp_root(root, sizeof root);
    rmrf(root);
    vault *v = vault_open(root, err, sizeof err);
    REQUIRE(v != NULL);

    browser *b = open_browser(&note, v);
    REQUIRE(b != NULL);

    CHECK_EQ(browser_count(b), 0);
    CHECK(browser_selected_id(b) == NULL);
    CHECK(!browser_open_selected(b, err, sizeof err));
    CHECK(!browser_delete_selected(b, err, sizeof err));

    /* Zeichnen muss trotzdem gehen. */
    bitmap bm;
    REQUIRE(bitmap_init(&bm, 200, 100));
    gc g;
    gc_init(&g, &bm);
    browser_layout(b, rect_make(0, 0, 200, 100));
    browser_draw(b, &g);
    bitmap_free(&bm);

    browser_destroy(b);
    drop_vault(v);
    teardown();
}

TEST(editing_keeps_the_body)
{
    /* Ein Gemtext-Feld IST der Körper. Läse der Browser es als Feld aus dem
     * Front Matter, käme beim Öffnen nichts an, und das Speichern schriebe
     * einen leeren Körper zurück - der Text wäre weg, ohne dass irgendwo eine
     * Meldung erschiene. */
    REQUIRE(setup());

    schema task;
    REQUIRE(load_schema(&task, "task"));
    vault *v = fresh_vault(task.folder, TASKS, 3);
    REQUIRE(v != NULL);

    browser *b = open_browser(&task, v);
    REQUIRE(b != NULL);

    char err[256] = "";
    browser_select(b, 1);                /* Müller anrufen */
    const char *id = browser_selected_id(b);
    REQUIRE(id != NULL);

    char keep[RECORD_ID_LEN + 1];
    snprintf(keep, sizeof keep, "%s", id);

    CHECK(browser_open_selected(b, err, sizeof err));
    CHECK(browser_save(b, err, sizeof err));

    record *saved = vault_load(v, task.folder, keep, err, sizeof err);
    REQUIRE(saved != NULL);
    CHECK(strstr(record_body(saved), "Lieferung") != NULL);
    record_free(saved);

    browser_destroy(b);
    drop_vault(v);
    teardown();
}

TEST(a_row_shows_the_data_and_does_not_translate_it)
{
    /* Ein Datensatz, der zufällig wie ein Katalogschlüssel heißt, muss
     * dastehen, wie er heißt. Ginge die Liste über T(), stünde hier „OK". */
    REQUIRE(setup());

    schema note;
    REQUIRE(load_schema(&note, "note"));

    static const char *const tricky[] = {
        "---\nid: 20260401T090000-0001\ntitle: button.ok\n---\nnichts\n",
    };
    vault *v = fresh_vault(note.folder, tricky, 1);
    REQUIRE(v != NULL);

    browser *b = open_browser(&note, v);
    REQUIRE(b != NULL);

    CHECK_EQ(browser_count(b), 1);
    CHECK_STR(browser_row_text(b, 0), "button.ok");

    /* Und die Liste zeigt es auch so an. „button.ok" steht wirklich im
     * Katalog und ergäbe „OK" - dass hier trotzdem der Titel dasteht, ist der
     * ganze Unterschied zwischen den beiden Füllfunktionen. */
    CHECK_STR(list_item_text(browser_list(b), 0), "button.ok");
    CHECK_STR(T(g_cat, "button.ok"), "OK");

    browser_destroy(b);
    drop_vault(v);
    teardown();
}

TEST(a_multiline_column_shows_only_its_first_line)
{
    /* Ein Körper geht über mehrere Zeilen. Stünde er ungekürzt in einer
     * Listenzeile, ginge das Zeichnen über den Rand hinaus oder schriebe den
     * Umbruch als Kästchen. */
    REQUIRE(setup());

    const char *path = "/tmp/pda_browser_body.schema";
    FILE       *fp   = fopen(path, "wb");
    REQUIRE(fp != NULL);
    fputs("type n\nfolder Notizen\nlabel app.notes\nsort title\n"
          "columns title body\nform title body\n"
          "field title\n    kind text\n    label field.title\n"
          "field body\n    kind gemtext\n    label field.notes\n", fp);
    fclose(fp);

    schema s;
    char   err[256] = "";
    REQUIRE(schema_load(&s, path, err, sizeof err));

    static const char *const notes[] = {
        "---\nid: 20260501T090000-0001\ntitle: Titel\n---\nerste Zeile\nzweite Zeile\n",
    };
    vault *v = fresh_vault(s.folder, notes, 1);
    REQUIRE(v != NULL);

    browser *b = open_browser(&s, v);
    REQUIRE(b != NULL);

    const char *row = browser_row_text(b, 0);
    REQUIRE(row != NULL);
    CHECK(strstr(row, "erste Zeile") != NULL);
    CHECK(strstr(row, "zweite Zeile") == NULL);
    CHECK(strchr(row, '\n') == NULL);

    browser_destroy(b);
    drop_vault(v);
    teardown();
}

TEST(an_input_that_does_not_fit_its_kind_is_refused)
{
    REQUIRE(setup());

    schema task;
    REQUIRE(load_schema(&task, "task"));
    vault *v = fresh_vault(task.folder, TASKS, 3);
    REQUIRE(v != NULL);

    browser *b = open_browser(&task, v);
    REQUIRE(b != NULL);

    char err[256] = "";
    CHECK(browser_open_selected(b, err, sizeof err));

    /* „morgen" ist kein Datum. Das Formular bleibt offen, damit der Nutzer
     * sieht, was er getippt hat - stillschweigend etwas anderes zu speichern
     * wäre der schlimmere Ausgang. */
    widget *due = browser_form_widget(b, "due");
    REQUIRE(due != NULL);
    text_widget_set_value(due, "morgen");

    CHECK(!browser_save(b, err, sizeof err));
    CHECK_EQ(browser_view_of(b), BROWSE_FORM);
    CHECK(strstr(err, T(g_cat, "field.due")) != NULL);

    browser_destroy(b);
    drop_vault(v);
    teardown();
}

TEST(a_newline_smuggled_into_a_scalar_is_refused)
{
    /* Der gefährliche Fall: der zweite Teil sähe im Front Matter wie ein
     * eigenes Feld aus. Ohne Prüfung entstünde beim Speichern ein Feld, das
     * niemand angelegt hat. */
    REQUIRE(setup());

    schema task;
    REQUIRE(load_schema(&task, "task"));
    vault *v = fresh_vault(task.folder, TASKS, 3);
    REQUIRE(v != NULL);

    browser *b = open_browser(&task, v);
    REQUIRE(b != NULL);

    char err[256] = "";
    CHECK(browser_open_selected(b, err, sizeof err));

    widget *title = browser_form_widget(b, "title");
    REQUIRE(title != NULL);
    text_widget_set_value(title, "harmlos\nheimlich: eingeschmuggelt");

    CHECK(!browser_save(b, err, sizeof err));
    CHECK_EQ(browser_view_of(b), BROWSE_FORM);

    browser_destroy(b);
    drop_vault(v);
    teardown();
}

TEST(empty_fields_are_not_written_at_all)
{
    /* Eine Zeile „due:" ohne Wert wäre kein Fehler, aber sie stünde in der
     * Datei des Nutzers und sagte nichts. Was leer ist, wird weggelassen. */
    REQUIRE(setup());

    schema task;
    REQUIRE(load_schema(&task, "task"));

    char root[600], err[256] = "";
    temp_root(root, sizeof root);
    rmrf(root);
    vault *v = vault_open(root, err, sizeof err);
    REQUIRE(v != NULL);

    browser *b = open_browser(&task, v);
    REQUIRE(b != NULL);

    CHECK(browser_new(b, err, sizeof err));

    const schema_field *tf = schema_field_by_name(&task, "title");
    REQUIRE(tf != NULL);
    fieldkind_of(tf)->write(tf, g_cat, browser_form_widget(b, "title"), "Nur ein Titel");

    CHECK(browser_save(b, err, sizeof err));
    CHECK_EQ(browser_count(b), 1);

    record *saved = vault_load(v, task.folder, browser_selected_id(b), err, sizeof err);
    REQUIRE(saved != NULL);

    frontmatter *fm = record_fields(saved);
    CHECK(frontmatter_has(fm, "title"));
    CHECK(!frontmatter_has(fm, "due"));
    CHECK(!frontmatter_has(fm, "category"));

    /* „done" ist ein Kästchen und liefert immer einen Wert - „no" ist nicht
     * leer, sondern eine Antwort. */
    CHECK(frontmatter_has(fm, "done"));
    record_free(saved);

    browser_destroy(b);
    drop_vault(v);
    teardown();
}

/* --- Aussehen ---------------------------------------------------------------------- */

TEST(two_applications_from_two_files)
{
    /* Das Bild zu D-7: links Aufgaben, rechts Kontakte, und zwischen beiden
     * liegt keine Zeile Programmcode - nur zwei Schemadateien. */
    REQUIRE(setup());

    bitmap bm;
    REQUIRE(bitmap_init(&bm, 460, 120));
    gc g;
    gc_init(&g, &bm);
    g.pat = PAT_WHITE;
    gfx_fill_rect(&g, rect_make(0, 0, 460, 120));

    schema task;
    REQUIRE(load_schema(&task, "task"));
    vault *v = fresh_vault(task.folder, TASKS, 3);
    REQUIRE(v != NULL);
    browser *b = open_browser(&task, v);
    REQUIRE(b != NULL);

    browser_layout(b, rect_make(8, 8, 240, 104));
    browser_draw(b, &g);

    browser_destroy(b);
    drop_vault(v);

    schema contact;
    REQUIRE(load_schema(&contact, "contact"));
    v = fresh_vault(contact.folder, CONTACTS, 2);
    REQUIRE(v != NULL);
    b = open_browser(&contact, v);
    REQUIRE(b != NULL);

    browser_layout(b, rect_make(260, 8, 192, 104));
    browser_draw(b, &g);

    CHECK(golden_check("browser_two_apps", &bm));

    browser_destroy(b);
    drop_vault(v);
    bitmap_free(&bm);
    teardown();
}

int main(void)
{
    RUN(the_same_calls_give_three_different_applications);
    RUN(sorting_follows_the_schema_and_the_collation);
    RUN(the_filter_narrows_the_list);

    RUN(the_form_shows_what_the_schema_lists);
    RUN(editing_and_saving_keeps_the_same_record);
    RUN(a_new_record_appears_in_the_list);
    RUN(deleting_removes_it_from_the_vault);
    RUN(an_empty_collection_is_not_an_error);

    RUN(editing_keeps_the_body);
    RUN(a_row_shows_the_data_and_does_not_translate_it);
    RUN(a_multiline_column_shows_only_its_first_line);
    RUN(an_input_that_does_not_fit_its_kind_is_refused);
    RUN(a_newline_smuggled_into_a_scalar_is_refused);
    RUN(empty_fields_are_not_written_at_all);

    RUN(two_applications_from_two_files);

    return test_summary();
}
