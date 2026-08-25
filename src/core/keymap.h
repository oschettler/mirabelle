/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Tastenbelegung aus einer Datendatei.
 *
 * Kürzel stehen nicht im Quelltext, sondern in data/keys/default.keys. Das hat
 * zwei Gründe. Erstens ist die Belegung damit austauschbar, ohne zu übersetzen.
 * Zweitens - und das ist der wichtigere - erzeugen die Menüs ihre angezeigten
 * Kürzel aus derselben Quelle wie die Tastaturauswertung. Ein Menü kann dann
 * gar nicht behaupten, ein Befehl liege auf einer Taste, auf der er nicht liegt.
 *
 * Bereiche: ein Kürzel gilt in einem Bereich ("list", "form", "app") oder
 * überall ("global"). Gesucht wird erst im angegebenen Bereich, dann global.
 * Damit kann Return in einer Liste etwas anderes tun als in einem Formular,
 * ohne dass irgendwo eine Fallunterscheidung steht.
 */
#ifndef PDA_CORE_KEYMAP_H
#define PDA_CORE_KEYMAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct keymap keymap;

/* Liest die Datei. Bei einem Fehler NULL und eine Meldung im Format
 * "datei:zeile: meldung" in err, sofern err nicht NULL ist. */
keymap *keymap_load(const char *path, char *err, size_t err_size);
void    keymap_free(keymap *km);

int     keymap_count(const keymap *km);

/* Liefert den Namen der Aktion oder NULL. scope darf NULL sein, dann wird nur
 * global gesucht. Die Zeichenkette gehört der keymap. */
const char *keymap_lookup(const keymap *km, int key, uint8_t mods,
                          const char *scope);

/* Umgekehrter Weg für die Menüanzeige: schreibt das Kürzel einer Aktion in
 * lesbarer Form nach out, etwa "Cmd+N". false, wenn die Aktion keines hat. */
bool keymap_describe(const keymap *km, const char *action,
                     char *out, size_t out_size);

/* Zerlegt eine Kürzelangabe wie "Cmd+Shift+Z" in Taste und Modifikatoren.
 * Öffentlich, weil auch Tests und spätere Werkzeuge sie brauchen. */
bool keymap_parse_shortcut(const char *text, int *key, uint8_t *mods);

#endif /* PDA_CORE_KEYMAP_H */
