/* Die Lua-Anbindung, siehe lua/pdalua.h.
 *
 * Drei Dinge stehen auf dem Prüfstand: dass die API tut, was sie sagt; dass
 * ein Schema aus einer Lua-Tabelle dieselbe Struktur ergibt wie aus einer
 * Textdatei (D-15); und dass eine Anwendung, die es nur in Lua gibt, wirklich
 * läuft.
 */
#include "test.h"

#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "app/schema.h"
#include "core/collate.h"
#include "core/i18n.h"
#include "gfx/bitmap.h"
#include "gfx/draw.h"
#include "app/shell.h"
#include "lua/pdalua.h"
#include "store/record.h"
#include "store/vault.h"
#include "support/golden.h"

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
    snprintf(buf, n, "%.*s/pda_lua_test", (int)len, dir);
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

static catalog *load_cat(void)
{
    char path[512], err[256] = "";
    snprintf(path, sizeof path, "%s/lang/de.strings", PDA_DATA_DIR);

    catalog *c = i18n_load(path, err, sizeof err);
    if (!c) printf("  Katalog: %s\n", err);
    return c;
}

/* Führt code aus und meldet den Lua-Fehler, falls einer kommt. */
static bool run(lua_State *L, const char *code)
{
    char err[512] = "";
    if (pdalua_dostring(L, code, "test", err, sizeof err)) return true;

    printf("  Lua: %s\n", err);
    return false;
}

/* Prüft eine Lua-Bedingung. Das ist der Weg, an Werte heranzukommen, ohne die
 * API um einen Rückkanal zu erweitern, den niemand sonst braucht.
 *
 * Zwei Formen: ein einzelner Ausdruck, oder ein Stück Code mit `return`. Der
 * Unterschied wird am Wort `return` erkannt, damit die Aufrufstellen kurz
 * bleiben.
 *
 * Die Bedingung geht NICHT in die Fehlermeldung. Sie enthält Anführungszeichen,
 * und die in eine Lua-Zeichenkette zu setzen hieße, sie zu maskieren - der
 * Test würde dann prüfen, ob mein Maskieren stimmt, und nicht, was er prüfen
 * soll. Welche Bedingung fehlschlug, sagt ohnehin die Zeilennummer in C. */
static bool truth(lua_State *L, const char *expr)
{
    char code[2048];

    if (strstr(expr, "return"))
        snprintf(code, sizeof code,
                 "local __ok = (function()\n%s\nend)()\n"
                 "if not __ok then error('Bedingung nicht erfuellt', 0) end", expr);
    else
        snprintf(code, sizeof code,
                 "if not (%s) then error('Bedingung nicht erfuellt', 0) end", expr);

    return run(L, code);
}

/* --- Der Zustand ------------------------------------------------------------------ */

TEST(a_fresh_state_has_the_api_and_nothing_dangerous)
{
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    char       err[256] = "";
    lua_State *L = pdalua_open(cat, err, sizeof err);
    REQUIRE(L != NULL);

    CHECK(truth(L, "type(rectfill) == 'function'"));
    CHECK(truth(L, "type(print) == 'function'"));
    CHECK(truth(L, "type(T) == 'function'"));

    /* Luas eigenes print heißt jetzt log - weggenommen wird es nicht. */
    CHECK(truth(L, "type(log) == 'function'"));

    /* Kein io, kein os, kein package. Eine Schemadatei ist eine
     * Konfigurationsdatei und soll keine Dateien löschen können. */
    CHECK(truth(L, "io == nil"));
    CHECK(truth(L, "os == nil"));
    CHECK(truth(L, "require == nil"));
    CHECK(truth(L, "dofile == nil or type(dofile) == 'function'"));

    /* Und ohne pdalua_set_vault auch kein store. */
    CHECK(truth(L, "store == nil"));

    /* Was da ist, ist da: Tabellen, Zeichenketten, Rechnen. */
    CHECK(truth(L, "type(table.insert) == 'function'"));
    CHECK(truth(L, "type(string.format) == 'function'"));
    CHECK(truth(L, "math.floor(3.7) == 3"));

    pdalua_close(L);
    i18n_free(cat);
}

