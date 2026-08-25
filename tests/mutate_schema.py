# SPDX-License-Identifier: GPL-3.0-or-later
"""Mutationstest fuer src/app/schema.c. Direkt uebersetzt, mit Sanitizer."""
import subprocess, pathlib, tempfile, os

SRC  = pathlib.Path("src/app/schema.c")
DEPS = ["tests/unit/test_schema.c"]
orig = SRC.read_text(encoding="utf-8")

MUTS = [
 ("Kopf nicht geprueft",        "    if (!s->type[0])   return fail(err, err_size, path, 0, \"type fehlt\");", "    if (false)   return false;"),
 ("folder nicht geprueft",      "    if (!s->folder[0]) return fail(err, err_size, path, 0, \"folder fehlt\");", "    if (false) return false;"),
 ("label oben nicht geprueft",  "    if (!s->label[0])  return fail(err, err_size, path, 0, \"label fehlt\");", "    if (false)  return false;"),
 ("kein Feld erlaubt",          "    if (s->field_count == 0) return fail(err, err_size, path, 0, \"kein einziges Feld\");", "    (void)0;"),

 ("Feld ohne label erlaubt",    "        if (!f->label[0])\n            return fail(err, err_size, path, 0, \"Feld „%s“: label fehlt\", f->name);", "        (void)0;"),
 ("choice ohne values erlaubt", "        if (f->kind == FIELD_CHOICE && f->value_count == 0)", "        if (false)"),
 ("values ohne choice erlaubt", "        if (f->kind != FIELD_CHOICE && f->value_count > 0)", "        if (false)"),
 ("zwei Koerper erlaubt",       "    if (gemtext > 1)", "    if (false)"),
 ("Koerper wird nicht gezaehlt","        if (s->fields[i].kind == FIELD_GEMTEXT) gemtext++;", "        (void)i;"),

 ("columns nicht geprueft",     "    for (int i = 0; i < s->column_count; i++)\n        if (!schema_field_by_name(s, s->columns[i]))", "    for (int i = 0; i < 0; i++)\n        if (!schema_field_by_name(s, s->columns[i]))"),
 ("form nicht geprueft",        "    for (int i = 0; i < s->form_count; i++)\n        if (!schema_field_by_name(s, s->form[i]))", "    for (int i = 0; i < 0; i++)\n        if (!schema_field_by_name(s, s->form[i]))"),
 ("sort nicht geprueft",        "    if (s->sort[0] && !schema_field_by_name(s, s->sort))", "    if (false)"),
 ("leerer sort wird geprueft",  "    if (s->sort[0] && !schema_field_by_name(s, s->sort))", "    if (!schema_field_by_name(s, s->sort))"),

 ("title_field mit unbekanntem Feld",
  "        const schema_field *f = schema_field_by_name(s, s->title_field);\n        if (!f)", 
  "        const schema_field *f = schema_field_by_name(s, s->title_field);\n        if (false)"),
 ("title_field darf alles sein",
  "        if (f->kind != FIELD_TEXT)\n            return fail(err, err_size, path, 0,\n                        \"title_field braucht ein Feld vom Typ text; „%s“ ist %s\",",
  "        if (false)\n            return fail(err, err_size, path, 0,\n                        \"title_field braucht ein Feld vom Typ text; „%s“ ist %s\","),
 ("leeres title_field wird geprueft",
  "    if (s->title_field[0]) {", "    if (true) {"),
 ("Ansicht ohne Datumsfeld erlaubt", "        if (f->kind != FIELD_DATE)", "        if (false)"),
 ("Ansicht mit unbekanntem Feld",    "        const schema_field *f = schema_field_by_name(s, s->view_field);\n        if (!f)", "        const schema_field *f = schema_field_by_name(s, s->view_field);\n        if (false)"),
 ("Ansicht wird nicht unterschieden","    if (s->view == VIEW_MONTH) {", "    if (false) {"),

 ("Feldsuche findet immer das erste", "        if (strcmp(s->fields[i].name, name) == 0) return &s->fields[i];", "        return &s->fields[i];"),
 ("Dateiname fehlt in der Meldung",   "        else          snprintf(err, err_size, \"%s: %s\", path, msg);", "        else          snprintf(err, err_size, \"%s\", msg);"),
]

def run(text):
    with tempfile.TemporaryDirectory() as d:
        src = os.path.join(d, "schema.c"); exe = os.path.join(d, "t")
        open(src, "w").write(text)
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
