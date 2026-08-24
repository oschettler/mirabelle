"""Mutationstest fuer src/ui/widget_scroll.c.

Direkt uebersetzt, nicht ueber CMake - warum, steht in tests/mutate_scroll.py.
Der Sollbildtest bleibt hier aussen vor: er braucht das ganze Bausystem, und
was er prueft, ist das Aussehen, nicht die Logik.
"""
import subprocess, pathlib, tempfile, os

SRC = pathlib.Path("src/ui/widget_scroll.c")
DEPS = ["src/ui/scroll.c", "src/ui/widget.c", "src/ui/theme.c",
        "tests/unit/test_scrollbar.c"]
orig = SRC.read_text(encoding="utf-8")

MUTS = [
 ("Pfeil oben bewegt nach unten",   "scroll_by(sb->m, -1);",              "scroll_by(sb->m, +1);"),
 ("Pfeil unten bewegt nach oben",   "scroll_by(sb->m, +1);",              "scroll_by(sb->m, -1);"),
 ("Rinne bewegt nur eine Einheit",  "scroll_pages(sb->m,",                "scroll_by(sb->m,"),
 ("Rinne immer nach unten",         "sb_axis(sb, thumb.x, thumb.y) ? -1 : +1);", "sb_axis(sb, thumb.x, thumb.y) ? +1 : +1);"),
 ("Schieber vor Pfeilfeld geprueft","if (rect_contains(geom.arrow_lo, e->x, e->y)) {",
                                    "if (false && rect_contains(geom.arrow_lo, e->x, e->y)) {"),
 ("Griffabstand verworfen",         "sb->grab     = sb_axis(sb, e->x, e->y) -\n                           sb_axis(sb, thumb.x, thumb.y);",
                                    "sb->grab     = 0;"),
 ("Ziehen ohne Griffabstand",       "int     pos  = sb_axis(sb, e->x, e->y) - sb->grab - geom.track_pos;",
                                    "int     pos  = sb_axis(sb, e->x, e->y) - geom.track_pos;"),
 ("Ziehen ohne Rinnenanfang",       "int     pos  = sb_axis(sb, e->x, e->y) - sb->grab - geom.track_pos;",
                                    "int     pos  = sb_axis(sb, e->x, e->y) - sb->grab;"),
 ("Ziehen ohne Fassen",             "if (!sb->dragging) return false;\n\n        /* Beim Ziehen", "if (false) return false;\n\n        /* Beim Ziehen"),
 ("Maustaste los beendet nichts",   "sb->dragging = false;",              "sb->dragging = true;"),
 ("Mausrad falsch herum",           "scroll_by(sb->m, -e->wheel);",       "scroll_by(sb->m, e->wheel);"),
 ("Klick ausserhalb angenommen",    "if (!rect_contains(w->frame, e->x, e->y)) return false;\n        if (!scroll_needed",
                                    "if (false) return false;\n        if (!scroll_needed"),
 ("Rad ausserhalb angenommen",      "if (!rect_contains(w->frame, e->x, e->y)) return false;\n        scroll_by(sb->m, -e->wheel);",
                                    "if (false) return false;\n        scroll_by(sb->m, -e->wheel);"),
 ("toter Balken faellt durch",      "if (!scroll_needed(sb->m)) return true;", "if (!scroll_needed(sb->m)) return false;"),
 ("Balken wird Tabulatorstation",   "sb->base.wants_focus = false;",      "sb->base.wants_focus = true;"),
 ("Modell darf fehlen",             "if (!m) return NULL;",              "if (false) return NULL;"),
 ("Rinne ohne geteilte Randlinie",  "g.track_pos = r.y + s - 1;\n        g.track_len = r.h - 2 * s + 2;",
                                    "g.track_pos = r.y + s;\n        g.track_len = r.h - 2 * s;"),
 ("kleinste Schieberlaenge egal",   "return sb->base.th->scrollbar_w;",  "return 1;"),
 ("Messung quer statt laengs",      "if (pw) *pw = s;\n        if (ph) *ph = 3 * s;", "if (pw) *pw = 3 * s;\n        if (ph) *ph = s;"),
]

def run(text):
    with tempfile.TemporaryDirectory() as d:
        src = os.path.join(d, "widget_scroll.c")
        exe = os.path.join(d, "t")
        open(src, "w").write(text)
        cmd = ["cc", "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Isrc", "-Itests",
               "-DPDA_DATA_DIR=\"data\"", src] + DEPS + [
               "src/core/i18n.c", "src/core/utf8.c", "src/gfx/bitmap.c",
               "src/gfx/draw.c", "src/gfx/pattern.c", "src/gfx/text.c",
               "src/gfx/font.c", "src/gfx/pbm.c", "src/ui/textbuf.c", "src/ui/widget_text.c",
               "build/font_system12.c", "tests/support/golden.c",
               "-DPDA_GOLDEN_DIR=\"tests/golden\"", "-o", exe]
        b = subprocess.run(cmd, capture_output=True, text=True)
        if b.returncode != 0:
            return None, b.stderr
        r = subprocess.run([exe], capture_output=True, text=True)
        return r.returncode, r.stdout

rc, out = run(orig)
assert rc == 0, "Der unveraenderte Stand muss gruen sein:\n" + str(out)

survivors = []
for name, a, b in MUTS:
    assert a in orig, "Muster nicht gefunden: " + name
    rc, out = run(orig.replace(a, b, 1))
    if rc is None:
        print("BAUT NICHT  " + name); survivors.append(name)
    elif rc == 0:
        print("UEBERLEBT   " + name); survivors.append(name)
    else:
        print("erkannt     " + name)

print()
print("Ueberlebende:", survivors if survivors else "keine")
