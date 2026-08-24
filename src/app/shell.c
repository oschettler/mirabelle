/* Siehe shell.h für den Vertrag.
 *
 * Die Datei ist lang, aber sie enthält keine Entscheidung über eine bestimmte
 * Anwendung. Sie zählt Dateien, öffnet Fenster und reicht Ereignisse weiter;
 * was in einem Fenster geschieht, entscheidet der Browser aus dem Schema.
 */
#include "app/shell.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/browser.h"
#include "app/schema.h"
#include "ui/menu.h"
#include "ui/widget.h"
#include "ui/window.h"
#include "ui/wm.h"

#define APPS_MAX 16

/* Woher eine Anwendung kommt.
 *
 * Beides sind Anwendungen, beide bekommen ein Fenster und einen Menüeintrag.
 * Der Unterschied steckt nur darin, wer den Inhalt zeichnet - hier der
 * Browser, dort ein Skript. */
typedef enum {
    APP_SCHEMA,   /* aus data/schema, der Browser zeigt sie */
    APP_SCRIPT    /* aus data/apps, ein Skript zeigt sie */
} app_kind;

typedef struct {
    app_kind kind;
    char     file[128];      /* Dateiname, für die Reihenfolge und Meldungen */
    char     label[64];      /* Katalogschlüssel; zugleich der Aktionsname */

    schema  sch;             /* nur bei APP_SCHEMA */
    int     script;          /* nur bei APP_SCRIPT: Index beim Skriptsystem */

    window  *win;            /* NULL, solange nicht geöffnet */
    browser *br;
    widget  *bar;            /* Rollbalken neben der Übersicht */
} app_entry;

struct shell {
    shell_config cfg;
    theme        th;          /* eigene Kopie - siehe widget.h */

    app_entry apps[APPS_MAX];
    int       app_count;

    wm      *wm;
    menubar *mb;

    /* Die Menüs werden zur Laufzeit gebaut, weil erst dann feststeht, welche
     * Anwendungen es gibt. Die Felder müssen die Menüleiste überleben, also
     * liegen sie hier und nicht auf einem Stapel. */
    menu_item app_items[APPS_MAX];
    menu      menus[3];

    char last_action[64];
    char last_error[256];
    bool running;
};

/* --- Schemata einlesen -------------------------------------------------------------
 *
 * Gezählt werden Dateien, nicht Anwendungen. Eine fünfte Schemadatei ist eine
 * fünfte Anwendung, und in dieser Datei ändert sich dafür nichts.
 */

static int by_name(const void *a, const void *b)
{
    return strcmp(((const app_entry *)a)->file, ((const app_entry *)b)->file);
}

static bool ends_with(const char *s, const char *suffix)
{
    size_t n = strlen(s), m = strlen(suffix);
    return n >= m && strcmp(s + n - m, suffix) == 0;
}

static bool load_schemas(shell *s, char *err, size_t err_size)
{
    char dir[512];
    snprintf(dir, sizeof dir, "%s/schema", s->cfg.data_dir);

    static plat_dirent entries[64];
    int                n = 0;

    if (!plat_list(dir, entries, 64, &n)) {
        snprintf(err, err_size, "%s: nicht lesbar", dir);
        return false;
    }

    for (int i = 0; i < n && s->app_count < APPS_MAX; i++) {
        if (entries[i].is_dir) continue;

        /* Nur die Textfassung. Es gibt auch Lua-Schemata (D-15), aber
         * data/schema/task.lua beschreibt dieselbe Anwendung wie task.schema -
         * beide zu laden ergäbe sie zweimal. Die Lua-Fassung ist der Beweis,
         * dass zwei Lader dieselbe Struktur liefern, kein zweiter Vorrat. */
        if (!ends_with(entries[i].name, ".schema")) continue;

        char path[700];
        snprintf(path, sizeof path, "%s/%s", dir, entries[i].name);

        app_entry *a = &s->apps[s->app_count];
        memset(a, 0, sizeof *a);

        char msg[256] = "";
        if (!schema_load(&a->sch, path, msg, sizeof msg)) {
            /* Eine kaputte Datei nimmt nicht die ganze Anwendung mit - sie
             * fehlt eben, und der Nutzer erfährt warum. */
            snprintf(s->last_error, sizeof s->last_error, "%s", msg);
            continue;
        }

        a->kind = APP_SCHEMA;
        snprintf(a->file, sizeof a->file, "%s", entries[i].name);
        snprintf(a->label, sizeof a->label, "%s", a->sch.label);
        s->app_count++;
    }

    if (s->app_count == 0) {
        snprintf(err, err_size, "%s: kein brauchbares Schema", dir);
        return false;
    }

    qsort(s->apps, (size_t)s->app_count, sizeof s->apps[0], by_name);
    return true;
}

