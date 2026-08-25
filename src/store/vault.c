/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Siehe vault.h für den Vertrag.
 *
 * Der Vault kennt nur Pfade und Byte-Ströme, keine Anwendungslogik. Er baut
 * einen Dateipfad aus Wurzel, Sammlung und Kennung zusammen, reicht das
 * Lesen und Schreiben an record.c weiter und sorgt selbst nur für zwei
 * Dinge: dass jeder Datensatz beim Speichern eine Kennung bekommt, und dass
 * eine Datei nie halb geschrieben auf der Platte liegt.
 */
#include "vault.h"

#include "plat/plat.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Länge des Wurzelpfads, den vault_open entgegennimmt. Für einen Pfad wie
 * "/home/nutzer/PDA" reichlich; ein Vault liegt nie tief verschachtelt. */
#define VAULT_ROOT_MAX 400

/* Länge eines vollständigen Dateipfads: Wurzel, Sammlungsname, Kennung,
 * Endung und die anhängende ".tmp" für das sichere Schreiben. */
#define VAULT_PATH_MAX 512

/* Obergrenze für das Einlesen eines Sammlungsverzeichnisses. plat_list
 * meldet nicht, ob es Einträge abgeschnitten hat - deshalb wird hier
 * großzügig gescannt, dynamisch angelegt, damit es keinen zu großen
 * Eintrag auf dem Stapel eines eingebetteten Geräts gibt. Für die
 * Sammlungen dieser Anwendung (persönliche Notizen, keine Datenbank) ist
 * das reichlich; eine Sammlung mit mehr Einträgen ist eine bekannte Grenze. */
#define VAULT_SCAN_MAX 1024

struct vault {
    char     root[VAULT_ROOT_MAX];
    unsigned seq;   /* unterscheidet neu vergebene Kennungen in derselben Sekunde */
};

/* --- Meldungen ---------------------------------------------------------------- */

static void fail(char *err, size_t err_size, const char *fmt, ...)
{
    if (!err || err_size == 0) return;

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(err, err_size, fmt, ap);
    va_end(ap);
}

/* --- Pfade ---------------------------------------------------------------------- */

static bool join2(char *out, size_t out_size, const char *a, const char *b)
{
    int n = snprintf(out, out_size, "%s/%s", a, b);
    return n > 0 && (size_t)n < out_size;
}

static bool collection_dir(const vault *v, const char *collection,
                            char *out, size_t out_size)
{
    return join2(out, out_size, v->root, collection);
}

static bool record_path(const vault *v, const char *collection, const char *id,
                         const char *suffix, char *out, size_t out_size)
{
    char dir[VAULT_PATH_MAX];
    if (!collection_dir(v, collection, dir, sizeof dir)) return false;

    char filename[RECORD_ID_LEN + 8];
    int  n = snprintf(filename, sizeof filename, "%s%s", id, suffix);
    if (n <= 0 || (size_t)n >= sizeof filename) return false;

    return join2(out, out_size, dir, filename);
}

/* --- Eine ganze Datei lesen ------------------------------------------------------
 *
 * plat.h kennt keine Größenabfrage, deshalb wird in wachsenden Schritten
 * gelesen, bis plat_read weniger liefert als angefordert - wie bei fread
 * heißt das entweder Dateiende oder Fehler, und beides beendet das Lesen
 * hier gleich. */
static bool read_whole_file(const char *path, char **out_text, size_t *out_len)
{
    plat_file *f = plat_open(path, PLAT_READ);
    if (!f) return false;

    size_t cap = 4096, len = 0;
    char  *buf = malloc(cap);
    if (!buf) {
        plat_close(f);
        return false;
    }

    for (;;) {
        if (len == cap) {
            size_t newcap = cap * 2;
            char  *p      = realloc(buf, newcap);
            if (!p) {
                free(buf);
                plat_close(f);
                return false;
            }
            buf = p;
            cap = newcap;
        }

        size_t n = plat_read(f, buf + len, cap - len);
        len += n;
        if (n == 0) break;
    }
    plat_close(f);

    char *shrunk = realloc(buf, len + 1);
    if (!shrunk) {
        free(buf);
        return false;
    }
    shrunk[len] = '\0';

    *out_text = shrunk;
    *out_len  = len;
    return true;
}

