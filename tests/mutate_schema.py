"""Mutationstest fuer src/app/schema.c. Direkt uebersetzt, mit Sanitizer."""
import subprocess, pathlib, tempfile, os

SRC  = pathlib.Path("src/app/schema.c")
DEPS = ["tests/unit/test_schema.c"]
orig = SRC.read_text(encoding="utf-8")

MUTS = [
 ("Einzug wird nicht gemessen", "        bool  indented = (line[0] == ' ' || line[0] == '\\t');", "        bool  indented = false;"),
 ("alles gilt als eingerueckt", "        bool  indented = (line[0] == ' ' || line[0] == '\\t');", "        bool  indented = (current != NULL);"),
 ("offenes Feld bleibt offen",  "        current = NULL;\n        ok      = top_level(", "        ok      = top_level("),
 ("Kommentar bleibt stehen",    "        char *hash = strchr(line, '#');\n        if (hash) *hash = '\\0';", "        char *hash = NULL;\n        if (hash) *hash = '\\0';"),
 ("Feldnamen doppelt erlaubt",  "            if (schema_field_by_name(&tmp, name)) {", "            if (false) {"),
 ("zu viele Felder erlaubt",    "            if (tmp.field_count >= SCHEMA_FIELDS_MAX) {", "            if (false) {"),
 ("Namen werden abgeschnitten", "    if (strlen(src) >= cap)\n        return fail(err, err_size, path, line, \"%s ist zu lang (höchstens %zu Zeichen)\",\n                    what, cap - 1);", "    (void)err; (void)err_size; (void)path; (void)line; (void)what;"),
 ("leere Listen erlaubt",       "    if (*count == 0)\n        return fail(err, err_size, path, line, \"%s: leer\", what);", "    (void)0;"),
 ("Listen laufen ueber",        "        if (*count >= cap)\n            return fail(err, err_size, path, line, \"%s: höchstens %d Einträge\",\n                        what, cap);", "        if (false) return false;"),
 ("unbekannter Schluessel egal","    return fail(err, err_size, path, line, \"unbekannter Schlüssel „%s“\", key);", "    (void)key; return true;"),
 ("unbekannter Feldtyp egal",   "        if (!parse_kind(word, &f->kind))\n            return fail(err, err_size, path, line, \"unbekannter Feldtyp „%s“\", word);", "        parse_kind(word, &f->kind);"),
 ("Richtung nicht geprueft",    "            else return fail(err, err_size, path, line,\n                             \"sort: „%s“ ist keine Richtung (asc oder desc)\", dir);", "            else s->sort_desc = false;"),
 ("desc wird ignoriert",        "            if (strcmp(dir, \"desc\") == 0)      s->sort_desc = true;", "            if (strcmp(dir, \"desc\") == 0)      s->sort_desc = false;"),
 ("columns nicht geprueft",     "    for (int i = 0; i < s->column_count; i++)\n        if (!schema_field_by_name(s, s->columns[i]))", "    for (int i = 0; i < 0; i++)\n        if (!schema_field_by_name(s, s->columns[i]))"),
 ("form nicht geprueft",        "    for (int i = 0; i < s->form_count; i++)\n        if (!schema_field_by_name(s, s->form[i]))", "    for (int i = 0; i < 0; i++)\n        if (!schema_field_by_name(s, s->form[i]))"),
 ("sort nicht geprueft",        "    if (s->sort[0] && !schema_field_by_name(s, s->sort))", "    if (false)"),
 ("Kopf nicht geprueft",        "    if (!s->type[0])   return fail(err, err_size, path, 0, \"type fehlt\");", "    if (false)   return false;"),
 ("folder nicht geprueft",      "    if (!s->folder[0]) return fail(err, err_size, path, 0, \"folder fehlt\");", "    if (false) return false;"),
 ("label oben nicht geprueft",  "    if (!s->label[0])  return fail(err, err_size, path, 0, \"label fehlt\");", "    if (false)  return false;"),
 ("Feld ohne label erlaubt",    "        if (!f->label[0])\n            return fail(err, err_size, path, 0, \"Feld „%s“: label fehlt\", f->name);", "        (void)0;"),
 ("choice ohne values erlaubt", "        if (f->kind == FIELD_CHOICE && f->value_count == 0)", "        if (false)"),
 ("values ohne choice erlaubt", "        if (f->kind != FIELD_CHOICE && f->value_count > 0)", "        if (false)"),
 ("kein Feld erlaubt",          "    if (s->field_count == 0) return fail(err, err_size, path, 0, \"kein einziges Feld\");", "    (void)0;"),
 ("Halbfertiges wird uebernommen", "    if (!ok) return false;\n    if (!check_whole(&tmp, err, err_size, path)) return false;\n\n    *s = tmp;", "    *s = tmp;\n    if (!ok) return false;\n    if (!check_whole(&tmp, err, err_size, path)) return false;"),
 ("required no wird true",      "        else if (strcmp(word, \"no\") == 0)  f->required = false;", "        else if (strcmp(word, \"no\") == 0)  f->required = true;"),
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
