# SPDX-License-Identifier: GPL-3.0-or-later
"""Mutationstest fuer src/ui/caret.c.

Uebersetzt bewusst DIREKT, nicht ueber CMake. Ein inkrementelles Bausystem
entscheidet an Zeitstempeln, ob es etwas neu baut; liegen Quelle, Objektdatei
und Programm in derselben Sekunde, laeuft der alte Stand weiter und jede
Mutation sieht aus, als haette sie ueberlebt. Zwei Dateien uebersetzen sich in
Sekundenbruchteilen; dafuer braucht es kein Bausystem.
"""
import subprocess, pathlib, tempfile, os

SRC  = pathlib.Path("src/ui/caret.c")
TEST = "tests/unit/test_caret.c"
orig = SRC.read_text(encoding="utf-8")

MUTS = [
 ("erster Tick stellt schon die Phase", "if (!started || woke) {", "if (woke) {"),
 ("Wecken vergisst die Phase",          "if (!started || woke) {", "if (!started) {"),
 ("Wecken macht nicht sichtbar",        "    on   = true;\n    woke = true;", "    woke = true;"),
 ("Grenze eine Millisekunde zu spaet",  "if (elapsed < CARET_BLINK_MS) return;",
                                        "if (elapsed < CARET_BLINK_MS + 1) return;"),
 ("gerade Spruenge kippen auch",        "if ((elapsed / CARET_BLINK_MS) % 2 != 0) on = !on;", "on = !on;"),
 ("Phase wird verworfen",               "since = now_ms - elapsed % CARET_BLINK_MS;", "since = now_ms;"),
 ("Takt steht still",                   "if ((elapsed / CARET_BLINK_MS) % 2 != 0) on = !on;", "(void)0;"),
 ("Ruecksetzen laesst den Takt laufen",  "    started = false;\n    woke    = false;", "    woke    = false;"),
]

def build_and_run(text):
    with tempfile.TemporaryDirectory() as d:
        src = os.path.join(d, "caret.c")
        exe = os.path.join(d, "t")
        open(src, "w").write(text)
        b = subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Wpedantic",
                            "-fsanitize=address,undefined",
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
