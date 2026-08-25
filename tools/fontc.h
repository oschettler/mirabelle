/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Übersetzt eine .fnt-Textdatei (siehe docs/fnt-format.md) in C-Quelltext,
 * der die Datenstrukturen aus gfx/font.h füllt.
 *
 * Die eigentliche Arbeit steckt in fontc_run(), damit sie ohne Prozessstart
 * aus Tests aufrufbar ist. main() in fontc.c ist nur noch Argumentprüfung.
 */
#ifndef PDA_TOOLS_FONTC_H
#define PDA_TOOLS_FONTC_H

#include <stddef.h>

/* Liest in_path, schreibt bei Erfolg C-Quelltext nach out_path mit den
 * Bezeichnern <symbol>_bits, <symbol>_glyphs und <symbol>.
 *
 * Rückgabe 0 bei Erfolg. Bei Fehler 1, keine Ausgabedatei, und errbuf enthält
 * eine Meldung im Format "datei:zeile: meldung" (zeile 0, wenn kein Bezug
 * zu einer bestimmten Zeile besteht, etwa wenn die Datei nicht geöffnet
 * werden kann). errbuf darf NULL sein, wenn die Meldung nicht interessiert.
 */
int fontc_run(const char *in_path, const char *out_path, const char *symbol,
              char *errbuf, size_t errbuf_size);

#endif /* PDA_TOOLS_FONTC_H */
