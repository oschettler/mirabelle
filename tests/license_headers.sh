#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Jede Quelldatei nennt ihre Lizenz.
#
# Eine Zeile "SPDX-License-Identifier" je Datei ersetzt fünfzehn Zeilen
# Rechtstext und sagt dasselbe - aber nur, solange sie überall steht. Eine neue
# Datei ohne sie fällt beim Lesen niemandem auf; hier fällt sie auf.
#
# Geprüft werden die Dateien, die Programmcode enthalten. Datendateien unter
# data/ bleiben außen vor: sie werden von Parsern gelesen, die von einer
# zusätzlichen Zeile nichts wissen, und die Lizenz des Repositorys deckt sie.

set -eu
root="${1:-.}"
status=0
missing=0

for f in $(find "$root/src" "$root/tests" "$root/tools" \
                -name '*.c' -o -name '*.h' -o -name '*.py' -o -name '*.sh')
do
    if ! head -5 "$f" | grep -q "SPDX-License-Identifier: GPL-3.0-or-later"; then
        echo "FEHLER: $f nennt keine Lizenz."
        missing=$((missing + 1))
        status=1
    fi
done

for f in "$root/CMakeLists.txt" "$root/Makefile" "$root"/data/apps/*.lua \
         "$root"/data/schema/*.lua
do
    [ -f "$f" ] || continue
    if ! head -5 "$f" | grep -q "SPDX-License-Identifier: GPL-3.0-or-later"; then
        echo "FEHLER: $f nennt keine Lizenz."
        missing=$((missing + 1))
        status=1
    fi
done

if [ "$status" -ne 0 ]; then
    echo ""
    echo "$missing Datei(en) ohne Lizenzzeile. Die erste Zeile lautet, je nach"
    echo "Sprache: '/* SPDX-License-Identifier: GPL-3.0-or-later */',"
    echo "'-- SPDX-License-Identifier: GPL-3.0-or-later' oder"
    echo "'# SPDX-License-Identifier: GPL-3.0-or-later'."
else
    echo "jede Quelldatei nennt ihre Lizenz"
fi
exit "$status"
