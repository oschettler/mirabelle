/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Siehe fieldkind.h für den Vertrag.
 *
 * Jeder Feldtyp ist eine Struktur mit fünf Funktionen. Fehlt einem Typ eine
 * davon, springt eine gemeinsame Voreinstellung ein - Text ist der Normalfall,
 * und die meisten Typen weichen nur an ein, zwei Stellen davon ab.
 */
#include "app/fieldkind.h"

#include <stdio.h>
#include <string.h>

/* --- Gemeinsames ---------------------------------------------------------------- */

static void copy(char *out, size_t out_size, const char *src)
{
    snprintf(out, out_size, "%s", src ? src : "");
}

static void text_format(const schema_field *f, const catalog *cat,
                        const char *stored, char *out, size_t out_size)
{
    (void)f; (void)cat;
    copy(out, out_size, stored);
}

static bool text_parse(const schema_field *f, const catalog *cat,
                       const char *input, char *out, size_t out_size)
{
    (void)f; (void)cat;
    if (input && strlen(input) >= out_size) return false;
    copy(out, out_size, input);
    return true;
}

static widget *text_widget(const schema_field *f, const theme *th, const catalog *cat)
{
    (void)f;
    return text_field_create(th, cat);
}

/* Aus dem Textfeld kommt die ANZEIGEform - also muss die parse-Funktion des
 * Feldtyps darüber, nicht die von Text. Sonst käme ein Datum als „05.03.2026"
 * zurück und stünde so im Datensatz, wo JJJJ-MM-TT hingehört. Dass sich Daten
 * als Zeichenketten sortieren lassen, wäre damit hin.
 *
 * Deshalb geht diese Funktion über fieldkind_of(f) und nicht direkt über
 * text_parse: sie wird von mehreren Feldtypen benutzt, und jeder bringt seine
 * eigene Umkehrung mit. */
static bool text_read(const schema_field *f, const catalog *cat, widget *w,
                      char *out, size_t out_size)
{
    return fieldkind_of(f)->parse(f, cat, text_widget_value(w), out, out_size);
}

static void text_write(const schema_field *f, const catalog *cat, widget *w,
                       const char *stored)
{
    char shown[512];
    fieldkind_of(f)->format(f, cat, stored, shown, sizeof shown);
    text_widget_set_value(w, shown);
}

/* --- Gemtext ---------------------------------------------------------------------- */

static widget *gemtext_widget(const schema_field *f, const theme *th, const catalog *cat)
{
    (void)f;
    return text_area_create(th, cat);
}

/* --- Datum ------------------------------------------------------------------------
 *
 * Gespeichert wird JJJJ-MM-TT, angezeigt, was `date.format` im Katalog sagt.
 * Erkannt werden %d, %m und %Y; alles andere im Format ist ein Trennzeichen und
 * muss beim Einlesen genau so wiederkommen.
 *
 * Mehr Format braucht es nicht, und mehr wäre gefährlich: eine vollständige
 * strftime-Nachbildung hätte Formatzeichen, die aus einer Katalogdatei kämen -
 * also aus einer Datei, die ein Nutzer bearbeiten darf.
 */

static const char *date_pattern(const catalog *cat)
{
    const char *fmt = T(cat, "date.format");
    /* Fehlt der Eintrag, liefert T den Schlüssel zurück. Der taugt nicht als
     * Format, also die ISO-Form nehmen - sichtbar nüchtern, aber richtig. */
    if (!fmt || strchr(fmt, '%') == NULL) return "%Y-%m-%d";
    return fmt;
}

/* Zieht n Ziffern aus *p und rückt weiter. false, wenn dort keine stehen. */
static bool take_digits(const char **p, int n, int *out)
{
    int v = 0;
    for (int i = 0; i < n; i++) {
        if (**p < '0' || **p > '9') return false;
        v = v * 10 + (**p - '0');
        (*p)++;
    }
    *out = v;
    return true;
}

static void date_format(const schema_field *f, const catalog *cat,
                        const char *stored, char *out, size_t out_size)
{
    (void)f;
    out[0] = '\0';
    if (!stored || !*stored) return;

    /* Was nicht wie JJJJ-MM-TT aussieht, wird unverändert gezeigt. Ein Feld,
     * das ein Mensch von Hand verstellt hat, soll sichtbar bleiben und nicht
     * verschwinden. */
    const char *p = stored;
    int y, m, d;
    if (!take_digits(&p, 4, &y) || *p++ != '-' ||
        !take_digits(&p, 2, &m) || *p++ != '-' ||
        !take_digits(&p, 2, &d) || *p != '\0') {
        copy(out, out_size, stored);
        return;
    }

    const char *fmt = date_pattern(cat);
    size_t      n   = 0;

    for (const char *q = fmt; *q && n + 5 < out_size; q++) {
        if (*q != '%') { out[n++] = *q; continue; }

        q++;
        switch (*q) {
        case 'd': n += (size_t)snprintf(out + n, out_size - n, "%02d", d); break;
        case 'm': n += (size_t)snprintf(out + n, out_size - n, "%02d", m); break;
        case 'Y': n += (size_t)snprintf(out + n, out_size - n, "%04d", y); break;
        case '%': out[n++] = '%'; break;
        case '\0': q--; break;      /* ein % am Ende ist einfach ein % */
        default:  out[n++] = *q; break;
        }
    }
    out[n] = '\0';
}

