/* Siehe index.h für den Vertrag.
 *
 * Das Schema ist absichtlich schmal: eine Zeile je Feldwert, dazu eine
 * Volltexttabelle. Keine Spalte je Feldname, kein Schema je Sammlung - der
 * Browser aus M11 baut seine Masken aus Schemadateien, und der Index soll
 * nicht mitwachsen müssen, wenn jemand ein Feld hinzufügt.
 *
 * Zusammengebaut wird SQL nur hier, und Werte gehen nie in den Text, sondern
 * immer als gebundener Parameter. Damit ist ein Apostroph in einem Namen ein
 * Apostroph und kein Syntaxfehler.
 */
#include "store/index.h"

#include <sqlite3.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "store/frontmatter.h"

#define SQL_MAX    4096
#define PARAMS_MAX 64
#define FOLD_MAX   1024

struct index_db {
    sqlite3       *db;
    const collate *sort;
    const collate *search;
};

/* --- Meldungen --------------------------------------------------------------- */

static bool fail(char *err, size_t err_size, const char *fmt, ...)
{
    if (err && err_size) {
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(err, err_size, fmt, ap);
        va_end(ap);
    }
    return false;
}

static bool fail_sqlite(index_db *ix, char *err, size_t err_size, const char *what)
{
    return fail(err, err_size, "%s: %s", what, sqlite3_errmsg(ix->db));
}

/* --- Falten -------------------------------------------------------------------
 *
 * Passt eine gefaltete Fassung nicht in den Puffer, wird der ungefaltete Wert
 * abgelegt. Das ist besser als abzubrechen: der Datensatz bleibt auffindbar,
 * nur eben ohne Umlautbehandlung, und ein Feldwert von über tausend Byte ist
 * ohnehin kein Name, nach dem jemand sortiert.
 */
static const char *fold_or_raw(const collate *c, const char *value,
                               char *buf, size_t buf_size)
{
    if (!c) return value;
    if (collate_fold(c, value, buf, buf_size) == (size_t)-1) return value;
    return buf;
}

/* --- Schema -------------------------------------------------------------------
 *
 * fields hält je Feldwert eine Zeile - ein Listenfeld wird damit zu mehreren,
 * und eine Bedingung auf einem Etikett trifft, sobald EIN Etikett passt. Genau
 * das tut query_matches() auch; die beiden Wege müssen dieselbe Antwort geben.
 */
static const char *SCHEMA =
    "PRAGMA journal_mode=WAL;"
    "CREATE TABLE IF NOT EXISTS records ("
    "  coll TEXT NOT NULL, id TEXT NOT NULL,"
    "  PRIMARY KEY (coll, id));"
    "CREATE TABLE IF NOT EXISTS fields ("
    "  coll TEXT NOT NULL, id TEXT NOT NULL,"
    "  field TEXT NOT NULL, seq INTEGER NOT NULL,"
    "  value TEXT NOT NULL,"      /* unverändert, für Gleichheit und Vergleiche */
    "  sortkey TEXT NOT NULL,"    /* nach der Sortiertabelle gefaltet */
    "  findkey TEXT NOT NULL);"   /* nach der Suchtabelle gefaltet */
    "CREATE INDEX IF NOT EXISTS fields_lookup ON fields (coll, field, value);"
    "CREATE INDEX IF NOT EXISTS fields_record ON fields (coll, id);"
    "CREATE VIRTUAL TABLE IF NOT EXISTS fulltext USING fts5("
    "  coll UNINDEXED, id UNINDEXED, content,"
    "  tokenize='unicode61 remove_diacritics 2');";

index_db *index_open(const char *path, const collate *sort, const collate *search,
                     char *err, size_t err_size)
{
    index_db *ix = calloc(1, sizeof *ix);
    if (!ix) {
        fail(err, err_size, "kein Speicher");
        return NULL;
    }
    ix->sort   = sort;
    ix->search = search;

    if (sqlite3_open(path, &ix->db) != SQLITE_OK) {
        fail(err, err_size, "%s: %s", path, sqlite3_errmsg(ix->db));
        index_close(ix);
        return NULL;
    }

    char *msg = NULL;
    if (sqlite3_exec(ix->db, SCHEMA, NULL, NULL, &msg) != SQLITE_OK) {
        fail(err, err_size, "Schema: %s", msg ? msg : "unbekannter Fehler");
        sqlite3_free(msg);
        index_close(ix);
        return NULL;
    }
    return ix;
}

void index_close(index_db *ix)
{
    if (!ix) return;
    if (ix->db) sqlite3_close(ix->db);
    free(ix);
}

/* --- Kleine Helfer ------------------------------------------------------------- */

