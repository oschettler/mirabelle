#include "test.h"

#include "core/i18n.h"
#include "core/utf8.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef PDA_DATA_DIR
#define PDA_DATA_DIR "data"
#endif

/* --- Hilfsfunktionen, wie in test_keymap.c ---------------------------------- */

static void lang_path(char *buf, size_t bufsize, const char *name)
{
    snprintf(buf, bufsize, "%s/lang/%s", PDA_DATA_DIR, name);
}

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

/* Liest den nächsten Schlüssel aus einer .strings-Datei. Unabhängig von
 * i18n.c, damit der Vergleich der Kataloge wirklich die Dateien prüft und
 * nicht die internen Datenstrukturen (wie next_data_entry in test_keymap.c). */
static bool next_i18n_key(FILE *f, char *key, size_t key_size)
{
    char line[800];
    while (fgets(line, sizeof line, f)) {
        char *hash = strchr(line, '#');
        if (hash) *hash = '\0';

        char *start = line;
        while (*start == ' ' || *start == '\t') start++;
        char *end = start + strlen(start);
        while (end > start && (end[-1] == ' ' || end[-1] == '\t' ||
                                end[-1] == '\n' || end[-1] == '\r'))
            end--;
        *end = '\0';
        if (*start == '\0') continue;

        char *eq = strchr(start, '=');
        if (!eq) continue;

        char *kend = eq;
        while (kend > start && (kend[-1] == ' ' || kend[-1] == '\t')) kend--;

        size_t klen = (size_t)(kend - start);
        if (klen >= key_size) klen = key_size - 1;
        memcpy(key, start, klen);
        key[klen] = '\0';
        return true;
    }
    return false;
}

/* --- Laden der echten Kataloge ------------------------------------------------ */

TEST(load_real_german_catalog_has_enough_entries)
{
    char path[512];
    lang_path(path, sizeof path, "de.strings");

    char err[256];
    catalog *c = i18n_load(path, err, sizeof err);
    REQUIRE(c != NULL);
    CHECK_EQ(err[0], '\0');
    CHECK(i18n_count(c) >= 35);

    i18n_free(c);
}

TEST(load_real_english_catalog_has_enough_entries)
{
    char path[512];
    lang_path(path, sizeof path, "en.strings");

    char err[256];
    catalog *c = i18n_load(path, err, sizeof err);
    REQUIRE(c != NULL);
    CHECK_EQ(err[0], '\0');
    CHECK(i18n_count(c) >= 35);

    i18n_free(c);
}

/* Der wichtigste Test dieser Datei: fehlt in einem der beiden Kataloge ein
 * Schlüssel, ist die Übersetzbarkeit kaputt. Die Schlüssel werden unabhängig
 * von i18n_load direkt aus den Dateien gelesen und in beide Richtungen gegen
 * den jeweils anderen Katalog geprüft. */
TEST(real_catalogs_have_the_same_keys)
{
    char de_path[512], en_path[512];
    lang_path(de_path, sizeof de_path, "de.strings");
    lang_path(en_path, sizeof en_path, "en.strings");

    char err[256];
    catalog *de = i18n_load(de_path, err, sizeof err);
    REQUIRE(de != NULL);
    catalog *en = i18n_load(en_path, err, sizeof err);
    REQUIRE(en != NULL);

    FILE *fp;
    char  key[128];

    fp = fopen(de_path, "rb");
    REQUIRE(fp != NULL);
    while (next_i18n_key(fp, key, sizeof key)) {
        if (!i18n_has(en, key))
            TEST_FAIL("Schlüssel '%s' aus de.strings fehlt in en.strings", key);
    }
    fclose(fp);

    fp = fopen(en_path, "rb");
    REQUIRE(fp != NULL);
    while (next_i18n_key(fp, key, sizeof key)) {
        if (!i18n_has(de, key))
            TEST_FAIL("Schlüssel '%s' aus en.strings fehlt in de.strings", key);
    }
    fclose(fp);

    i18n_free(de);
    i18n_free(en);
}

