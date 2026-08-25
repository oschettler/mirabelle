/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Modale Dialoge.
 *
 * Ein Dialog ist ein gewöhnliches Fenster mit der Flagge WIN_MODAL; die
 * Fensterverwaltung lenkt dadurch alle Eingaben dorthin. Der Dialog selbst
 * kümmert sich nur um Text, Knöpfe und Tastaturbedienung.
 *
 * Mit der Tastatur vollständig bedienbar, und das ist Pflicht: Tab wechselt
 * den Knopf, Return löst den Voreinstellungsknopf aus, Esc bricht ab. Auf dem
 * Gerät gibt es womöglich gar keinen Zeiger.
 */
#ifndef PDA_UI_DIALOG_H
#define PDA_UI_DIALOG_H

#include <stdbool.h>

#include "core/i18n.h"
#include "ui/wm.h"

#define DIALOG_MAX_BUTTONS 3

/* Noch offen, solange dialog_result() das liefert. */
#define DIALOG_OPEN (-1)

typedef struct dialog dialog;

/* Öffnet einen modalen Dialog in der Mitte des Schirms.
 *
 * body_key ist der Schlüssel des Textes; args/argc füllen dessen Platzhalter.
 * button_keys nennt die Knöpfe von links nach rechts; der LETZTE ist der
 * Voreinstellungsknopf und wird umrandet, so wie in System 1.
 *
 * NULL bei Speichermangel. */
dialog *dialog_open(wm *m, const catalog *cat,
                    const char *body_key,
                    const char *const *args, int argc,
                    const char *const *button_keys, int button_count);

/* Schließt den Dialog und gibt ihn frei. */
void dialog_close(dialog *d);

/* DIALOG_OPEN, solange nichts gewählt wurde; sonst der Index des Knopfs.
 * Esc liefert 0 - der erste Knopf ist immer der abbrechende. */
int dialog_result(const dialog *d);

/* Zeichnet den Inhalt in das Fenster des Dialogs. Der Rahmen kommt von der
 * Fensterverwaltung. */
void dialog_draw(dialog *d);

/* true, wenn verbraucht. */
bool dialog_event(dialog *d, const event *e);

/* Das Fenster des Dialogs, etwa um es an wm_close zu übergeben. */
window *dialog_window(const dialog *d);

#endif /* PDA_UI_DIALOG_H */