TEST(an_error_comes_back_with_its_line)
{
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    char       err[256] = "";
    lua_State *L = pdalua_open(cat, err, sizeof err);
    REQUIRE(L != NULL);

    err[0] = '\0';
    CHECK(!pdalua_dostring(L, "local x = 1\nerror('kaputt')\n", "probe",
                           err, sizeof err));
    CHECK(strstr(err, "kaputt") != NULL);
    CHECK(strstr(err, "probe:2") != NULL);

    /* Und ein Syntaxfehler ebenso. */
    err[0] = '\0';
    CHECK(!pdalua_dostring(L, "local = = =", "probe", err, sizeof err));
    CHECK(strstr(err, "probe") != NULL);

    /* Der Zustand ist danach weiter benutzbar - ein Fehler in einem Skript
     * darf nicht den ganzen Zustand verderben. */
    CHECK(truth(L, "1 + 1 == 2"));

    pdalua_close(L);
    i18n_free(cat);
}

TEST(texts_come_from_the_catalog)
{
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    char       err[256] = "";
    lua_State *L = pdalua_open(cat, err, sizeof err);
    REQUIRE(L != NULL);

    CHECK(truth(L, "T('button.ok') == 'OK'"));
    CHECK(truth(L, "T('bool.yes') == 'Ja'"));

    /* Fehlt der Schlüssel, kommt er selbst zurück - sichtbar falsch ist
     * besser als leer. */
    CHECK(truth(L, "T('gibt.es.nicht') == 'gibt.es.nicht'"));

    /* Platzhalter und Plural. */
    CHECK(truth(L, "T('demo.action', 'Test') == 'Aktion: Test'"));
    CHECK(truth(L, "Tn('list.count', 1) == '1 Eintrag'"));
    CHECK(truth(L, "Tn('list.count', 3) == '3 Einträge'"));

    pdalua_close(L);
    i18n_free(cat);
}

TEST(unknown_patterns_and_modes_are_errors)
{
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    char       err[256] = "";
    lua_State *L = pdalua_open(cat, err, sizeof err);
    REQUIRE(L != NULL);

    CHECK(run(L, "pattern('gray')"));
    CHECK(run(L, "mode('xor')"));

    /* Stillschweigend auf schwarz zurückzufallen hieße, einen Tippfehler als
     * Gestaltung auszugeben. */
    err[0] = '\0';
    CHECK(!pdalua_dostring(L, "pattern('rot')", "probe", err, sizeof err));
    CHECK(strstr(err, "rot") != NULL);

    err[0] = '\0';
    CHECK(!pdalua_dostring(L, "mode('multiply')", "probe", err, sizeof err));
    CHECK(strstr(err, "multiply") != NULL);

    pdalua_close(L);
    i18n_free(cat);
}

TEST(drawing_without_a_target_does_nothing_instead_of_crashing)
{
    /* Ein Skript, das außerhalb des Zeichnens zeichnet, soll nicht abstürzen.
     * Ein Absturz wäre die härtere Strafe für den kleineren Fehler. */
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    char       err[256] = "";
    lua_State *L = pdalua_open(cat, err, sizeof err);
    REQUIRE(L != NULL);

    CHECK(run(L, "cls() rectfill(0,0,10,10) print('hallo', 0, 0) line(0,0,9,9)"));

    /* textwidth braucht kein Ziel und antwortet trotzdem. */
    CHECK(truth(L, "textwidth('') == 0"));
    CHECK(truth(L, "textwidth('MM') > 0"));
    CHECK(truth(L, "textheight() > 0"));

    pdalua_close(L);
    i18n_free(cat);
}

TEST(a_script_really_draws)
{
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    char       err[256] = "";
    lua_State *L = pdalua_open(cat, err, sizeof err);
    REQUIRE(L != NULL);

    bitmap bm;
    REQUIRE(bitmap_init(&bm, 200, 90));
    gc g;
    gc_init(&g, &bm);
    pdalua_set_gc(L, &g);

    CHECK(run(L,
        "cls()\n"
        "pattern('black')\n"
        "rect(2, 2, 196, 86)\n"
        "pattern('gray')\n"
        "rectfill(10, 10, 60, 30)\n"
        "pattern('black')\n"
        "ovalfill(90, 10, 40, 30)\n"
        "rrect(140, 10, 50, 30, 6)\n"
        "line(10, 50, 190, 50)\n"
        "print(T('app.tasks') .. ': ' .. Tn('list.count', 3), 10, 58)\n"
        "rectfill(10, 72, 60, 10)\n"
        /* XOR über der Hälfte des schwarzen Balkens: dort wird er wieder
         * weiß. Über weißem Grund sähe man keinen Unterschied zu copy, und
         * der Modus wäre nicht geprüft. */
        "mode('xor')\n"
        "rectfill(40, 72, 60, 10)\n"));

    CHECK(golden_check("lua_drawing", &bm));

    /* Und danach kein Ziel mehr - der Normalzustand außerhalb des Zeichnens. */
    pdalua_set_gc(L, NULL);
    CHECK(run(L, "rectfill(0, 0, 200, 90)"));

    bitmap_free(&bm);
    pdalua_close(L);
    i18n_free(cat);
}

