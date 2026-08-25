#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Die Zahl im Kommentar von plat.h muss stimmen.
#
# Der Kommentar nennt die Zahl der Plattformfunktionen, und dieselbe Zahl steht
# in DESIGN.md und im Handbuch: sie ist das Versprechen, dass eine Portierung
# überschaubar bleibt. Ein Versprechen, das niemand nachzählt, altert still -
# es standen dort schon zwölf, dreizehn, vierzehn und achtzehn, während es
# tatsächlich mehr waren.
#
# Gezählt werden Deklarationen am Zeilenanfang, die ein plat_-Symbol einführen.
# Das ist die Form, in der plat.h geschrieben ist.

set -eu
root="${1:-.}"
header="$root/src/plat/plat.h"

zahlwort() {
    case "$1" in
        12) echo "Zwölf" ;;   13) echo "Dreizehn" ;;   14) echo "Vierzehn" ;;
        15) echo "Fünfzehn" ;; 16) echo "Sechzehn" ;;  17) echo "Siebzehn" ;;
        18) echo "Achtzehn" ;; 19) echo "Neunzehn" ;;  20) echo "Zwanzig" ;;
        21) echo "Einundzwanzig" ;; 22) echo "Zweiundzwanzig" ;;
        *)  echo "" ;;
    esac
}

n=$(grep -cE '^[a-z].*[[:space:]*]plat_[a-z_]+\(' "$header")
wort=$(zahlwort "$n")

if [ -z "$wort" ]; then
    echo "FEHLER: $n Funktionen - dafür kennt dieser Test kein Zahlwort."
    echo "Trag es in tests/plat_count.sh nach."
    exit 1
fi

status=0
for f in "$header" "$root/DESIGN.md" "$root/handbuch/src/04-aufbau.adoc" \
         "$root/handbuch/src/11-api.adoc"; do
    [ -f "$f" ] || continue

    # Klein- wie großgeschrieben; im Fließtext steht "die neunzehn Funktionen".
    if ! grep -qiE "$wort" "$f"; then
        echo "FEHLER: $f nennt nicht $wort."
        status=1
    fi
done

if [ "$status" -ne 0 ]; then
    echo ""
    echo "src/plat/plat.h deklariert $n Funktionen ($wort)."
else
    echo "$wort Plattformfunktionen, überall dieselbe Zahl"
fi
exit "$status"
