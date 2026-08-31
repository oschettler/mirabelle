/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Siehe browser.h für den Vertrag.
 *
 * Die Datei ist länger als die anderen, aber sie enthält keine einzige
 * Entscheidung über eine bestimmte Sammlung. Überall, wo man eine erwarten
 * würde, steht stattdessen ein Blick ins Schema oder in die
 * Feldtyp-Registratur.
 *
 * Das ist der Punkt von D-7, und er ist nachprüfbar: es gibt in dieser Datei
 * kein Zeichenkettenliteral, das eine Sammlung, ein Feld oder eine Anwendung
 * benennt. Jeder Name kommt aus dem Schema.
 */
#include "app/browser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/fieldkind.h"
#include "app/monthview.h"
#include "core/date.h"
#include "gfx/font.h"
#include "gfx/text.h"
#include "store/query.h"
#include "store/record.h"
#include "ui/panel.h"
#include "ui/widget.h"

extern const font system12;

#define ROW_MAX     256
#define RECORDS_MAX 512
#define VALUE_MAX   512

struct browser {
    const schema  *s;
    vault         *v;
    const catalog *cat;
    const collate *sort;
    const collate *search;
    theme          th;      /* Kopie - siehe widget.h, die Regel gilt auch hier */

    browser_view view;

    /* Die Liste. ids und rows laufen parallel: Zeile i zeigt Datensatz ids[i]. */
    char ids[RECORDS_MAX][RECORD_ID_LEN + 1];
    char rows[RECORDS_MAX][ROW_MAX];

    /* Nur bei VIEW_MONTH gefüllt: der Tag, an dem Datensatz i liegt. Ohne
     * gültiges Datum hat er keinen Tag und taucht im Raster nicht auf. */
    date days[RECORDS_MAX];
    bool has_day[RECORDS_MAX];
    int  count;

    /* Genau eines von beiden, je nach Schema. */
    widget *list;
    widget *month;

    /* Das Formular. widgets[i] gehört zu s->form[i]. */
    panel  *form;
    widget *widgets[SCHEMA_FIELDS_MAX];
    char    editing[RECORD_ID_LEN + 1];   /* leer heißt: ein neuer Datensatz */

    char filter[QUERY_VALUE_MAX];
    rect area;
};

/* --- Anlegen ------------------------------------------------------------------- */

browser *browser_create(const schema *s, vault *v, const theme *th,
                        const catalog *cat,
                        const collate *sort, const collate *search)
{
    if (!s || !v || !th) return NULL;

    browser *b = calloc(1, sizeof *b);
    if (!b) return NULL;

    b->s      = s;
    b->v      = v;
    b->cat    = cat;
    b->sort   = sort;
    b->search = search;
    b->th     = *th;          /* kopieren, nie zeigen */
    b->view   = BROWSE_LIST;

    /* Welche Übersicht es wird, entscheidet das Schema - und diese eine
     * Verzweigung ist alles, was der Browser davon merkt. */
    if (s->view == VIEW_MONTH) {
        date start = date_today();
        b->month = monthview_create(&b->th, cat, start);
    } else {
        b->list = list_create(&b->th, cat);
    }

    if (!b->list && !b->month) {
        free(b);
        return NULL;
    }
    return b;
}

static void form_close(browser *b)
{
    if (b->form) panel_destroy(b->form);
    b->form = NULL;
    memset(b->widgets, 0, sizeof b->widgets);
    b->editing[0] = '\0';
}

void browser_destroy(browser *b)
{
    if (!b) return;
    form_close(b);
    widget_destroy(b->list);
    widget_destroy(b->month);
    free(b);
}

/* --- Einen Wert aus einem Datensatz holen ---------------------------------------
 *
 * Ein Gemtext-Feld ist der Körper, alles andere ein Eintrag im Front Matter.
 * Diese eine Fallunterscheidung steht genau hier und sonst nirgends.
 */
static const char *field_value(const schema_field *f, record *rec)
{
    if (f->kind == FIELD_GEMTEXT) return record_body(rec);

    const char *v = frontmatter_get(record_fields(rec), f->name);
    return v ? v : "";
}

/* --- Die Liste ------------------------------------------------------------------
 *
 * Eine Zeile ist die Aneinanderreihung der Spalten aus dem Schema, jede in
 * ihrer Anzeigeform. Getrennt wird durch zwei Leerzeichen - das ist wenig
 * hübsch, aber es ist eine Entscheidung, die das Zeichnen später verfeinern
 * kann, ohne dass sich hier etwas ändert.
 */