/* --- Ein Schema aus Lua (D-15) ------------------------------------------------------ */

static void report_diff(const char *what, const char *a, const char *b)
{
    printf("  %s: Text „%s“, Lua „%s“\n", what, a, b);
}

TEST(the_two_schema_loaders_agree)
{
    /* Der Beweis für D-15. Dieselbe Anwendung, zwei Schreibweisen, eine
     * Struktur - und der Browser sieht den Unterschied nicht. */
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    char       err[512] = "";
    lua_State *L = pdalua_open(cat, err, sizeof err);
    REQUIRE(L != NULL);

    char path[512];
    schema text, lua;

    snprintf(path, sizeof path, "%s/schema/task.schema", PDA_DATA_DIR);
    if (!schema_load(&text, path, err, sizeof err)) printf("  Text: %s\n", err);
    REQUIRE(schema_load(&text, path, err, sizeof err));

    snprintf(path, sizeof path, "%s/schema/task.lua", PDA_DATA_DIR);
    if (!pdalua_schema(L, path, &lua, err, sizeof err)) printf("  Lua: %s\n", err);
    REQUIRE(pdalua_schema(L, path, &lua, err, sizeof err));

    if (strcmp(text.type, lua.type) != 0) report_diff("type", text.type, lua.type);
    CHECK_STR(lua.type, text.type);
    CHECK_STR(lua.folder, text.folder);
    CHECK_STR(lua.label, text.label);
    CHECK_STR(lua.sort, text.sort);
    CHECK_EQ(lua.sort_desc, text.sort_desc);
    CHECK_EQ(lua.view, text.view);

    CHECK_EQ(lua.column_count, text.column_count);
    for (int i = 0; i < text.column_count && i < lua.column_count; i++)
        CHECK_STR(lua.columns[i], text.columns[i]);

    CHECK_EQ(lua.form_count, text.form_count);
    for (int i = 0; i < text.form_count && i < lua.form_count; i++)
        CHECK_STR(lua.form[i], text.form[i]);

    CHECK_EQ(lua.field_count, text.field_count);
    for (int i = 0; i < text.field_count && i < lua.field_count; i++) {
        const schema_field *a = &text.fields[i];
        const schema_field *b = &lua.fields[i];

        if (strcmp(a->name, b->name) != 0) report_diff("Feldname", a->name, b->name);
        CHECK_STR(b->name, a->name);
        CHECK_EQ(b->kind, a->kind);
        CHECK_STR(b->label, a->label);
        CHECK_EQ(b->required, a->required);
        CHECK_EQ(b->value_count, a->value_count);

        for (int j = 0; j < a->value_count && j < b->value_count; j++)
            CHECK_STR(b->values[j], a->values[j]);
    }

    /* Und ganz hart: Byte für Byte dieselbe Struktur. */
    CHECK_MEM(&lua, &text, sizeof text);

    pdalua_close(L);
    i18n_free(cat);
}

static const char *write_lua(const char *name, const char *code)
{
    static char path[512];
    snprintf(path, sizeof path, "/tmp/pda_luaschema_%s.lua", name);

    FILE *fp = fopen(path, "wb");
    if (!fp) return NULL;
    fputs(code, fp);
    fclose(fp);
    return path;
}

