/* Die Schale: das Programm, das aus den Bausteinen eine Anwendung macht.
 *
 * Bis hierher gab es Fenster, Widgets, Schemata, einen Browser, einen
 * Kalender, einen Gemtext-Anzeiger und eine Skriptanbindung - jedes für sich
 * geprüft, keines mit den anderen verbunden. Diese Datei verbindet sie.
 *
 * ## Was sie tut
 *
 * Sie liest beim Start alle Schemadateien aus `data/schema` und macht aus
 * jeder eine Anwendung. Aufgaben, Kontakte, Notizen und Termine entstehen
 * damit ohne eine Zeile Code, die sie namentlich nennt (D-7) - die Schale
 * zählt Dateien, nicht Anwendungen.
 *
 * Jede Anwendung bekommt auf Wunsch ein Fenster, darin einen Browser und
 * daneben einen Rollbalken. Die Menüleiste entsteht ebenfalls aus dem, was
 * gefunden wurde.
 *
 * ## Was sie nicht tut
 *
 * Sie enthält keine Anwendungslogik. Alles, was ein Nutzer tut - suchen,
 * anlegen, ändern, löschen -, ist ein Aufruf an browser.h. Ginge hier ein
 * Sonderfall für Aufgaben hinein, wäre D-7 verloren, und niemand würde es
 * merken, bis die fünfte Anwendung dazukommt.
 */
#ifndef PDA_APP_SHELL_H
#define PDA_APP_SHELL_H

#include <stdbool.h>
#include <stddef.h>

#include "core/collate.h"
#include "core/i18n.h"
#include "core/keymap.h"
#include "gfx/draw.h"
#include "plat/plat.h"
#include "store/vault.h"
#include "ui/theme.h"

typedef struct shell shell;

typedef struct {
    const char *data_dir;    /* enthält schema/, lang/, themes/ ... */
    vault      *vault;       /* gehört dem Aufrufer */

    const theme   *theme;
    const catalog *catalog;
    const keymap  *keymap;

    const collate *sort;     /* darf NULL sein */
    const collate *search;

    int screen_w, screen_h;
} shell_config;

/* Legt die Schale an und liest die Schemata ein.
 *
 * Ein Schema, das sich nicht laden lässt, wird übersprungen und gemeldet -
 * eine kaputte Datei nimmt nicht die ganze Anwendung mit. Nur wenn gar keine
 * gefunden wird, ist das ein Fehler: dann gäbe es nichts zu tun. */
shell *shell_create(const shell_config *cfg, char *err, size_t err_size);
void   shell_destroy(shell *s);

/* Die gefundenen Anwendungen, in der Reihenfolge ihrer Dateinamen. */
int         shell_app_count(const shell *s);
const char *shell_app_label(const shell *s, int index);   /* Katalogschlüssel */

/* Öffnet das Fenster einer Anwendung, oder holt es nach vorn, wenn es schon
 * offen ist. false und eine Meldung, wenn die Sammlung nicht lesbar ist. */
bool shell_open_app(shell *s, int index, char *err, size_t err_size);

/* true, wenn das Fenster dieser Anwendung offen ist. */
bool shell_app_is_open(const shell *s, int index);

/* Die zuletzt ausgelöste Aktion - für Tests und für eine Statuszeile. */
const char *shell_last_action(const shell *s);

/* Die letzte Fehlermeldung, oder ein leerer Text. Wird beim nächsten
 * erfolgreichen Schritt gelöscht. */
const char *shell_last_error(const shell *s);

/* --- Betrieb -------------------------------------------------------------------- */

void shell_draw(shell *s, gc *g);
void shell_event(shell *s, const event *e);

/* false, sobald der Nutzer beenden will. */
bool shell_running(const shell *s);

/* Führt eine Aktion aus, so als käme sie aus dem Menü. Für Tests und für
 * alles, was Aktionen von woanders auslöst. */
void shell_run_action(shell *s, const char *action);

#endif /* PDA_APP_SHELL_H */
