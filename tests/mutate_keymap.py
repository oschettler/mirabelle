"""Mutationstest fuer src/core/keymap.c.

Uebersetzt bewusst DIREKT, nicht ueber CMake - warum, steht in
tests/mutate_scroll.py.
"""
import subprocess, pathlib, tempfile, os

SRC  = pathlib.Path("src/core/keymap.c")
DEPS = ["src/core/lines.c", "src/plat/plat_headless.c", "src/gfx/bitmap.c",
        "tests/unit/test_keymap.c"]
orig = SRC.read_text(encoding="utf-8")

MUTS = [
 ("zu wenige Felder erlaubt",     "if (r.count != 3) {",  "if (r.count > 3) {"),
 ("zu viele Felder erlaubt",      "if (r.count != 3) {",  "if (r.count < 3) {"),
 ("Kuerzel wird nicht geprueft",
  "if (!keymap_parse_shortcut(shortcut, &e.key, &e.mods)) {", "if (false) {"),
 ("Doppelbelegung erlaubt",
  "            if (o->key == e.key && o->mods == e.mods &&\n                strcmp(o->scope, e.scope) == 0) {",
  "            if (false) {"),
 ("Bereich zaehlt nicht mit",
  "            if (o->key == e.key && o->mods == e.mods &&\n                strcmp(o->scope, e.scope) == 0) {",
  "            if (o->key == e.key && o->mods == e.mods) {"),
 ("doppelte Aktion erlaubt",
  "            if (strcmp(o->action, e.action) == 0) {", "            if (false) {"),
 ("langer Aktionsname erlaubt",
  "if (strlen(action) > KEYMAP_NAME_MAX) {", "if (false) {"),
 ("langer Bereichsname erlaubt",
  "if (strlen(scope) > KEYMAP_NAME_MAX) {", "if (false) {"),
 ("Halbfertiges wird zurueckgegeben",
  "static keymap *give_up(keymap *km, linereader *r)\n{\n    lines_close(r);\n    keymap_free(km);\n    return NULL;\n}",
  "static keymap *give_up(keymap *km, linereader *r)\n{\n    lines_close(r);\n    return km;\n}"),
 ("Zeilennummer wird nicht gemerkt", "        e.line = r.line;", "        e.line = 0;"),
 ("Bereich wird nicht bevorzugt",
  "    if (scope) {", "    if (false) {"),
 ("global faellt bei der Suche weg",
  "        if (e->key == key && e->mods == mods && strcmp(e->scope, \"global\") == 0)\n            return e->action;",
  "        (void)e;"),
]

def build_and_run(text):
    with tempfile.TemporaryDirectory() as d:
        src = os.path.join(d, "keymap.c")
        exe = os.path.join(d, "t")
        open(src, "w").write(text)
        b = subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Wpedantic",
                            "-fsanitize=address,undefined",
                            "-Isrc", "-Isrc/core", "-Itests",
                            "-DPDA_DATA_DIR=\"data\"", src] + DEPS + ["-o", exe],
                           capture_output=True, text=True)
        if b.returncode != 0:
            return None, b.stderr
        r = subprocess.run([exe], capture_output=True, text=True, timeout=30)
        return r.returncode, r.stdout

rc, out = build_and_run(orig)
assert rc == 0, "Der unveraenderte Stand muss gruen sein:\n" + str(out)

survivors = []
for name, a, b in MUTS:
    assert a in orig, "Muster nicht gefunden: " + name
    rc, out = build_and_run(orig.replace(a, b, 1))
    if rc is None:
        print("BAUT NICHT  " + name); survivors.append(name)
    elif rc == 0:
        print("UEBERLEBT   " + name); survivors.append(name)
    else:
        print("erkannt     " + name)

print()
print("Ueberlebende:", survivors if survivors else "keine")