static void build_row(const browser *b, record *rec, char *out, size_t out_size)
{
    size_t n = 0;
    out[0] = '\0';

    for (int i = 0; i < b->s->column_count; i++) {
        const schema_field *f = schema_field_by_name(b->s, b->s->columns[i]);
        if (!f) continue;      /* schema_check hat das schon ausgeschlossen */

        char shown[VALUE_MAX];
        fieldkind_of(f)->format(f, b->cat, field_value(f, rec), shown, sizeof shown);

        /* Ein Körper geht über mehrere Zeilen; in einer Listenzeile zählt nur
         * die erste. */
        char *nl = strchr(shown, '\n');
        if (nl) *nl = '\0';

        if (n > 0 && n + 2 < out_size) {
            out[n++] = ' ';
            out[n++] = ' ';
        }
        n += (size_t)snprintf(out + n, out_size - n, "%s", shown);
        if (n >= out_size - 1) { n = out_size - 1; break; }
    }
    out[n] = '\0';
}

/* Trägt die Tage des angezeigten Monats ins Raster ein.
 *
 * Wird nach jedem Neuladen gerufen und nach jedem Ereignis, das den Monat
 * gewechselt haben kann. Der Kalender vergisst seine Markierungen beim
 * Blättern selbst (monthview.h) - sonst stünden Striche an Tagen, an denen
 * nichts ist. */
static void refresh_marks(browser *b)
{
    if (!b->month) return;

    monthview_clear_marks(b->month);
    for (int i = 0; i < b->count; i++)
        if (b->has_day[i]) monthview_mark(b->month, b->days[i]);
}

/* Der Vergleich für qsort. Die Abfrage und die Tabelle stehen in Dateiglobalen,
 * weil qsort keinen Kontext durchreicht; sie leben nur während des Sortierens. */
static const query   *g_query;
static const collate *g_sort;

typedef struct { char id[RECORD_ID_LEN + 1]; record *rec; } entry;

static int entry_cmp(const void *a, const void *b)
{
    const entry *x = a, *y = b;
    return query_compare(g_query, x->rec, y->rec, g_sort);
}

bool browser_reload(browser *b, char *err, size_t err_size)
{
    char ids[RECORDS_MAX][RECORD_ID_LEN + 1];
    int  n = 0;

    if (!vault_list(b->v, b->s->folder, ids, RECORDS_MAX, &n, err, err_size))
        return false;

    query q;
    query_init(&q, b->s->folder);
    if (b->filter[0]) query_text(&q, b->filter);
    query_order(&q, b->s->sort, b->s->sort_desc);

    /* Alle Datensätze laden, filtern, sortieren, und erst danach die Zeilen
     * bauen: die Sortierung braucht die Datensätze, die Anzeige nur den Text. */
    static entry keep[RECORDS_MAX];
    int          kept = 0;

    for (int i = 0; i < n; i++) {
        record *rec = vault_load(b->v, b->s->folder, ids[i], err, err_size);
        if (!rec) continue;   /* eine kaputte Datei bringt die Liste nicht zu Fall */

        if (!query_matches(&q, rec, b->search)) {
            record_free(rec);
            continue;
        }

        snprintf(keep[kept].id, sizeof keep[kept].id, "%s", ids[i]);
        keep[kept].rec = rec;
        kept++;
    }

    g_query = &q;
    g_sort  = b->sort;
    qsort(keep, (size_t)kept, sizeof keep[0], entry_cmp);
    g_query = NULL;
    g_sort  = NULL;

    const schema_field *daysrc = b->s->view == VIEW_MONTH
                               ? schema_field_by_name(b->s, b->s->view_field)
                               : NULL;

    const char *rows[RECORDS_MAX];
    for (int i = 0; i < kept; i++) {
        snprintf(b->ids[i], sizeof b->ids[i], "%s", keep[i].id);
        build_row(b, keep[i].rec, b->rows[i], ROW_MAX);
        rows[i] = b->rows[i];

        b->has_day[i] = daysrc &&
                        date_from_iso(field_value(daysrc, keep[i].rec), &b->days[i]);

        record_free(keep[i].rec);
    }
    b->count = kept;

    if (b->list) {
        /* Kopieren lassen: rows zeigt in b->rows, und das nächste Neuladen
         * überschreibt es. Die Liste soll nicht darauf angewiesen sein, wann
         * das geschieht. */
        if (!list_set_items_copy(b->list, rows, kept)) {
            if (err && err_size) snprintf(err, err_size, "kein Speicher für die Liste");
            return false;
        }
    }
    refresh_marks(b);

    if (err && err_size) err[0] = '\0';
    return true;
}