/* Schreibt text sicher: erst nach path+".tmp", dann darüberbenannt. So liegt
 * bei einem Abbruch mittendrin die alte Fassung noch da statt einer halben
 * neuen. */
static bool write_file_safely(const char *path, const char *text, size_t len,
                               char *err, size_t err_size)
{
    char tmp_path[VAULT_PATH_MAX];
    int  n = snprintf(tmp_path, sizeof tmp_path, "%s.tmp", path);
    if (n <= 0 || (size_t)n >= sizeof tmp_path) {
        fail(err, err_size, "%s: Pfad zu lang", path);
        return false;
    }

    plat_file *f = plat_open(tmp_path, PLAT_WRITE);
    if (!f) {
        fail(err, err_size, "%s: lässt sich nicht anlegen", tmp_path);
        return false;
    }

    size_t written = plat_write(f, text, len);
    plat_close(f);

    if (written != len) {
        plat_remove(tmp_path);
        fail(err, err_size, "%s: unvollständig geschrieben", tmp_path);
        return false;
    }

    if (!plat_rename(tmp_path, path)) {
        plat_remove(tmp_path);
        fail(err, err_size, "%s: lässt sich nicht umbenennen", tmp_path);
        return false;
    }

    return true;
}

/* --- Leben ------------------------------------------------------------------------ */

vault *vault_open(const char *path, char *err, size_t err_size)
{
    if (err && err_size > 0) err[0] = '\0';

    if (!path || !*path) {
        fail(err, err_size, "Pfad ist leer");
        return NULL;
    }

    size_t len = strlen(path);
    while (len > 1 && path[len - 1] == '/') len--;   /* Schlusstrennzeichen abschneiden */

    if (len >= VAULT_ROOT_MAX) {
        fail(err, err_size, "%s: Pfad zu lang", path);
        return NULL;
    }

    vault *v = malloc(sizeof *v);
    if (!v) {
        fail(err, err_size, "Speicher reicht nicht");
        return NULL;
    }

    memcpy(v->root, path, len);
    v->root[len] = '\0';
    v->seq       = 0;

    if (!plat_mkdir(v->root)) {
        fail(err, err_size, "%s: lässt sich nicht anlegen", v->root);
        free(v);
        return NULL;
    }

    return v;
}

void vault_close(vault *v)
{
    free(v);
}

/* --- Speichern ----------------------------------------------------------------- */

/* Kopiert [src, src+len) nach dst, lässt dabei aber jede Zeile weg, die mit
 * "id:" beginnt - also ein schon vorhandenes, aber leeres "id"-Feld. Ohne
 * das gäbe es nach dem Einfügen der neuen Kennung zwei "id"-Schlüssel im
 * selben Block, und frontmatter_parse weist doppelte Schlüssel ab. Die
 * schließende "---"-Zeile beginnt nie mit "id:" und bleibt unangetastet.
 * dst muss mindestens len + 1 Byte groß sein. */
static size_t copy_without_id_line(char *dst, const char *src, size_t len)
{
    size_t      out = 0;
    const char *p   = src;
    const char *end = src + len;

    while (p < end) {
        const char *nl       = memchr(p, '\n', (size_t)(end - p));
        size_t      line_len = nl ? (size_t)(nl - p) : (size_t)(end - p);
        bool        is_id    = line_len >= 3 && memcmp(p, "id:", 3) == 0;

        if (!is_id) {
            memcpy(dst + out, p, line_len);
            out += line_len;
            dst[out++] = '\n';
        }

        if (!nl) break;
        p = nl + 1;
    }
    return out;
}

/* Baut aus fields und body einen vollständigen Datensatztext mit einem neu
 * eingefügten Feld "id: <id>" als erster Zeile des Blocks. Gebraucht nur,
 * wenn rec noch keine Kennung trägt: da frontmatter.h keinen Setter kennt,
 * ist ein neuer Block als Text der einzige Weg über die öffentliche
 * Schnittstelle, siehe record.c. */