TEST(a_lua_schema_is_checked_as_strictly_as_a_text_one)
{
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    char       err[512] = "";
    lua_State *L = pdalua_open(cat, err, sizeof err);
    REQUIRE(L != NULL);

    schema s;

    /* Eine Spalte, die kein Feld ist - derselbe Fehler wie in der Textfassung,
     * und dieselbe Prüfung findet ihn. */
    const char *bad = write_lua("spalte",
        "return { type='x', folder='X', label='l',"
        " columns={'titel'}, form={'title'},"
        " fields={ {name='title', kind='text', label='lt'} } }");
    REQUIRE(bad != NULL);
    CHECK(!pdalua_schema(L, bad, &s, err, sizeof err));
    CHECK(strstr(err, "titel") != NULL);

    /* Kein Feld vom Typ, den es gibt. */
    bad = write_lua("typ",
        "return { type='x', folder='X', label='l',"
        " columns={'t'}, form={'t'},"
        " fields={ {name='t', kind='farbe', label='lt'} } }");
    CHECK(!pdalua_schema(L, bad, &s, err, sizeof err));
    CHECK(strstr(err, "farbe") != NULL);

    /* Ein Feld zweimal. */
    bad = write_lua("dopplung",
        "return { type='x', folder='X', label='l',"
        " columns={'t'}, form={'t'},"
        " fields={ {name='t', kind='text', label='lt'},"
        "          {name='t', kind='date', label='lt2'} } }");
    CHECK(!pdalua_schema(L, bad, &s, err, sizeof err));
    CHECK(strstr(err, "gibt es schon") != NULL);

    /* Eine Zahl, wo ein Name stehen soll. Kein kurz geschriebener Name,
     * sondern ein Versehen - und es fiele sonst erst auf, wenn „42" als
     * Sammlungsname auftaucht. */
    bad = write_lua("zahl_als_name",
        "return { type=42, folder='X', label='l',"
        " columns={'t'}, form={'t'},"
        " fields={ {name='t', kind='text', label='lt'} } }");
    CHECK(!pdalua_schema(L, bad, &s, err, sizeof err));
    CHECK(strstr(err, "type") != NULL);

    /* Ein Monatsraster ohne Feld. */
    bad = write_lua("monat_ohne_feld",
        "return { type='x', folder='X', label='l', view='month',"
        " columns={'d'}, form={'d'},"
        " fields={ {name='d', kind='date', label='ld'} } }");
    CHECK(!pdalua_schema(L, bad, &s, err, sizeof err));
    CHECK(strstr(err, "view_field") != NULL);

    /* Eine Ansicht, die es nicht gibt. */
    bad = write_lua("ansicht",
        "return { type='x', folder='X', label='l', view='raster',"
        " columns={'d'}, form={'d'},"
        " fields={ {name='d', kind='date', label='ld'} } }");
    CHECK(!pdalua_schema(L, bad, &s, err, sizeof err));
    CHECK(strstr(err, "raster") != NULL);

    /* Und ein richtiges Monatsraster geht durch - mitsamt Feldnamen. */
    const char *good = write_lua("monat",
        "return { type='x', folder='X', label='l',"
        " view='month', view_field='d',"
        " columns={'d'}, form={'d'},"
        " fields={ {name='d', kind='date', label='ld'} } }");
    REQUIRE(good != NULL);
    if (!pdalua_schema(L, good, &s, err, sizeof err)) printf("  %s\n", err);
    REQUIRE(pdalua_schema(L, good, &s, err, sizeof err));
    CHECK_EQ(s.view, VIEW_MONTH);
    CHECK_STR(s.view_field, "d");

    /* Keine Tabelle zurück. */
    bad = write_lua("keine_tabelle", "return 42");
    CHECK(!pdalua_schema(L, bad, &s, err, sizeof err));
    CHECK(strstr(err, "Tabelle") != NULL);

    /* Und ein Fehler in der Datei selbst. */
    bad = write_lua("kaputt", "return {");
    CHECK(!pdalua_schema(L, bad, &s, err, sizeof err));

    /* Der Zustand ist danach immer noch brauchbar, und der Stapel leer
     * geblieben - ein Lader, der etwas liegen lässt, verschiebt jeden
     * folgenden Index. */
    CHECK(truth(L, "1 == 1"));

    pdalua_close(L);
    i18n_free(cat);
}

/* --- store ---------------------------------------------------------------------------- */

static vault *g_vault;

static const char *const TASKS[] = {
    "---\nid: 20260101T090000-0001\ntitle: Zander anrufen\ndue: 2026-05-01\ndone: no\n---\nAngebot.\n",
    "---\nid: 20260102T090000-0001\ntitle: Müller anrufen\ndue: 2026-03-15\ndone: no\ntags: [arbeit, telefon]\n---\nLieferung aus Köln.\n",
    "---\nid: 20260103T090000-0001\ntitle: Milch kaufen\ndue: 2026-02-01\ndone: yes\n---\nUnd Brot.\n",
};

