# Arbeitsstand

Kurzer Wiedereinstieg. Der Entwurf steht in [DESIGN.md](DESIGN.md), der
Fahrplan in [ROADMAP.md](ROADMAP.md).

## Wo wir stehen

**Fertig: M1 bis M13 und M17.** 42 Testsuiten, warnungsfrei unter
`-Wall -Wextra -Wpedantic`, ebenso unter Address- und UB-Sanitizer. Alle
Mutationsprüfstände laufen ohne unerklärte Überlebende.

| Meilenstein | Inhalt |
|---|---|
| M1 | Bitmap, PBM, Testläufer |
| M2 | Zeichenprimitive, Muster, Übertragungsmodi |
| M3 | Bitmapschrift, UTF-8, `fontc` |
| M4 | Plattformschicht, SDL3, headless |
| M5 | Maus, Tastatur, Tastenbelegung aus Datei |
| M6 | Überlappende Fenster, Themen |
| M7 | Textkatalog, Menüs, modale Dialoge |
| M8 | Widgets, Layout, Fokus, Textmodell, Textfelder |
| M9 | Gemtext, Front Matter, Datensätze, Vault |
| M10 | Sortierung nach DIN 5007, Abfragen, SQLite mit FTS5 |
| M11 | Schemata, Feldtyp-Registratur, generischer Browser |
| M12 | Datumsrechnung, Monatsansicht, Ansichtsregistratur |
| M13 | Lua 5.4, die API, der Schemalader, Skriptanwendungen |
| M17 | SPARTAN-Protokoll, Netz in `plat.h`, Gemtext-Anzeige |
| — | Die Schale: Fenster, Menüs, Rollbalken, Dialoge, Skriptanwendungen |

Das Programm trägt sieben Anwendungen: **Aufgaben, Kontakte, Notizen, Termine**
aus je einer Schemadatei und **Spartan, Gliederung, Agenda** aus je einer
Lua-Datei. Keine davon wird im C-Code namentlich genannt.

    ./build/mirabelle              starten, Vault unter ~/PDA
    ./build/mirabelle --apps       auflisten, was gefunden wurde
    ./build/mirabelle --version    Fassung und Lizenz nennen und beenden
    --vault <verzeichnis>    anderer Ort (oder PDA_VAULT)
    --lang en                andere Sprache
    --shot <datei.pbm>       ein Bild schreiben und beenden

## Was als Nächstes dran ist

Es folgen **M14 bis M16** (aufgezeichnete Bedienung, Touch, die Portierung auf
den ESP32-S3; siehe ROADMAP.md). M17 ist vorgezogen und fertig.

## Offene Punkte

- **Handbuch weiter überarbeiten.** Die siebzehn Kapitel sind inhaltlich auf
  Stand, aber nicht alle im Ton von `handbuch/STIL.md`: kein Erlebnisbericht,
  sondern ein Entwurf aus einem Guss. Wer ein Kapitel anfasst, zieht es dabei
  mit.
- **Ein Font-Editor als Lua-Anwendung** ist vorgesehen und braucht eine
  Entscheidung: Lua kommt über `store.*` nur an den Vault, es gibt kein `io`
  und kein `os`. Zum Bearbeiten von `data/fonts/*.part` bräuchte es entweder
  einen ausdrücklich erlaubten Pfad für Bestandsdaten oder Zeichensätze als
  Datensätze im Vault.
- `radio`, `popup_menu`, `date_field` kommen, wenn eine Anwendung sie braucht.

## Arbeitsweise, die sich bewährt hat

- **Klein schneiden und einzeln committen.** Ein Meilenstein zerfällt in drei
  Teile, jeder für sich lauffähig und geprüft.
- **Mutationstests.** Nach jedem Baustein absichtlich einen Fehler einbauen und
  nachsehen, ob ein Test anschlägt. Etwa ein Drittel der gefundenen Lücken kam
  so ans Licht.
- **`make asan`.** Speicherfehler fallen im gewöhnlichen Bau nicht auf.
- **Sollbilder ansehen, bevor man sie übernimmt.** `make test ACCEPT=1` blind
  auszuführen heißt, keinen Test mehr zu haben.
