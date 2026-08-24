"""Mutationstest fuer src/app/fieldkind.c. Direkt uebersetzt, mit Sanitizer."""
import subprocess, pathlib, tempfile, os

SRC  = pathlib.Path("src/app/fieldkind.c")
DEPS = ["src/app/schema.c", "src/core/i18n.c", "src/core/utf8.c",
        "src/core/keymap.c", "src/core/lines.c", "src/ui/theme.c", "src/ui/widget.c",
        "src/ui/widget_list.c", "src/ui/widget_text.c", "src/ui/caret.c", "src/ui/widget_scroll.c",
        "src/ui/scroll.c", "src/ui/textbuf.c", "src/gfx/bitmap.c", "src/gfx/draw.c",
        "src/gfx/pattern.c", "src/gfx/font.c", "src/gfx/text.c",
        "build/font_system12.c", "tests/unit/test_fieldkind.c"]
orig = SRC.read_text(encoding="utf-8")

MUTS = [
 ("read nimmt immer text_parse", "    return fieldkind_of(f)->parse(f, cat, text_widget_value(w), out, out_size);", "    return text_parse(f, cat, text_widget_value(w), out, out_size);"),
 ("write formatiert nicht",      "    fieldkind_of(f)->format(f, cat, stored, shown, sizeof shown);\n    text_widget_set_value(w, shown);", "    (void)shown; (void)cat; text_widget_set_value(w, stored);"),
 ("Text laeuft ueber",           "    if (input && strlen(input) >= out_size) return false;", "    if (false) return false;"),
 ("Datumsformat aus dem Code",   "    if (!fmt || strchr(fmt, '%') == NULL) return \"%Y-%m-%d\";\n    return fmt;", "    (void)fmt; return \"%Y-%m-%d\";"),
 ("Tag und Monat vertauscht",    "        case 'd': n += (size_t)snprintf(out + n, out_size - n, \"%02d\", d); break;", "        case 'd': n += (size_t)snprintf(out + n, out_size - n, \"%02d\", m); break;"),
 ("kaputtes Datum verschwindet", "        copy(out, out_size, stored);\n        return;", "        return;"),
 ("Datum ohne Pruefung",         "    if (y < 0 || m < 1 || m > 12 || d < 1 || d > 31) return false;", "    (void)0;"),
 ("Rest hinter dem Datum egal",  "    if (*p) return false;               /* hinten ist noch etwas übrig */", "    (void)0;"),
 ("Trennzeichen egal",           "            if (*p != *q) return false;\n            p++;", "            p++;"),
 ("Ziffern nicht geprueft",      "        if (**p < '0' || **p > '9') return false;", "        if (false) return false;"),
 ("leeres Datum abgelehnt",      "    if (end == input) return true;      /* leer ist erlaubt: kein Datum */", "    if (end == input) return false;"),
 ("bool speichert uebersetzt",   "    copy(out, out_size, yes ? \"yes\" : \"no\");", "    copy(out, out_size, yes ? T(cat, \"bool.yes\") : T(cat, \"bool.no\"));"),
 ("bool zeigt ungefiltert",      "    copy(out, out_size, T(cat, is_yes(stored) ? \"bool.yes\" : \"bool.no\"));", "    copy(out, out_size, is_yes(stored) ? \"yes\" : \"no\");"),
 ("bool nimmt nur englisch",     "    bool yes = input && (strcmp(input, \"yes\") == 0 ||\n                         strcmp(input, T(cat, \"bool.yes\")) == 0);", "    bool yes = input && strcmp(input, \"yes\") == 0;"),
 ("Kaestchen liest immer ja",    "    copy(out, out_size, checkbox_value(w) ? \"yes\" : \"no\");", "    copy(out, out_size, \"yes\");"),
 ("Kaestchen schreibt nicht",    "    checkbox_set_value(w, is_yes(stored));", "    (void)stored; (void)w;"),
 ("Auswahl ohne Katalog",        "    copy(out, out_size, i18n_has(cat, key) ? T(cat, key) : stored);", "    copy(out, out_size, T(cat, key));"),
 ("Auswahl ohne Rueckfall",      "    copy(out, out_size, i18n_has(cat, key) ? T(cat, key) : stored);", "    copy(out, out_size, stored);"),
 ("Auswahl nimmt alles an",      "    return false;    /* etwas, das im Schema nicht vorgesehen ist */", "    copy(out, out_size, input); return true;"),
 ("Auswahl nur Speicherform",    "        if (strcmp(input, v) == 0 || strcmp(input, shown) == 0) {", "        if (strcmp(input, v) == 0) {"),
 ("Auswahl liest immer den ersten", "    int sel = list_selected(w);", "    int sel = 0;"),
 ("Auswahl waehlt nicht",        "        if (stored && strcmp(stored, f->values[i]) == 0) { list_select(w, i); return; }", "        if (false) { list_select(w, i); return; }"),
 ("Auswahl kopiert nicht",       "    if (!list_set_items_copy(w, keys, f->value_count)) {", "    if (!list_set_items_copy(w, keys, 1)) {"),
 ("Registratur ohne Grenzen",    "    if ((int)kind < 0 || (size_t)kind >= sizeof OPS / sizeof OPS[0]) return &OPS[FIELD_TEXT];", "    (void)0;"),
]

def run(text):
    with tempfile.TemporaryDirectory() as d:
        src = os.path.join(d, "fieldkind.c"); exe = os.path.join(d, "t")
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