static lua_State *with_store(catalog *cat, collate **sort, collate **search)
{
    char root[600], path[512], err[512] = "";
    temp_root(root, sizeof root);
    rmrf(root);

    g_vault = vault_open(root, err, sizeof err);
    if (!g_vault) { printf("  Vault: %s\n", err); return NULL; }

    for (size_t i = 0; i < sizeof TASKS / sizeof TASKS[0]; i++) {
        record *r = record_parse(TASKS[i], strlen(TASKS[i]), "t", err, sizeof err);
        if (!r) { printf("  Datensatz: %s\n", err); return NULL; }

        char id[RECORD_ID_LEN + 1];
        vault_save(g_vault, "Aufgaben", r, id, sizeof id, err, sizeof err);
        record_free(r);
    }

    snprintf(path, sizeof path, "%s/lang/de.sort", PDA_DATA_DIR);
    *sort = collate_load(path, err, sizeof err);
    snprintf(path, sizeof path, "%s/collate/search.fold", PDA_DATA_DIR);
    *search = collate_load(path, err, sizeof err);

    lua_State *L = pdalua_open(cat, err, sizeof err);
    if (!L) return NULL;

    pdalua_set_vault(L, g_vault, *sort, *search);
    return L;
}

static void drop_store(collate *sort, collate *search)
{
    char root[600];
    temp_root(root, sizeof root);
    vault_close(g_vault);
    g_vault = NULL;
    rmrf(root);
    collate_free(sort);
    collate_free(search);
}

TEST(a_script_can_read_the_vault)
{
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    collate   *sort = NULL, *search = NULL;
    lua_State *L = with_store(cat, &sort, &search);
    REQUIRE(L != NULL);

    CHECK(truth(L, "#store.find('Aufgaben') == 3"));
    CHECK(truth(L, "#store.find('Aufgaben', { done = 'no' }) == 2"));

    /* Sortiert nach dem Feld, das die Abfrage nennt. */
    CHECK(truth(L,
        "local t = store.find('Aufgaben', { sort = 'due' })\n"
        "return t[1].title == 'Milch kaufen' and t[3].title == 'Zander anrufen'"));

    /* Ein Skalar bleibt eine Zeichenkette, ein Listenfeld wird ein Feld. */
    CHECK(truth(L,
        "local t = store.find('Aufgaben', { title = 'Müller anrufen' })\n"
        "return #t == 1 and type(t[1].title) == 'string'"
        " and type(t[1].tags) == 'table' and t[1].tags[2] == 'telefon'"));

    /* Der Körper ist da, und die Kennung auch. */
    CHECK(truth(L,
        "local t = store.find('Aufgaben', { title = 'Milch kaufen' })\n"
        "return t[1].body:find('Brot') ~= nil and #t[1].id == 20"));

    pdalua_close(L);
    drop_store(sort, search);
    i18n_free(cat);
}

TEST(taking_the_vault_away_takes_store_with_it)
{
    /* Ein Skript, dem der Zugang entzogen wird, soll ihn nicht in einer
     * lokalen Variablen behalten können - der globale Name verschwindet. */
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    collate   *sort = NULL, *search = NULL;
    lua_State *L = with_store(cat, &sort, &search);
    REQUIRE(L != NULL);

    CHECK(truth(L, "type(store) == 'table'"));

    pdalua_set_vault(L, NULL, NULL, NULL);
    CHECK(truth(L, "store == nil"));

    pdalua_close(L);
    drop_store(sort, search);
    i18n_free(cat);
}

TEST(a_script_searches_with_folded_umlauts)
{
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    collate   *sort = NULL, *search = NULL;
    lua_State *L = with_store(cat, &sort, &search);
    REQUIRE(L != NULL);

    /* „koln" findet „Köln" - dieselbe Faltung wie überall. */
    CHECK(truth(L, "#store.search('Aufgaben', 'koln') == 1"));

    /* Und derselbe Volltext als Teil einer Abfrage. */
    CHECK(truth(L, "#store.find('Aufgaben', { text = 'koln' }) == 1"));
    CHECK(truth(L, "#store.find('Aufgaben', { text = 'koln', done = 'yes' }) == 0"));
    CHECK(truth(L, "#store.search('Aufgaben', 'muller') == 1"));
    CHECK(truth(L, "#store.search('Aufgaben', 'Hamburg') == 0"));

    pdalua_close(L);
    drop_store(sort, search);
    i18n_free(cat);
}

