#include "test.h"

#include "core/keymap.h"
#include "plat/plat.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef PDA_DATA_DIR
#define PDA_DATA_DIR "data"
#endif

/* --- Hilfsfunktionen, wie in test_fontc.c ---------------------------------- */

static void make_temp_path(char *buf, size_t bufsize, const char *name)
{
    const char *dir = getenv("TMPDIR");
    if (!dir || !*dir) dir = "/tmp";

    size_t len = strlen(dir);
    if (len > 0 && dir[len - 1] == '/')
        snprintf(buf, bufsize, "%s%s", dir, name);
    else
        snprintf(buf, bufsize, "%s/%s", dir, name);
}

static bool write_text_file(const char *path, const char *content)
{
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    bool ok = fputs(content, f) >= 0;
    return fclose(f) == 0 && ok;
}

static bool errbuf_has_location(const char *err, const char *path, int line)
{
    char prefix[300];
    snprintf(prefix, sizeof prefix, "%s:%d:", path, line);
    return strstr(err, prefix) != NULL;
}

/* Liest die nächste unkommentierte, nichtleere Zeile aus einer Kürzeldatei
 * und zerlegt sie in Aktion und Kürzel (der Bereich wird hier nicht
 * gebraucht). Unabhängig von keymap.c, damit der Rundlauftest wirklich die
 * Datei prüft und nicht die internen Datenstrukturen. */
