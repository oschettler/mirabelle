#include "test.h"

#include "store/vault.h"
#include "store/record.h"
#include "store/frontmatter.h"
#include "plat/plat.h"

#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* --- Aufräumen und Pfade ------------------------------------------------------
 *
 * Der Vault selbst geht für alle Dateizugriffe über plat.h (siehe vault.h).
 * Das Anlegen und Wegräumen der Testverzeichnisse hier ist Gerüst, kein Teil
 * des geprüften Verhaltens, und darf deshalb - wie schon in test_plat.c - die
 * Standardbibliothek direkt benutzen. */

static void tmp_base(char *buf, size_t n)
{
    const char *dir = getenv("TMPDIR");
    if (!dir || !*dir) dir = "/tmp";

    size_t len = strlen(dir);
    while (len > 1 && dir[len - 1] == '/') len--;
    snprintf(buf, n, "%.*s", (int)len, dir);
}

static void temp_root(char *buf, size_t n)
{
    char base[400];
    tmp_base(base, sizeof base);
    snprintf(buf, n, "%s/pda_vault_test", base);
}

/* Löscht path samt Inhalt, falls es existiert. Kein Fehler, wenn nicht. */
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
                char child[600];
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

static bool file_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

/* Liest die rohen Bytes einer Datei, ohne über record.c zu gehen - für den
 * Rundlauf-Test muss geprüft werden, was wirklich auf der Platte liegt. */
static bool read_raw(const char *path, char **out, size_t *out_len)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) return false;

    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return false; }
    long sz = ftell(fp);
    if (sz < 0) { fclose(fp); return false; }
    if (fseek(fp, 0, SEEK_SET) != 0) { fclose(fp); return false; }

    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(fp); return false; }
    size_t n = fread(buf, 1, (size_t)sz, fp);
    fclose(fp);
    buf[n] = '\0';

    *out     = buf;
    *out_len = n;
    return true;
}

/* --- Öffnen -------------------------------------------------------------------- */

TEST(open_creates_the_directory_when_missing)
{
    char root[512];
    temp_root(root, sizeof root);
    rmrf(root);
    CHECK(!file_exists(root));

    char err[256] = "";
    vault *v = vault_open(root, err, sizeof err);
    REQUIRE(v != NULL);
    CHECK_EQ(err[0], '\0');
    CHECK(file_exists(root));

    vault_close(v);
    rmrf(root);
}

/* --- Speichern und Laden -------------------------------------------------------- */

TEST(save_writes_a_file_and_load_reads_the_same_record_back)
{
    char root[512];
    temp_root(root, sizeof root);
    rmrf(root);
    char err[256] = "";
    vault *v = vault_open(root, err, sizeof err);
    REQUIRE(v != NULL);

    const char *text =
        "---\n"
        "id: 20240101T000000-0001\n"
        "title: Einkauf\n"
        "---\n"
        "Milch\nBrot\n";
    record *rec = record_parse(text, strlen(text), "quelle.gmi", err, sizeof err);
    REQUIRE(rec != NULL);

    char id_out[RECORD_ID_LEN + 1] = "";
    CHECK(vault_save(v, "Sammlung", rec, id_out, sizeof id_out, err, sizeof err));
    CHECK_STR(id_out, "20240101T000000-0001");
    record_free(rec);

    record *loaded = vault_load(v, "Sammlung", id_out, err, sizeof err);
    REQUIRE(loaded != NULL);
    CHECK_STR(frontmatter_get(record_fields(loaded), "title"), "Einkauf");
    CHECK_STR(record_body(loaded), "Milch\nBrot\n");
    record_free(loaded);

    vault_close(v);
    rmrf(root);
}

TEST(save_assigns_an_id_when_the_record_has_none)
{
    char root[512];
    temp_root(root, sizeof root);
    rmrf(root);
    char err[256] = "";
    vault *v = vault_open(root, err, sizeof err);
    REQUIRE(v != NULL);

    const char *text = "---\ntitle: Ohne Kennung\n---\nInhalt.\n";
    record *rec = record_parse(text, strlen(text), "quelle.gmi", err, sizeof err);
    REQUIRE(rec != NULL);
    CHECK(!frontmatter_has(record_fields(rec), "id"));

    char id_out[RECORD_ID_LEN + 1] = "";
    CHECK(vault_save(v, "Sammlung", rec, id_out, sizeof id_out, err, sizeof err));
    CHECK(record_id_valid(id_out));
    record_free(rec);

    record *loaded = vault_load(v, "Sammlung", id_out, err, sizeof err);
    REQUIRE(loaded != NULL);
    CHECK_STR(frontmatter_get(record_fields(loaded), "id"), id_out);
    CHECK_STR(frontmatter_get(record_fields(loaded), "title"), "Ohne Kennung");
    record_free(loaded);

    vault_close(v);
    rmrf(root);
}