static bool exec_simple(index_db *ix, const char *sql, char *err, size_t err_size)
{
    char *msg = NULL;
    if (sqlite3_exec(ix->db, sql, NULL, NULL, &msg) != SQLITE_OK) {
        fail(err, err_size, "%s", msg ? msg : "unbekannter Fehler");
        sqlite3_free(msg);
        return false;
    }
    return true;
}

bool index_clear(index_db *ix, char *err, size_t err_size)
{
    return exec_simple(ix,
        "DELETE FROM records; DELETE FROM fields; DELETE FROM fulltext;",
        err, err_size);
}

/* --- Eintragen ------------------------------------------------------------------ */

bool index_remove(index_db *ix, const char *collection, const char *id,
                  char *err, size_t err_size)
{
    static const char *stmts[] = {
        "DELETE FROM records  WHERE coll = ? AND id = ?",
        "DELETE FROM fields   WHERE coll = ? AND id = ?",
        "DELETE FROM fulltext WHERE coll = ? AND id = ?",
    };

    for (size_t i = 0; i < sizeof stmts / sizeof stmts[0]; i++) {
        sqlite3_stmt *st = NULL;
        if (sqlite3_prepare_v2(ix->db, stmts[i], -1, &st, NULL) != SQLITE_OK)
            return fail_sqlite(ix, err, err_size, "Löschen");

        sqlite3_bind_text(st, 1, collection, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, id, -1, SQLITE_TRANSIENT);

        int rc = sqlite3_step(st);
        sqlite3_finalize(st);
        if (rc != SQLITE_DONE) return fail_sqlite(ix, err, err_size, "Löschen");
    }
    return true;
}

/* Hängt text an den Volltextpuffer an, soweit Platz ist. Läuft er über, wird
 * abgeschnitten statt abgebrochen: ein sehr langer Datensatz soll auffindbar
 * bleiben, auch wenn nicht jedes Wort daraus im Index steht. */
static void append_text(char *buf, size_t buf_size, size_t *len, const char *text)
{
    size_t n = strlen(text);
    if (*len + n + 2 >= buf_size) n = buf_size > *len + 2 ? buf_size - *len - 2 : 0;
    if (!n) return;

    memcpy(buf + *len, text, n);
    *len += n;
    buf[(*len)++] = '\n';
    buf[*len]     = '\0';
}

bool index_put(index_db *ix, const char *collection, const char *id,
               record *rec, char *err, size_t err_size)
{
    if (!index_remove(ix, collection, id, err, err_size)) return false;

    sqlite3_stmt *st = NULL;

