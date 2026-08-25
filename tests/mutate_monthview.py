# SPDX-License-Identifier: GPL-3.0-or-later
"""Mutationstest fuer src/app/monthview.c. Direkt uebersetzt, mit Sanitizer.
Der Sollbildtest bleibt aussen vor - er braucht das Bausystem.

Eine Mutation ueberlebt, und sie ist gleichwertig:

  "Zellenhoehe ohne Schutz" - cell_h() gibt null zurueck, wenn der Rahmen
  nicht einmal die Kopfzeile fasst. Ohne diesen Schutz kaeme eine negative
  Zahl heraus. Beobachtbar ist der Unterschied nicht: jede Aufrufstelle prueft
  auf "hoechstens null", und bei den Hoehen, die vorkommen, teilt die
  ganzzahlige Division ohnehin zu null ab. Der Schutz bleibt trotzdem stehen -
  eine negative Zellenhoehe waere eine Zahl, die nichts bedeutet, und die
  naechste Aufrufstelle prueft vielleicht nur auf null.
"""
import subprocess, pathlib, tempfile, os

SRC  = pathlib.Path("src/app/monthview.c")
DEPS = ["src/core/date.c", "src/core/i18n.c", "src/core/utf8.c",
        "src/ui/theme.c", "src/core/lines.c", "src/ui/widget.c", "src/ui/widget_list.c",
        "src/ui/widget_text.c", "src/ui/caret.c", "src/ui/widget_scroll.c", "src/ui/scroll.c",
        "src/ui/textbuf.c", "src/gfx/bitmap.c", "src/gfx/pbm.c", "src/gfx/draw.c",
        "src/gfx/pattern.c", "src/gfx/font.c", "src/gfx/text.c",
        "build/font_system12.c", "tests/support/golden.c",
        "tests/unit/test_monthview.c"]
orig = SRC.read_text(encoding="utf-8")

