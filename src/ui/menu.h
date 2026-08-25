/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Menüleiste und Aufklappmenüs im Stil von Macintosh System 1.
 *
 * Menüs sind Daten. Ein Eintrag nennt einen Schlüssel für seinen Text und den
 * Namen einer Aktion; das angezeigte Kürzel erzeugt die Menüleiste selbst aus
 * der Tastenbelegung. Deshalb kann ein Menü gar nicht behaupten, ein Befehl
 * liege auf einer Taste, auf der er nicht liegt.
 *
 * Zwei Bedienweisen nebeneinander, und beide sollen sich richtig anfühlen:
 *
 *   Durchziehen  - drücken, ins Menü fahren, loslassen. Die System-1-Geste.
 *   Kurzer Klick - klicken, das Menü bleibt offen, dann in Ruhe hineinfahren
 *                  und den Eintrag anklicken. Das erwartet heute jeder.
 *
 * Unterschieden wird an der Stelle, nicht an der Zeit: wurde auf einem Titel
 * gedrückt und auf demselben Titel losgelassen, war der Zeiger nie im Menü,
 * also bleibt es offen. Kein Zeitgeber, keine Schwelle, nichts, was auf einem
 * langsamen Gerät anders ausginge.
 *
 * Ein zweiter Klick auf denselben Titel klappt wieder zu.
 *
 * Mit der Tastatur geht es gleichwertig: F10 hinein, Pfeiltasten, Return
 * wählt, Esc bricht ab.
 */
#ifndef PDA_UI_MENU_H
#define PDA_UI_MENU_H

#include <stdbool.h>

#include "core/i18n.h"
#include "core/keymap.h"
#include "gfx/draw.h"
#include "plat/plat.h"
#include "ui/theme.h"

typedef struct {
    const char *key;     /* Schlüssel im Katalog; NULL ergibt eine Trennlinie */
    const char *action;  /* Name der Aktion; NULL heißt: kein Kürzel anzeigen */
} menu_item;

typedef struct {
    const char      *key;    /* Schlüssel für den Titel in der Leiste */
    const menu_item *items;
    int              count;
} menu;

/* Fragt, ob ein Eintrag gerade wählbar ist. Nicht wählbare Einträge werden
 * grau gerastert gezeichnet und lassen sich nicht auslösen. NULL heißt:
 * alles ist wählbar. */
typedef bool (*menu_enabled_fn)(const char *action, void *user);

typedef struct menubar menubar;

menubar *menubar_create(const menu *menus, int count, const catalog *cat,
                        const keymap *km, const theme *th);
void     menubar_free(menubar *mb);

void menubar_set_enabled_fn(menubar *mb, menu_enabled_fn fn, void *user);

/* Höhe der Leiste. Darunter beginnt der Schreibtisch. */
int  menubar_height(const menubar *mb);
bool menubar_is_open(const menubar *mb);

/* Zeichnet Leiste und ein etwaiges offenes Menü. Muss nach allen Fenstern
 * gezeichnet werden - die Leiste liegt immer oben. */
void menubar_draw(menubar *mb, gc *g, int screen_w);

/* Verarbeitet ein Ereignis. Liefert true, wenn die Leiste es verbraucht hat.
 * Wird ein Eintrag ausgelöst, steht sein Aktionsname danach in *action; sonst
 * NULL. action darf nicht NULL sein. */
bool menubar_event(menubar *mb, const event *e, int screen_w,
                   const char **action);

/* Öffnet die Leiste mit der Tastatur, wie es F10 tut. */
void menubar_enter(menubar *mb);
void menubar_dismiss(menubar *mb);

#endif /* PDA_UI_MENU_H */