browser_view browser_view_of(const browser *b) { return b->view; }
int          browser_count(const browser *b)   { return b->count; }

widget *browser_list(const browser *b)  { return b->list; }
widget *browser_month(const browser *b) { return b->month; }

/* Der Index des Datensatzes, auf dem die Übersicht gerade steht.
 *
 * Im Raster gibt es keinen Index, sondern einen Tag - also den ersten
 * Datensatz dieses Tages. Liegt an dem Tag nichts, ist nichts ausgewählt; das
 * ist im Kalender ein gewöhnlicher Zustand und kein Fehler. */
int browser_selected(const browser *b)
{
    if (b->list) return list_selected(b->list);

    date sel = monthview_selected(b->month);
    for (int i = 0; i < b->count; i++)
        if (b->has_day[i] && date_compare(b->days[i], sel) == 0) return i;
    return -1;
}

void browser_select(browser *b, int index)
{
    if (b->list) { list_select(b->list, index); return; }

    if (index >= 0 && index < b->count && b->has_day[index]) {
        monthview_select(b->month, b->days[index]);
        refresh_marks(b);
    }
}

const char *browser_selected_id(const browser *b)
{
    int i = browser_selected(b);
    if (i < 0 || i >= b->count) return NULL;
    return b->ids[i];
}

const char *browser_row_text(const browser *b, int index)
{
    if (index < 0 || index >= b->count) return NULL;
    return b->rows[index];
}

bool browser_set_filter(browser *b, const char *text)
{
    if (!text) text = "";
    if (strlen(text) >= sizeof b->filter) return false;

    snprintf(b->filter, sizeof b->filter, "%s", text);
    return true;
}

/* --- Das Formular ---------------------------------------------------------------
 *
 * Für jedes Feld aus s->form eine Beschriftung und das Bedienelement, das sein
 * Feldtyp mitbringt. Kein Feldname steht im Code.
 */
static bool form_build(browser *b, record *rec, char *err, size_t err_size)
{
    form_close(b);

    b->form = panel_create(&b->th, b->cat);
    if (!b->form) {
        if (err && err_size) snprintf(err, err_size, "kein Speicher für das Formular");
        return false;
    }
    panel_set_layout(b->form, LAYOUT_FORM, b->th.button_gap, b->th.dialog_pad / 2);

    for (int i = 0; i < b->s->form_count; i++) {
        const schema_field *f = schema_field_by_name(b->s, b->s->form[i]);
        if (!f) continue;

        const field_kind_ops *ops = fieldkind_of(f);

        /* Das Kästchen trägt seine Beschriftung selbst; ihm noch eine
         * danebenzustellen ergäbe sie zweimal. Deshalb bekommt es eine leere
         * Zelle links - das Layout ist paarweise. */
        widget *label = f->kind == FIELD_BOOL
                      ? label_create(&b->th, b->cat, "")
                      : label_create(&b->th, b->cat, f->label);

        widget *w = ops->make_widget(f, &b->th, b->cat);
        if (!label || !w) {
            widget_destroy(label);
            widget_destroy(w);
            form_close(b);
            if (err && err_size) snprintf(err, err_size, "kein Speicher für ein Feld");
            return false;
        }

        /* Ein mehrzeiliges Feld bekommt einen Rollbalken daneben. Beide
         * zusammen sind ein verschachteltes Panel (panel.h) - für das
         * Formular ist es ein Element, für den Nutzer ein Textfeld, an dem
         * man sieht, wie weit man ist.
         *
         * b->widgets[i] bleibt dabei das Textfeld: gelesen und geschrieben
         * wird dort, nicht am Rahmen darum. */
        widget *cell = w;
        if (f->kind == FIELD_GEMTEXT) {
            panel  *row = panel_create(&b->th, b->cat);
            widget *bar = row ? scrollbar_create(&b->th, b->cat, SCROLLBAR_VERTICAL,
                                                 text_widget_scroll(w))
                              : NULL;
            widget *wrapped = NULL;

            if (row && bar) {
                panel_set_layout(row, LAYOUT_HSTACK_FILL, 0, 0);

                if (panel_add(row, w) && panel_add(row, bar)) {
                    wrapped = panel_as_widget(row);
                    text_widget_set_focus_extra(w, bar);
                }
            }

            if (!wrapped) {
                if (row) panel_destroy(row);
                else     { widget_destroy(w); widget_destroy(bar); }

                widget_destroy(label);
                form_close(b);
                if (err && err_size) snprintf(err, err_size, "kein Speicher für ein Feld");
                return false;
            }
            cell = wrapped;
        }

        if (!panel_add(b->form, label) || !panel_add(b->form, cell)) {
            form_close(b);
            if (err && err_size) snprintf(err, err_size, "das Formular ist voll");
            return false;
        }

        b->widgets[i] = w;
        if (rec) ops->write(f, b->cat, w, field_value(f, rec));
    }

    panel_focus_next(b->form);
    b->view = BROWSE_FORM;
    if (err && err_size) err[0] = '\0';
    return true;
}

