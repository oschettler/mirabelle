# Fahrplan — siebzehn Kapitel

Jede Stufe ist ein lauffähiger Stand, ein Kurskapitel und ein Pull Request.
Regel: **Eine Stufe ist erst fertig, wenn ihre Tests grün sind.** Nichts wird
„später getestet“.

Alle Entwurfsentscheidungen stehen in [DESIGN.md](DESIGN.md).

## Stand

Fertig: **M1 bis M13** und **M17**. Offen: M14 (aufgezeichnete Bedienabläufe),
M15 (Touch und Bildschirmtastatur) und M16 (der Port auf das Gerät).

M17 wurde vorgezogen. Er hängt an nichts aus M14 bis M16, und er beantwortet
eine Frage, die man besser früh stellt: taugt die Anwendungsschnittstelle für
etwas, das beim Entwurf nicht auf dem Tisch lag? Die Antwort steht in
`data/apps/spartan.lua` — der Browser ist ein Skript, und kein C-Code weiß von
ihm.

Dazwischen ist etwas entstanden, das im Fahrplan nicht steht: **die Shell**
(`src/app/shell.c`). Sie liest die Schemadateien, macht aus jeder eine
Anwendung, gibt ihr ein Fenster und baut die Menüleiste daraus. Sie gehört
sachlich zu M11, wurde aber erst gebraucht, als es mehr als eine Anwendung
gab.

Wo der Stand im Einzelnen steht und wie man wieder einsteigt: [STAND.md](STAND.md).

---

## Teil I — Pixel (M1 bis M3)

Noch kein Fenster, kein SDL. Alles läuft im Terminal und schreibt Bilder.

**M1 · Bitmap und Testläufer**
`gfx/bitmap.c`, `tests/test.h`, PBM lesen und schreiben — P1 für Ausschnitte,
P4 für Vollbilder.
*Lehrstoff:* Bits in Bytes, Zeilenlänge, und warum dir ein Testläufer mit 80
Zeilen genügt. Gleich hier die Größenregel aus DESIGN.md Abschnitt 13 umsetzen:
ein Vollbild mit 800 × 480 wäre als ASCII rund 750 kB, als Ausschnitt ein paar
Kilobyte. *Ergebnis:* `make test` läuft und vergleicht das erste Sollbild.

**M2 · Ein kleines QuickDraw**
Linien, Rechtecke, Ovale, Clipping, Muster mit 8 x 8 Bit, Übertragungsmodi.
*Lehrstoff:* Warum XOR das Zeichnen *und* das Löschen erledigt.
*Ergebnis:* das Schreibtischmuster als Sollbild.

**M3 · Schrift, UTF-8, Umlaute**
Textformat für Zeichensätze, Übersetzer nach C, Glyphentabelle, `utf8_next` und
`utf8_prev`, Textsatz.
*Lehrstoff:* Codepunkt ist nicht Byte — sonst halbiert die Rücktaste ein `ä`.
*Ergebnis:* „Grüße aus Köln, Fräulein Müller“ als Sollbild, plus der Test, der
über den Pflicht-Zeichenvorrat wacht.

## Teil II — Ein Fenster auf dem Schirm (M4 bis M5)

**M4 · Plattformschicht und Hauptschleife**
`plat.h`, `plat_sdl3.c`, `plat_headless.c`. Bild mit 800 × 480 anzeigen,
wahlweise ganzzahlig vergrößert (2fach ergibt 1600 × 960 auf dem Monitor).
*Lehrstoff:* Vierzehn Funktionen als einzige Naht zur Außenwelt, warum die
Headless-Variante zuerst entsteht, und warum die Vergrößerung ausschließlich in
der Anzeige passiert: die logische Auflösung ist auf beiden Zielen dieselbe (D-9),
sonst gälten deine Sollbilder nur für eine Seite.

**M5 · Maus, Tastatur, Text**
Ereignisse, Doppelklick, Modifikatoren, `SDL_EVENT_TEXT_INPUT`, Kürzel aus
`data/keys/default.keys`.
*Lehrstoff:* D-2 in der Praxis. Probier tote Tasten und AltGr aus, bevor du
weitermachst.