static char *build_text_with_id(const frontmatter *fields, const char *id,
                                 const char *body)
{
    char  *old_block = NULL;
    size_t old_len   = 0;

    if (frontmatter_count(fields) > 0) {
        size_t needed = 0;
        frontmatter_write(fields, NULL, 0, &needed);
        old_block = malloc(needed);
        if (!old_block) return NULL;
        frontmatter_write(fields, old_block, needed, NULL);
        old_len = needed - 1;   /* ohne die abschließende Null */
    }

    /* old_block beginnt, falls vorhanden, immer mit "---\n" (4 Byte) - das
     * wird übersprungen, weil der neue Block selbst mit "---\n" beginnt.
     * Der Rest (restliche Felder plus die schließende "---\n"-Zeile) landet
     * gefiltert in rest. */
    const char *old_rest     = old_block ? old_block + 4 : "---\n";
    size_t      old_rest_len = old_block ? old_len - 4 : 4;

    char *rest = malloc(old_rest_len + 1);
    if (!rest) {
        free(old_block);
        return NULL;
    }
    size_t rest_len = copy_without_id_line(rest, old_rest, old_rest_len);
    free(old_block);

    size_t body_len = strlen(body);
    size_t prefix_len = strlen("---\nid: ") + strlen(id) + 1 /* '\n' */;
    size_t total       = prefix_len + rest_len + body_len;

    char *text = malloc(total + 1);
    if (!text) {
        free(rest);
        return NULL;
    }

    char *p = text;
    p += snprintf(p, prefix_len + 1, "---\nid: %s\n", id);
    memcpy(p, rest, rest_len);
    p += rest_len;
    memcpy(p, body, body_len);
    p += body_len;
    *p = '\0';

    free(rest);
    return text;
}

bool vault_save(vault *v, const char *collection, record *rec,
                 char *id_out, size_t id_out_size,
                 char *err, size_t err_size)
{
    if (err && err_size > 0) err[0] = '\0';

    frontmatter *fields   = record_fields(rec);
    const char  *existing = frontmatter_get(fields, "id");
    bool         have_id  = existing && existing[0] != '\0';

    char id[RECORD_ID_LEN + 1];
    if (have_id) {
        snprintf(id, sizeof id, "%s", existing);
        /* Ein Feld aus einer von Hand bearbeiteten Datei ist nicht vertrauens-
         * würdig - ohne diese Prüfung könnte ein "id"-Wert mit Schrägstrichen
         * den Dateipfad verlassen. */
        if (!record_id_valid(id)) {
            fail(err, err_size, "%s: ungültige Kennung im Datensatz", existing);
            return false;
        }
    } else {
        record_make_id(id, sizeof id, (long)time(NULL), v->seq++);
    }

    record *to_write = rec;
    record *owned    = NULL;   /* freizugeben, falls hier neu angelegt */

    if (!have_id) {
        char *text = build_text_with_id(fields, id, record_body(rec));
        if (!text) {
            fail(err, err_size, "Speicher reicht nicht");
            return false;
        }

        owned = record_parse(text, strlen(text), id, err, err_size);
        free(text);
        if (!owned) return false;   /* err ist bereits gesetzt */

        to_write = owned;
    }

    char dir[VAULT_PATH_MAX];
    if (!collection_dir(v, collection, dir, sizeof dir)) {
        fail(err, err_size, "%s: Pfad zu lang", collection);
        record_free(owned);
        return false;
    }
    if (!plat_mkdir(dir)) {
        fail(err, err_size, "%s: lässt sich nicht anlegen", dir);
        record_free(owned);
        return false;
    }

    char path[VAULT_PATH_MAX];
    if (!record_path(v, collection, id, ".gmi", path, sizeof path)) {
        fail(err, err_size, "%s/%s: Pfad zu lang", collection, id);
        record_free(owned);
        return false;
    }

    size_t needed = 0;
    record_write(to_write, NULL, 0, &needed);
    char *buf = malloc(needed);
    if (!buf) {
        fail(err, err_size, "Speicher reicht nicht");
        record_free(owned);
        return false;
    }
    size_t written = 0;
    if (!record_write(to_write, buf, needed, &written)) {
        fail(err, err_size, "Datensatz lässt sich nicht zusammensetzen");
        free(buf);
        record_free(owned);
        return false;
    }

    bool ok = write_file_safely(path, buf, written - 1, err, err_size);

    free(buf);
    record_free(owned);

    if (ok && id_out) snprintf(id_out, id_out_size, "%s", id);
    return ok;
}

