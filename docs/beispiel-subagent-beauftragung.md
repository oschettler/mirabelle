(( Bitte ignorieren. Das ist nur ein Beispiel für eine Subagent-Beauftragung, die ich mir merken wollte. ))

Arbeitsverzeichnis: /Users/olav.schettlerdw.com/Documents/privat/pda

Setze das Textmodell um (Meilenstein M8). Das ist der schwierigste Teil des ganzen Kapitels — nimm dir Zeit für die Randfälle, aber baue nichts Kluges.

LIES ZUERST, aber knapp:
- src/ui/textbuf.h         (die Schnittstelle, vollständig kommentiert — verbindlich)
- src/core/utf8.h und utf8.c   (utf8_next, utf8_prev — jede Bewegung geht darüber)
- tests/test.h
- tests/unit/test_utf8.c   (Vorbild für Aufbau und Stil)

ERZEUGE GENAU ZWEI DATEIEN:
1. src/ui/textbuf.c
2. tests/unit/test_textbuf.c

Ändere KEINE andere Datei.

## Grundsätze

**Alle Positionen sind Byte-Versätze**, niemals Zeichenzahlen. Jede Bewegung um
ein Zeichen geht über `utf8_next` beziehungsweise `utf8_prev`. Die Schreibmarke
darf nie mitten in einem Mehrbytezeichen stehen.

**Einfach, nicht clever.** Der Puffer ist ein einzelnes `char`-Feld, das mit
`realloc` wächst und mit `memmove` verschoben wird. Kein Lückenpuffer, kein
Seilbaum. Für ein Notizbuch reicht das mit großem Abstand.

## Umsetzung

**Puffer.** Wächst durch Verdopplung, mindestens 64 Bytes. Immer
nullterminiert. `textbuf_len` liefert die Bytelänge ohne die Null.

**Schreibmarke und Anker.** Zwei `size_t`. Auswahl ist der Bereich dazwischen;
`textbuf_selection` liefert sie aufsteigend sortiert. `textbuf_set_cursor` zieht
einen Versatz, der mitten in einem Zeichen liegt, auf dessen Anfang zurück —
erkennbar daran, dass das Byte dort ein Folgebyte ist, also `(b & 0xC0) == 0x80`.

**Bewegen.** MOVE_LEFT und MOVE_RIGHT über utf8_prev/utf8_next.

Für MOVE_WORD_LEFT und MOVE_WORD_RIGHT: ein Wort besteht aus allem, was kein
Leerraum ist. Nach links erst Leerraum überspringen, dann bis zum Anfang des
Wortes. Nach rechts erst das Wort zu Ende, dann den folgenden Leerraum.

MOVE_UP und MOVE_DOWN sollen die Spalte möglichst beibehalten. Miss die Spalte
in CODEPUNKTEN vom Zeilenanfang, nicht in Bytes — sonst springt die Marke in
einer Zeile mit Umlauten. Merke dir eine Wunschspalte, die beim Auf- und
Abwandern erhalten bleibt und bei jeder anderen Bewegung neu gesetzt wird; sonst
verliert man beim Wandern über eine kurze Zeile die Spalte für immer.

**Ändern.** `textbuf_insert` ersetzt zuerst eine etwaige Auswahl.
`textbuf_delete_back` und `_forward` löschen die Auswahl, sonst genau einen
Codepunkt.

**Widerrufen.** Ein Stapel von Schritten. Ein Schritt merkt sich, was an
welcher Stelle eingefügt oder gelöscht wurde, dazu Schreibmarke und Anker
davor. Aufeinanderfolgende Einfügungen werden zusammengefasst, solange die neue
direkt an der vorigen anschließt und kein Zeilenumbruch dabei ist. Löschungen
werden ebenso zusammengefasst, solange sie an derselben Stelle fortsetzen.
`textbuf_break_undo` beendet die Zusammenfassung.

Nach einem Widerruf und einer neuen Änderung wird der Wiederholen-Stapel
geleert — sonst könnte man in eine Geschichte zurückkehren, die es nicht mehr
gibt.

**Zeilen.** Gezählt wird an `\n`. `textbuf_line_count` ist die Zahl der Zeilen,
also Zahl der Umbrüche plus eins. `textbuf_line_start(tb, 0)` ist 0.

## test_textbuf.c

Das ist der wichtigste Teil deines Auftrags. Decke mindestens ab:

**UTF-8 und Schreibmarke**
- Einfügen von "Grüße", dann fünfmal MOVE_LEFT: die Marke steht bei 0, nicht
  irgendwo in der Mitte eines Zeichens
- textbuf_delete_back nach "äöü" lässt vier Bytes übrig, nicht fünf
- textbuf_set_cursor mitten in ein ü zieht auf dessen Anfang zurück
- Bewegen über einen Text, der nur aus Mehrbytezeichen besteht

**Auswahl**
- Auswahl aufziehen mit extend, textbuf_selection liefert sortiert
- Einfügen bei bestehender Auswahl ersetzt sie
- Löschen bei bestehender Auswahl löscht sie ganz
- textbuf_select_all

**Wortweise**
- MOVE_WORD_RIGHT über "Hallo Welt  und mehr"
- MOVE_WORD_LEFT zurück
- an den Enden des Textes passiert nichts Schlimmes

**Zeilen und Spalten**
- MOVE_UP und MOVE_DOWN behalten die Spalte
- Wandern über eine KURZE Zeile hinweg und wieder in eine lange: die
  Wunschspalte muss erhalten bleiben. Das ist der Test, an dem die meisten
  Editoren scheitern.
- MOVE_UP in der ersten Zeile und MOVE_DOWN in der letzten tun nichts Schlimmes
- Spalten in einer Zeile mit Umlauten stimmen

**Widerrufen**
- mehrere Buchstaben tippen, einmal widerrufen: alles ist weg, nicht nur der letzte
- nach textbuf_break_undo werden sie getrennt zurückgenommen
- widerrufen und wiederholen führt zum selben Text
- nach einem Widerruf und einer neuen Änderung ist can_redo false
- widerrufen stellt auch Schreibmarke und Auswahl wieder her

**Ränder**
- leerer Puffer: alle Bewegungen und beide Löschfunktionen tun nichts Schlimmes
- textbuf_set leert die Widerruf-Geschichte

## KONVENTIONEN, strikt
- C11, keine Warnung bei -Wall -Wextra -Wpedantic. Achte auf size_t gegen int.
- Bezeichner und Testfunktionsnamen ENGLISCH. Projektregel D-1.
- Kommentare DEUTSCH in UTF-8 mit echten Umlauten. Niemals ae/oe/ue/ss.
- Stil wie src/core/keymap.c: 4 Leerzeichen, keine Tabs, K&R-Klammern.
- Sparsame Kommentare, nur was nicht offensichtlich ist.

Baue nichts, starte nichts. Berichte am Ende in drei Sätzen, wie du das
Widerrufen und die Wunschspalte gelöst hast und wo du unsicher warst.