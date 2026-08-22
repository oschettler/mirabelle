# Das .fnt-Format

Ein Zeichensatz ist eine Textdatei. Sie ist im Editor bearbeitbar, `git diff`
zeigt Änderungen an einer Glyphe als das, was sie sind, und man braucht kein
Werkzeug, um sie zu lesen.

## Aufbau

```
name    Systemschrift
size    12
ascent  9

glyph U+0041 width 8        # A
  ...##...
  ...##...
  ..#..#..
  ..#..#..
  .#....#.
  .######.
  .#....#.
  #......#
  #......#
  ........
  ........
  ........
```

* `name` ist beschreibend und landet im erzeugten C-Code.
* `size` ist die Höhe **jeder** Glyphe in Pixeln. Alle Glyphen einer Schrift
  sind gleich hoch; nur die Breite wechselt.
* `ascent` ist die Anzahl Zeilen oberhalb der Grundlinie. Die Grundlinie liegt
  also zwischen Zeile `ascent - 1` und `ascent`.
* `glyph U+XXXX width N` beginnt eine Glyphe, danach folgen genau `size` Zeilen
  mit je genau `N` Zeichen: `#` für gesetzt, `.` für leer.
* Alles ab `#` **außerhalb** einer Pixelzeile ist Kommentar. In Pixelzeilen ist
  `#` ein Pixel, dort gibt es keine Kommentare.
* Leerzeilen und führende Leerzeichen sind bedeutungslos.

## Breite ist Vorschub

`width` ist zugleich die Anzahl gezeichneter Spalten **und** der Vorschub zum
nächsten Zeichen. Es gibt keinen getrennten Abstandswert: der Abstand steht als
leere Spalte rechts in der Glyphe selbst. Das ist eine Vereinfachung gegenüber
echten Bitmap-Schriften, spart aber einen ganzen Satz Sonderfälle und reicht für
eine Oberfläche vollkommen aus.

## Pflichten

Jede Schrift muss `U+FFFD` enthalten. Fehlt ein angefordertes Zeichen, wird
diese Ersatzglyphe gezeichnet — sichtbar, nie stillschweigend nichts.

Welche Codepunkte darüber hinaus vorhanden sein müssen, steht in
`data/fonts/required.set`. Ein Test prüft das für jede Schrift.

## Warum vor dem Bau übersetzt wird

`tools/fontc` erzeugt aus einer `.fnt`-Datei C-Quelltext, der mitübersetzt wird.

Der naheliegende Gegenentwurf wäre, `.fnt` zur Laufzeit zu lesen. Dagegen
sprechen drei Dinge. Erstens müsste `gfx/` dafür Dateien lesen können und damit
die Plattformschicht kennen — das verletzt die Schichtenregel aus DESIGN.md
Abschnitt 2. Zweitens landet erzeugter C-Code auf dem ESP32 im Flash statt im
knappen RAM. Drittens ist es Verschwendung, beim Start eines Geräts einen
Textparser über hundert Glyphen laufen zu lassen.

Der erzeugte Code ist lesbar und darf angesehen werden; er wird aber nie von
Hand geändert, sondern immer neu erzeugt.