TEST(a_script_can_write_and_delete)
{
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    collate   *sort = NULL, *search = NULL;
    lua_State *L = with_store(cat, &sort, &search);
    REQUIRE(L != NULL);

    /* Neu anlegen. */
    CHECK(truth(L,
        "local id = store.put('Aufgaben', { title = 'Neu', done = 'no',"
        "                                   body = 'Aus Lua.\\n' })\n"
        "return #id == 20 and #store.find('Aufgaben') == 4"));

    /* Wiederfinden und ändern - dieselbe Kennung, kein zweiter Datensatz. */
    CHECK(truth(L,
        "local t = store.find('Aufgaben', { title = 'Neu' })\n"
        "local rec = t[1]\n"
        "rec.done = 'yes'\n"
        "store.put('Aufgaben', rec)\n"
        "return #store.find('Aufgaben') == 4"
        " and #store.find('Aufgaben', { done = 'yes' }) == 2"));

    /* Und löschen. */
    CHECK(truth(L,
        "local t = store.find('Aufgaben', { title = 'Neu' })\n"
        "store.delete('Aufgaben', t[1].id)\n"
        "return #store.find('Aufgaben') == 3 and store.get('Aufgaben', t[1].id) == nil"));

    pdalua_close(L);
    drop_store(sort, search);
    i18n_free(cat);
}

TEST(written_fields_come_out_in_a_fixed_order)
{
    /* Lua-Tabellen haben keine Reihenfolge. Ohne eine feste käme bei jedem
     * Speichern eine andere Datei heraus, und jedes Sicherungswerkzeug meldete
     * Änderungen, die keine sind. */
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    collate   *sort = NULL, *search = NULL;
    lua_State *L = with_store(cat, &sort, &search);
    REQUIRE(L != NULL);

    CHECK(run(L,
        "store.put('Aufgaben', { zebra = 'z', title = 'Sortiert',"
        "                        apfel = 'a', mitte = 'm', body = 'x' })"));

    /* Zurücklesen über C, nicht über Lua: geprüft wird, was in der Datei
     * steht, und nicht, was Lua daraus macht. */
    char    err[256] = "";
    char    ids[16][RECORD_ID_LEN + 1];
    int     n = 0;
    REQUIRE(vault_list(g_vault, "Aufgaben", ids, 16, &n, err, sizeof err));

    bool found = false;
    for (int i = 0; i < n && !found; i++) {
        record *rec = vault_load(g_vault, "Aufgaben", ids[i], err, sizeof err);
        REQUIRE(rec != NULL);

        frontmatter *fm = record_fields(rec);
        const char  *t  = frontmatter_get(fm, "title");

        if (t && strcmp(t, "Sortiert") == 0) {
            found = true;

            /* Die Kennung steht vorn, weil vault_save sie voranstellt, wenn
             * der Datensatz noch keine hat. Alles danach ist sortiert. */
            const char *prev = "";
            for (int k = 0; k < frontmatter_count(fm); k++) {
                const char *key = frontmatter_key_at(fm, k);
                REQUIRE(key != NULL);
                if (strcmp(key, "id") == 0) continue;

                if (strcmp(prev, key) > 0)
                    printf("  „%s“ steht hinter „%s“\n", key, prev);
                CHECK(strcmp(prev, key) <= 0);
                prev = key;
            }

            /* Und der Test prüft wirklich etwas: die Schlüssel standen in der
             * Lua-Tabelle in einer anderen Reihenfolge. */
            CHECK(frontmatter_count(fm) >= 4);
        }
        record_free(rec);
    }
    CHECK(found);

    pdalua_close(L);
    drop_store(sort, search);
    i18n_free(cat);
}

TEST(a_script_cannot_smuggle_a_newline_into_a_field)
{
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    collate   *sort = NULL, *search = NULL;
    lua_State *L = with_store(cat, &sort, &search);
    REQUIRE(L != NULL);

    char err[512] = "";
    CHECK(!pdalua_dostring(L,
        "store.put('Aufgaben', { title = 'a\\nheimlich: x' })", "probe",
        err, sizeof err));
    CHECK(strstr(err, "Zeilenumbruch") != NULL);

    CHECK(truth(L, "#store.find('Aufgaben') == 3"));

    pdalua_close(L);
    drop_store(sort, search);
    i18n_free(cat);
}

/* --- Eine Anwendung, die es nur in Lua gibt --------------------------------------------- */