/* Die Skriptanwendungen anhängen - nach den Schemaanwendungen, damit die
 * Reihenfolge der vier eingebauten festliegt und nicht davon abhängt, was
 * jemand in data/apps legt. */
static void load_scripts(shell *s)
{
    const shell_scripting *sc = s->cfg.scripts;
    if (!sc || !sc->count) return;

    int n = sc->count(sc->user);
    for (int i = 0; i < n && s->app_count < APPS_MAX; i++) {
        const char *title = sc->title ? sc->title(sc->user, i) : NULL;
        if (!title) continue;

        app_entry *a = &s->apps[s->app_count++];
        memset(a, 0, sizeof *a);

        a->kind   = APP_SCRIPT;
        a->script = i;
        snprintf(a->label, sizeof a->label, "%s", title);
        snprintf(a->file, sizeof a->file, "%s", title);
    }
}

/* --- Menüs ------------------------------------------------------------------------- */

static const menu_item FILE_ITEMS[] = {
    { "menu.file.new",    "record.new"    },
    { "menu.file.save",   "record.save"   },
    { "menu.file.delete", "record.delete" },
    { NULL,               NULL            },
    { "menu.file.close",  "window.close"  },
    { "menu.file.quit",   "app.quit"      },
};

static const menu_item EDIT_ITEMS[] = {
    { "menu.edit.undo",  "edit.undo"  },
    { "menu.edit.redo",  "edit.redo"  },
    { NULL,              NULL         },
    { "menu.edit.cut",   "edit.cut"   },
    { "menu.edit.copy",  "edit.copy"  },
    { "menu.edit.paste", "edit.paste" },
};

/* Der Katalogschlüssel des Schemas ist zugleich der Name der Aktion, die die
 * Anwendung öffnet.
 *
 * Das ist kein Kunstgriff, sondern spart eine Zuordnungstabelle: in
 * data/keys/default.keys steht „app.tasks Cmd+1 global", und damit öffnet
 * kbd:[Cmd+1] die Aufgaben - ohne dass irgendwo eine Zahl mit einer Anwendung
 * verknüpft würde. Eine fünfte Schemadatei bekommt ihr Kürzel durch eine Zeile
 * in der Tastenbelegung, nicht durch Code. */
static bool build_menus(shell *s)
{
    for (int i = 0; i < s->app_count; i++) {
        s->app_items[i].key    = s->apps[i].label;
        s->app_items[i].action = s->apps[i].label;
    }

    s->menus[0] = (menu){ "menu.file",  FILE_ITEMS, 6 };
    s->menus[1] = (menu){ "menu.edit",  EDIT_ITEMS, 6 };
    s->menus[2] = (menu){ "menu.apps",  s->app_items, s->app_count };

    s->mb = menubar_create(s->menus, 3, s->cfg.catalog, s->cfg.keymap, &s->th);
    return s->mb != NULL;
}

/* --- Anlegen und Abräumen ----------------------------------------------------------- */

/* Wird gerufen, wenn ein Fenster über sein Schließfeld verschwindet. Ohne das
 * bliebe hier ein Zeiger auf ein freigegebenes Fenster stehen - genau der
 * Fehler, der dieses Projekt schon einmal zum Absturz gebracht hat. */