TEST(save_keeps_an_existing_id)
{
    char root[512];
    temp_root(root, sizeof root);
    rmrf(root);
    char err[256] = "";
    vault *v = vault_open(root, err, sizeof err);
    REQUIRE(v != NULL);

    const char *text = "---\nid: 20250505T121212-00ff\n---\nSchon vergeben.\n";
    record *rec = record_parse(text, strlen(text), "quelle.gmi", err, sizeof err);
    REQUIRE(rec != NULL);

    char id_out[RECORD_ID_LEN + 1] = "";
    CHECK(vault_save(v, "Sammlung", rec, id_out, sizeof id_out, err, sizeof err));
    CHECK_STR(id_out, "20250505T121212-00ff");
    record_free(rec);

    vault_close(v);
    rmrf(root);
}

/* Der wichtigste Test dieser Datei: speichern, laden, wieder speichern - die
 * Datei auf der Platte muss byteweise gleich bleiben. Sonst meldet jedes
 * Sicherungswerkzeug Änderungen, die keine sind. */
TEST(round_trip_through_disk_is_byte_exact)
{
    char root[512];
    temp_root(root, sizeof root);
    rmrf(root);
    char err[256] = "";
    vault *v = vault_open(root, err, sizeof err);
    REQUIRE(v != NULL);

    const char *text =
        "---\n"
        "id: 20240707T070707-00aa\n"
        "title: Rundlauf\n"
        "tags: [a, b]\n"
        "---\n"
        "Körper mit Umlauten: äöüß.\n";
    record *rec1 = record_parse(text, strlen(text), "quelle.gmi", err, sizeof err);
    REQUIRE(rec1 != NULL);

    char id_out[RECORD_ID_LEN + 1] = "";
    REQUIRE(vault_save(v, "Sammlung", rec1, id_out, sizeof id_out, err, sizeof err));
    record_free(rec1);

    char path[600];
    snprintf(path, sizeof path, "%s/Sammlung/%s.gmi", root, id_out);

    char  *bytes1 = NULL;
    size_t len1   = 0;
    REQUIRE(read_raw(path, &bytes1, &len1));

    record *rec2 = vault_load(v, "Sammlung", id_out, err, sizeof err);
    REQUIRE(rec2 != NULL);

    char id_out2[RECORD_ID_LEN + 1] = "";
    REQUIRE(vault_save(v, "Sammlung", rec2, id_out2, sizeof id_out2, err, sizeof err));
    CHECK_STR(id_out2, id_out);
    record_free(rec2);

    char  *bytes2 = NULL;
    size_t len2   = 0;
    REQUIRE(read_raw(path, &bytes2, &len2));

    CHECK_EQ(len2, len1);
    CHECK_MEM(bytes2, bytes1, len1);

    free(bytes1);
    free(bytes2);
    vault_close(v);
    rmrf(root);
}

/* --- Auflisten ------------------------------------------------------------------- */

TEST(list_returns_ids_sorted_ascending_regardless_of_save_order)
{
    char root[512];
    temp_root(root, sizeof root);
    rmrf(root);
    char err[256] = "";
    vault *v = vault_open(root, err, sizeof err);
    REQUIRE(v != NULL);

    static const char *ids_in_save_order[] = {
        "20240301T000000-0000",
        "20240101T000000-0000",
        "20240201T000000-0000",
    };
    for (int i = 0; i < 3; i++) {
        char text[128];
        snprintf(text, sizeof text, "---\nid: %s\n---\nEintrag.\n", ids_in_save_order[i]);
        record *rec = record_parse(text, strlen(text), "quelle.gmi", err, sizeof err);
        REQUIRE(rec != NULL);
        char id_out[RECORD_ID_LEN + 1] = "";
        REQUIRE(vault_save(v, "Sammlung", rec, id_out, sizeof id_out, err, sizeof err));
        record_free(rec);
    }

    char ids[8][RECORD_ID_LEN + 1];
    int  count = -1;
    REQUIRE(vault_list(v, "Sammlung", ids, 8, &count, err, sizeof err));
    CHECK_EQ(count, 3);
    CHECK_STR(ids[0], "20240101T000000-0000");
    CHECK_STR(ids[1], "20240201T000000-0000");
    CHECK_STR(ids[2], "20240301T000000-0000");

    vault_close(v);
    rmrf(root);
}