/* --- T -------------------------------------------------------------------------- */

TEST(t_returns_known_texts)
{
    char de_path[512], en_path[512];
    lang_path(de_path, sizeof de_path, "de.strings");
    lang_path(en_path, sizeof en_path, "en.strings");

    char err[256];
    catalog *de = i18n_load(de_path, err, sizeof err);
    REQUIRE(de != NULL);
    catalog *en = i18n_load(en_path, err, sizeof err);
    REQUIRE(en != NULL);

    /* Umlaut */
    CHECK_STR(T(de, "menu.file.open"), "Öffnen");
    CHECK_STR(T(en, "menu.file.open"), "Open");

    /* typografische Anführungszeichen */
    CHECK_STR(T(de, "dialog.discard.body"),
              "„{0}“ wurde geändert. Sollen die Änderungen verworfen werden?");
    CHECK_STR(T(en, "dialog.discard.body"),
              "“{0}” has been changed. Discard the changes?");

    i18n_free(de);
    i18n_free(en);
}

TEST(t_returns_key_itself_when_unknown)
{
    char path[512];
    lang_path(path, sizeof path, "de.strings");

    char err[256];
    catalog *c = i18n_load(path, err, sizeof err);
    REQUIRE(c != NULL);

    CHECK_STR(T(c, "kein.solcher.schluessel"), "kein.solcher.schluessel");

    i18n_free(c);
}

TEST(t_returns_key_itself_when_catalog_is_null)
{
    CHECK_STR(T(NULL, "irgendein.schluessel"), "irgendein.schluessel");
}

/* --- i18n_has, i18n_count -------------------------------------------------------- */

TEST(has_and_count_report_the_catalog_content)
{
    char path[512];
    lang_path(path, sizeof path, "de.strings");

    char err[256];
    catalog *c = i18n_load(path, err, sizeof err);
    REQUIRE(c != NULL);

    CHECK(i18n_has(c, "menu.file.open"));
    CHECK(!i18n_has(c, "kein.solcher.schluessel"));
    CHECK_EQ(i18n_count(c), 40);

    i18n_free(c);
}

/* --- Tf --------------------------------------------------------------------------- */

TEST(tf_substitutes_single_argument)
{
    char path[512];
    lang_path(path, sizeof path, "de.strings");

    char err[256];
    catalog *c = i18n_load(path, err, sizeof err);
    REQUIRE(c != NULL);

    const char *args[1] = { "Sichern" };
    char        out[64];
    CHECK(Tf(c, "demo.action", out, sizeof out, args, 1));
    CHECK_STR(out, "Aktion: Sichern");

    i18n_free(c);
}

TEST(tf_substitutes_multiple_arguments_in_text_order)
{
    char path[512];
    lang_path(path, sizeof path, "de.strings");

    char err[256];
    catalog *c = i18n_load(path, err, sizeof err);
    REQUIRE(c != NULL);

    const char *args[3] = { "10", "20", "3" };
    char        out[64];
    CHECK(Tf(c, "demo.click", out, sizeof out, args, 3));
    CHECK_STR(out, "Klick: 10, 20 (3x)");

    i18n_free(c);
}

/* {1} steht vor {0} im Text - eine eigene Testdatei, weil kein Eintrag im
 * echten Katalog die Argumente umstellt. */
TEST(tf_substitutes_arguments_out_of_order)
{
    char path[256];
    make_temp_path(path, sizeof path, "pda_test_i18n_reorder.strings");
    CHECK(write_text_file(path, "test.reorder = {1} then {0}\n"));

    char err[256];
    catalog *c = i18n_load(path, err, sizeof err);
    REQUIRE(c != NULL);

    const char *args[2] = { "first", "second" };
    char        out[64];
    CHECK(Tf(c, "test.reorder", out, sizeof out, args, 2));
    CHECK_STR(out, "second then first");

    i18n_free(c);
    remove(path);
}

