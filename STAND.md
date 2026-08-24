# Arbeitsstand

Kurzer Wiedereinstieg. Der Entwurf steht in [DESIGN.md](DESIGN.md), der
Fahrplan in [ROADMAP.md](ROADMAP.md).

## Wo wir stehen

**Fertig: M1 bis M9.** 387 Tests in 25 Suiten, warnungsfrei unter
`-Wall -Wextra -Wpedantic`, ebenso unter Address- und UB-Sanitizer.

| Meilenstein | Inhalt |
|---|---|
| M1 | Bitmap, PBM, Testläufer |
| M2 | Zeichenprimitive, Muster, Übertragungsmodi |
| M3 | Bitmapschrift (127 Glyphen), UTF-8, `fontc` |
| M4 | Plattformschicht (14 Funktionen), SDL3, headless |
| M5 | Maus, Tastatur, Tastenbelegung aus Datei |
| M6 | Überlappende Fenster, Themen |
| M7 | Textkatalog, Menüs, modale Dialoge |
| M8 | Widgets, Layout, Fokus, Textmodell, Textfelder |
| M9 | Gemtext, Front Matter, Datensätze, Vault |

Dazu: Handbuch (14 Kapitel, baut zu PDF/EPUB/HTML), Maße nach
`docs/ui-style-guide.md`.

## Was als Nächstes dran ist

**M10 — Index, Volltextsuche, Sortierung.** Geplant in drei Teilen, die
einzeln testbar und committebar sind:

1. **Deutsche Sortierung und Diakritika-Faltung** — reine Textarbeit in
   `src/core/collate.c`, ohne SQLite. Sortieren nach DIN 5007 Variante 1
   (ä wie a, ß wie ss); Suchen mit beidseitiger Faltung, damit „Muller" auch
   „Müller" findet. Das sind zwei verschiedene Probleme und bekommen zwei
   verschiedene Funktionen.
2. **Abfragen als Datenstruktur** — `src/store/query.c`. Anwendungen bauen
   nie SQL-Zeichenketten; SQL entsteht an genau einer Stelle.
3. **SQLite mit FTS5** — `src/store/index_sqlite.c`. Der Index ist ableitbar:
   ihn zu löschen darf kein Byte Nutzdaten kosten, und ein Test erzwingt das.

Danach M11 (generischer Browser), M12 (Kalender), M13 (Lua).

## Offene Punkte

- **Handbuch überarbeiten**, am Ende. `handbuch/STIL.md` verlangt seit
  `0b5b716` einen Entwurf aus einem Guss statt eines Erlebnisberichts; die 14
  vorhandenen Kapitel sind noch im alten Ton geschrieben und werden dabei
  spürbar kürzer.
- **Rollbalken** fehlen noch. `scrollbar_w` steht im Thema, das Widget nicht.
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
- Beim Mutieren die Datei nach dem Zurückkopieren **`touch`en**, sonst ist der
  Bau veraltet und man hält eine Testlücke für echt, die keine ist.

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
