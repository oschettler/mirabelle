/* UTF-8.
 *
 * Zeichenketten sind im ganzen System UTF-8. Niemals wird ein Byte als Zeichen
 * behandelt: der Schreibcursor bewegt sich über Codepunkte, sonst zerlegt die
 * Rücktaste ein ä in ein halbes Zeichen.
 *
 * Ungültige Folgen liefern U+FFFD und rücken um genau ein Byte vor. Damit
 * terminiert jede Schleife über eine beliebige Bytefolge, und kaputte Eingabe
 * wird sichtbar statt stillschweigend verschluckt.
 */
#ifndef PDA_CORE_UTF8_H
#define PDA_CORE_UTF8_H

#include <stddef.h>
#include <stdint.h>

#define UTF8_REPLACEMENT 0xFFFDu

/* Liefert den Codepunkt an *p und rückt *p hinter ihn. Am Ende der Zeichenkette
 * wird 0 geliefert und *p nicht weitergerückt. */
uint32_t utf8_next(const char **p);

/* Rückt *p auf den Anfang des vorhergehenden Codepunkts zurück und liefert ihn.
 * Steht *p schon auf start, wird 0 geliefert und nichts verändert. */
uint32_t utf8_prev(const char *start, const char **p);

/* Anzahl Codepunkte, nicht Bytes. */
size_t utf8_count(const char *s);

/* Schreibt bis zu vier Bytes, liefert die Anzahl. 0 bei ungültigem Codepunkt. */
int utf8_encode(uint32_t codepoint, char out[4]);

/* Gültig heißt: kein überlanges Kodieren, keine Surrogate (U+D800..U+DFFF),
 * nicht über U+10FFFF. */
int utf8_valid(const char *s);

#endif /* PDA_CORE_UTF8_H */
