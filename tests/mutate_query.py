"""Mutationstest fuer src/store/query.c. Direkt uebersetzt, mit Sanitizer -
warum, steht in tests/mutate_scroll.py und tests/mutate_collate.py."""
import subprocess, pathlib, tempfile, os

SRC  = pathlib.Path("src/store/query.c")
DEPS = ["src/core/utf8.c", "src/core/collate.c", "src/store/record.c",
        "src/store/frontmatter.c", "src/store/gemtext.c",
        "tests/unit/test_query.c"]
orig = SRC.read_text(encoding="utf-8")

MUTS = [
 ("zu lange Werte werden abgeschnitten", "    if (n >= cap) return false;", "    if (n >= cap) { memcpy(dst, src, cap - 1); dst[cap-1] = 0; return true; }"),
 ("Bedingungsliste laeuft ueber",   "if (q->filter_count >= QUERY_FILTERS_MAX) return false;", "if (false) return false;"),
 ("leeres Feld zugelassen",         "if (!field || !*field) return false;", "if (false) return false;"),
 ("Wert bei PRESENT uebernommen",   "if (op != QF_PRESENT && op != QF_ABSENT) {", "if (true) {"),
 ("EQUALS faltet doch",             "case QF_EQUALS:  return strcmp(v, f->value) == 0;", "case QF_EQUALS:  return text_contains(search, v, f->value);"),
 ("CONTAINS ohne Faltung",          "case QF_CONTAINS:return text_contains(search, v, f->value);", "case QF_CONTAINS:return strstr(v, f->value) != NULL;"),
 ("PREFIX nicht verankert",         "case QF_PREFIX:  return text_starts_with(search, v, f->value);", "case QF_PREFIX:  return text_contains(search, v, f->value);"),
 ("LESS und GREATER vertauscht",    "case QF_LESS:    return strcmp(v, f->value) < 0;", "case QF_LESS:    return strcmp(v, f->value) > 0;"),
 ("PRESENT ignoriert Leere",        "case QF_PRESENT: return *v != '\\0';", "case QF_PRESENT: return true;"),
 ("ABSENT ignoriert Leere",         "        return !v || !*v;", "        return !v;"),
 ("nur der erste Listeneintrag",    "    for (int i = 0; i < n; i++) {\n        const char *v = frontmatter_list_at(fm, f->field, i);", "    for (int i = 0; i < n && i < 1; i++) {\n        const char *v = frontmatter_list_at(fm, f->field, i);"),
 ("Bedingungen als Oder statt Und", "        if (!filter_matches(&q->filters[i], fm, search)) return false;", "        if (filter_matches(&q->filters[i], fm, search)) return true;"),
 ("Volltext nur im Koerper",        "    frontmatter *fm = record_fields(rec);\n    int          nf = frontmatter_count(fm);", "    frontmatter *fm = record_fields(rec);\n    int          nf = 0;"),
 ("Volltext ohne Koerper",          "    if (text_contains(search, record_body(rec), needle)) return true;", "    (void)rec;"),
 ("Volltext nur ein Wort",          "        if (!record_has_word(rec, start, (size_t)(p - start), search))\n            return false;", "        return record_has_word(rec, start, (size_t)(p - start), search);"),
 ("Volltext als Oder",              "            return false;\n    }\n    return true;\n}\n\n/* --- Messen", "            return true;\n    }\n    return false;\n}\n\n/* --- Messen"),
 ("Volltext wird uebersprungen",    "    if (q->text[0] && !text_matches(q, rec, search)) return false;", "    (void)0;"),
 ("Sortierfeld faellt auf nichts",  "    const char *field = q->order_field[0] ? q->order_field : \"id\";", "    const char *field = q->order_field;"),
 ("Leere nicht ans Ende",           "    if (ea || eb) {\n        if (ea && eb) return 0;\n        return ea ? 1 : -1;\n    }", "    if (ea && eb) return 0;"),
 ("Leere folgt der Richtung",       "        return ea ? 1 : -1;\n    }", "        return (ea ? 1 : -1) * (q->descending ? -1 : 1);\n    }"),
 ("Richtung wird ignoriert",        "    return q->descending ? -r : r;", "    return r;"),
 ("Sortierung ohne Tabelle",        "    int r = sort ? collate_compare(sort, va, vb) : strcmp(va, vb);", "    int r = strcmp(va, vb);"),
]

def run(text):
    with tempfile.TemporaryDirectory() as d:
        src = os.path.join(d, "query.c"); exe = os.path.join(d, "t")
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