- Beim Mutieren **nicht über das Bausystem übersetzen**, sondern die betroffene
  Datei direkt mit `cc` zusammen mit ihrem Test. Ein inkrementeller Bau
  entscheidet an Zeitstempeln; liegen Quelle, Objektdatei und Programm in
  derselben Sekunde, läuft der alte Stand weiter und jede Mutation sieht aus,
  als hätte sie überlebt. Ein `touch` genügt dagegen nicht - es hilft der
  Objektdatei, nicht dem Binden. Woran man es merkt: **zwei Läufe liefern
  verschiedene Überlebende.** Dann ist der Prüfstand falsch, nicht der Test.
- **Ein Thema wird kopiert, nie als Zeiger gehalten.** Diese Falle hat vier
  Mal zugeschlagen: `wm_create`, `menubar_create`, die Panel-Widgets, und
  zuletzt der Rollbalken der Vorführung, der in kein Panel wandert und den
  Zeiger deshalb behält. Sichtbar wurde es an einer Balkenbreite von null,
  nicht an einem Absturz - der Zeiger zeigte auf einen aufgegebenen Stapel,
  der zufällig noch lesbar war. Wer ein Widget außerhalb eines Panels hält,
  hält auch eine Kopie des Themas.
- **Was das Programm schon hat, baut ein Skript nicht nach.** Rollbalken,
  Gemtext-Anzeige und Schreibmarke waren in `data/apps/spartan.lua` einmal
  nachgebaut. Jeder Nachbau sah dem Original ähnlich, bis jemand hinsah - dem
  Balken fehlte der untere Pfeil. Wenn ein Skript etwas nachbaut, das es im
  Programm gibt, ist das ein Loch in der Schnittstelle, kein Fleiß (D-17).
- **Zahlen in Prosa veralten still.** Die Zahl der Plattformfunktionen stand an
  vier Stellen und war an dreien falsch. `tests/plat_count.sh` zählt sie jetzt
  nach. Wo eine Angabe im Text von etwas Zählbarem abhängt, ist ein Test
  billiger als Aufmerksamkeit.
- Eine überlebende Mutation ist noch kein Befund. Erst nachrechnen, ob sie
  überhaupt etwas ändert. Ist sie gleichwertig, weil der geänderte Zweig nie
  erreicht wird, gehört nicht ein Test hinzu, sondern der tote Code weg.
- **Wer eine Mutation überleben lässt, schreibt dazu, warum.** In den
  Prüfständen steht die Begründung oben im Kopf, damit ein Lauf sich selbst
  erklärt. Eine Liste von Überlebenden ohne Erklärung sieht nach einer Lücke
  aus, und beim nächsten Mal glaubt sie jemand.

  Zurzeit überleben vier, alle gleichwertig: eine Wache gegen eine negative
  Zellenhöhe (`monthview`), zwei Wachen im Bildlaufmodell (`scroll`), und die
  Schemaprüfung in `spartan`, die dasselbe abweist wie die Portprüfung eine
  Zeile später - nur mit der besseren Meldung.

## Bauen

Gebraucht werden ein C11-Compiler, CMake 3.18, SDL 3 und **Lua 5.4**; SQLite
ist wahlfrei. Lua ist Pflicht, weil die Schemadateien Lua-Tabellen sind (D-16)
- ohne sie gäbe es keine Anwendung.

Das System heißt **mirabelle**. Das Programm heißt `mirabelle`, das Zeichen in
der Menüleiste ist eine Mirabelle (`data/fonts/system12-logo.part`, U+E000),
und das Logo liegt als `docs/mirabelle.svg`.

Nicht umbenannt sind der Vault unter `~/PDA`, die Variable `PDA_VAULT` und die
Namen im Quelltext (`pda_ui`, `PDA_DATA_DIR`). Die ersten beiden zeigen auf ein
Verzeichnis mit fremden Notizen darin; sie umzubenennen ließe es verwaist
zurück. Die dritten sind Bezeichner, kein Produktname.

Das Projekt steht unter der **GPL, Version 3 oder neuer** (`LICENSE`). Jede
Quelldatei trägt eine Zeile `SPDX-License-Identifier: GPL-3.0-or-later`; wer
eine neue anlegt, setzt sie mit dazu - `tests/license_headers.sh` zählt nach.

```
make            übersetzen
make test       Tests
make test ACCEPT=1   abweichende Sollbilder übernehmen (erst hinsehen!)
make asan       Tests unter Address- und UB-Sanitizer
./build/mirabelle     das Programm
./build/mirabelle --shot bild.pbm    Bildspeicher schreiben und beenden
```

Handbuch: `cd handbuch/build && make html` (oder `pdf`, `epub`, `diagrams`).