/* --- Laden und Löschen ----------------------------------------------------------- */

record *vault_load(vault *v, const char *collection, const char *id,
                    char *err, size_t err_size)
{
    if (err && err_size > 0) err[0] = '\0';

    if (!record_id_valid(id)) {
        fail(err, err_size, "%s: ungültige Kennung", id ? id : "(null)");
        return NULL;
    }

    char path[VAULT_PATH_MAX];
    if (!record_path(v, collection, id, ".gmi", path, sizeof path)) {
        fail(err, err_size, "%s/%s: Pfad zu lang", collection, id);
        return NULL;
    }

    char  *text;
    size_t len;
    if (!read_whole_file(path, &text, &len)) {
        fail(err, err_size, "%s: nicht lesbar", path);
        return NULL;
    }

    record *r = record_parse(text, len, path, err, err_size);
    free(text);
    return r;
}

bool vault_delete(vault *v, const char *collection, const char *id,
                   char *err, size_t err_size)
{
    if (err && err_size > 0) err[0] = '\0';

    if (!record_id_valid(id)) {
        fail(err, err_size, "%s: ungültige Kennung", id ? id : "(null)");
        return false;
    }

    char path[VAULT_PATH_MAX];
    if (!record_path(v, collection, id, ".gmi", path, sizeof path)) {
        fail(err, err_size, "%s/%s: Pfad zu lang", collection, id);
        return false;
    }

    if (!plat_remove(path)) {
        fail(err, err_size, "%s: lässt sich nicht löschen", path);
        return false;
    }
    return true;
}

/* --- Auflisten --------------------------------------------------------------------- */

static int cmp_ids(const void *a, const void *b)
{
    return strcmp((const char *)a, (const char *)b);
}

bool vault_list(vault *v, const char *collection,
                char (*ids_out)[RECORD_ID_LEN + 1], int cap, int *count_out,
                char *err, size_t err_size)
{
    if (err && err_size > 0) err[0] = '\0';
    if (count_out) *count_out = 0;

    char dir[VAULT_PATH_MAX];
    if (!collection_dir(v, collection, dir, sizeof dir)) {
        fail(err, err_size, "%s: Pfad zu lang", collection);
        return false;
    }

    plat_dirent *entries = malloc(VAULT_SCAN_MAX * sizeof *entries);
    if (!entries) {
        fail(err, err_size, "Speicher reicht nicht");
        return false;
    }

    int raw_count = 0;
    if (!plat_list(dir, entries, VAULT_SCAN_MAX, &raw_count)) {
        /* Verzeichnis gibt es noch nicht - eine Sammlung ohne Datensätze ist
         * kein Fehler. */
        free(entries);
        return true;
    }

    /* malloc(0) darf NULL liefern, ohne dass das ein Fehlschlag wäre - bei
     * einem leeren Verzeichnis wird deshalb gar nicht erst angefordert. */
    char (*valid)[RECORD_ID_LEN + 1] = NULL;
    if (raw_count > 0) {
        valid = malloc((size_t)raw_count * sizeof *valid);
        if (!valid) {
            free(entries);
            fail(err, err_size, "Speicher reicht nicht");
            return false;
        }
    }

    int valid_count = 0;
    for (int i = 0; i < raw_count; i++) {
        if (entries[i].is_dir) continue;

        const char *name = entries[i].name;
        size_t      nlen = strlen(name);
        const char *ext  = ".gmi";
        size_t      elen = strlen(ext);

        if (nlen <= elen || strcmp(name + nlen - elen, ext) != 0) continue;

        size_t idlen = nlen - elen;
        if (idlen != RECORD_ID_LEN) continue;

        char id[RECORD_ID_LEN + 1];
        memcpy(id, name, idlen);
        id[idlen] = '\0';
        if (!record_id_valid(id)) continue;

        memcpy(valid[valid_count], id, idlen + 1);
        valid_count++;
    }
    free(entries);

    if (valid_count > 0) qsort(valid, (size_t)valid_count, sizeof *valid, cmp_ids);

    int n = valid_count < cap ? valid_count : cap;
    for (int i = 0; i < n; i++) memcpy(ids_out[i], valid[i], RECORD_ID_LEN + 1);
    if (count_out) *count_out = n;

    free(valid);
    return true;
}
