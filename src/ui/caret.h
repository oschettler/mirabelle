/* Der Takt der Schreibmarke.
 *
 * Eine blinkende Schreibmarke sagt zweierlei: hier landet, was du tippst -
 * und das Programm lebt noch. Beides geht verloren, wenn jedes Textfeld
 * seinen eigenen Takt hätte: zwei Schreibmarken auf einem Bildschirm, die
 * gegeneinander blinken, sehen aus wie ein Fehler.
 *
 * Deshalb ist der Takt bewusst das einzige Stück verstecktes Wissen im
 * Zeichencode. Er gehört nicht zu einem Widget, sondern zum Bildschirm.
 *
 * Wer zeichnet, fragt caret_on(). Wer Bilder erzeugt, ruft caret_tick() je
 * Bild mit der aktuellen Zeit - genau einmal, im Hauptprogramm. Wer nie
 * tickt, bekommt eine stehende, sichtbare Schreibmarke: so bleiben Sollbilder
 * und Tests unabhängig von der Uhr.
 */
#ifndef PDA_UI_CARET_H
#define PDA_UI_CARET_H

#include <stdbool.h>
#include <stdint.h>

/* Halbe Periode: 500 ms sichtbar, 500 ms nicht. So blinkte System 1. */
#define CARET_BLINK_MS 500u

/* Je Bild zu rufen. Ein Sprung in der Zeit - Haltepunkt im Debugger,
 * zugeklappter Deckel - kostet keine Schleife, nur die Phase. */
void caret_tick(uint32_t now_ms);

/* Ist die Schreibmarke gerade zu sehen? */
bool caret_on(void);

/* Nach einer Eingabe fängt der Takt von vorn an, sichtbar. Sonst tippt man
 * in eine Lücke und sieht nicht, wo man ist. */
void caret_wake(void);

/* Zurück in den Anfangszustand: sichtbar, Takt noch nicht gestartet.
 * Für Tests, damit eine Prüfung die nächste nicht beeinflusst. */
void caret_reset(void);

#endif /* PDA_UI_CARET_H */