    if (sqlite3_prepare_v2(ix->db, "INSERT INTO records (coll, id) VALUES (?, ?)",
                           -1, &st, NULL) != SQLITE_OK)
        return fail_sqlite(ix, err, err_size, "Eintragen");
    sqlite3_bind_text(st, 1, collection, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, id, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    if (rc != SQLITE_DONE) return fail_sqlite(ix, err, err_size, "Eintragen");

    /* Die Felder, jeder Listeneintrag als eigene Zeile. */
    frontmatter *fm = record_fields(rec);
    int          nf = frontmatter_count(fm);

    static char ft[64 * 1024];
    size_t      ft_len = 0;
    ft[0] = '\0';

    for (int i = 0; i < nf; i++) {
        const char *key = frontmatter_key_at(fm, i);
        if (!key) continue;

        int nv = frontmatter_list_count(fm, key);
        for (int j = 0; j < nv; j++) {
            const char *value = frontmatter_list_at(fm, key, j);
            if (!value) value = "";

            char sortbuf[FOLD_MAX], findbuf[FOLD_MAX];
            const char *sortkey = fold_or_raw(ix->sort,   value, sortbuf, sizeof sortbuf);
            const char *findkey = fold_or_raw(ix->search, value, findbuf, sizeof findbuf);

            if (sqlite3_prepare_v2(ix->db,
                    "INSERT INTO fields (coll, id, field, seq, value, sortkey, findkey)"
                    " VALUES (?, ?, ?, ?, ?, ?, ?)", -1, &st, NULL) != SQLITE_OK)
                return fail_sqlite(ix, err, err_size, "Feld eintragen");

            sqlite3_bind_text(st, 1, collection, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(st, 2, id,         -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(st, 3, key,        -1, SQLITE_TRANSIENT);
            sqlite3_bind_int (st, 4, j);
            sqlite3_bind_text(st, 5, value,      -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(st, 6, sortkey,    -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(st, 7, findkey,    -1, SQLITE_TRANSIENT);

            rc = sqlite3_step(st);
            sqlite3_finalize(st);
            if (rc != SQLITE_DONE)
                return fail_sqlite(ix, err, err_size, "Feld eintragen");

            append_text(ft, sizeof ft, &ft_len, value);
        }
    }

    append_text(ft, sizeof ft, &ft_len, record_body(rec));

    if (sqlite3_prepare_v2(ix->db,
            "INSERT INTO fulltext (coll, id, content) VALUES (?, ?, ?)",
            -1, &st, NULL) != SQLITE_OK)
        return fail_sqlite(ix, err, err_size, "Volltext eintragen");

    sqlite3_bind_text(st, 1, collection, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, id,         -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, ft,         -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(st);
    sqlite3_finalize(st);
    if (rc != SQLITE_DONE)
        return fail_sqlite(ix, err, err_size, "Volltext eintragen");

    return true;
}

/* --- Aus einer Abfrage wird SQL -------------------------------------------------
 *
 * Der einzige Ort im Programm, an dem das passiert. Jede Bedingung wird zu
 * einem EXISTS über fields; die Werte gehen als Parameter mit, nie in den Text.
 */

typedef struct {
    char        sql[SQL_MAX];
    size_t      len;
    const char *params[PARAMS_MAX];   /* zeigen in die query oder in bufs */
    char        bufs[PARAMS_MAX][FOLD_MAX];
    int         buf_used;
    int         count;
    bool        overflow;
} builder;

static void put(builder *b, const char *text)
{
    size_t n = strlen(text);
    if (b->len + n + 1 > sizeof b->sql) { b->overflow = true; return; }
    memcpy(b->sql + b->len, text, n + 1);
    b->len += n;
}

/* Merkt sich einen Parameter. Zeigt value in die Abfrage, wird er nur
 * verwiesen; gefaltete Werte brauchen dagegen einen eigenen Puffer, der bis
 * zum Ausführen lebt - deshalb liegt er im builder und nicht auf dem Stapel
 * der Schleife. */
static void bind_later(builder *b, const char *value)
{
    if (b->count >= PARAMS_MAX) { b->overflow = true; return; }
    b->params[b->count++] = value;
}

static const char *fold_param(builder *b, const collate *c, const char *value)
{
    if (!c) return value;
    if (b->buf_used >= PARAMS_MAX) { b->overflow = true; return value; }

    char *buf = b->bufs[b->buf_used];
    if (collate_fold(c, value, buf, FOLD_MAX) == (size_t)-1) return value;

    b->buf_used++;
    return buf;
}

static void build_filter(builder *b, const index_db *ix, const query_filter *f)
{
    if (f->op == QF_ABSENT) {
        put(b, " AND NOT EXISTS (SELECT 1 FROM fields x WHERE x.coll = r.coll"
               " AND x.id = r.id AND x.field = ? AND x.value <> '')");
        bind_later(b, f->field);
        return;
    }

    put(b, " AND EXISTS (SELECT 1 FROM fields x WHERE x.coll = r.coll"
           " AND x.id = r.id AND x.field = ?");
    bind_later(b, f->field);

    switch (f->op) {
    case QF_EQUALS:
        put(b, " AND x.value = ?");
        bind_later(b, f->value);
        break;
    case QF_CONTAINS:
        /* instr() statt LIKE: LIKE müsste % und _ im Suchtext maskieren, und
         * eine vergessene Maskierung fiele erst auf, wenn jemand einen
         * Unterstrich eintippt. */
        put(b, " AND instr(x.findkey, ?) > 0");
        bind_later(b, fold_param(b, ix->search, f->value));
        break;
    case QF_PREFIX:
        put(b, " AND instr(x.findkey, ?) = 1");
        bind_later(b, fold_param(b, ix->search, f->value));
        break;
    case QF_LESS:
        put(b, " AND x.value < ?");
        bind_later(b, f->value);
        break;
    case QF_GREATER:
        put(b, " AND x.value > ?");
        bind_later(b, f->value);
        break;
    case QF_PRESENT:
        put(b, " AND x.value <> ''");
        break;
    case QF_ABSENT:
        break;   /* oben erledigt */
    }
    put(b, ")");
}

/* Baut den MATCH-Ausdruck für FTS5: jedes Wort in Anführungszeichen, mit AND
 * verbunden. Anführungszeichen im Wort werden verdoppelt - das ist die
 * Maskierung, die FTS5 versteht. */
static void build_match(char *out, size_t out_size, const char *words)
{
    size_t      n = 0;
    const char *p = words;
    bool        first = true;

    out[0] = '\0';
    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;

        if (!first && n + 5 < out_size) {
            memcpy(out + n, " AND ", 5);
            n += 5;
        }
        first = false;

        if (n + 1 < out_size) out[n++] = '"';
        while (*p && *p != ' ') {
            if (*p == '"' && n + 1 < out_size) out[n++] = '"';
            if (n + 1 < out_size) out[n++] = *p;
            p++;
        }
        if (n + 1 < out_size) out[n++] = '"';
        out[n] = '\0';
    }
}

bool index_query(index_db *ix, const query *q,
                 char (*ids_out)[RECORD_ID_LEN + 1], int cap, int *count_out,
                 char *err, size_t err_size)
{
    builder b;
    memset(&b, 0, sizeof b);

    put(&b, "SELECT r.id FROM records r WHERE 1=1");

    if (q->collection[0]) {
        put(&b, " AND r.coll = ?");
        bind_later(&b, q->collection);
    }

    for (int i = 0; i < q->filter_count; i++)
        build_filter(&b, ix, &q->filters[i]);

    char match[QUERY_VALUE_MAX * 3];
    if (q->text[0]) {
        build_match(match, sizeof match, q->text);
        put(&b, " AND EXISTS (SELECT 1 FROM fulltext f WHERE f.coll = r.coll"
                " AND f.id = r.id AND fulltext MATCH ?)");
        bind_later(&b, match);
    }

    /* Sortieren über die abgelegte Sortierfassung. Dass SQL damit nach DIN 5007
     * ordnet, ohne die Regel zu kennen, ist der ganze Zweck der Spalte.
     *
     * Leere Felder ans Ende, in beiden Richtungen - dieselbe Entscheidung wie
     * in query_compare(), und sie steht auch hier VOR der Richtung. */
    const char *field = q->order_field[0] ? q->order_field : "id";
    put(&b, " ORDER BY (SELECT COUNT(*) FROM fields o WHERE o.coll = r.coll"
            " AND o.id = r.id AND o.field = ? AND o.value <> '') = 0 ASC,"
            " (SELECT MIN(o.sortkey) FROM fields o WHERE o.coll = r.coll"
            " AND o.id = r.id AND o.field = ?)");
    bind_later(&b, field);
    bind_later(&b, field);
    put(&b, q->descending ? " DESC" : " ASC");
    put(&b, ", r.id ASC");

    if (q->limit > 0) {
        char tail[64];
        snprintf(tail, sizeof tail, " LIMIT %d OFFSET %d", q->limit, q->offset);
        put(&b, tail);
    } else if (q->offset > 0) {
        char tail[64];
        snprintf(tail, sizeof tail, " LIMIT -1 OFFSET %d", q->offset);
        put(&b, tail);
    }

    if (b.overflow)
        return fail(err, err_size, "Abfrage zu groß");

    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(ix->db, b.sql, -1, &st, NULL) != SQLITE_OK)
        return fail_sqlite(ix, err, err_size, "Abfrage");

    for (int i = 0; i < b.count; i++)
        sqlite3_bind_text(st, i + 1, b.params[i], -1, SQLITE_TRANSIENT);

    int n = 0;
    for (;;) {
        int rc = sqlite3_step(st);
        if (rc == SQLITE_DONE) break;
        if (rc != SQLITE_ROW) {
            sqlite3_finalize(st);
            return fail_sqlite(ix, err, err_size, "Abfrage");
        }
        if (n >= cap) break;

        const unsigned char *id = sqlite3_column_text(st, 0);
        snprintf(ids_out[n], RECORD_ID_LEN + 1, "%s", id ? (const char *)id : "");
        n++;
    }
    sqlite3_finalize(st);

    if (count_out) *count_out = n;
    return true;
}

/* --- Neu aufbauen --------------------------------------------------------------- */

bool index_rebuild(index_db *ix, vault *v,
                   const char *const *collections, int count,
                   char *err, size_t err_size)
{
    if (!index_clear(ix, err, err_size)) return false;
    if (!exec_simple(ix, "BEGIN", err, err_size)) return false;

    enum { BATCH = 512 };
    static char ids[BATCH][RECORD_ID_LEN + 1];

    for (int ci = 0; ci < count; ci++) {
        int n = 0;
        if (!vault_list(v, collections[ci], ids, BATCH, &n, err, err_size)) {
            exec_simple(ix, "ROLLBACK", NULL, 0);
            return false;
        }

        for (int i = 0; i < n; i++) {
            record *rec = vault_load(v, collections[ci], ids[i], err, err_size);
            if (!rec) {
                /* Eine kaputte Datei bringt den Neuaufbau nicht zu Fall - sie
                 * fehlt dann eben im Index, so wie ein fremder Dateiname in
                 * vault_list übersprungen wird. */
                continue;
            }

            bool ok = index_put(ix, collections[ci], ids[i], rec, err, err_size);
            record_free(rec);
            if (!ok) {
                exec_simple(ix, "ROLLBACK", NULL, 0);
                return false;
            }
        }
    }

    return exec_simple(ix, "COMMIT", err, err_size);
}