static void on_window_closed(window *w, void *user)
{
    shell *s = user;

    for (int i = 0; i < s->app_count; i++)
        if (s->apps[i].win == w) {
            widget_destroy(s->apps[i].bar);
            browser_destroy(s->apps[i].br);
            s->apps[i].bar = NULL;
            s->apps[i].br  = NULL;
            s->apps[i].win = NULL;
            return;
        }
}

shell *shell_create(const shell_config *cfg, char *err, size_t err_size)
{
    if (!cfg || !cfg->data_dir || !cfg->vault || !cfg->theme) {
        if (err && err_size) snprintf(err, err_size, "unvollständige Angaben");
        return NULL;
    }

    shell *s = calloc(1, sizeof *s);
    if (!s) {
        if (err && err_size) snprintf(err, err_size, "kein Speicher");
        return NULL;
    }

    s->cfg     = *cfg;
    s->th      = *cfg->theme;      /* kopieren, nie zeigen */
    s->running = true;

    if (!load_schemas(s, err, err_size)) {
        free(s);
        return NULL;
    }
    load_scripts(s);

    s->wm = wm_create(&s->th, cfg->screen_w, cfg->screen_h);
    if (!s->wm || !build_menus(s)) {
        shell_destroy(s);
        if (err && err_size) snprintf(err, err_size, "kein Speicher");
        return NULL;
    }

    if (err && err_size) err[0] = '\0';
    return s;
}

void shell_destroy(shell *s)
{
    if (!s) return;

    if (s->mb) menubar_free(s->mb);

    /* Die Fensterverwaltung zuerst. Sie schließt jedes Fenster, und jedes
     * Schließen ruft on_window_closed, und das räumt Browser und Rollbalken
     * weg und setzt die Zeiger auf NULL.
     *
     * Umgekehrt herum wäre es ein doppeltes Freigeben: erst hier wegräumen,
     * dann noch einmal über den Rückruf. Wer einen Rückruf beim Schließen
     * einrichtet, muss ihn auch beim Aufräumen mitdenken. */
    if (s->wm) wm_destroy(s->wm);

    /* Was nie ein Fenster hatte, hat auch nichts belegt - der Durchgang hier
     * ist die Zusicherung, dass nichts übrigbleibt, wenn sich das einmal
     * ändert. */
    for (int i = 0; i < s->app_count; i++) {
        widget_destroy(s->apps[i].bar);
        browser_destroy(s->apps[i].br);
    }

    free(s);
}

int shell_app_count(const shell *s) { return s->app_count; }

const char *shell_app_label(const shell *s, int index)
{
    if (index < 0 || index >= s->app_count) return NULL;
    return s->apps[index].label;
}

bool shell_app_is_open(const shell *s, int index)
{
    if (index < 0 || index >= s->app_count) return false;
    return s->apps[index].win != NULL;
}

window *shell_app_window(const shell *s, int index)
{
    if (index < 0 || index >= s->app_count) return NULL;
    return s->apps[index].win;
}

browser *shell_app_browser(const shell *s, int index)
{
    if (index < 0 || index >= s->app_count) return NULL;
    return s->apps[index].br;
}

int shell_active_app(const shell *s)
{
    window *w = wm_active(s->wm);
    if (!w) return -1;

    for (int i = 0; i < s->app_count; i++)
        if (s->apps[i].win == w) return i;
    return -1;
}

int shell_window_count(const shell *s) { return wm_count(s->wm); }

const char *shell_last_action(const shell *s) { return s->last_action; }
const char *shell_last_error(const shell *s)  { return s->last_error; }
bool        shell_running(const shell *s)     { return s->running; }

/* --- Fenster öffnen ------------------------------------------------------------------
 *
 * Die Fenster werden versetzt gestapelt, wie es sich für einen Schreibtisch
 * gehört: das zweite liegt ein Stück rechts unter dem ersten, und keines
 * verdeckt das andere vollständig.
 */
