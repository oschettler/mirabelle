/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Das Bildlaufmodell, siehe scroll.h für den Vertrag.
 *
 * Eine Datei ohne jede Abhängigkeit außer scroll.h: kein Zeichnen, kein
 * Ereignis, kein Thema. Nur Arithmetik auf drei Zahlen.
 *
 * Die Zwischenprodukte laufen über long. Die Werte hier sind Zeilen- und
 * Pixelzahlen und damit klein, aber value * track ist ein Produkt zweier
 * Eingaben, und ein Überlauf wäre in C undefiniert - für den einen Buchstaben
 * ist das nicht der Ort zum Sparen.
 */
#include "ui/scroll.h"

/* Klemmt v auf [lo, hi]. */
static int clamp(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* Teilt und rundet zur nächsten ganzen Zahl, für nichtnegative Werte.
 * Abschneiden ließe den Schieber am unteren Ende der Rinne kleben. */
static int div_round(long num, long den)
{
    return (int)((num + den / 2) / den);
}

void scroll_set(scrollmodel *m, int total, int page)
{
    m->total = total > 0 ? total : 0;
    m->page  = page  > 0 ? page  : 0;
    scroll_to(m, m->value);
}

int scroll_max(const scrollmodel *m)
{
    int max = m->total - m->page;
    return max > 0 ? max : 0;
}

bool scroll_needed(const scrollmodel *m)
{
    return m->total > m->page;
}

void scroll_to(scrollmodel *m, int value)
{
    m->value = clamp(value, 0, scroll_max(m));
}

void scroll_by(scrollmodel *m, int delta)
{
    scroll_to(m, m->value + delta);
}

void scroll_pages(scrollmodel *m, int pages)
{
    int step = m->page > 0 ? m->page : 1;
    scroll_to(m, m->value + pages * step);
}

bool scroll_reveal(scrollmodel *m, int index)
{
    if (m->total <= 0) return false;

    index = clamp(index, 0, m->total - 1);
    int before = m->value;

    if (index < m->value)
        m->value = index;
    else if (m->page > 0 && index >= m->value + m->page)
        m->value = index - m->page + 1;

    scroll_to(m, m->value);
    return m->value != before;
}

/* --- Geometrie des Schiebers -------------------------------------------------
 *
 * Die Länge zeigt an, welcher Anteil des Ganzen zu sehen ist: ein halb so
 * langer Schieber heißt, dass die Hälfte fehlt. Deshalb len = track * page /
 * total und nicht etwa eine feste Größe.
 *
 * min_len hält ihn greifbar. Ohne diese Untergrenze wäre der Schieber bei
 * tausend Zeilen in einer Rinne von hundert Pixeln keinen Pixel hoch und
 * damit nicht mehr zu treffen.
 */

/* Beide Richtungen brauchen dieselbe Länge; einmal gerechnet, zweimal
 * benutzt. Aufgerufen nur, wenn wirklich etwas zu scrollen ist - dann ist
 * total mindestens 1 und die Division sicher.
 *
 * Nach oben muss nichts begrenzt werden: page ist kleiner als total, also
 * bleibt der Bruch unter der ganzen Rinne, und min_len ist zuvor auf sie
 * begrenzt worden. */
static int thumb_len(const scrollmodel *m, int track, int min_len)
{
    if (min_len < 1) min_len = 1;
    if (min_len > track) min_len = track;

    int len = div_round((long)track * m->page, m->total);
    return len < min_len ? min_len : len;
}

void scroll_thumb(const scrollmodel *m, int track, int min_len,
                  int *pos, int *len)
{
    if (track <= 0) {
        if (pos) *pos = 0;
        if (len) *len = 0;
        return;
    }

    if (!scroll_needed(m)) {
        if (pos) *pos = 0;
        if (len) *len = track;
        return;
    }

    int l    = thumb_len(m, track, min_len);
    int span = track - l;

    /* Ist etwas zu scrollen, ist total größer als page und scroll_max damit
     * mindestens 1 - eine Prüfung auf null braucht es hier nicht. */
    if (pos) *pos = div_round((long)m->value * span, scroll_max(m));
    if (len) *len = l;
}

int scroll_value_at(const scrollmodel *m, int track, int min_len, int pos)
{
    if (track <= 0 || !scroll_needed(m)) return 0;

    int span = track - thumb_len(m, track, min_len);

    /* Füllt der Schieber die ganze Rinne, drückt keine Zieh-Bewegung mehr
     * einen Unterschied aus. Dann bleibt es, wie es ist, statt an den Anfang
     * zu springen. */
    if (span <= 0) return m->value;

    /* Einmal klemmen reicht: liegt pos zwischen 0 und span, liegt das
     * Ergebnis zwischen 0 und scroll_max. Zweimal zu klemmen sähe sorgfältig
     * aus, wäre aber toter Code - und toter Code lässt sich nicht prüfen. */
    pos = clamp(pos, 0, span);
    return div_round((long)pos * scroll_max(m), span);
}
