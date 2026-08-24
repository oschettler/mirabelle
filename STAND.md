# Arbeitsstand

Kurzer Wiedereinstieg. Der Entwurf steht in [DESIGN.md](DESIGN.md), der
Fahrplan in [ROADMAP.md](ROADMAP.md).

## Wo wir stehen

**Fertig: M1 bis M13 und M17.** 39 Testsuiten, warnungsfrei unter
`-Wall -Wextra -Wpedantic`, ebenso unter Address- und UB-Sanitizer.

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
| M13 | Lua 5.4, die API, ein zweiter Schemalader, Skriptanwendungen |
| M17 | SPARTAN-Protokoll, Netz in `plat.h`, Gemtext-Anzeige |
| — | Die Schale: Fenster, Menüs, Rollbalken, Dialoge, Skriptanwendungen |

Das Programm trägt sieben Anwendungen: **Aufgaben, Kontakte, Notizen, Termine**
aus je einer Schemadatei und **SPARTAN, Gliederung, Agenda** aus je einer
Lua-Datei. Keine davon wird im C-Code namentlich genannt.

    ./build/pda              starten, Vault unter ~/PDA
    ./build/pda --apps       auflisten, was gefunden wurde
    --vault <verzeichnis>    anderer Ort (oder PDA_VAULT)
    --lang en                andere Sprache
    --shot <datei.pbm>       ein Bild schreiben und beenden

## Was als Nächstes dran ist

Teil V ist abgeschlossen. Es folgen **M14 bis M17** (Härten und Portieren,
siehe ROADMAP.md) und die **Überarbeitung des Handbuchs**: die vierzehn
vorhandenen Kapitel sind noch im alten Ton geschrieben und müssen nach
`handbuch/STIL.md` neu gefasst werden - kein Erlebnisbericht, sondern ein
Entwurf aus einem Guss. Dabei kommen die Kapitel zu M9 bis M13 dazu.

## Offene Punkte

- **Handbuch überarbeiten**, am Ende. `handbuch/STIL.md` verlangt seit
  `0b5b716` einen Entwurf aus einem Guss statt eines Erlebnisberichts; die 14
  vorhandenen Kapitel sind noch im alten Ton geschrieben und werden dabei
  spürbar kürzer.
- **Rollbalken sind fertig.** Modell (`ui/scroll.h`), Widget, die Anbindung von
  Liste und mehrzeiligem Textfeld, und im Schreibtischfenster der Vorführung
  steht einer neben dem Notizfeld. Wer einen will, hängt ihn an
  `list_scroll()` beziehungsweise `text_widget_scroll()`.
- **Panels lassen sich noch nicht verschachteln.** `panel.h` stellt es in
  Aussicht, gebaut ist es nicht. Deshalb steht der Balken der Vorführung neben
  dem Formular und nicht darin - die Anwendung zieht ihm den Platz vom Layout
  ab und reicht ihm Ereignisse selbst. Das ist tragfähig, aber jede weitere
  Anwendung müsste es abschreiben; spätestens für M11 lohnt sich entweder ein
  Panel als Widget oder ein Bedienelement, das Inhalt und Balken zusammenfasst.
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
- Eine überlebende Mutation ist noch kein Befund. Erst nachrechnen, ob sie
  überhaupt etwas ändert. Ist sie gleichwertig, weil der geänderte Zweig nie
  erreicht wird, gehört nicht ein Test hinzu, sondern der tote Code weg.

## Bauen

```
make            übersetzen
make test       Tests
make test ACCEPT=1   abweichende Sollbilder übernehmen (erst hinsehen!)
make asan       Tests unter Address- und UB-Sanitizer
./build/pda     das Programm
./build/pda --shot bild.pbm    Bildspeicher schreiben und beenden
```

Handbuch: `cd handbuch/build && make html` (oder `pdf`, `epub`, `diagrams`).