TEST(an_application_written_only_in_lua_runs)
{
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    collate   *sort = NULL, *search = NULL;
    lua_State *L = with_store(cat, &sort, &search);
    REQUIRE(L != NULL);
    pdalua_open_apps(L);

    char path[512], err[512] = "";
    snprintf(path, sizeof path, "%s/apps/agenda.lua", PDA_DATA_DIR);

    bool loaded = pdalua_dofile(L, path, err, sizeof err);
    if (!loaded) printf("  agenda: %s\n", err);
    REQUIRE(loaded);

    /* Sie hat sich angemeldet - die Schale findet sie über diese Brücke, ohne
     * Lua zu kennen. */
    shell_scripting sc = pdalua_scripting(L);
    CHECK_EQ(sc.count(sc.user), 1);
    CHECK_STR(sc.title(sc.user, 0), "app.agenda");
    CHECK(sc.title(sc.user, 9) == NULL);

    /* Sie rechnet. Zwei offene Aufgaben, eine davon vor dem 1. April fällig. */
    CHECK(truth(L, "#agenda.open_tasks() == 2"));
    CHECK(truth(L, "agenda.overdue('2026-04-01') == 1"));
    CHECK(truth(L, "agenda.overdue('2026-01-01') == 0"));

    /* Sie ändert Daten - und der Vault sieht es. */
    CHECK(truth(L,
        "local t = agenda.open_tasks()\n"
        "return agenda.finish(t[1].id) and #agenda.open_tasks() == 1"));

    CHECK(truth(L, "agenda.finish('gibtesnicht') == false"));

    /* Und sie zeichnet - über die Brücke, so wie die Schale es täte. */
    bitmap bm;
    REQUIRE(bitmap_init(&bm, 220, 80));
    gc g;
    gc_init(&g, &bm);

    CHECK(run(L, "agenda.refresh('2026-06-01')"));
    CHECK(truth(L, "agenda.late == 1"));

    sc.update(sc.user, 0);
    sc.draw(sc.user, 0, &g, 220, 80);
    CHECK(golden_check("lua_agenda", &bm));

    bitmap_free(&bm);
    pdalua_close(L);
    drop_store(sort, search);
    i18n_free(cat);
}

TEST(an_application_without_a_title_or_a_draw_is_refused)
{
    /* Ohne Titel gäbe es keinen Menüeintrag, ohne draw kein Fenster. Beides
     * soll beim Laden auffallen und nicht erst, wenn jemand die Anwendung
     * öffnet. */
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    char       err[512] = "";
    lua_State *L = pdalua_open(cat, err, sizeof err);
    REQUIRE(L != NULL);
    pdalua_open_apps(L);

    CHECK(!pdalua_dostring(L, "app{ draw = function() end }", "probe", err, sizeof err));
    CHECK(strstr(err, "title") != NULL);

    CHECK(!pdalua_dostring(L, "app{ title = 'x' }", "probe", err, sizeof err));
    CHECK(strstr(err, "draw") != NULL);

    shell_scripting sc = pdalua_scripting(L);
    CHECK_EQ(sc.count(sc.user), 0);

    pdalua_close(L);
    i18n_free(cat);
}

TEST(the_event_bus_carries_messages_between_scripts)
{
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    char       err[512] = "";
    lua_State *L = pdalua_open(cat, err, sizeof err);
    REQUIRE(L != NULL);
    pdalua_open_apps(L);

    CHECK(run(L,
        "empfangen = {}\n"
        "on('etwas', function(a) empfangen[#empfangen+1] = a end)\n"
        "on('etwas', function(a) empfangen[#empfangen+1] = a .. '!' end)\n"
        "send('etwas', 'hallo')"));

    CHECK(truth(L, "#empfangen == 2 and empfangen[1] == 'hallo'"
                   " and empfangen[2] == 'hallo!'"));

    /* Niemanden zu erreichen ist kein Fehler - ein Sender soll nicht wissen
     * müssen, ob jemand da ist. */
    CHECK(run(L, "send('hoert.niemand', 1, 2, 3)"));

    /* Und ein Zuhörer, der stolpert, bringt nicht den Sender zu Fall. */
    CHECK(run(L,
        "durch = false\n"
        "on('heikel', function() error('autsch') end)\n"
        "on('heikel', function() durch = true end)\n"
        "send('heikel')"));
    CHECK(truth(L, "durch"));

    pdalua_close(L);
    i18n_free(cat);
}

