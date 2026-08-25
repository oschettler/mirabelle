#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Kein sichtbarer Text im Quelltext.
#
# Geprüft wird eng und dafür verlässlich: an gfx_text und wm_open darf kein
# Zeichenkettenliteral als Text beziehungsweise Titel übergeben werden. Alles,
# was auf dem Schirm landet, geht durch eine dieser beiden Stellen.
#
# Ausdrücklich NICHT betroffen:
#   - Meldungen an stderr. Die liest ein Entwickler, kein Nutzer.
#   - "Cmd", "Shift" und die Tastennamen in keymap.c. Das ist die Schreibweise
#     des Dateiformats, keine Oberflächensprache.
#
# Aufrufe stehen oft über mehrere Zeilen, deshalb wird jede Datei vor der Suche
# zu einer einzigen Zeile zusammengezogen. Damit entfällt die Zeilennummer;
# dafür entgeht uns kein umbrochener Aufruf. Aufrufe von T, Tf und Tn werden
# vorher ausgeblendet - deren Schlüssel sind ja gerade der richtige Weg.
#
# Der Sinn: solange Texte verstreut im Code stehen, merkt niemand, wenn einer
# im Zeichensatz gar nicht darstellbar ist oder wenn eine Sprache ihn nicht hat.

set -eu
root="${1:-.}"
status=0

for f in $(find "$root/src" -name '*.c'); do
    # Katalogaufrufe ausblenden: T(cat, "schlüssel") ist genau der richtige Weg,
    # sein Literal darf den Wächter also nicht auslösen.
    flat=$(tr '\n' ' ' < "$f" | sed -E 's/T[fn]?\([^()]*\)/KATALOG/g')

    if printf '%s' "$flat" | grep -qE 'gfx_text[[:space:]]*\([^;]*,[[:space:]]*"'; then
        echo "FEHLER: $f übergibt gfx_text ein Zeichenkettenliteral."
        status=1
    fi

    if printf '%s' "$flat" | grep -qE 'wm_open[[:space:]]*\([^;]*,[[:space:]]*"[^"]'; then
        echo "FEHLER: $f übergibt wm_open einen festen Titel."
        status=1
    fi
done

if [ "$status" -ne 0 ]; then
    echo ""
    echo "Sichtbarer Text gehört in data/lang/de.strings und kommt über T()."
else
    echo "kein fest verdrahteter sichtbarer Text gefunden"
fi
exit "$status"
