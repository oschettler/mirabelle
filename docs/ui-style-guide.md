# UI Style Guide

Für ein authentisches Retro-Programm in SDL3, das dem originalen Macintosh-Design (System 1 bis System 6) folgen soll, müssen Sie die exakten 1-Bit-Pixelmaße einhalten. Damals gab es keine Vektoren oder Antialiasing – jedes Element wurde auf den Pixel genau gerastert. [1, 2, 3] 
Die genauen Maße, Abstände und Verhaltensweisen basieren auf den historischen Spezifikationen der Inside Macintosh Bände und den frühen Human Interface Guidelines:
## 1. Die System-Schriftarten (Susan Kare Design)
Das gesamte Interface nutzt proportionale Bitmap-Schriften. Für ein pixelgenaues Rendering sollten Sie freie TrueType-Klone wie „Chicago“ oder „Geneva“ nutzen, diese aber strikt ungeglättet (ohne Antialiasing) in der exakten Zielgröße rendern: [4, 5, 6] 

* Chicago 12pt (System-Schrift): Genutzt für Menüleisten, Fenstertitel, Buttons und Dialogüberschriften. Die Versalhöhe (Cap Height) beträgt exakt 9 Pixel. Die Zeilenhöhe (Baseline-to-Baseline) in Menüs beträgt 19 Pixel.
* Geneva 9pt / 12pt (Anwendungsschrift): Genutzt für Dateinamen unter Icons, Listenansichten und Fließtext. [1, 4, 7, 8] 

------------------------------
## 2. Die Menüleiste (Global Menu Bar)
Die Menüleiste sitzt fest am oberen Bildschirmrand. Sie besitzt eine Gesamthöhe von 20 Pixeln: [9] 

* Hintergrund: Rein weiß (RGB 255, 255, 255).
* Untere Trennlinie: 1 Pixel stark, tiefschwarz (RGB 0, 0, 0).
* Text-Positionierung: Der Text (Chicago 12pt) sitzt auf einer Baseline, die exakt 15 Pixel vom oberen Bildschirmrand entfernt ist.
* Abstände:
* Linkes Padding (vor dem Apple-Logo): 16 Pixel.
   * Horizontaler Abstand zwischen den Menütiteln: 16 Pixel.
* Dropdown-Menüs: Öffnen sich direkt unterhalb der 20-Pixel-Marke. Sie besitzen einen 1-Pixel-schwarzen Rahmen sowie einen 2-Pixel-breiten Schlagschatten (unten und rechts).
* Dropdown-Einträge: Jeder Eintrag hat eine feste Höhe von 16 bis 18 Pixeln (je nach Systemversion), wobei Text links 16 Pixel Padding für optionale Checkmarks (Häkchen) freihält.

------------------------------
## 3. Fenster-Architektur (Standard Document Window)
Fenster bestehen aus vordefinierten funktionalen Komponenten:

| Komponente | Pixel-Maß | Beschreibung / Stil |
|---|---|---|
| Titelzeile (Title Bar) | 20 px Höhe | Beinhaltet den Fenstertitel (Chicago 12pt, zentriert). |
| Titelzeilen-Textur | 6 Linien | Wenn das Fenster aktiv ist, wird der Titel von je 6 horizontalen, schwarzen 1-Pixel-Linien flankiert (mit 1 Pixel Abstand zueinander). Die Linien enden 3 px vor dem Rahmen, nicht an ihm. |
| Rahmen (Border) | 1 px | Umlaufender, tiefschwarzer Rahmen um das gesamte Fenster. |
| Schließfeld (Go-Away Box) | 11 × 11 px | Sitzt oben links in der Titelzeile. Abstand: 5 px von oben, 7 px von links. Ober- und Unterkante liegen genau auf der ersten und der letzten der sechs Linien — sechs Linien im Abstand von 2 px überspannen 11 Pixel, ein 12 px hohes Feld endete eine Zeile darunter. |
| Scrollbars (Bildlaufleisten) | 16 px Breite | Sitzen rechts/unten. Nutzen das klassische graue 50%-Schachbrettmuster (0xAA/0x55) als Hintergrund. |
| Scroll-Pfeile | 16 × 16 px | Quadratische Quadrate an den Enden der Scrollbars mit zentrierten Pfeil-Bitmaps. |
| Größenfeld (Size Box / Grow Window) | 16 × 16 px | Ganz unten rechts in der Ecke der Scrollbar-Kreuzung. Darin zwei ineinandergeschobene Quadrate: das kleinere hinten links oben, das größere davor rechts unten. |

