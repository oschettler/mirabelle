"""Mutationstest fuer src/app/gemview.c. Direkt uebersetzt, mit Sanitizer.
Der Sollbildtest bleibt aussen vor - er braucht das Bausystem."""
import subprocess, pathlib, tempfile, os

SRC  = pathlib.Path("src/app/gemview.c")
DEPS = ["src/core/i18n.c", "src/core/utf8.c", "src/store/gemtext.c",
        "src/ui/theme.c", "src/ui/widget.c", "src/ui/scroll.c",
        "src/ui/widget_list.c", "src/ui/widget_text.c", "src/ui/widget_scroll.c",
        "src/ui/textbuf.c",
        "src/gfx/bitmap.c", "src/gfx/pbm.c", "src/gfx/draw.c", "src/gfx/pattern.c",
        "src/gfx/font.c", "src/gfx/text.c", "build/font_system12.c",
        "tests/support/golden.c", "tests/unit/test_gemview.c"]
orig = SRC.read_text(encoding="utf-8")

MUTS = [
 ("Verweise werden nicht gezaehlt", "            gv->link_count++;\n            link = gv->link_count;", "            link = gv->link_count;"),
 ("Verweisadresse fehlt",           "            gv->links[gv->link_count].url     = line->url;", "            gv->links[gv->link_count].url     = line->text;"),
 ("Verweis ohne Name bleibt leer",  "    if (line->kind == GEM_LINK && len == 0) {\n        text = line->url;\n        len  = line->url_len;\n    }", "    (void)0;"),
 ("Vorformatiertes wird umbrochen", "    if (line->kind == GEM_PRE) {\n        push_line(gv, text, len, GEM_PRE, 0, 0);\n        return;\n    }", "    (void)0;"),
 ("Umbruch ignoriert die Breite",   "            if (used > avail && take > 0) break;", "            if (false) break;"),
 ("Umbruch nur an Leerzeichen",     "        size_t cut = take;\n        if (start + take < len && last_space > 0) cut = last_space;", "        size_t cut = last_space > 0 ? last_space : take;"),
 ("Umbruch ohne harte Trennung",    "        if (start + take < len && last_space > 0) cut = last_space;", "        cut = last_space;"),
 ("leere Zeile faellt weg",         "    if (len == 0) {\n        push_line(gv, text, 0, kind, level, link);\n        return;\n    }", "    if (len == 0) return;"),
 ("Einzug wird nicht abgezogen",    "    int avail = width - indent;", "    int avail = width;"),
 ("Umbruch wird nie erneuert",      "    if (!gv->wrap_valid || gv->wrap_w != w) {", "    if (!gv->wrap_valid) {"),
 ("Modell wird nicht nachgezogen",  "    scroll_set(&gv->sc, gv->line_count, visible_lines(&gv->base));", "    (void)0;"),
 ("Auswahl ueberlebt neuen Text",   "    gv->selected   = 0;\n    gv->sc.value   = 0;", "    gv->sc.value   = 0;"),
 ("Umbruch wird bei neuem Text nicht verworfen", "    gv->wrap_valid = false;\n    gv->selected", "    gv->selected"),
 ("Auswahl ohne Grenzen",           "    if (number < 0 || number > gv->link_count) return;", "    (void)0;"),
 ("Auswahl wird nicht ins Bild geholt", "        if (gv->lines[i].link == number) {\n            scroll_reveal(&gv->sc, i);\n            break;\n        }", "        if (false) break;"),
 ("Merker wird nicht geloescht",    "    gv->opened = false;\n    return o;", "    return o;"),
 ("Ziffer ohne Verweis behaelt die Auswahl", "        else gemview_select_link(w, 0);", "        else (void)0;"),
 ("zweistellige Nummern gehen nicht", "        int wide  = gv->selected * 10 + digit;", "        int wide  = digit;"),
 ("Buchstaben werden geschluckt",   "        if (ch < '0' || ch > '9' || e->text[1]) return false;", "        if (false) return false;"),
 ("Tastatur ohne Fokus",            "    case EV_KEY_DOWN:\n        if (!w->focused) return false;", "    case EV_KEY_DOWN:\n        if (false) return false;"),
 ("Return oeffnet ohne Auswahl",    "            if (gv->selected) gv->opened = true;", "            gv->opened = true;"),
 ("Klick auf Textzeile waehlt",     "        if (row >= 0 && idx < gv->line_count && gv->lines[idx].link) {", "        if (row >= 0 && idx < gv->line_count) {"),
 ("Klick ausserhalb angenommen",    "        if (!rect_contains(w->frame, e->x, e->y)) return false;\n\n        rect c   = content_rect(w);", "        if (false) return false;\n\n        rect c   = content_rect(w);"),
 ("Doppelklick oeffnet nicht",      "            if (e->clicks >= 2) gv->opened = true;", "            (void)0;"),
 ("Rad falsch herum",               "        scroll_by(&gv->sc, -e->wheel);", "        scroll_by(&gv->sc, e->wheel);"),
 ("Ende springt nicht ans Ende",    "        case KEY_END:       scroll_to(&gv->sc, gv->line_count); return true;", "        case KEY_END:       return true;"),
 ("Bildlauf ohne Blaettern",        "        case KEY_DOWN:      scroll_by(&gv->sc, +1); return true;", "        case KEY_DOWN:      return true;"),
 ("Zeilenhoehe null",               "    return system12.size + 2;", "    return 0;"),
]

def run(text):
    with tempfile.TemporaryDirectory() as d:
        src = os.path.join(d, "gemview.c"); exe = os.path.join(d, "t")
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
