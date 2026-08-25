"""Mutationstest fuer src/ui/scroll.c.

Uebersetzt bewusst DIREKT, nicht ueber CMake. Ein inkrementelles Bausystem
entscheidet an Zeitstempeln, ob es etwas neu baut; liegen Quelle, Objektdatei
und Programm in derselben Sekunde, laeuft der alte Stand weiter und jede
Mutation sieht aus, als haette sie ueberlebt. Das Ergebnis war von Lauf zu
Lauf verschieden - der Beweis, dass der Pruefstand falsch war, nicht die
Tests. Zwei Dateien uebersetzen sich in Sekundenbruchteilen; dafuer braucht
es kein Bausystem.

Zwei Mutationen ueberleben, und beide sind gleichwertig:

  "reveal: obere Grenze verschoben" - aus "index < m->value" wird "<=". Bei
  Gleichheit setzt der Zweig m->value auf genau den Wert, den es schon hat.

  "thumb: negative Rinne" - aus "track <= 0" wird "track < -1". Bei track = 0
  rechnet der Code danach ebenfalls (0, 0) aus: min_len wird auf track
  geklemmt, und die Laenge faellt auf null. Negative Rinnen gibt es nicht.
  Beide Wachen bleiben stehen; sie kosten nichts und sagen, was gilt.
"""
import subprocess, pathlib, tempfile, os

SRC  = pathlib.Path("src/ui/scroll.c")
TEST = "tests/unit/test_scroll.c"
orig = SRC.read_text(encoding="utf-8")

MUTS = [
 ("max: eine Einheit zu weit",        "int max = m->total - m->page;", "int max = m->total - m->page + 1;"),
 ("needed: Gleichstand zaehlt mit",   "return m->total > m->page;",    "return m->total >= m->page;"),
 ("div_round: abschneiden",           "return (int)((num + den / 2) / den);", "return (int)(num / den);"),
 ("thumb_len: Untergrenze ignoriert", "return len < min_len ? min_len : len;", "return len;"),
 ("reveal: eine Zeile zu knapp",      "m->value = index - m->page + 1;",      "m->value = index - m->page;"),
 ("reveal: obere Grenze verschoben",  "if (index < m->value)",                "if (index <= m->value)"),
 ("reveal: untere Grenze verschoben", "index >= m->value + m->page",          "index > m->value + m->page"),
 ("pages: Seitengroesse null",        "int step = m->page > 0 ? m->page : 1;","int step = m->page;"),
 ("set: springt nach oben",           "scroll_to(m, m->value);",              "scroll_to(m, 0);"),
 ("value_at: unbewegliche Rinne",     "if (span <= 0) return m->value;",      "if (span <= 0) return 0;"),
 ("value_at: pos nicht geklemmt",     "pos = clamp(pos, 0, span);",           "(void)0;"),
 ("thumb: negative Rinne",            "if (track <= 0) {",                    "if (track < -1) {"),
 ("thumb: Position vom Wert entkoppelt",
                                      "*pos = div_round((long)m->value * span, scroll_max(m));",
                                      "*pos = div_round((long)m->value * span, scroll_max(m) + 1);"),
 ("to: nicht geklemmt",               "m->value = clamp(value, 0, scroll_max(m));", "m->value = value < 0 ? 0 : value;"),
]

def build_and_run(text):
    with tempfile.TemporaryDirectory() as d:
        src = os.path.join(d, "scroll.c")
        exe = os.path.join(d, "t")
        open(src, "w").write(text)
        b = subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Wpedantic",
                            "-Isrc", "-Itests", src, TEST, "-o", exe],
                           capture_output=True, text=True)
        if b.returncode != 0:
            return None, b.stderr
        r = subprocess.run([exe], capture_output=True, text=True)
        return r.returncode, r.stdout

rc, out = build_and_run(orig)
assert rc == 0, "Der unveraenderte Stand muss gruen sein:\n" + str(out)

survivors = []
for name, a, b in MUTS:
    assert a in orig, "Muster nicht gefunden: " + a
    rc, out = build_and_run(orig.replace(a, b, 1))
    if rc is None:
        print("BAUT NICHT  " + name); survivors.append(name)
    elif rc == 0:
        print("UEBERLEBT   " + name); survivors.append(name)
    else:
        print("erkannt     " + name)

print()
print("Ueberlebende:", survivors if survivors else "keine")