bool browser_open_selected(browser *b, char *err, size_t err_size)
{
    const char *id = browser_selected_id(b);
    if (!id) {
        if (err && err_size) snprintf(err, err_size, "nichts ausgewählt");
        return false;
    }

    record *rec = vault_load(b->v, b->s->folder, id, err, err_size);
    if (!rec) return false;

    bool ok = form_build(b, rec, err, err_size);
    record_free(rec);

    if (ok) snprintf(b->editing, sizeof b->editing, "%s", id);
    return ok;
}

bool browser_new(browser *b, char *err, size_t err_size)
{
    if (!form_build(b, NULL, err, err_size)) return false;
    b->editing[0] = '\0';

    /* Im Kalender ist der ausgewählte Tag die Antwort auf die Frage „wann".
     * Ihn nicht zu übernehmen hieße, den Nutzer nach etwas zu fragen, das er
     * gerade angeklickt hat. */
    if (b->month) {
        const schema_field *f = schema_field_by_name(b->s, b->s->view_field);
        widget             *w = browser_form_widget(b, b->s->view_field);

        if (f && w) {
            char iso[16];
            date_to_iso(monthview_selected(b->month), iso, sizeof iso);
            fieldkind_of(f)->write(f, b->cat, w, iso);
        }
    }
    return true;
}

void browser_cancel(browser *b)
{
    form_close(b);
    b->view = BROWSE_LIST;
}

/* Setzt den Text des Datensatzes zusammen: Front Matter, dann der Körper.
 *
 * Der Umweg über den Text ist kein Umweg, sondern der einzige Weg:
 * frontmatter.h kennt keinen Setter, weil ein Datensatz aus seiner Datei
 * entsteht und nicht Feld für Feld zusammengeklickt wird. Wer speichern will,
 * schreibt die Datei - und liest sie zur Probe gleich wieder ein.
 */
static bool compose(browser *b, char *out, size_t out_size, char *err, size_t err_size)
{
    size_t n = 0;
    n += (size_t)snprintf(out + n, out_size - n, "---\n");

    /* Die Kennung bleibt erhalten. Ohne sie legte vault_save eine neue an, und
     * aus dem Bearbeiten würde ein zweiter Datensatz. */
    if (b->editing[0])
        n += (size_t)snprintf(out + n, out_size - n, "id: %s\n", b->editing);

    const char *body = "";

    for (int i = 0; i < b->s->form_count; i++) {
        const schema_field *f = schema_field_by_name(b->s, b->s->form[i]);
        if (!f || !b->widgets[i]) continue;

        static char value[VALUE_MAX];
        if (!fieldkind_of(f)->read(f, b->cat, b->widgets[i], value, sizeof value)) {
            if (err && err_size)
                snprintf(err, err_size, "%s: die Eingabe passt nicht zu %s",
                         T(b->cat, f->label), schema_kind_name(f->kind));
            return false;
        }

        if (f->required && !value[0]) {
            if (err && err_size)
                snprintf(err, err_size, "%s: darf nicht leer sein", T(b->cat, f->label));
            return false;
        }

        if (f->kind == FIELD_GEMTEXT) { body = value; continue; }
        if (!value[0]) continue;      /* leere Felder gar nicht erst schreiben */

        /* Ein Zeilenumbruch in einem Skalar würde das Front Matter zerreißen.
         * Die einzeiligen Bedienelemente erzeugen keinen; käme doch einer -
         * etwa über text_widget_set_value von außen -, ist Ablehnen richtig. */
        if (strchr(value, '\n')) {
            if (err && err_size)
                snprintf(err, err_size, "%s: ein Zeilenumbruch gehört da nicht hin",
                         T(b->cat, f->label));
            return false;
        }

        n += (size_t)snprintf(out + n, out_size - n, "%s: %s\n", f->name, value);
        if (n >= out_size) break;
    }

    n += (size_t)snprintf(out + n, out_size - n, "---\n%s", body);

    if (n >= out_size) {
        if (err && err_size) snprintf(err, err_size, "der Datensatz ist zu groß");
        return false;
    }
    return true;
}