TEST(tf_leaves_placeholder_without_argument_unchanged)
{
    char path[512];
    lang_path(path, sizeof path, "de.strings");

    char err[256];
    catalog *c = i18n_load(path, err, sizeof err);
    REQUIRE(c != NULL);

    /* nur zwei Argumente für drei Platzhalter - {2} bleibt stehen */
    const char *args[2] = { "10", "20" };
    char        out[64];
    CHECK(Tf(c, "demo.click", out, sizeof out, args, 2));
    CHECK_STR(out, "Klick: 10, 20 ({2}x)");

    i18n_free(c);
}

TEST(tf_fails_with_small_buffer)
{
    char path[512];
    lang_path(path, sizeof path, "de.strings");

    char err[256];
    catalog *c = i18n_load(path, err, sizeof err);
    REQUIRE(c != NULL);

    char out[3] = "xy";   /* "Öffnen" braucht mehr als 3 Byte */
    CHECK(!Tf(c, "menu.file.open", out, sizeof out, NULL, 0));
    CHECK_EQ(out[0], '\0');

    i18n_free(c);
}

/* --- Tn --------------------------------------------------------------------------- */

TEST(tn_selects_one_and_other_and_inserts_the_number)
{
    char path[512];
    lang_path(path, sizeof path, "de.strings");

    char err[256];
    catalog *c = i18n_load(path, err, sizeof err);
    REQUIRE(c != NULL);

    char out[64];

    CHECK(Tn(c, "list.count", 1, out, sizeof out));
    CHECK_STR(out, "1 Eintrag");

    CHECK(Tn(c, "list.count", 0, out, sizeof out));
    CHECK_STR(out, "0 Einträge");

    CHECK(Tn(c, "list.count", 2, out, sizeof out));
    CHECK_STR(out, "2 Einträge");

    CHECK(Tn(c, "list.count", 17, out, sizeof out));
    CHECK_STR(out, "17 Einträge");

    i18n_free(c);
}

TEST(tn_works_for_the_english_catalog_too)
{
    char path[512];
    lang_path(path, sizeof path, "en.strings");

    char err[256];
    catalog *c = i18n_load(path, err, sizeof err);
    REQUIRE(c != NULL);

    char out[64];

    CHECK(Tn(c, "list.count", 1, out, sizeof out));
    CHECK_STR(out, "1 entry");

    CHECK(Tn(c, "list.count", 17, out, sizeof out));
    CHECK_STR(out, "17 entries");

    i18n_free(c);
}

/* --- Fehler beim Laden -------------------------------------------------------------- */

TEST(load_rejects_missing_equals_sign)
{
    char path[256];
    make_temp_path(path, sizeof path, "pda_test_i18n_noequals.strings");
    CHECK(write_text_file(path, "kein.gleichheitszeichen\n"));

    char err[256];
    catalog *c = i18n_load(path, err, sizeof err);
    CHECK(c == NULL);
    CHECK(errbuf_has_location(err, path, 1));

    remove(path);
}

TEST(load_rejects_empty_key)
{
    char path[256];
    make_temp_path(path, sizeof path, "pda_test_i18n_emptykey.strings");
    CHECK(write_text_file(path, "   = Text ohne Schlüssel\n"));

    char err[256];
    catalog *c = i18n_load(path, err, sizeof err);
    CHECK(c == NULL);
    CHECK(errbuf_has_location(err, path, 1));

    remove(path);
}