TEST(list_skips_files_that_are_not_records)
{
    char root[512];
    temp_root(root, sizeof root);
    rmrf(root);
    char err[256] = "";
    vault *v = vault_open(root, err, sizeof err);
    REQUIRE(v != NULL);

    /* Ein echter Datensatz, damit das Sammlungsverzeichnis existiert. */
    const char *text = "---\nid: 20240101T000000-0001\n---\nEchter Eintrag.\n";
    record *rec = record_parse(text, strlen(text), "quelle.gmi", err, sizeof err);
    REQUIRE(rec != NULL);
    char legit_id[RECORD_ID_LEN + 1] = "";
    REQUIRE(vault_save(v, "Sammlung", rec, legit_id, sizeof legit_id, err, sizeof err));
    record_free(rec);

    /* Von Hand Fremddateien hineinlegen. Die ersten beiden scheitern schon an
     * Endung und Länge; die letzten beiden haben BEIDES richtig und sind
     * trotzdem keine gültigen Kennungen. Ohne sie prüfte der Test nur Endung
     * und Länge, und eine fehlende Gültigkeitsprüfung fiele nicht auf. */
    static const char *foreign_names[] = {
        "notizen.txt",                 /* falsche Endung */
        "kaputt.gmi",                  /* falsche Länge */
        "abcdefghTijklmn-opqr.gmi",    /* richtige Länge, Buchstaben statt Ziffern */
        "20261399T151400-a3f9.gmi",    /* Monat 13 */
        "20240101T000000-00AB.gmi",    /* Großbuchstaben im Hexteil */
    };

    for (size_t i = 0; i < sizeof foreign_names / sizeof foreign_names[0]; i++) {
        char path[600];
        snprintf(path, sizeof path, "%s/Sammlung/%s", root, foreign_names[i]);
        plat_file *f = plat_open(path, PLAT_WRITE);
        REQUIRE(f != NULL);
        plat_write(f, "x", 1);
        plat_close(f);
    }

    char ids[8][RECORD_ID_LEN + 1];
    int  count = -1;
    CHECK(vault_list(v, "Sammlung", ids, 8, &count, err, sizeof err));
    CHECK_EQ(err[0], '\0');
    CHECK_EQ(count, 1);
    CHECK_STR(ids[0], legit_id);

    vault_close(v);
    rmrf(root);
}

TEST(list_on_a_collection_without_a_directory_is_empty_not_an_error)
{
    char root[512];
    temp_root(root, sizeof root);
    rmrf(root);
    char err[256] = "";
    vault *v = vault_open(root, err, sizeof err);
    REQUIRE(v != NULL);

    char ids[4][RECORD_ID_LEN + 1];
    int  count = -1;
    CHECK(vault_list(v, "GibtEsNicht", ids, 4, &count, err, sizeof err));
    CHECK_EQ(count, 0);
    CHECK_EQ(err[0], '\0');

    vault_close(v);
    rmrf(root);
}

/* --- Löschen --------------------------------------------------------------------- */

TEST(delete_removes_the_record)
{
    char root[512];
    temp_root(root, sizeof root);
    rmrf(root);
    char err[256] = "";
    vault *v = vault_open(root, err, sizeof err);
    REQUIRE(v != NULL);

    const char *text = "---\nid: 20240101T000000-0002\n---\nWird gelöscht.\n";
    record *rec = record_parse(text, strlen(text), "quelle.gmi", err, sizeof err);
    REQUIRE(rec != NULL);
    char id_out[RECORD_ID_LEN + 1] = "";
    REQUIRE(vault_save(v, "Sammlung", rec, id_out, sizeof id_out, err, sizeof err));
    record_free(rec);

    CHECK(vault_delete(v, "Sammlung", id_out, err, sizeof err));
    CHECK_EQ(err[0], '\0');

    record *loaded = vault_load(v, "Sammlung", id_out, err, sizeof err);
    CHECK(loaded == NULL);
    if (loaded) record_free(loaded);

    char ids[4][RECORD_ID_LEN + 1];
    int  count = -1;
    REQUIRE(vault_list(v, "Sammlung", ids, 4, &count, err, sizeof err));
    CHECK_EQ(count, 0);

    vault_close(v);
    rmrf(root);
}

