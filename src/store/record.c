/* Siehe record.h für den Vertrag.
 *
 * Ein Datensatz ist nur ein Front-Matter-Block (siehe frontmatter.c) und ein
 * Körper, der als rohe Bytes durchgereicht wird. record.c fügt selbst keine
 * Logik über Feldinhalte hinzu - das bleibt Sache der Anwendung.
 *
 * Front Matter kennt keinen Setter (siehe frontmatter.h): wer ein Feld
 * braucht, das beim Einlesen noch nicht da war, muss den Block als Text neu
 * zusammensetzen und erneut parsen. record_parse ist damit nicht nur der Weg
 * von der Platte in den Speicher, sondern auch der einzige Weg, ein Feld
 * nachträglich einzufügen - genau davon macht vault.c beim Vergeben der
 * Kennung Gebrauch.
 */
#include "record.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct record {
    frontmatter *fields;
    char        *body;       /* immer vorhanden, nullterminiert */
    size_t       body_len;   /* strlen(body), ohne die Null */
};

/* --- Meldungen -------------------------------------------------------------- */

static void fail(char *err, size_t err_size, const char *name, const char *msg)
{
    if (err && err_size > 0)
        snprintf(err, err_size, "%s:0: %s", name ? name : "", msg);
}

/* --- Anlegen und Zerlegen ---------------------------------------------------- */

record *record_create(void)
{
    record *r = malloc(sizeof *r);
    if (!r) return NULL;

    /* Ein leerer Text hat kein "---", also liefert frontmatter_parse ein
     * gültiges, leeres Front Matter zurück - kein Fehlerfall, siehe
     * frontmatter.h. So entsteht ein Datensatz ohne Block, ganz ohne einen
     * eigenen Konstruktor für frontmatter zu brauchen. */
    size_t dummy_offset = 0;
    r->fields = frontmatter_parse("", 0, NULL, &dummy_offset, NULL, 0);
    if (!r->fields) {
        free(r);
        return NULL;
    }

    r->body = malloc(1);
    if (!r->body) {
        frontmatter_free(r->fields);
        free(r);
        return NULL;
    }
    r->body[0]  = '\0';
    r->body_len = 0;

    return r;
}

void record_free(record *r)
{
    if (!r) return;
    frontmatter_free(r->fields);
    free(r->body);
    free(r);
}

record *record_parse(const char *text, size_t len, const char *name,
                      char *err, size_t err_size)
{
    if (err && err_size > 0) err[0] = '\0';

    size_t       body_offset = 0;
    frontmatter *fm = frontmatter_parse(text, len, name, &body_offset, err, err_size);
    if (!fm) return NULL;

    record *r = malloc(sizeof *r);
    if (!r) {
        frontmatter_free(fm);
        fail(err, err_size, name, "Speicher reicht nicht");
        return NULL;
    }
    r->fields = fm;

    size_t body_len = len - body_offset;
    r->body = malloc(body_len + 1);
    if (!r->body) {
        frontmatter_free(fm);
        free(r);
        fail(err, err_size, name, "Speicher reicht nicht");
        return NULL;
    }
    if (body_len) memcpy(r->body, text + body_offset, body_len);
    r->body[body_len] = '\0';
    r->body_len        = body_len;

    return r;
}

/* --- Zusammensetzen ----------------------------------------------------------
 *
 * Hat der Datensatz keine Felder, entfällt der "---"-Block ganz - sonst
 * bekäme eine Datei, die nie Front Matter hatte, beim bloßen Zurückschreiben
 * plötzlich eins. Andernfalls endet der von frontmatter_write gelieferte Text
 * bereits mit "---\n"; der Körper folgt direkt danach, ohne dass hier noch
 * ein Zeilenumbruch eingefügt werden müsste - genau der Bytebereich, den
 * frontmatter_parse als body_offset markiert hatte. */
bool record_write(const record *r, char *out, size_t out_size, size_t *needed)
{
    size_t fm_len = 0;   /* Bytes des Blocks, ohne die abschließende Null */

    if (frontmatter_count(r->fields) > 0) {
        size_t fm_needed = 0;
        frontmatter_write(r->fields, NULL, 0, &fm_needed);
        fm_len = fm_needed - 1;
    }

    size_t total = fm_len + r->body_len;
    if (needed) *needed = total + 1;
    if (out_size < total + 1) return false;

    if (fm_len > 0) frontmatter_write(r->fields, out, fm_len + 1, NULL);
    if (r->body_len) memcpy(out + fm_len, r->body, r->body_len);
    out[total] = '\0';

    return true;
}

frontmatter *record_fields(record *r)
{
    return r->fields;
}

const char *record_body(const record *r)
{
    return r->body;
}

bool record_set_body(record *r, const char *gemtext)
{
    const char *g   = gemtext ? gemtext : "";
    size_t      len = strlen(g);

    char *copy = malloc(len + 1);
    if (!copy) return false;
    memcpy(copy, g, len + 1);

    free(r->body);
    r->body     = copy;
    r->body_len = len;
    return true;
}

/* --- Kennungen ---------------------------------------------------------------
 *
 * Form: JJJJMMTT"T"hhmmss-xxxx, also acht Ziffern für das Datum, ein
 * literales "T" als Trenner, sechs Ziffern für die Uhrzeit, ein Bindestrich
 * und vier Hexadezimalziffern für seq - zusammen RECORD_ID_LEN (20) Zeichen,
 * wie im Kommentar bei RECORD_ID_LEN in record.h vorgerechnet (15 + 1 + 4).
 */

void record_make_id(char *out, size_t out_size, long unix_time, unsigned seq)
{
    if (!out || out_size == 0) return;

    time_t     t = (time_t)unix_time;
    struct tm  tmv = {0};
    struct tm *g   = gmtime(&t);   /* liefert intern statischen Speicher */
    if (g) tmv = *g;

    unsigned seq16 = seq & 0xFFFFu;

    snprintf(out, out_size, "%04d%02d%02dT%02d%02d%02d-%04x",
              tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
              tmv.tm_hour, tmv.tm_min, tmv.tm_sec, seq16);
}

/* Nur Kleinbuchstaben.
 *
 * record_make_id schreibt ausschließlich klein, eine Kennung mit großem A-F
 * stammt also nicht von uns. Wichtiger noch: auf einem Dateisystem, das
 * Groß- und Kleinschreibung nicht unterscheidet - macOS im Auslieferungs-
 * zustand -, wären "...-00ab.gmi" und "...-00AB.gmi" dieselbe Datei, würden
 * aber als zwei Kennungen gelten. */
static bool is_hex_digit(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}

bool record_id_valid(const char *id)
{
    if (!id) return false;
    if (strlen(id) != RECORD_ID_LEN) return false;

    for (int i = 0; i < 8; i++)
        if (!isdigit((unsigned char)id[i])) return false;
    if (id[8] != 'T') return false;
    for (int i = 9; i < 15; i++)
        if (!isdigit((unsigned char)id[i])) return false;
    if (id[15] != '-') return false;
    for (int i = 16; i < 20; i++)
        if (!is_hex_digit(id[i])) return false;

    int month = (id[4] - '0') * 10 + (id[5] - '0');
    int day   = (id[6] - '0') * 10 + (id[7] - '0');
    if (month < 1 || month > 12) return false;
    if (day   < 1 || day   > 31) return false;

    return true;
}