TEST(load_rejects_invalid_character_in_key)
{
    /* Ein Leerzeichen ist der heimtückischste Fall: der Schlüssel wird ja bis
     * zum Gleichheitszeichen gelesen, "menu.file .new" käme also klaglos
     * durch und ließe sich danach von niemandem nachschlagen. */
    static const char *bad[] = {
        "Bad.Key = Text\n",
        "menu.file .new = Neu\n",
        "menu/file = Text\n",
        "menü.datei = Text\n",
    };

    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        char path[256];
        make_temp_path(path, sizeof path, "pda_test_i18n_badkey.strings");
        CHECK(write_text_file(path, bad[i]));

        char     err[256] = "";
        catalog *c        = i18n_load(path, err, sizeof err);

        if (c != NULL) {
            printf("  nicht abgewiesen: %s", bad[i]);
            CHECK(false);
            i18n_free(c);
        } else {
            CHECK(errbuf_has_location(err, path, 1));
        }
        remove(path);
    }
}

TEST(load_rejects_key_that_is_too_long)
{
    char path[256];
    make_temp_path(path, sizeof path, "pda_test_i18n_longkey.strings");

    char content[256] = {0};
    memset(content, 'a', 97);   /* ein Byte über der Grenze von 96 */
    strcpy(content + 97, " = Text\n");
    CHECK(write_text_file(path, content));

    char err[256];
    catalog *c = i18n_load(path, err, sizeof err);
    CHECK(c == NULL);
    CHECK(errbuf_has_location(err, path, 1));

    remove(path);
}

TEST(load_rejects_text_that_is_too_long)
{
    char path[256];
    make_temp_path(path, sizeof path, "pda_test_i18n_longtext.strings");

    char content[600] = "some.key = ";
    size_t prefix = strlen(content);
    memset(content + prefix, 'x', 513);   /* ein Byte über der Grenze von 512 */
    strcpy(content + prefix + 513, "\n");
    CHECK(write_text_file(path, content));

    char err[256];
    catalog *c = i18n_load(path, err, sizeof err);
    CHECK(c == NULL);
    CHECK(errbuf_has_location(err, path, 1));

    remove(path);
}

TEST(load_rejects_duplicate_key)
{
    char path[256];
    make_temp_path(path, sizeof path, "pda_test_i18n_dupkey.strings");

    const char *content =
        "dup.key = zuerst\n"
        "dup.key = dann nochmal\n";
    CHECK(write_text_file(path, content));

    char err[256];
    catalog *c = i18n_load(path, err, sizeof err);
    CHECK(c == NULL);
    CHECK(errbuf_has_location(err, path, 2));
    CHECK(strstr(err, "Zeile 1") != NULL);
    CHECK(strstr(err, "dup.key") != NULL);

    remove(path);
}

TEST(load_rejects_missing_file)
{
    char path[256];
    make_temp_path(path, sizeof path, "pda_test_i18n_missing.strings");
    remove(path);   /* sicherstellen, dass sie wirklich nicht existiert */

    char err[256];
    catalog *c = i18n_load(path, err, sizeof err);
    CHECK(c == NULL);
    CHECK(err[0] != '\0');
    CHECK(errbuf_has_location(err, path, 0));
}

/* --- Format: Kommentare, Leerraum -------------------------------------------------- */

TEST(load_skips_comments_and_blank_lines)
{
    char path[256];
    make_temp_path(path, sizeof path, "pda_test_i18n_comments.strings");

    const char *content =
        "# Kommentar am Zeilenanfang\n"
        "\n"
        "a.one = Hallo\n"
        "   # eingerückter Kommentar\n"
        "\n"
        "a.two = Welt\n";
    CHECK(write_text_file(path, content));

    char err[256];
    catalog *c = i18n_load(path, err, sizeof err);
    REQUIRE(c != NULL);
    CHECK_EQ(i18n_count(c), 2);
    CHECK_EQ(err[0], '\0');

    i18n_free(c);
    remove(path);
}

TEST(hash_inside_text_ends_the_line_too)
{
    char path[256];
    make_temp_path(path, sizeof path, "pda_test_i18n_hashintext.strings");
    CHECK(write_text_file(path, "greet.text = Hallo # Welt\n"));

    char err[256];
    catalog *c = i18n_load(path, err, sizeof err);
    REQUIRE(c != NULL);
    CHECK_EQ(i18n_count(c), 1);
    CHECK_STR(T(c, "greet.text"), "Hallo");

    i18n_free(c);
    remove(path);
}

