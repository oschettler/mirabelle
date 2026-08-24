/* Sortieren und Suchen mit Umlauten.
 *
 * Zwei verschiedene Probleme, und es lohnt sich, sie auseinanderzuhalten:
 *
 *   Sortieren  bringt eine Liste in eine Reihenfolge, die ein Mensch erwartet.
 *              Nach DIN 5007 Variante 1 zählt „ä" wie „a" und „ß" wie „ss";
 *              „Müller" steht damit zwischen „Mulde" und „Multi".
 *   Suchen     soll großzügig sein: wer „Muller" tippt, will „Müller" finden,
 *              und wer „Straße" tippt, auch „Strasse".
 *
 * Beides läuft über dieselbe Mechanik - eine Tabelle, die einem Zeichen einen
 * Ersatztext zuordnet - aber über zwei verschiedene Tabellen. Das ist kein
 * Zufall: für Schwedisch gehört „ä" beim Sortieren hinter „z", beim Suchen
 * aber weiterhin mit „a" zusammen. Eine gemeinsame Tabelle müsste sich für
 * eines von beidem entscheiden.
 *
 * Deshalb lädt die Anwendung zwei Tabellen:
 *
 *     collate *sort   = collate_load("data/lang/de.sort", ...);
 *     collate *search = collate_load("data/collate/search.fold", ...);
 *
 * Die Sortiertabelle hängt an der Sprache und liegt bei ihr. Die Suchtabelle
 * faltet nur Diakritika und gilt für alle Sprachen - dieselbe Regel, die
 * SQLite mit `remove_diacritics 2` anwendet.
 *
 * Im Code steht damit nirgends ein „wenn Deutsch". Eine weitere Sprache ist
 * eine Datei.
 */
#ifndef PDA_CORE_COLLATE_H
#define PDA_CORE_COLLATE_H

#include <stdbool.h>
#include <stddef.h>

typedef struct collate collate;

/* Format wie bei den anderen Datendateien: eine Zeile je Zeichen, # leitet
 * einen Kommentar ein.
 *
 *     # data/lang/de.sort
 *     ä  a
 *     ß  ss
 *
 * Links genau ein Zeichen, rechts sein Ersatz beim Sortieren - der darf
 * mehrere Zeichen lang sein, wie bei „ß". Beides UTF-8.
 *
 * Bei einem Fehler NULL und eine Meldung "datei:zeile: text" in err. */
collate *collate_load(const char *path, char *err, size_t err_size);
void     collate_free(collate *c);

/* Was ohne Tabelleneintrag passiert:
 *
 * Großbuchstaben A bis Z werden klein. Das ist die einzige Regel, die im Code
 * steht und nicht in der Tabelle, und sie gilt nur, solange die Tabelle nichts
 * anderes sagt - eine türkische Tabelle würde „I" auf „ı" abbilden und damit
 * gewinnen. Alles Übrige bleibt, wie es ist.
 */

/* Schreibt die gefaltete Fassung von utf8 nach out und liefert ihre Länge ohne
 * das Nullbyte. Passt sie nicht, wird nichts geschrieben und (size_t)-1
 * geliefert.
 *
 * Gebraucht, wo eine gefaltete Fassung gespeichert oder weitergereicht wird -
 * etwa als Sortierschlüssel in einem Index. Zum bloßen Vergleichen ist sie
 * nicht nötig: collate_compare kommt ohne Puffer aus. */
size_t collate_fold(const collate *c, const char *utf8, char *out, size_t out_size);

/* Vergleicht wie strcmp: kleiner null, null, größer null.
 *
 * Verglichen wird die gefaltete Fassung. Sind zwei Texte danach gleich - etwa
 * „Muller" und „Müller" -, entscheidet der ungefaltete Text. Ohne diesen
 * zweiten Schritt hinge ihre Reihenfolge davon ab, in welcher sie ankamen, und
 * eine Liste sähe nach jedem Sortieren anders aus. */
int collate_compare(const collate *c, const char *a, const char *b);

/* true, wenn needle in haystack vorkommt - beide Seiten gefaltet.
 *
 * Beidseitig zu falten ist der ganze Trick: „Muller" findet „Müller", und
 * „Müller" findet „Muller". Nur eine Seite zu falten fände jeweils nur eine
 * Richtung. Eine leere Nadel kommt überall vor. */
bool collate_contains(const collate *c, const char *haystack, const char *needle);

/* Wie collate_contains, aber der Text muss am Anfang stehen. Für Register und
 * Vervollständigung. */
bool collate_starts_with(const collate *c, const char *text, const char *prefix);

#endif /* PDA_CORE_COLLATE_H */
