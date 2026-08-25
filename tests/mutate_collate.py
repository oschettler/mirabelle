# SPDX-License-Identifier: GPL-3.0-or-later
"""Mutationstest fuer src/core/collate.c.

Direkt uebersetzt, nicht ueber CMake - warum, steht in tests/mutate_scroll.py.
"""
import subprocess, pathlib, tempfile, os

SRC  = pathlib.Path("src/core/collate.c")
DEPS = ["src/core/utf8.c", "src/core/lines.c", "tests/unit/test_collate.c"]
orig = SRC.read_text(encoding="utf-8")

# Nicht in der Liste, weil gleichwertig: "if (ca < 0)" zu "if (ca <= 0)" aendert
# nichts. An dieser Stelle sind beide Bytes gleich, und ein Nullbyte kann dort
# nicht stehen - die Eingabe ist eine C-Zeichenkette und die Ersetzungen sind es
# auch. Eine gleichwertige Mutation ueberlebt jeden Test und saehe im Ergebnis
# aus wie eine Luecke.

MUTS = [
 ("binaere Suche findet nichts",     "if (c->entries[mid].cp == cp) return c->entries[mid].repl;", "if (false) return c->entries[mid].repl;"),
 ("binaere Suche laeuft falsch",     "if (c->entries[mid].cp < cp)  lo = mid + 1;", "if (c->entries[mid].cp > cp)  lo = mid + 1;"),
 ("Kleinschreibung faellt weg",      "if (cp >= 'A' && cp <= 'Z') cp += 'a' - 'A';", "(void)0;"),
 ("Kleinschreibung schlaegt Tabelle","        const char *r = lookup(f->c, cp);\n        if (r) {", "        const char *r = (cp >= 'A' && cp <= 'Z') ? NULL : lookup(f->c, cp);\n        if (r) {"),
 ("Ersatz wird nicht weitergereicht","if (*f->repl) return (unsigned char)*f->repl++;", "if (*f->repl) return (unsigned char)*f->repl;"),
 ("Rohbytes werden nicht ausgegeben","        f->raw[n]  = '\\0';\n        f->raw_pos = 0;", "        f->raw[n]  = '\\0';\n        f->raw_pos = -1;"),
 ("Vergleich ohne zweite Stufe",     "    int raw = strcmp(a, b);\n    return raw < 0 ? -1 : (raw > 0 ? 1 : 0);", "    (void)a; (void)b;\n    return 0;"),
 ("Vergleich verkehrt herum",        "if (ca != cb) return ca < cb ? -1 : 1;", "if (ca != cb) return ca < cb ? 1 : -1;"),
 ("leere Nadel findet nichts",       "if (cn < 0) return true;", "if (cn < 0) return false;"),
 ("Suche rueckt nicht vor",          "        if (fold_next(&f) < 0) return false;", "        return false;"),
 ("Suche haengt an einer Stelle",    "        if (matches_here(f, c, needle)) return true;\n        if (fold_next(&f) < 0) return false;", "        if (matches_here(f, c, needle)) return true;\n        if (!*f.p) return false;"),
 ("Praefix nicht verankert",         "    folder f;\n    fold_start(&f, c, text);\n    return matches_here(f, c, prefix);", "    return collate_contains(c, text, prefix);"),
 ("Falten prueft die Groesse nicht", "        if (n + 1 >= out_size) return (size_t)-1;", "        if (false) return (size_t)-1;"),
 ("Falten terminiert nicht",         "    out[n] = '\\0';\n    return n;", "    return n;"),
 ("Dopplung wird zugelassen",        "    if (i < c->count && c->entries[i].cp == cp)", "    if (false)"),
 ("Ersatzlaenge nicht geprueft",     "if (strlen(r.word[1]) > COLLATE_REPL_MAX) {", "if (false) {"),
 ("fehlender Ersatz zugelassen",     "if (r.count != 2) {", "if (r.count > 2) {"),
 ("zu viele Angaben zugelassen",     "if (r.count != 2) {", "if (r.count < 2) {"),
 ("zwei Zeichen links zugelassen",   "        if (*left) {", "        if (false) {"),
 ("ungueltiges Zeichen zugelassen",  "if (cp == 0 || cp == UTF8_REPLACEMENT) {", "if (false) {"),
 ("Einsortieren an falscher Stelle", "    while (i < c->count && c->entries[i].cp < cp) i++;", "    i = c->count;"),
]

def run(text):
    with tempfile.TemporaryDirectory() as d:
        src = os.path.join(d, "collate.c"); exe = os.path.join(d, "t")
        open(src, "w").write(text)
        # Mit Sanitizer: manche Mutationen aendern nicht das Ergebnis, sondern
        # schreiben ueber einen Puffer hinaus. Ohne ihn saehe der Test nichts.
        b = subprocess.run(["cc","-std=c11","-Wall","-Wextra","-Wpedantic",
                            "-fsanitize=address,undefined","-g","-Isrc","-Itests",
                            "-DPDA_DATA_DIR=\"data\"", src] + DEPS + ["-o", exe],
                           capture_output=True, text=True)
        if b.returncode != 0: return None, b.stderr
        r = subprocess.run([exe], capture_output=True, timeout=30)
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