bool shell_open_app(shell *s, int index, char *err, size_t err_size)
{
    if (index < 0 || index >= s->app_count) {
        snprintf(err, err_size, "diese Anwendung gibt es nicht");
        return false;
    }

    app_entry *a = &s->apps[index];
    if (a->win) {
        wm_activate(s->wm, a->win);
        return true;
    }

    /* Versetzt gestapelt, wie es sich für einen Schreibtisch gehört: das
     * zweite Fenster liegt ein Stück rechts unter dem ersten, und keines
     * verdeckt das andere vollständig.
     *
     * Der Versatz geht von der Größe ab, nicht nur von der Lage - sonst hinge
     * das sechste Fenster unten aus dem Bildschirm heraus, und seine untere
     * Kante samt Größenfeld wäre nicht mehr zu greifen. */
    int top    = menubar_height(s->mb) + 12;
    int offset = index * 24;

    int w = s->cfg.screen_w / 2;
    int h = s->cfg.screen_h - top - offset - 24;

    if (h < 6 * s->th.titlebar_h) h = 6 * s->th.titlebar_h;

    rect frame = rect_make(24 + offset, top + offset, w, h);

    a->win = wm_open(s->wm, frame, T(s->cfg.catalog, a->label), WIN_NORMAL);
    if (!a->win) {
        snprintf(err, err_size, "kein Fenster mehr frei");
        return false;
    }
    window_set_on_close(a->win, on_window_closed, s);

    /* Eine Skriptanwendung braucht weder Browser noch Rollbalken - sie
     * zeichnet selbst. Das Fenster bekommt sie trotzdem von hier, damit sie
     * sich wie jede andere verschieben und schließen lässt. */
    if (a->kind == APP_SCRIPT) {
        if (err && err_size) err[0] = '\0';
        return true;
    }

    a->br = browser_create(&a->sch, s->cfg.vault, &s->th, s->cfg.catalog,
                           s->cfg.sort, s->cfg.search);
    if (!a->br) {
        wm_close(s->wm, a->win);
        snprintf(err, err_size, "kein Speicher");
        return false;
    }

    if (!browser_reload(a->br, err, err_size)) {
        /* Die Sammlung ist nicht lesbar. Das Fenster bleibt trotzdem offen -
         * leer und mit einer Meldung ist besser als gar nicht, denn sonst
         * sieht der Nutzer nur, dass nichts passiert. */
        snprintf(s->last_error, sizeof s->last_error, "%s", err);
    }

    /* Der Rollbalken hängt am Modell der Übersicht. Beim Kalender gibt es
     * keine Liste, also auch keinen Balken - ein Monatsraster blättert
     * monatsweise und nicht zeilenweise. */
    widget *ov = browser_list(a->br);
    if (ov) {
        a->bar = scrollbar_create(&s->th, s->cfg.catalog, SCROLLBAR_VERTICAL,
                                  list_scroll(ov));
    }

    if (err && err_size) err[0] = '\0';
    return true;
}

/* --- Aktionen -------------------------------------------------------------------------
 *
 * Menü und Tastenkürzel landen beide hier. Sonst könnten sie auseinanderlaufen,
 * und dann täte kbd:[Cmd+S] etwas anderes als „Sichern".
 */

static app_entry *active_app(shell *s)
{
    int i = shell_active_app(s);
    return i < 0 ? NULL : &s->apps[i];
}

/* Was die Schale selbst tut, im Unterschied zu dem, was ein Bedienelement tut.
 *
 * Die Tastenbelegung kennt beides nebeneinander: `list.next` bewegt die
 * Auswahl und gehört der Liste, `list.open` öffnet einen Datensatz und gehört
 * der Anwendung. Ohne diese Unterscheidung würde die Schale entweder alles
 * schlucken - dann bewegte sich keine Auswahl mehr - oder alles durchlassen,
 * und dann täte Return nichts.
 *
 * Anwendungsnamen stehen nicht in der Liste: sie werden getrennt geprüft, weil
 * es sie erst gibt, wenn die Schemadateien gelesen sind. */
static bool is_app_label(const shell *s, const char *action)
{
    for (int i = 0; i < s->app_count; i++)
        if (strcmp(action, s->apps[i].label) == 0) return true;
    return false;
}

