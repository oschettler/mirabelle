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
│   ├── 01-einleitung.adoc
│   ├── ...
│   └── diagramme/        erzeugte SVG — nicht von Hand ändern
├── diagramme/            Diagrammquellen im Nomnoml-Format
├── build/                Makefile, Dockerfile, Metadaten, PDF-Layout
└── output/               entsteht beim Bauen
```

## Voraussetzungen

- Docker, gestartet
- nomnoml für die Diagramme: `npm install -g nomnoml`

Eine lokale Ruby- oder Asciidoctor-Installation ist nicht nötig.

## Bauen

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