MUTS = [
 ("Wochenbeginn fest auf Montag", "    int         v = (s && *s >= '1' && *s <= '7') ? *s - '0' : 1;", "    (void)s; int v = 1;"),
 ("Wochenbeginn ohne Nullbasis",  "    return v - 1;", "    return v;"),
 ("Wochentagsnamen ignoriert",    "        if (i == weekday) {", "        if (i == 0) {"),
 ("Versatz ohne Umbruch",         "    return off < 0 ? off + 7 : off;", "    return off;"),
 ("Versatz nicht abgezogen",      "    int idx = row * COLS + col - first_offset(mv);", "    int idx = row * COLS + col;"),
 ("Tage ueber das Monatsende",    "    if (idx < 0 || idx >= len) return 0;", "    if (idx < 0 || idx >= 31) return 0;"),
 ("Auswahl nicht nach unten geklemmt", "    if (mv->selected_day < 1)   mv->selected_day = 1;", "    (void)0;"),
 ("Auswahl nicht nach oben geklemmt",  "    if (mv->selected_day > len) mv->selected_day = len;", "    (void)0;"),
 ("Monat wird nicht auf den Ersten gesetzt", "    mv->shown        = nxt;\n    mv->shown.day    = 1;", "    mv->shown        = nxt;"),
 ("Blaettern nimmt die Auswahl nicht mit", "    mv->selected_day = nxt.day;", "    (void)0;"),
 ("Blaettern mit Tagen statt Monaten", "    date nxt = date_add_months(cur, months);", "    date nxt = date_add_days(cur, months * 31);"),
 ("Blaettern vergisst die Marken nicht", "    monthview_clear_marks(w);\n}\n\nvoid monthview_clear_marks", "    (void)0;\n}\n\nvoid monthview_clear_marks"),
 ("Auswahl in fremdem Monat blaettert nicht", "    if (d.year != mv->shown.year || d.month != mv->shown.month) {\n        mv->shown = d;", "    if (false) {\n        mv->shown = d;"),
 ("ungueltige Auswahl angenommen", "    if (!date_valid(d)) return false;\n\n    if (d.year", "    if (false) return false;\n\n    if (d.year"),
 ("Marken aus fremdem Monat",     "    if (d.year != mv->shown.year || d.month != mv->shown.month) return;", "    (void)0;"),
 ("Marken ohne Grenzen",          "    if (day < 1 || day > 31) return false;", "    (void)0;"),
 ("Anlegen prueft das Datum nicht", "    if (!date_valid(shown)) return NULL;", "    if (false) return NULL;"),
 ("Pfeil rechts geht eine Woche", "        case KEY_RIGHT: mv->selected_day++;     clamp_selection(mv); return true;", "        case KEY_RIGHT: mv->selected_day += 7;  clamp_selection(mv); return true;"),
 ("Pfeil runter geht einen Tag",  "        case KEY_DOWN:  mv->selected_day += 7;  clamp_selection(mv); return true;", "        case KEY_DOWN:  mv->selected_day += 1;  clamp_selection(mv); return true;"),
 ("Ende geht nicht ans Monatsende", "        case KEY_END:   mv->selected_day = len; return true;", "        case KEY_END:   mv->selected_day = 28; return true;"),
 ("Bild-ab blaettert rueckwaerts", "        case KEY_PAGE_DOWN: monthview_show_month(w, +1); return true;", "        case KEY_PAGE_DOWN: monthview_show_month(w, -1); return true;"),
 ("Tastatur ohne Fokus",          "        if (!w->focused) return false;", "        if (false) return false;"),
 ("Rad falsch herum",             "        monthview_show_month(w, -e->wheel);", "        monthview_show_month(w, e->wheel);"),
 ("Rad ausserhalb angenommen",    "        if (!rect_contains(w->frame, e->x, e->y)) return false;\n        monthview_show_month", "        if (false) return false;\n        monthview_show_month"),
 ("Doppelklick oeffnet nicht",    "            if (e->clicks >= 2) mv->opened = true;", "            (void)0;"),
 ("leere Zelle waehlt trotzdem",  "        int day = day_at_cell(mv, col, row);\n        if (day) {", "        int day = day_at_cell(mv, col, row);\n        if (true) {"),
 ("Merker wird nicht geloescht",  "    mv->opened = false;\n    return o;", "    return o;"),
 ("Klick daneben angenommen",     "            return rect_contains(w->frame, e->x, e->y);", "            return true;"),
 ("Kopfzeilenhoehe null",         "static int header_h(const widget *w) { return w->th->menu_item_h; }", "static int header_h(const widget *w) { (void)w; return 0; }"),
 ("Zellenhoehe ohne Schutz",      "    return h > 0 ? h / ROWS : 0;", "    return h / ROWS;"),
]

def run(text):
    with tempfile.TemporaryDirectory() as d:
        src = os.path.join(d, "monthview.c"); exe = os.path.join(d, "t")
        open(src, "w").write(text)
        b = subprocess.run(["cc","-std=c11","-Wall","-Wextra","-Wpedantic",
                            "-fsanitize=address,undefined","-g","-Isrc","-Itests",
                            "-DPDA_DATA_DIR=\"data\"",
                            "-DPDA_GOLDEN_DIR=\"tests/golden\"", src] + DEPS +
                           ["-o", exe], capture_output=True, text=True)
        if b.returncode != 0: return None, b.stderr
        r = subprocess.run([exe], capture_output=True, timeout=60)
        return r.returncode, r.stdout.decode("utf-8", "replace")

rc, out = run(orig)
assert rc == 0, "Der unveraenderte Stand muss gruen sein:\n" + str(out)

survivors = []
for name, a, b in MUTS:
    assert a in orig, "Muster nicht gefunden: " + name
    try:
        rc, out = run(orig.replace(a, b, 1))
    except subprocess.TimeoutExpired:
        print("erkannt     " + name + " (haengt)"); continue
    if rc is None:
        print("BAUT NICHT  " + name); survivors.append(name)
    elif rc == 0:
        print("UEBERLEBT   " + name); survivors.append(name)
    else:
        print("erkannt     " + name)

print()
print("Ueberlebende:", survivors if survivors else "keine")