static bool shell_handles(const char *action)
{
    static const char *const OURS[] = {
        "app.quit", "window.close",
        "record.new", "record.save", "record.delete",
        "list.open", "form.accept", "form.cancel",
    };

    for (size_t i = 0; i < sizeof OURS / sizeof OURS[0]; i++)
        if (strcmp(OURS[i], action) == 0) return true;
    return false;
}

/* Der Inhalt eines Skriptfensters. Getrennt, weil beide Aufrufstellen -
 * Zeichnen und Ereignis - dieselbe Größe brauchen. */
static bool script_ok(const shell *s, const app_entry *a)
{
    return a->kind == APP_SCRIPT && s->cfg.scripts && a->win;
}

void shell_run_action(shell *s, const char *action)
{
    if (!action) return;
    snprintf(s->last_action, sizeof s->last_action, "%s", action);

    if (strcmp(action, "app.quit") == 0) {
        s->running = false;
        return;
    }

    for (int i = 0; i < s->app_count; i++) {
        if (strcmp(action, s->apps[i].label) != 0) continue;

        char msg[256] = "";
        if (!shell_open_app(s, i, msg, sizeof msg))
            snprintf(s->last_error, sizeof s->last_error, "%s", msg);
        return;
    }

    app_entry *a = active_app(s);
    if (!a) return;

    if (strcmp(action, "window.close") == 0) {
        wm_close(s->wm, a->win);
        return;
    }

    /* Alles Weitere sind Aufrufe an den Browser. Eine Skriptanwendung hat
     * keinen; was sie kann, entscheidet sie selbst über ihr event. */
    if (!a->br) return;

    char msg[256] = "";
    bool ok       = true;

    /* Return und Esc bedeuten in einer Liste etwas anderes als in einem
     * Formular - genau dafür hat die Tastenbelegung Bereiche. Hier stehen
     * beide Bedeutungen nebeneinander, und welche gilt, hat die Suche nach dem
     * Bereich schon entschieden. */
    if (strcmp(action, "record.new") == 0)          ok = browser_new(a->br, msg, sizeof msg);
    else if (strcmp(action, "record.save") == 0)    ok = browser_save(a->br, msg, sizeof msg);
    else if (strcmp(action, "form.accept") == 0)    ok = browser_save(a->br, msg, sizeof msg);
    else if (strcmp(action, "record.delete") == 0)  ok = browser_delete_selected(a->br, msg, sizeof msg);
    else if (strcmp(action, "list.open") == 0)      ok = browser_open_selected(a->br, msg, sizeof msg);
    else if (strcmp(action, "form.cancel") == 0)  { browser_cancel(a->br); return; }
    else return;

    snprintf(s->last_error, sizeof s->last_error, "%s", ok ? "" : msg);
}

/* --- Zeichnen -------------------------------------------------------------------------- */

/* Verteilt den Inhalt eines Fensters: die Übersicht bekommt alles außer dem
 * Streifen rechts, auf dem der Rollbalken sitzt. Ein Pixel Überlappung, damit
 * beide sich die Randlinie teilen - dieselbe Regel wie überall. */
static void layout_app(shell *s, app_entry *a)
{
    rect cr = window_content_rect(a->win);
    int  bw = a->bar ? s->th.scrollbar_w : 0;

    rect area = rect_make(0, 0, cr.w - bw + (bw ? 1 : 0), cr.h);
    browser_layout(a->br, area);

    if (a->bar)
        a->bar->frame = rect_make(area.w - 1, 0, bw, cr.h);
}

void shell_draw(shell *s, gc *g)
{
    for (int i = 0; i < s->app_count; i++) {
        app_entry *a = &s->apps[i];
        if (!a->win) continue;

        gc wg;
        window_gc(a->win, &wg);
        wg.pat  = PAT_WHITE;
        wg.mode = GFX_COPY;
        gfx_clear(&wg);
        wg.pat = PAT_BLACK;

        if (script_ok(s, a)) {
            rect cr = window_content_rect(a->win);

            if (s->cfg.scripts->update) s->cfg.scripts->update(s->cfg.scripts->user, a->script);
            if (s->cfg.scripts->draw)   s->cfg.scripts->draw(s->cfg.scripts->user, a->script,
                                                             &wg, cr.w, cr.h);
            continue;
        }

        layout_app(s, a);
        browser_draw(a->br, &wg);
        if (a->bar) widget_draw(a->bar, &wg);
    }

    wm_draw(s->wm, g);
    menubar_draw(s->mb, g, s->cfg.screen_w);
}