bool browser_save(browser *b, char *err, size_t err_size)
{
    if (b->view != BROWSE_FORM) {
        if (err && err_size) snprintf(err, err_size, "kein Formular offen");
        return false;
    }

    static char text[64 * 1024];
    if (!compose(b, text, sizeof text, err, err_size)) return false;

    record *rec = record_parse(text, strlen(text), b->s->folder, err, err_size);
    if (!rec) return false;

    char id[RECORD_ID_LEN + 1];
    bool ok = vault_save(b->v, b->s->folder, rec, id, sizeof id, err, err_size);
    record_free(rec);
    if (!ok) return false;

    form_close(b);
    b->view = BROWSE_LIST;

    if (!browser_reload(b, err, err_size)) return false;

    /* Auf den gerade gespeicherten Datensatz stellen - der Nutzer soll sehen,
     * wo das gelandet ist, was er geschrieben hat.
     *
     * Über browser_select() und nicht über die Liste: im Kalender gibt es
     * keine, und dort heißt „dorthin stellen", den Tag aufzuschlagen. */
    for (int i = 0; i < b->count; i++)
        if (strcmp(b->ids[i], id) == 0) { browser_select(b, i); break; }

    return true;
}

bool browser_delete_selected(browser *b, char *err, size_t err_size)
{
    const char *id = browser_selected_id(b);
    if (!id) {
        if (err && err_size) snprintf(err, err_size, "nichts ausgewählt");
        return false;
    }

    char keep[RECORD_ID_LEN + 1];
    snprintf(keep, sizeof keep, "%s", id);
    int was = browser_selected(b);

    if (!vault_delete(b->v, b->s->folder, keep, err, err_size)) return false;
    if (!browser_reload(b, err, err_size)) return false;

    /* Die Auswahl dort lassen, wo sie war, statt an den Anfang zu springen -
     * wer drei Einträge hintereinander löscht, will nicht jedes Mal wieder
     * nach unten scrollen. Im Kalender bleibt der Tag ohnehin stehen; dort
     * tut dieser Aufruf nichts, und das ist richtig so. */
    if (b->list && b->count > 0)
        list_select(b->list, was < b->count ? was : b->count - 1);
    return true;
}

widget *browser_form_widget(const browser *b, const char *field)
{
    if (b->view != BROWSE_FORM || !b->form) return NULL;

    for (int i = 0; i < b->s->form_count; i++)
        if (strcmp(b->s->form[i], field) == 0) return b->widgets[i];
    return NULL;
}

/* --- Oberfläche ------------------------------------------------------------------ */

/* Das Widget der Übersicht - Liste oder Raster. Ab hier interessiert der
 * Unterschied nicht mehr. */
static widget *overview(const browser *b)
{
    return b->list ? b->list : b->month;
}

void browser_layout(browser *b, rect area)
{
    b->area = area;

    if (b->view == BROWSE_FORM && b->form) panel_layout(b->form, area);
    else                                   overview(b)->frame = area;
}

void browser_draw(const browser *b, gc *g)
{
    if (b->view == BROWSE_FORM && b->form) panel_draw(b->form, g);
    else                                   widget_draw(overview(b), g);
}

bool browser_event(browser *b, const event *e)
{
    if (b->view == BROWSE_FORM && b->form)
        return panel_event(b->form, e, NULL);

    widget *w = overview(b);

    /* Die Übersicht bekommt den Fokus fest: es gibt in dieser Ansicht nichts
     * anderes, was ihn haben könnte. */
    w->focused = true;
    bool used = widget_event(w, e);

    /* Ein Ereignis kann den Monat gewechselt haben, und das Raster hat seine
     * Markierungen dabei vergessen. Sie stehen in den geladenen Datensätzen,
     * lassen sich also wieder eintragen, ohne den Vault zu fragen. */
    if (used) refresh_marks(b);
    return used;
}

bool browser_was_opened(browser *b)
{
    return b->list ? list_was_opened(b->list) : monthview_was_opened(b->month);
}