static bool next_data_entry(FILE *f, char *action, size_t action_size,
                             char *shortcut, size_t shortcut_size)
{
    char line[512];
    while (fgets(line, sizeof line, f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\r' || *p == '\0') continue;

        char act[128], sc[128], scope[128];
        if (sscanf(p, "%127s %127s %127s", act, sc, scope) == 3) {
            snprintf(action, action_size, "%s", act);
            snprintf(shortcut, shortcut_size, "%s", sc);
            return true;
        }
    }
    return false;
}

/* --- keymap_parse_shortcut -------------------------------------------------- */

TEST(parse_shortcut_examples)
{
    int key; uint8_t mods;

    CHECK(keymap_parse_shortcut("A", &key, &mods));
    CHECK_EQ(key, 'a'); CHECK_EQ(mods, 0);

    CHECK(keymap_parse_shortcut("Cmd+N", &key, &mods));
    CHECK_EQ(key, 'n'); CHECK_EQ(mods, MOD_CMD);

    CHECK(keymap_parse_shortcut("Cmd+Shift+Z", &key, &mods));
    CHECK_EQ(key, 'z'); CHECK_EQ(mods, MOD_CMD | MOD_SHIFT);

    CHECK(keymap_parse_shortcut("Shift+Tab", &key, &mods));
    CHECK_EQ(key, KEY_TAB); CHECK_EQ(mods, MOD_SHIFT);

    CHECK(keymap_parse_shortcut("F10", &key, &mods));
    CHECK_EQ(key, KEY_F10); CHECK_EQ(mods, 0);

    CHECK(keymap_parse_shortcut("PageDown", &key, &mods));
    CHECK_EQ(key, KEY_PAGE_DOWN); CHECK_EQ(mods, 0);

    CHECK(keymap_parse_shortcut("Cmd+Backspace", &key, &mods));
    CHECK_EQ(key, KEY_BACKSPACE); CHECK_EQ(mods, MOD_CMD);

    CHECK(keymap_parse_shortcut("Space", &key, &mods));
    CHECK_EQ(key, KEY_SPACE); CHECK_EQ(mods, 0);
}

TEST(parse_shortcut_case_insensitive)
{
    int key1, key2, key3; uint8_t mods1, mods2, mods3;

    CHECK(keymap_parse_shortcut("cmd+z", &key1, &mods1));
    CHECK(keymap_parse_shortcut("CMD+Z", &key2, &mods2));
    CHECK(keymap_parse_shortcut("Cmd+z", &key3, &mods3));

    CHECK_EQ(key1, key2); CHECK_EQ(key2, key3);
    CHECK_EQ(mods1, mods2); CHECK_EQ(mods2, mods3);
    CHECK_EQ(key1, 'z'); CHECK_EQ(mods1, MOD_CMD);
}

TEST(parse_shortcut_rejects_invalid)
{
    int key; uint8_t mods;

    CHECK(!keymap_parse_shortcut("", &key, &mods));
    CHECK(!keymap_parse_shortcut("Cmd+", &key, &mods));
    CHECK(!keymap_parse_shortcut("Cmd+Nonsens", &key, &mods));
    CHECK(!keymap_parse_shortcut("Hyper+A", &key, &mods));
    CHECK(!keymap_parse_shortcut("Cmd+F13", &key, &mods));
}

/* --- Laden der echten Datei -------------------------------------------------- */

TEST(load_real_keymap_has_enough_entries)
{
    char path[512];
    snprintf(path, sizeof path, "%s/keys/default.keys", PDA_DATA_DIR);

    char err[256];
    keymap *km = keymap_load(path, err, sizeof err);
    REQUIRE(km != NULL);
    CHECK_EQ(err[0], '\0');
    CHECK(keymap_count(km) >= 25);

    keymap_free(km);
}

/* --- Bereichsauflösung -------------------------------------------------------- */

TEST(lookup_resolves_scope_then_global)
{
    char path[512];
    snprintf(path, sizeof path, "%s/keys/default.keys", PDA_DATA_DIR);

    char err[256];
    keymap *km = keymap_load(path, err, sizeof err);
    REQUIRE(km != NULL);

    CHECK_STR(keymap_lookup(km, KEY_RETURN, 0, "list"), "list.open");
    CHECK_STR(keymap_lookup(km, KEY_RETURN, 0, "form"), "form.accept");
    CHECK(keymap_lookup(km, KEY_RETURN, 0, NULL) == NULL);

    keymap_free(km);
}

TEST(lookup_finds_global_shortcut_with_scope_given)
{
    char path[512];
    snprintf(path, sizeof path, "%s/keys/default.keys", PDA_DATA_DIR);

    char err[256];
    keymap *km = keymap_load(path, err, sizeof err);
    REQUIRE(km != NULL);

    /* app.quit liegt in "global" auf Cmd+Q - muss auch gefunden werden,
     * wenn ein Bereich angegeben ist, der selbst kein Cmd+Q kennt. */
    CHECK_STR(keymap_lookup(km, 'q', MOD_CMD, "list"), "app.quit");
    CHECK_STR(keymap_lookup(km, 'q', MOD_CMD, NULL), "app.quit");

    keymap_free(km);
}

/* --- keymap_describe ---------------------------------------------------------- */

TEST(describe_known_actions)
{
    char path[512];
    snprintf(path, sizeof path, "%s/keys/default.keys", PDA_DATA_DIR);

    char err[256];
    keymap *km = keymap_load(path, err, sizeof err);
    REQUIRE(km != NULL);

    char out[64];
    CHECK(keymap_describe(km, "record.new", out, sizeof out));
    CHECK_STR(out, "Cmd+N");

    CHECK(keymap_describe(km, "edit.redo", out, sizeof out));
    CHECK_STR(out, "Cmd+Shift+Z");

    CHECK(keymap_describe(km, "field.prev", out, sizeof out));
    CHECK_STR(out, "Shift+Tab");

    keymap_free(km);
}

TEST(describe_unknown_action_fails)
{
    char path[512];
    snprintf(path, sizeof path, "%s/keys/default.keys", PDA_DATA_DIR);

    char err[256];
    keymap *km = keymap_load(path, err, sizeof err);
    REQUIRE(km != NULL);

    char out[64] = "unverändert";
    CHECK(!keymap_describe(km, "nichts.sowas", out, sizeof out));
    CHECK_EQ(out[0], '\0');

    keymap_free(km);
}

TEST(describe_with_small_buffer_fails)
{
    char path[512];
    snprintf(path, sizeof path, "%s/keys/default.keys", PDA_DATA_DIR);

    char err[256];
    keymap *km = keymap_load(path, err, sizeof err);
    REQUIRE(km != NULL);

    char out[3];   /* "Cmd+N" braucht sechs Bytes samt Nullbyte */
    CHECK(!keymap_describe(km, "record.new", out, sizeof out));
    CHECK_EQ(out[0], '\0');

    keymap_free(km);
}

/* Der wichtigste Test: für jeden Eintrag der echten Datei muss die
 * Beschreibung wieder auf dieselbe Taste und dieselben Modifikatoren führen. */
TEST(describe_parse_roundtrip_over_real_file)
{
    char path[512];
    snprintf(path, sizeof path, "%s/keys/default.keys", PDA_DATA_DIR);

    char err[256];
    keymap *km = keymap_load(path, err, sizeof err);
    REQUIRE(km != NULL);

    FILE *fp = fopen(path, "rb");
    REQUIRE(fp != NULL);

    char action[128], shortcut[128];
    int  checked = 0;

    while (next_data_entry(fp, action, sizeof action, shortcut, sizeof shortcut)) {
        int     want_key; uint8_t want_mods;
        CHECK(keymap_parse_shortcut(shortcut, &want_key, &want_mods));

        char desc[64];
        bool described = keymap_describe(km, action, desc, sizeof desc);
        CHECK(described);

        if (described) {
            int     got_key; uint8_t got_mods;
            CHECK(keymap_parse_shortcut(desc, &got_key, &got_mods));
            CHECK_EQ(got_key, want_key);
            CHECK_EQ(got_mods, want_mods);
        }
        checked++;
    }
    fclose(fp);
    CHECK(checked >= 25);

    keymap_free(km);
}

/* --- Fehler beim Laden --------------------------------------------------------- */

TEST(load_rejects_duplicate_shortcut)
{
    char path[256];
    make_temp_path(path, sizeof path, "pda_test_keymap_dupshortcut.keys");

    const char *content =
        "foo.bar   Cmd+N   app\n"
        "baz.qux   Cmd+N   app\n";
    CHECK(write_text_file(path, content));

    char err[256];
    keymap *km = keymap_load(path, err, sizeof err);
    CHECK(km == NULL);
    CHECK(errbuf_has_location(err, path, 2));
    CHECK(strstr(err, "Zeile 1") != NULL);
    CHECK(strstr(err, "foo.bar") != NULL);

    remove(path);
}

TEST(load_rejects_duplicate_action)
{
    char path[256];
    make_temp_path(path, sizeof path, "pda_test_keymap_dupaction.keys");

    const char *content =
        "dup.action   Cmd+N   app\n"
        "dup.action   Cmd+M   app\n";
    CHECK(write_text_file(path, content));

    char err[256];
    keymap *km = keymap_load(path, err, sizeof err);
    CHECK(km == NULL);
    CHECK(errbuf_has_location(err, path, 2));
    CHECK(strstr(err, "Zeile 1") != NULL);
    CHECK(strstr(err, "dup.action") != NULL);

    remove(path);
}

TEST(load_rejects_wrong_field_count)
{
    char path[256];
    make_temp_path(path, sizeof path, "pda_test_keymap_fields.keys");

    const char *content = "only.two   Cmd+N\n";
    CHECK(write_text_file(path, content));

    char err[256];
    keymap *km = keymap_load(path, err, sizeof err);
    CHECK(km == NULL);
    CHECK(errbuf_has_location(err, path, 1));

    remove(path);
}

TEST(load_skips_comments_and_blank_lines)
{
    char path[256];
    make_temp_path(path, sizeof path, "pda_test_keymap_comments.keys");

    const char *content =
        "# Kommentar am Zeilenanfang\n"
        "\n"
        "a.one   Cmd+N   app\n"
        "   # eingerückter Kommentar\n"
        "\n"
        "a.two   Cmd+M   app\n";
    CHECK(write_text_file(path, content));

    char err[256];
    keymap *km = keymap_load(path, err, sizeof err);
    REQUIRE(km != NULL);
    CHECK_EQ(keymap_count(km), 2);
    CHECK_EQ(err[0], '\0');

    keymap_free(km);
    remove(path);
}

TEST(load_rejects_missing_file)
{
    char path[256];
    make_temp_path(path, sizeof path, "pda_test_keymap_missing.keys");
    remove(path);   /* sicherstellen, dass sie wirklich nicht existiert */

    char err[256];
    keymap *km = keymap_load(path, err, sizeof err);
    CHECK(km == NULL);
    CHECK(err[0] != '\0');
    CHECK(errbuf_has_location(err, path, 0));
}

int main(void)
{
    RUN(parse_shortcut_examples);
    RUN(parse_shortcut_case_insensitive);
    RUN(parse_shortcut_rejects_invalid);
    RUN(load_real_keymap_has_enough_entries);
    RUN(lookup_resolves_scope_then_global);
    RUN(lookup_finds_global_shortcut_with_scope_given);
    RUN(describe_known_actions);
    RUN(describe_unknown_action_fails);
    RUN(describe_with_small_buffer_fails);
    RUN(describe_parse_roundtrip_over_real_file);
    RUN(load_rejects_duplicate_shortcut);
    RUN(load_rejects_duplicate_action);
    RUN(load_rejects_wrong_field_count);
    RUN(load_skips_comments_and_blank_lines);
    RUN(load_rejects_missing_file);
    return test_summary();
}