/* --- Ereignisse ------------------------------------------------------------------------- */

void shell_event(shell *s, const event *e)
{
    if (e->kind == EV_QUIT) {
        s->running = false;
        return;
    }

    /* Erst die Tastenkürzel. Gesucht wird in der Reihenfolge, in der die
     * Bereiche gelten: was gerade den Fokus hat, dann die Anwendung, dann
     * überall (data/keys/default.keys erklärt das Format).
     *
     * keymap_lookup fällt selbst auf „global" zurück, also bleiben zwei
     * Aufrufe - einer für den engeren Bereich, einer für „app". */
    if (e->kind == EV_KEY_DOWN && s->cfg.keymap) {
        app_entry  *cur   = active_app(s);
        const char *inner = "list";

        if (cur && cur->br && browser_view_of(cur->br) == BROWSE_FORM)
            inner = "form";

        const char *action = keymap_lookup(s->cfg.keymap, e->key, e->mods, inner);
        if (!action)
            action = keymap_lookup(s->cfg.keymap, e->key, e->mods, "app");

        /* Was in einem Bereich steht, aber der Schale nichts bedeutet -
         * `list.next` etwa -, geht weiter an das Widget. Es kennt seine Tasten
         * selbst, und die Schale hat dazu nichts zu sagen. */
        if (action && (shell_handles(action) || is_app_label(s, action))) {
            shell_run_action(s, action);
            return;
        }
    }

    /* Dann die Menüleiste. Sie liegt immer oben und bekommt deshalb den
     * ersten Zugriff auf Klicks. */
    const char *action = NULL;
    if (menubar_event(s->mb, e, s->cfg.screen_w, &action)) {
        shell_run_action(s, action);
        return;
    }

    /* Dann der Inhalt des aktiven Fensters. */
    app_entry *a = active_app(s);
    if (a && a->win) {
        rect  cr    = window_content_rect(a->win);
        event local = *e;

        bool positional = (e->kind == EV_MOUSE_DOWN || e->kind == EV_MOUSE_UP ||
                           e->kind == EV_MOUSE_MOVE || e->kind == EV_WHEEL);

        if (!positional || rect_contains(cr, e->x, e->y)) {
            if (positional) {
                local.x -= cr.x;
                local.y -= cr.y;
            }

            if (script_ok(s, a)) {
                if (s->cfg.scripts->event &&
                    s->cfg.scripts->event(s->cfg.scripts->user, a->script, &local))
                    return;

                /* Was das Skript nicht will, geht an die Fensterverwaltung -
                 * verschieben und schließen muss immer gehen, auch wenn ein
                 * Skript sich sonst um nichts kümmert. */
                wm_event(s->wm, e);
                return;
            }

            if (!a->br) { wm_event(s->wm, e); return; }

            layout_app(s, a);

            /* Der Rollbalken zuerst: er liegt neben der Übersicht, und ein
             * Klick auf ihn ist keiner in sie. */
            if (a->bar && widget_event(a->bar, &local)) return;
            if (browser_event(a->br, &local)) {
                /* Ein Doppelklick in der Liste öffnet den Datensatz. Diese
                 * Entscheidung gehört der Anwendung, nicht dem Widget -
                 * deshalb fragt die Schale nach und der Browser nicht. */
                if (browser_was_opened(a->br)) {
                    char msg[256] = "";
                    if (!browser_open_selected(a->br, msg, sizeof msg))
                        snprintf(s->last_error, sizeof s->last_error, "%s", msg);
                }
                return;
            }
        }
    }

    /* Zuletzt die Fensterverwaltung: aktivieren, verschieben, vergrößern,
     * schließen. */
    wm_event(s->wm, e);
}
