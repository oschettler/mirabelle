# Stilblatt für das Handbuch

Wer ein Kapitel schreibt, hält sich hieran. Das Buch soll wie *ein* Buch
klingen, nicht wie vierzehn.

## Ton

Du sprichst die Leserin mit **du** an. Nicht anbiedernd, nicht kumpelhaft —
so, wie eine Kollegin einer anderen etwas erklärt, die es noch nicht kennt.

Zielgruppe ist ab etwa dreizehn Jahren, mit etwas Programmiererfahrung, ohne
Ausbildung. Das heißt:

- Kein Fachwort ohne Erklärung beim ersten Vorkommen. Danach darfst du es
  benutzen.
- Keine Herablassung. Niemand ist „nur Anfänger". Wer dreizehn ist, versteht
  einen Zeiger, wenn man ihn ordentlich erklärt.
- Keine falsche Munterkeit. Kein „Super!", kein „Ganz easy!". Wenn etwas
  schwierig ist, sag, dass es schwierig ist.

## Was ein gutes Kapitel ausmacht

**Erst das Warum, dann das Wie.** Bevor du zeigst, wie ein XOR-Umriss
gezeichnet wird, sag, welches Problem das löst.

**Echter Code aus dem Projekt.** Zitiere aus den echten Dateien und nenne den
Pfad. Erfinde keinen Beispielcode, der so nicht im Projekt steht — die Leserin
soll ihn wiederfinden und ausprobieren können.

**Fehlschläge gehören dazu.** Wo im Projekt etwas schiefging und repariert
wurde, erzähl es. Das ist der lehrreichste Teil. Beispiele: das `t`, das wie
ein Pluszeichen aussah; der Zeiger auf ein Thema, das nicht mehr existierte;
der Test, der wegen zweier Knöpfe gar nichts prüfen konnte.

**Sag, was noch nicht existiert.** Kapitel über M8 und später beschreiben
Geplantes. Setz oben einen Hinweis:

    [NOTE]
    .Geplant
    ====
    Dieses Kapitel beschreibt Schritt M10. Gebaut sind zurzeit M1 bis M7.
    ====

## Form

AsciiDoc. Überschriften mit `==` beginnen (der Master setzt `leveloffset=+1`).

- Codeblöcke immer mit Sprache: `[source,c]`, `[source,lua]`, `[source,bash]`
- Dateipfade als `` `src/gfx/bitmap.c` ``
- Tastenkürzel mit `kbd:[Cmd+N]`
- Kästen sparsam: `[NOTE]`, `[TIP]`, `[WARNING]`, `[IMPORTANT]`
- Diagramme als Nomnoml unter `diagramme/`, eingebunden als erzeugtes SVG

**Keine ASCII-Grafik.** Keine Kästen aus Bindestrichen, keine Pfeile aus
Zeichen. Wenn ein Bild nötig ist, wird es ein Nomnoml-Diagramm oder ein
gerendertes Bildschirmfoto.

**Keine Emoji.**

**Echte Umlaute**, immer. Auch in Bezeichnern? Nein — Bezeichner im Code sind
englisch (Projektregel D-1), Fließtext ist deutsch.

## Länge

Ein Kapitel hat 150 bis 400 Zeilen AsciiDoc. Lieber gründlich als vollständig:
ein Kapitel, das eine Sache wirklich erklärt, ist mehr wert als eines, das
zehn Sachen aufzählt.

## Am Ende jedes Kapitels

Ein Abschnitt „Zum Ausprobieren" mit zwei bis vier Aufgaben, die sich mit dem
vorhandenen Code wirklich lösen lassen. Keine Aufgaben zu Dingen, die es noch
nicht gibt.