static bool date_parse(const schema_field *f, const catalog *cat,
                       const char *input, char *out, size_t out_size)
{
    (void)f;
    out[0] = '\0';
    if (!input) return true;

    /* Führenden und folgenden Leerraum verzeihen - er entsteht beim Tippen. */
    while (*input == ' ') input++;
    const char *end = input + strlen(input);
    while (end > input && end[-1] == ' ') end--;
    if (end == input) return true;      /* leer ist erlaubt: kein Datum */

    char buf[64];
    size_t len = (size_t)(end - input);
    if (len >= sizeof buf) return false;
    memcpy(buf, input, len);
    buf[len] = '\0';

    const char *fmt = date_pattern(cat);
    const char *p   = buf;
    int y = -1, m = -1, d = -1;

    for (const char *q = fmt; *q; q++) {
        if (*q != '%') {
            if (*p != *q) return false;
            p++;
            continue;
        }
        q++;
        switch (*q) {
        case 'd': if (!take_digits(&p, 2, &d)) return false; break;
        case 'm': if (!take_digits(&p, 2, &m)) return false; break;
        case 'Y': if (!take_digits(&p, 4, &y)) return false; break;
        case '%': if (*p++ != '%') return false; break;
        case '\0': q--; break;
        default:  if (*p++ != *q) return false; break;
        }
    }
    if (*p) return false;               /* hinten ist noch etwas übrig */

    if (y < 0 || m < 1 || m > 12 || d < 1 || d > 31) return false;
    if (out_size < 11) return false;

    snprintf(out, out_size, "%04d-%02d-%02d", y, m, d);
    return true;
}

/* --- Wahrheitswert -----------------------------------------------------------------
 *
 * Gespeichert wird "yes" oder "no" - englisch, wie jeder Feldinhalt, den nicht
 * ein Mensch getippt hat (D-1). Angezeigt wird, was der Katalog sagt.
 */

static bool is_yes(const char *stored)
{
    return stored && strcmp(stored, "yes") == 0;
}

static void bool_format(const schema_field *f, const catalog *cat,
                        const char *stored, char *out, size_t out_size)
{
    (void)f;
    copy(out, out_size, T(cat, is_yes(stored) ? "bool.yes" : "bool.no"));
}

static bool bool_parse(const schema_field *f, const catalog *cat,
                       const char *input, char *out, size_t out_size)
{
    (void)f;
    if (out_size < 4) return false;

    bool yes = input && (strcmp(input, "yes") == 0 ||
                         strcmp(input, T(cat, "bool.yes")) == 0);
    copy(out, out_size, yes ? "yes" : "no");
    return true;
}

static widget *bool_widget(const schema_field *f, const theme *th, const catalog *cat)
{
    return checkbox_create(th, cat, f->label, false);
}

static bool bool_read(const schema_field *f, const catalog *cat, widget *w,
                      char *out, size_t out_size)
{
    (void)f; (void)cat;
    if (out_size < 4) return false;
    copy(out, out_size, checkbox_value(w) ? "yes" : "no");
    return true;
}

static void bool_write(const schema_field *f, const catalog *cat, widget *w,
                       const char *stored)
{
    (void)f; (void)cat;
    checkbox_set_value(w, is_yes(stored));
}

/* --- Auswahl -----------------------------------------------------------------------
 *
 * Gespeichert wird der Wert, wie er im Schema steht. Angezeigt wird der
 * Katalogtext zu `value.<wert>`, falls es ihn gibt - sonst der Wert selbst.
 *
 * Die Zweistufigkeit ist Absicht: ein Schema soll sich schreiben lassen, ohne
 * dass jemand vorher Katalogeinträge anlegt, und trotzdem übersetzbar bleiben.
 */

static void choice_key(const char *value, char *out, size_t out_size)
{
    snprintf(out, out_size, "value.%s", value ? value : "");
}

static void choice_format(const schema_field *f, const catalog *cat,
                          const char *stored, char *out, size_t out_size)
{
    (void)f;
    if (!stored || !*stored) { out[0] = '\0'; return; }

    char key[SCHEMA_NAME_MAX + 8];
    choice_key(stored, key, sizeof key);

    copy(out, out_size, i18n_has(cat, key) ? T(cat, key) : stored);
}

