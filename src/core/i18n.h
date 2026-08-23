/* Der Textkatalog.
 *
 * Kein sichtbarer Text steht im Quelltext. Das ist keine Vorratshaltung für
 * eine ferne Übersetzung, sondern hat einen näheren Grund: solange Texte im
 * Code stehen, merkt niemand, wenn einer davon im Zeichensatz gar nicht
 * darstellbar ist oder wenn die Menüanzeige und die Wirklichkeit auseinander
 * laufen. Ein Katalog lässt sich prüfen, verstreute Zeichenketten nicht.
 *
 * Deutsch ist die Voreinstellung, aber eine Sprache unter mehreren.
 * data/lang/en.strings gibt es von Anfang an - nicht weil wir es brauchen,
 * sondern damit auffällt, wenn die Übersetzbarkeit kaputtgeht.
 *
 * Format: "schlüssel = text", # leitet einen Kommentar ein. Platzhalter sind
 * nummeriert ({0}, {1}), niemals werden Zeichenketten zusammengeklebt - die
 * Wortstellung ändert sich zwischen Sprachen.
 */
#ifndef PDA_CORE_I18N_H
#define PDA_CORE_I18N_H

#include <stdbool.h>
#include <stddef.h>

typedef struct catalog catalog;

catalog *i18n_load(const char *path, char *err, size_t err_size);
void     i18n_free(catalog *c);

int  i18n_count(const catalog *c);
bool i18n_has(const catalog *c, const char *key);

/* Der Text zum Schlüssel. Fehlt er, wird der Schlüssel selbst geliefert -
 * sichtbar falsch ist besser als leer, und ein Test wacht ohnehin darüber.
 * c darf NULL sein, dann kommt immer der Schlüssel zurück. */
const char *T(const catalog *c, const char *key);

/* Wie T, setzt aber {0}, {1}, ... aus args ein. Ein Platzhalter ohne
 * passendes Argument bleibt unverändert stehen. false, wenn out zu klein ist.
 *
 * Die Argumente sind bewusst ein Feld und keine variable Argumentliste: so
 * lässt sich die Zahl der Argumente prüfen, und es gibt keine Formatzeichen,
 * die jemand aus einer Datei einschleusen könnte. */
bool Tf(const catalog *c, const char *key, char *out, size_t out_size,
        const char *const *args, int argc);

/* Plural. Sucht "<key>.one" bei n == 1, sonst "<key>.other", und setzt n für
 * {0} ein. Deutsch und Englisch brauchen zwei Formen; das Format erlaubt mehr,
 * ohne dass sich Aufrufe ändern. */
bool Tn(const catalog *c, const char *key, int n, char *out, size_t out_size);

#endif /* PDA_CORE_I18N_H */
