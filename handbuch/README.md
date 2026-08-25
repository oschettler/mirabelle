# Handbuch zum PDA-Projekt

Dieses Verzeichnis enthält das Handbuch als AsciiDoc-Quelltext und die
Docker-basierte Bau-Pipeline, die daraus EPUB, PDF und HTML erzeugt.

Das Handbuch ist zugleich Lehrbuch: es richtet sich an alle ab etwa dreizehn,
die etwas Programmiererfahrung haben und wissen wollen, wie ein Fenstersystem,
eine Bitmapschrift oder ein Dateiparser wirklich funktionieren.

## Verzeichnisse

```
handbuch/
├── STIL.md               Stilblatt — verbindlich für alle, die ein Kapitel schreiben
├── src/                  Die Kapitel in AsciiDoc
│   ├── 00-master.adoc    bindet alle Kapitel ein
│   ├── 01-einleitung.adoc … 16-glossar.adoc
│   │                     Die Dateinummer ist die Kapitelnummer.
│   └── diagramme/        erzeugte SVG — nicht von Hand ändern
├── diagramme/            Diagrammquellen im Nomnoml-Format
├── build/                Makefile, Dockerfile, Metadaten, PDF-Layout
└── output/               entsteht beim Bauen
```

## Die sechzehn Kapitel

| | Kapitel | Worum es geht |
|---|---|---|
| 1 | Worum es geht | Was gebaut wird und warum so |
| 2 | Bauen und starten | `make`, `make test`, das Programm |
| 3 | Die Oberfläche bedienen | Fenster, Menüs, Tastatur |
| 4 | Wie das Programm aufgebaut ist | Schichten und die Plattformschicht |
| 5 | Ein Bild aus einzelnen Bits | Bitmap, Muster, Übertragungsmodi |
| 6 | Buchstaben von Hand | Bitmapschrift, UTF-8, Umlaute |
| 7 | Überlappende Fenster | Fensterverwaltung, Clipping, Ziehen |
| 8 | Wo die Daten liegen | Gemtext, Front Matter, Vault, Suche |
| 9 | Aus Dateien werden Anwendungen | Schemata, die Schale, Tastenbereiche |
| 10 | Lua in einer halben Stunde | Die Sprache |
| 11 | API-Referenz | C-Schnittstellen und die Lua-API |
| 12 | Ein Browser für ein kleines Netz | SPARTAN, Transport, der Browser in Lua |
| 13 | Selbst etwas dazubauen | Kürzel, Texte, Themen, eigene Anwendungen |
| 14 | Wie wir prüfen, ob es stimmt | Testläufer, Sollbilder, Mutationstests |
| 15 | Aufs eigene Gerät | Der Port auf den ESP32 — noch nicht gebaut |
| 16 | Glossar | |

## Voraussetzungen

Zwei Wege, und beide funktionieren.

**Mit Docker**, wie das `Makefile` in `build/` es tut — dann brauchst du nur
Docker und `nomnoml` für die Diagramme (`npm install -g nomnoml`).

**Ohne Docker**, wenn Asciidoctor lokal installiert ist:

```
cd handbuch
asciidoctor              -o output/mirabelle-handbuch.html src/00-master.adoc
asciidoctor-pdf -a pdf-theme=build/theme.yml \
                         -o output/mirabelle-handbuch.pdf  src/00-master.adoc
```

Der zweite Weg ist beim Schreiben bequemer: ein Durchgang dauert Sekunden.

## Bauen

Am schnellsten geht es lokal, wenn Asciidoctor installiert ist:

```bash
cd handbuch
asciidoctor -a toc=left -a toclevels=2 -a icons=font -a data-uri \
            -D output -o mirabelle-handbuch.html src/00-master.adoc
```

`-a data-uri` bettet die Diagramme als Daten-URI ein; die HTML-Datei braucht
danach keine Begleitdateien. Ohne diesen Schalter sucht der Browser die SVG
neben der HTML-Datei und findet sie nicht, weil sie unter `src/diagramme/`
liegen.

Für EPUB und PDF, und wenn kein Asciidoctor da ist, läuft alles über Docker.
Alle Befehle aus `handbuch/build/`:

```bash
cd handbuch/build

make diagrams   # SVG aus den Nomnoml-Quellen erzeugen
make html       # HTML, am schnellsten zum Nachsehen
make epub
make pdf
make all        # EPUB und PDF
make clean
```

## Ein Kapitel ändern oder schreiben

Lies zuerst `STIL.md`. Es legt Ton, Form und Länge fest, damit das Buch wie
*ein* Buch klingt und nicht wie vierzehn.

Zwei Regeln daraus sind besonders wichtig:

**Sag, was es noch nicht gibt.** Das Projekt entsteht in siebzehn Schritten;
zurzeit sind M1 bis M7 fertig. Kapitel über spätere Schritte tragen oben einen
Hinweis „Geplant". Ein Handbuch, das den Unterschied verwischt, kostet die
Leserin eine Stunde Suche nach einer Funktion, die es nicht gibt.

**Fehlschläge gehören hinein.** Wo im Projekt etwas schiefging und repariert
wurde, wird es erzählt. Das `t`, das wie ein Pluszeichen aussah. Der Zeiger auf
ein Thema, das nicht mehr existierte. Der Test, der wegen zweier Knöpfe gar
nichts prüfen konnte. Das sind die lehrreichsten Stellen, und ein Lehrbuch, das
nur die fertige Lösung zeigt, unterschlägt genau das, was man lernen will.

## Stand

| Kapitel | Inhalt | Beschreibt |
|---|---|---|
| 01 | Worum es geht | Vorhandenes |
| 02 | Bauen und starten | Vorhandenes |
| 03 | Die Oberfläche bedienen | Vorhandenes |
| 04 | Wie das Programm aufgebaut ist | Vorhandenes |
| 05 | Ein Bild aus einzelnen Bits | Vorhandenes |
| 06 | Buchstaben von Hand | Vorhandenes |
| 07 | Überlappende Fenster | Vorhandenes |
| 08 | Wo die Daten liegen | Geplant, M9 und M10 |
| 09 | Lua in einer halben Stunde | Sprache, unabhängig vom Projektstand |
| 10 | API-Referenz | Teil A vorhanden, Teil B geplant |
| 11 | Selbst etwas dazubauen | Vorhandenes |
| 12 | Wie wir prüfen, ob es stimmt | Vorhandenes |
| 13 | Aufs eigene Gerät | Geplant, M16 |
| 14 | Glossar | — |