## Teil III — Oberfläche (M6 bis M8)

**M6 · Überlappende Fenster**
z-Liste, Trefferprüfung, nach vorn holen, aktiv und inaktiv, Ziehen mit
XOR-Umriss, Schließfeld, Größenfeld.
*Lehrstoff:* Warum vollständiges Neuzeichnen (D-5) eine ganze Fehlerklasse
streicht. *Ergebnis:* drei überlappende Fenster als Sollbild.

**M7 · Menüs, Dialoge, Tastaturbedienung**
Menüleiste, Aufklappmenüs, gerasterte deaktivierte Einträge, modale Dialoge,
Tab-Reihenfolge, Menüs per Tastatur betreten.
*Lehrstoff:* Jeder Befehl ohne Maus erreichbar — Pflicht, nicht Kür, wegen M16.

**M8 · Widgets, Layout und Themen**
Widgets mit je vier Funktionen, Stapel-Layout, Fokus im Panel, und das
Textmodell getrennt vom Textwidget.
*Lehrstoff:* Rahmenmaße und Trefferflächen gehören ins Thema, nicht in den
Zeichencode — sonst wird M15 zur Verzweigungsorgie.

Das mehrzeilige Textfeld ist die eigentliche Arbeit dieses Kapitels, und die
Lehre daraus steht am Anfang, nicht am Ende: **zerlege es, bevor du anfängst.**
Puffer, Schreibmarke, Auswahl und Widerrufen haben mit Zeichnen nichts zu tun.
Als eigenes Modul lässt sich der schwierige Teil ohne Bildschirm durchspielen —
und der schwierige Teil ist nicht das Zeichnen, sondern dass die Marke nach
jeder Änderung noch richtig steht.

## Teil IV — Daten (M9 bis M10)

**M9 · Datensätze, Gemtext, Front Matter**
Der Mini-YAML-Parser mit Zeilennummern in Fehlermeldungen, IDs, der Vault,
Rundlauf von lesen nach schreiben.
*Lehrstoff:* Einen Parser von Hand schreiben, und einen Sprachausschnitt bewusst
*fest* umreißen statt „alles irgendwie“ zu unterstützen.

**M10 · Index, Volltextsuche, Sortierung**
SQLite-Schema, FTS5, Abfragen als Datenstrukturen, DIN 5007, Diakritika-Faltung.
*Lehrstoff:* Abgeleitete Daten (D-3) — `index.db` löschen darf nichts kosten,
und ein Test erzwingt es. „Muller“ findet „Müller“.

## Teil V — Die Anwendungen (M11 bis M13)

**M11 · Der generische Browser**
Schema lesen, Liste und Formular daraus bauen, Feldtyp-Registratur.
*Ergebnis:* **Aufgaben, Kontakte und Notizen entstehen aus je einer Schemadatei,
ohne eine Zeile Programmcode.** Der Aha-Moment des Kurses.

**M12 · Kalender: wo Generik aufhört**
Monats- und Wochenansicht als eigene Ansicht in derselben Registratur,
Wochenbeginn und Formate aus dem Katalog.
*Lehrstoff:* Wann man aufhört zu verallgemeinern — und warum das kein Rückschritt
ist.

**M13 · Lua-Anbindung**
Lua 5.4 einbetten, die API aus DESIGN.md Abschnitt 11 exportieren, die Schemata
darauf umstellen, eine reine Lua-Anwendung als Beweis schreiben.
*Lehrstoff:* Eine C-Bibliothek für ein Skript öffnen, ohne die Schichten zu
verletzen — und die Gegenrichtung: einem Skript die *echten* Bedienelemente
geben statt Bausteine, mit denen es sie nachbaut (D-17).

## Teil VI — Härten und portieren (M14 bis M17)