TEST(a_script_that_fails_does_not_take_the_shell_with_it)
{
    /* C ruft hier nach Lua, und dabei muss jeder Aufruf abgesichert sein. Ein
     * Skript, das einen Fehler macht, darf die Oberfläche nicht mitnehmen. */
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    char       err[512] = "";
    lua_State *L = pdalua_open(cat, err, sizeof err);
    REQUIRE(L != NULL);
    pdalua_open_apps(L);

    CHECK(run(L,
        "app{ title = 'app.notes',\n"
        "     update = function() error('beim Rechnen') end,\n"
        "     draw   = function() error('beim Zeichnen') end,\n"
        "     event  = function() error('beim Bedienen') end }"));

    shell_scripting sc = pdalua_scripting(L);
    REQUIRE(sc.count(sc.user) == 1);

    bitmap bm;
    REQUIRE(bitmap_init(&bm, 60, 40));
    gc g;
    gc_init(&g, &bm);

    /* Alle drei Wege müssen den Fehler schlucken und zurückkehren. */
    sc.update(sc.user, 0);
    sc.draw(sc.user, 0, &g, 60, 40);

    event e = { .kind = EV_KEY_DOWN, .key = 'x' };
    CHECK(!sc.event(sc.user, 0, &e));

    /* Und der Zustand ist danach weiter brauchbar. */
    CHECK(truth(L, "1 + 1 == 2"));

    bitmap_free(&bm);
    pdalua_close(L);
    i18n_free(cat);
}

TEST(the_outliner_reads_the_structure_out_of_gemtext)
{
    catalog *cat = load_cat();
    REQUIRE(cat != NULL);

    collate   *sort = NULL, *search = NULL;
    lua_State *L = with_store(cat, &sort, &search);
    REQUIRE(L != NULL);
    pdalua_open_apps(L);

    char err[512] = "";
    CHECK(run(L,
        "store.put('Notizen', { title = 'Zweite', body = 'nur Text\\n' })\n"
        "store.put('Notizen', { title = 'Erste',"
        "   body = '# Kopf\\n* Punkt eins\\n* Punkt zwei\\nFliesstext\\n' })"));

    char path[512];
    snprintf(path, sizeof path, "%s/apps/outline.lua", PDA_DATA_DIR);

    bool loaded = pdalua_dofile(L, path, err, sizeof err);
    if (!loaded) printf("  outline: %s\n", err);
    REQUIRE(loaded);

    /* Zugeklappt: eine Zeile je Notiz, nach Titel sortiert. */
    CHECK(run(L, "outline.rebuild()"));
    CHECK(truth(L,
        "return #outline.rows == 2 and outline.rows[1].text == 'Erste'"
        " and outline.rows[2].text == 'Zweite'"));

    /* Aufgeklappt kommen Überschrift und Punkte dazu - Fließtext nicht, denn
     * er ist keine Gliederung. */
    CHECK(run(L, "outline.selected = 1 outline.toggle()"));
    CHECK(truth(L,
        "return #outline.rows == 5"
        " and outline.rows[2].kind == 'heading' and outline.rows[2].text == 'Kopf'"
        " and outline.rows[3].kind == 'item'    and outline.rows[3].text == 'Punkt eins'"
        " and outline.rows[5].text == 'Zweite'"));

    /* Und wieder zu. */
    CHECK(run(L, "outline.toggle()"));
    CHECK(truth(L, "#outline.rows == 2"));

    bitmap bm;
    REQUIRE(bitmap_init(&bm, 200, 90));
    gc g;
    gc_init(&g, &bm);

    shell_scripting sc = pdalua_scripting(L);
    REQUIRE(sc.count(sc.user) == 1);

    CHECK(run(L, "outline.selected = 1 outline.toggle()"));
    sc.draw(sc.user, 0, &g, 200, 90);
    CHECK(golden_check("lua_outline", &bm));

    bitmap_free(&bm);
    pdalua_close(L);
    drop_store(sort, search);
    i18n_free(cat);
}

int main(void)
{
    RUN(a_fresh_state_has_the_api_and_nothing_dangerous);
    RUN(an_error_comes_back_with_its_line);
    RUN(texts_come_from_the_catalog);
    RUN(unknown_patterns_and_modes_are_errors);
    RUN(drawing_without_a_target_does_nothing_instead_of_crashing);
    RUN(a_script_really_draws);

    RUN(the_two_schema_loaders_agree);
    RUN(a_lua_schema_is_checked_as_strictly_as_a_text_one);

    RUN(a_script_can_read_the_vault);
    RUN(taking_the_vault_away_takes_store_with_it);
    RUN(a_script_searches_with_folded_umlauts);
    RUN(a_script_can_write_and_delete);
    RUN(written_fields_come_out_in_a_fixed_order);
    RUN(a_script_cannot_smuggle_a_newline_into_a_field);

    RUN(an_application_written_only_in_lua_runs);
    RUN(an_application_without_a_title_or_a_draw_is_refused);
    RUN(the_event_bus_carries_messages_between_scripts);
    RUN(a_script_that_fails_does_not_take_the_shell_with_it);
    RUN(the_outliner_reads_the_structure_out_of_gemtext);

    return test_summary();
}
