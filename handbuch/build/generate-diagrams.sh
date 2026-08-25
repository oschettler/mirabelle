#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Erzeugt die SVG-Diagramme aus den Nomnoml-Quellen und holt das Logo dazu.
#
# Aufruf: ./build/generate-diagrams.sh   (aus handbuch/build/ heraus)
#
# Zwei Nachbearbeitungen, beide gegen schwarze Diagramme:
#
# 1. Nomnoml schreibt fill="transparent". Der SVG-Darsteller in asciidoctor-pdf
#    kennt dieses Schlüsselwort nicht und malt dafür schwarz - Linien und Pfeile
#    stehen dann auf schwarzem Grund und sind unsichtbar. Ersetzt durch "none".
#
# 2. Zusätzlich bekommt jedes Diagramm ein weißes Rechteck als erstes Element,
#    mit absoluten Maßen aus der viewBox. Damit sieht es auch in einem
#    EPUB-Leser mit dunklem Thema richtig aus.

set -eu

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
DIAGRAMS_SRC="$PROJECT_DIR/diagramme"
DIAGRAMS_OUT="$PROJECT_DIR/src/diagramme"

mkdir -p "$DIAGRAMS_OUT"

# Das Logo liegt unter docs/ - dort gehört es hin, es ist kein Diagramm. Der
# Handbuchbau läuft aber in einem Container, der nur handbuch/ sieht, also
# bekommt es hier eine Kopie neben die Kapitel. Eine Kopie und keine zweite
# Zeichnung: geändert wird immer docs/mirabelle.svg.
cp "$PROJECT_DIR/../docs/mirabelle.svg" "$PROJECT_DIR/src/mirabelle.svg"
echo "Logo kopiert: src/mirabelle.svg"

if ! command -v nomnoml >/dev/null 2>&1; then
    echo "nomnoml fehlt. Installieren mit: npm install -g nomnoml" >&2
    exit 1
fi

count=0
for file in "$DIAGRAMS_SRC"/*.nomnoml; do
    [ -e "$file" ] || continue
    name="$(basename "$file" .nomnoml)"
    output="$DIAGRAMS_OUT/${name}.svg"

    # "transparent" durch "none" ersetzen.
    #
    # Das ist die eigentliche Ursache für schwarze Diagramme im PDF: Nomnoml
    # schreibt fill="transparent", und der SVG-Darsteller in asciidoctor-pdf
    # kennt dieses Schlüsselwort nicht - er malt dann schwarz. "none" ist der
    # standardisierte Weg, gar nicht zu füllen, und wird überall verstanden.
    nomnoml "$file" | sed 's/"transparent"/"none"/g' > "$output.tmp"

    # Weißes Rechteck direkt hinter das öffnende <svg ...> setzen.
    #
    # Mit ABSOLUTEN Maßen aus der viewBox, nicht mit Prozentangaben: der
    # SVG-Darsteller in asciidoctor-pdf zeichnet ein rect mit width="100%"
    # stillschweigend nicht, und dann steht das Diagramm wieder auf schwarzem
    # Grund.
    awk '
        !done && /<svg[^>]*>/ {
            print
            box = $0
            sub(/.*viewBox="/, "", box)
            sub(/".*/, "", box)
            n = split(box, v, / +/)
            if (n == 4)
                printf "\t<rect x=\"%s\" y=\"%s\" width=\"%s\" height=\"%s\" fill=\"#ffffff\"/>\n", v[1], v[2], v[3], v[4]
            done = 1
            next
        }
        { print }
    ' "$output.tmp" > "$output"
    rm -f "$output.tmp"

    echo "  $name.svg"
    count=$((count + 1))
done

echo "$count Diagramme erzeugt"