static bool choice_parse(const schema_field *f, const catalog *cat,
                         const char *input, char *out, size_t out_size)
{
    out[0] = '\0';
    if (!input || !*input) return true;   /* nichts gewählt ist erlaubt */

    /* Sowohl die Speicher- als auch die Anzeigeform annehmen. */
    for (int i = 0; i < f->value_count; i++) {
        const char *v = f->values[i];
        char shown[128];
        choice_format(f, cat, v, shown, sizeof shown);

        if (strcmp(input, v) == 0 || strcmp(input, shown) == 0) {
            if (strlen(v) >= out_size) return false;
            copy(out, out_size, v);
            return true;
        }
    }
    return false;    /* etwas, das im Schema nicht vorgesehen ist */
}

/* Eine Liste mit den erlaubten Werten. Ein Aufklappmenü wäre schöner, gibt es
 * aber noch nicht; eine Liste ist ein Bedienelement, das wir haben, und für
 * drei bis fünf Werte sieht sie aus wie ein Auswahlfeld in System 1. */
static widget *choice_widget(const schema_field *f, const theme *th, const catalog *cat)
{
    widget *w = list_create(th, cat);
    if (!w) return NULL;

    /* Kopieren lassen: die Werte liegen im Schema als Zeichenfeld, nicht als
     * Zeigerfeld, und ein gemeinsamer Umschreibpuffer wäre geteilt - zwei
     * Auswahlfelder in einem Formular würden einander überschreiben. */
    /* Die Anzeigeform in die Liste, nicht die Speicherform: dort soll
     * „Privat" stehen und nicht „privat". Zurückgelesen wird über den Index,
     * also bleibt die Speicherform davon unberührt. */
    char        shown[SCHEMA_VALUES_MAX][128];
    const char *keys[SCHEMA_VALUES_MAX];

    for (int i = 0; i < f->value_count; i++) {
        choice_format(f, cat, f->values[i], shown[i], sizeof shown[i]);
        keys[i] = shown[i];
    }

    if (!list_set_items_copy(w, keys, f->value_count)) {
        widget_destroy(w);
        return NULL;
    }

    /* Nichts vorgewählt. Ein Schema kennt keine Voreinstellung, also wäre der
     * erste Wert eine erfundene - und sie stünde nach dem Speichern im
     * Datensatz, ohne dass jemand sie gewählt hätte. */
    list_select_none(w);
    return w;
}

static bool choice_read(const schema_field *f, const catalog *cat, widget *w,
                        char *out, size_t out_size)
{
    (void)cat;
    int sel = list_selected(w);
    if (sel < 0 || sel >= f->value_count) { out[0] = '\0'; return true; }
    if (strlen(f->values[sel]) >= out_size) return false;

    copy(out, out_size, f->values[sel]);
    return true;
}

static void choice_write(const schema_field *f, const catalog *cat, widget *w,
                         const char *stored)
{
    (void)cat;
    for (int i = 0; i < f->value_count; i++)
        if (stored && strcmp(stored, f->values[i]) == 0) { list_select(w, i); return; }

    /* Nichts Passendes - dann auch nichts ausgewählt. Die alte Auswahl stehen
     * zu lassen hieße, dass ein Formular den Wert des zuvor geöffneten
     * Datensatzes zeigt. */
    list_select_none(w);
}

/* --- Die Registratur ----------------------------------------------------------------
 *
 * Die Reihenfolge folgt field_kind aus schema.h, und ein Test prüft, dass zu
 * jedem Wert ein Eintrag gehört. Ein neuer Feldtyp, den jemand hinzufügt und
 * hier vergisst, fällt damit sofort auf.
 */
static const field_kind_ops OPS[] = {
    [FIELD_TEXT] = {
        .name = "text", .format = text_format, .parse = text_parse,
        .make_widget = text_widget, .read = text_read, .write = text_write,
    },
    [FIELD_GEMTEXT] = {
        .name = "gemtext", .format = text_format, .parse = text_parse,
        .make_widget = gemtext_widget, .read = text_read, .write = text_write,
    },
    [FIELD_DATE] = {
        .name = "date", .format = date_format, .parse = date_parse,
        .make_widget = text_widget, .read = text_read, .write = text_write,
    },
    [FIELD_BOOL] = {
        .name = "bool", .format = bool_format, .parse = bool_parse,
        .make_widget = bool_widget, .read = bool_read, .write = bool_write,
    },
    [FIELD_CHOICE] = {
        .name = "choice", .format = choice_format, .parse = choice_parse,
        .make_widget = choice_widget, .read = choice_read, .write = choice_write,
    },
};

const field_kind_ops *fieldkind(field_kind kind)
{
    if ((int)kind < 0 || (size_t)kind >= sizeof OPS / sizeof OPS[0]) return &OPS[FIELD_TEXT];
    return &OPS[kind];
}

const field_kind_ops *fieldkind_of(const schema_field *f)
{
    return fieldkind(f->kind);
}