**M14 · Aufgezeichnete Bedienabläufe**
Das `.ui`-Skriptformat, Aufnahme aus der laufenden Anwendung, Wiedergabe headless.
*Lehrstoff:* Fehlerberichte werden zu Testdateien. Ab hier wächst die Abdeckung
von selbst — und du brauchst das Werkzeug in M15.

**M15 · Touch, Touch-Thema, Bildschirmtastatur**
`is_touch`, keine Hover-Abhängigkeiten, `hit_slop`, `data/themes/touch.lua`,
`system16.fnt`, `data/keys/osk_de.lua`.
*Lehrstoff:* Das alles entsteht **auf dem Arbeitsplatz**, lange bevor die
Platine gebraucht wird: die Skripte aus M14 laufen ein zweites Mal mit gesetztem
`is_touch`, und was nur mit Hover funktioniert, fällt dabei auf.
*Ergebnis:* die deutsche Bildschirmtastatur als Sollbild, Umlaute inklusive.

**M16 · ESP32-8048S070C**
`plat_esp32.c`, GT911 über I2C, `esp_lcd_rgb_panel` mit Bounce-Puffern, der
Bildpfad aus DESIGN.md 12.3 (memcmp, Ausklapptabelle, PSRAM), Vault auf microSD,
SQLite auf dem Gerät. Weil die Auflösung schon seit M4 dieselbe ist, gibt es hier
keine Layout-Überraschungen — nur den Bildpfad und die Eingabe.

**Fang mit DESIGN.md 12.6 an, nicht mit einem Datenblatt.** Anschlussbelegung,
Zeitlage, PCLK, Bounce-Puffer, Flash-Modus und die vertauschte Farbreihenfolge
sind auf genau dieser Platine verifiziert und stehen dort fertig. Wer das
übergeht, verbrennt einen Tag an einem Bootloader, der nicht startet, und einen
zweiten an blauem Text, der rot sein sollte.
*Lehrstoff:* Hier geht die Rechnung für die Plattformschicht auf — oder eben
nicht, und dann weißt du warum. Miss die Bildstabilität am Anfang des Kapitels,
nicht am Ende: unser Ausklappen nach RGB565 erzeugt Last, die der Referenzport
so nicht hatte.

**M17 · SPARTAN://-Browser**
Socket in der Plattformschicht, Protokoll, Textdarstellung mit den vorhandenen
Widgets, als weitere Anwendung eingehängt. Läuft dank WLAN an Bord auch auf dem
Gerät.
*Lehrstoff:* Ein einfaches Netzprotokoll von Hand — und der Beweis, dass die
Anwendungsschnittstelle für etwas taugt, das beim Entwurf nicht auf dem Tisch lag.

---

## Warum die Reihenfolge so ist

Grafik vor Plattform, weil sich Grafik ohne Fenster testen lässt.
Fenster vor Daten, weil man Daten sonst nicht anschauen kann.
Lua nach den Anwendungen, weil du erst weißt, welche API du brauchst, wenn du sie
in C schon zweimal geschrieben hast.
Der Aufnahmeapparat vor dem Touch-Thema, weil er der einzige Weg ist, die
Touch-Bedienung ohne Gerät zu prüfen.
Das Gerät zum Schluss, weil bis dahin alles Portierungsrelevante hinter den
Funktionen von `plat.h` liegt — inzwischen neunzehn: fünfzehn für Bildschirm,
Eingabe, Zeit und Dateien, vier für das Netz. `tests/plat_count.sh` zählt nach,
damit diese Zahl nicht still veraltet.

## Was zuerst schiefgehen wird

Erfahrungsgemäß in dieser Reihenfolge: das mehrzeilige Textfeld (M8) ist doppelt
so viel Arbeit wie geschätzt; die Sollbilder (M2) brauchen anfangs täglich ein
`ACCEPT=1`, bis der Zeichencode steht; der RGB-Bus reißt unter WLAN-Last (M16)
und kostet einen Nachmittag Puffergrößen; und in M11 wird die Versuchung groß,
auch den Kalender ins Schema zu pressen. Der letzte Punkt ist der einzige, bei
dem Nachgeben teuer wird.