------------------------------
## 4. UI-Elemente: Buttons, Checkboxen & Paddings

* Push-Buttons (Standard-Schaltflächen):
* Form: Abgerundete Rechtecke (Corner Radius: 4 Pixel).
   * Rahmen: 1 Pixel schwarz.
   * Standard-Button (Default): Besitzt einen zusätzlichen, 3 Pixel starken schwarzen Außenrahmen mit einem 1-Pixel-Weißraum-Abstand zum inneren Rahmen.
   * Innentext-Padding: Mindestens 6 Pixel links und rechts vom Text zum Rahmen.
* Checkboxen & Radio-Buttons:
* Checkbox: Quadrat von 12 × 12 Pixeln mit einem 1-Pixel-Rahmen. Das Kreuz darin wird aus zwei diagonalen 1-Pixel-Linien gebildet.
   * Radio-Button: Kreis mit 12 Pixel Durchmesser. Der aktive Zustand ist ein ausgefüllter innerer Kreis (6 Pixel Durchmesser).
   * Textabstand: Exakt 6 Pixel Leerraum zwischen der Box/dem Button und dem dazugehörigen Label-Text.

------------------------------
## 5. Dialogboxen & Alerts

* Rahmen: Typische modale Dialoge besitzen einen dicken 2-Pixel-Außenrahmen, gefolgt von 2 Pixeln Weißraum und einem inneren 1-Pixel-Rahmen.
* Inhalts-Paddings:
* Abstand vom Fensterrahmen zu Texten/Icons: 24 Pixel.
   * Abstand zwischen einem Warn-Icon (32 × 32 px) und dem Hinweistext: 16 Pixel.
   * Abstand von den Buttons zum unteren/rechten Rand der Dialogbox: 16 Pixel.

## SDL3-Tipp für die Umsetzung
Da SDL3 standardmäßig mit Texturen und modernen Renderern arbeitet, stellen Sie beim Erstellen Ihrer Texturen für Schriften und Linien das Skalierungs-Filter-Verhalten unbedingt auf Nearest Neighbor um, da das Interface sonst unscharf wird:

SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);

Zudem empfiehlt es sich, das Programm intern in einer festen Auflösung zu berechnen – wie dem historischen Mac-Standard 512 × 342 oder dem späteren kompakten Standard 640 × 400 – und dieses Target mittels SDL_SetRenderLogicalPresentation in SDL3 ganzzahlig (Integer Scaling) auf die native Bildschirmgröße des PDA-Targets hochzuskalieren. [2, 10] 
Möchten Sie als Nächstes die exakten Hex-Codes für die 8 klassischen System-Muster (Dithering), um Hintergründe und Scrollbars originalgetreu zu zeichnen, oder Auszüge aus dem C-Code für die UI-Berechnung?

[1] [https://www.mindstudio.ai](https://www.mindstudio.ai/blog/susan-kare-original-mac-icon-design)
[2] [https://en.wikipedia.org](https://en.wikipedia.org/wiki/Macintosh_128K)
[3] [https://guidebookgallery.org](https://guidebookgallery.org/articles/onethousandsquarepixelsofcanvas)
[4] [https://finance.biggo.com](https://finance.biggo.com/news/94324779b6032d5b)
[5] [https://nowintime.itch.io](https://nowintime.itch.io/apple-system-1-font)
[6] [https://www.behance.net](https://www.behance.net/gallery/81132513/Macintosh-bitmap-fonts)
[7] [https://www.instagram.com](https://www.instagram.com/p/DcERF9aAOrL/)
[8] [https://www.facebook.com](https://www.facebook.com/RealLogoDecks/posts/turning-pixels-into-meaning-the-vision-of-susan-karesusan-kare-is-the-visionary-/1004724845211178/)
[9] [https://www.howtogeek.com](https://www.howtogeek.com/732546/macintosh-system-1-what-was-apples-mac-os-1.0-like/)
[10] [https://68kmla.org](https://68kmla.org/bb/threads/macintosh-personal-color-a-fantasy-1986-68000-mac-with-a-4bpp-color-mode-emulator.49444/page-2)