TEST(delete_of_something_missing_is_not_a_crash)
{
    char root[512];
    temp_root(root, sizeof root);
    rmrf(root);
    char err[256] = "";
    vault *v = vault_open(root, err, sizeof err);
    REQUIRE(v != NULL);

    CHECK(vault_delete(v, "Sammlung", "20240101T000000-9999", err, sizeof err));

    vault_close(v);
    rmrf(root);
}

/* --- Sicheres Schreiben ------------------------------------------------------------ */

TEST(save_leaves_no_tmp_file_behind)
{
    char root[512];
    temp_root(root, sizeof root);
    rmrf(root);
    char err[256] = "";
    vault *v = vault_open(root, err, sizeof err);
    REQUIRE(v != NULL);

    const char *text = "---\ntitle: Sicher gespeichert\n---\nInhalt.\n";
    record *rec = record_parse(text, strlen(text), "quelle.gmi", err, sizeof err);
    REQUIRE(rec != NULL);
    char id_out[RECORD_ID_LEN + 1] = "";
    REQUIRE(vault_save(v, "Sammlung", rec, id_out, sizeof id_out, err, sizeof err));
    record_free(rec);

    char dir[600];
    snprintf(dir, sizeof dir, "%s/Sammlung", root);

    plat_dirent entries[16];
    int         count = -1;
    REQUIRE(plat_list(dir, entries, 16, &count));
    for (int i = 0; i < count; i++) {
        size_t      nlen = strlen(entries[i].name);
        const char *ext  = ".tmp";
        size_t      elen = strlen(ext);
        bool        is_tmp = nlen > elen && strcmp(entries[i].name + nlen - elen, ext) == 0;
        CHECK(!is_tmp);
    }

    vault_close(v);
    rmrf(root);
}

/* --- Pfad-Ausbruch ------------------------------------------------------------------ */

/* Eine Kennung wie "../../evil" darf niemals als Dateiname verwendet werden -
 * sonst schreibt vault_save außerhalb der Sammlung, im schlimmsten Fall
 * außerhalb des ganzen Vaults. vault_save darf das entweder ablehnen oder
 * eine neue, gültige Kennung vergeben; in jedem Fall darf draußen nichts
 * entstehen. */
TEST(save_refuses_a_path_escaping_id)
{
    char root[512];
    temp_root(root, sizeof root);
    rmrf(root);
    char err[256] = "";
    vault *v = vault_open(root, err, sizeof err);
    REQUIRE(v != NULL);

    const char *text = "---\nid: ../../evil\n---\nBöse Nutzlast.\n";
    record *rec = record_parse(text, strlen(text), "quelle.gmi", err, sizeof err);
    REQUIRE(rec != NULL);

    char err2[256]                 = "";
    char id_out[RECORD_ID_LEN + 1] = "";
    bool ok = vault_save(v, "Sammlung", rec, id_out, sizeof id_out, err2, sizeof err2);
    if (ok) {
        CHECK(record_id_valid(id_out));
        CHECK(strcmp(id_out, "../../evil") != 0);
    } else {
        CHECK(err2[0] != '\0');
    }
    record_free(rec);

    /* "Sammlung/../../evil.gmi" landet, von der Wurzel des Vaults aus
     * gerechnet, im übergeordneten Verzeichnis des Vaults selbst - genau dort
     * darf nichts entstanden sein. */
    char base[400];
    tmp_base(base, sizeof base);
    char escaped[500];
    snprintf(escaped, sizeof escaped, "%s/evil.gmi", base);
    CHECK(!file_exists(escaped));
    unlink(escaped);   /* Sicherheitsnetz, falls doch etwas entstanden wäre */

    vault_close(v);
    rmrf(root);
}

int main(void)
{
    plat_config cfg = { .width = 32, .height = 16 };
    if (!plat_init(&cfg)) {
        printf("plat_init fehlgeschlagen\n");
        return 1;
    }

    RUN(open_creates_the_directory_when_missing);

    RUN(save_writes_a_file_and_load_reads_the_same_record_back);
    RUN(save_assigns_an_id_when_the_record_has_none);
    RUN(save_keeps_an_existing_id);
    RUN(round_trip_through_disk_is_byte_exact);

    RUN(list_returns_ids_sorted_ascending_regardless_of_save_order);
    RUN(list_skips_files_that_are_not_records);
    RUN(list_on_a_collection_without_a_directory_is_empty_not_an_error);

    RUN(delete_removes_the_record);
    RUN(delete_of_something_missing_is_not_a_crash);

    RUN(save_leaves_no_tmp_file_behind);
    RUN(save_refuses_a_path_escaping_id);

    plat_shutdown();
    return test_summary();
}