TEST(whitespace_around_equals_is_removed_but_kept_inside_text)
{
    char path[256];
    make_temp_path(path, sizeof path, "pda_test_i18n_whitespace.strings");
    CHECK(write_text_file(path, "  spacey.key   =   a   b   c   \n"));

    char err[256];
    catalog *c = i18n_load(path, err, sizeof err);
    REQUIRE(c != NULL);
    CHECK(i18n_has(c, "spacey.key"));
    CHECK_STR(T(c, "spacey.key"), "a   b   c");

    i18n_free(c);
    remove(path);
}

TEST(text_may_be_empty)
{
    char path[256];
    make_temp_path(path, sizeof path, "pda_test_i18n_emptytext.strings");
    CHECK(write_text_file(path, "empty.key =\n"));

    char err[256];
    catalog *c = i18n_load(path, err, sizeof err);
    REQUIRE(c != NULL);
    CHECK_EQ(err[0], '\0');
    CHECK(i18n_has(c, "empty.key"));
    CHECK_STR(T(c, "empty.key"), "");

    i18n_free(c);
    remove(path);
}

/* --- Zeichenvorrat -------------------------------------------------------------------
 *
 * Der Parser behandelt den Text als Bytefolge und darf ihn nirgends mitten in
 * einem Mehrbytezeichen abschneiden. utf8_valid deckt genau das auf: eine
 * verstümmelte Folge wäre nicht mehr gültig kodiert. */
TEST(catalog_texts_are_valid_utf8)
{
    char de_path[512], en_path[512];
    lang_path(de_path, sizeof de_path, "de.strings");
    lang_path(en_path, sizeof en_path, "en.strings");

    char err[256];
    catalog *de = i18n_load(de_path, err, sizeof err);
    REQUIRE(de != NULL);
    catalog *en = i18n_load(en_path, err, sizeof err);
    REQUIRE(en != NULL);

    CHECK(utf8_valid(T(de, "menu.file.open")));
    CHECK(utf8_valid(T(de, "dialog.discard.body")));
    CHECK(utf8_valid(T(de, "demo.greeting")));
    CHECK(utf8_valid(T(en, "dialog.discard.body")));
    CHECK(utf8_valid(T(en, "demo.greeting")));

    i18n_free(de);
    i18n_free(en);
}

int main(void)
{
    RUN(load_real_german_catalog_has_enough_entries);
    RUN(load_real_english_catalog_has_enough_entries);
    RUN(real_catalogs_have_the_same_keys);

    RUN(t_returns_known_texts);
    RUN(t_returns_key_itself_when_unknown);
    RUN(t_returns_key_itself_when_catalog_is_null);

    RUN(has_and_count_report_the_catalog_content);

    RUN(tf_substitutes_single_argument);
    RUN(tf_substitutes_multiple_arguments_in_text_order);
    RUN(tf_substitutes_arguments_out_of_order);
    RUN(tf_leaves_placeholder_without_argument_unchanged);
    RUN(tf_fails_with_small_buffer);

    RUN(tn_selects_one_and_other_and_inserts_the_number);
    RUN(tn_works_for_the_english_catalog_too);

    RUN(load_rejects_missing_equals_sign);
    RUN(load_rejects_empty_key);
    RUN(load_rejects_invalid_character_in_key);
    RUN(load_rejects_key_that_is_too_long);
    RUN(load_rejects_text_that_is_too_long);
    RUN(load_rejects_duplicate_key);
    RUN(load_rejects_missing_file);

    RUN(load_skips_comments_and_blank_lines);
    RUN(hash_inside_text_ends_the_line_too);
    RUN(whitespace_around_equals_is_removed_but_kept_inside_text);
    RUN(text_may_be_empty);

    RUN(catalog_texts_are_valid_utf8);

    return test_summary();
}
