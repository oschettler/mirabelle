"""Mutationstest fuer src/store/index_sqlite.c. Direkt uebersetzt, mit
Sanitizer - warum, steht in tests/mutate_scroll.py."""
import subprocess, pathlib, tempfile, os

SRC  = pathlib.Path("src/store/index_sqlite.c")
DEPS = ["src/core/utf8.c", "src/core/collate.c", "src/store/record.c",
        "src/store/frontmatter.c", "src/store/gemtext.c", "src/store/query.c",
        "src/store/vault.c", "src/plat/plat_headless.c",
        "src/plat/plat_files_posix.c", "src/plat/expand.c",
        "src/gfx/bitmap.c", "src/gfx/pbm.c",
        "tests/unit/test_index.c"]
orig = SRC.read_text(encoding="utf-8")

MUTS = [
 ("Volltext bleibt beim Loeschen stehen",
  '"DELETE FROM fulltext WHERE coll = ? AND id = ?",', '"SELECT 1 WHERE ? IS NOT NULL AND ? IS NOT NULL",'),
 ("Felder bleiben beim Loeschen stehen",
  '"DELETE FROM fields   WHERE coll = ? AND id = ?",', '"SELECT 1 WHERE ? IS NOT NULL AND ? IS NOT NULL",'),
 ("Eintragen ersetzt nicht",
  "    if (!index_remove(ix, collection, id, err, err_size)) return false;", "    (void)0;"),
 ("nur der erste Listeneintrag",
  "        for (int j = 0; j < nv; j++) {", "        for (int j = 0; j < nv && j < 1; j++) {"),
 ("Sortierfassung nicht gefaltet",
  "            const char *sortkey = fold_or_raw(ix->sort,   value, sortbuf, sizeof sortbuf);",
  "            const char *sortkey = value; (void)sortbuf;"),
 ("Suchfassung nicht gefaltet",
  "            const char *findkey = fold_or_raw(ix->search, value, findbuf, sizeof findbuf);",
  "            const char *findkey = value; (void)findbuf;"),
 ("Felder nicht in den Volltext",
  "            append_text(ft, sizeof ft, &ft_len, value);", "            (void)value;"),
 ("Koerper nicht in den Volltext",
  "    append_text(ft, sizeof ft, &ft_len, record_body(rec));", "    (void)0;"),
 ("Sammlung wird nicht eingegrenzt",
  "    if (q->collection[0]) {\n        put(&b, \" AND r.coll = ?\");", "    if (false) {\n        put(&b, \" AND r.coll = ?\");"),
 ("Bedingungen werden uebergangen",
  "    for (int i = 0; i < q->filter_count; i++)\n        build_filter(&b, ix, &q->filters[i]);", "    (void)0;"),
 ("Volltext wird uebergangen",
  "    if (q->text[0]) {\n        build_match(match, sizeof match, q->text);", "    if (false) {\n        build_match(match, sizeof match, q->text);"),
 ("CONTAINS als Praefix",
  '        put(b, " AND instr(x.findkey, ?) > 0");', '        put(b, " AND instr(x.findkey, ?) = 1");'),
 ("PREFIX nicht verankert",
  '        put(b, " AND instr(x.findkey, ?) = 1");', '        put(b, " AND instr(x.findkey, ?) > 0");'),
 ("CONTAINS sucht ungefaltet",
  "        bind_later(b, fold_param(b, ix->search, f->value));\n        break;\n    case QF_PREFIX:",
  "        bind_later(b, f->value);\n        break;\n    case QF_PREFIX:"),
 ("CONTAINS als LIKE ohne Maskierung",
  '        put(b, " AND instr(x.findkey, ?) > 0");\n        bind_later(b, fold_param(b, ix->search, f->value));',
  '        put(b, " AND x.findkey LIKE \'%\' || ? || \'%\'");\n        bind_later(b, fold_param(b, ix->search, f->value));'),
 ("PRESENT ignoriert Leere",
  '        put(b, " AND x.value <> \'\'");\n        break;\n    case QF_ABSENT:', '        break;\n    case QF_ABSENT:'),
 ("ABSENT ignoriert Leere",
  "               \" AND x.id = r.id AND x.field = ? AND x.value <> '')\");", "               \" AND x.id = r.id AND x.field = ?)\");"),
 ("LESS und GREATER vertauscht",
  '        put(b, " AND x.value < ?");', '        put(b, " AND x.value > ?");'),
 ("Sortierung ueber den Rohwert",
  "\" (SELECT MIN(o.sortkey) FROM fields o WHERE o.coll = r.coll\"", "\" (SELECT MIN(o.value) FROM fields o WHERE o.coll = r.coll\""),
 ("Leere folgen der Richtung",
  "') = 0 ASC,\"", "') = 0 DESC,\""),
 ("Richtung wird ignoriert",
  '    put(&b, q->descending ? " DESC" : " ASC");', '    put(&b, " ASC");'),
 ("Grenze wird ignoriert",
  "    if (q->limit > 0) {", "    if (false) {"),
 ("Volltextwoerter als Oder",
  '            memcpy(out + n, " AND ", 5);', '            memcpy(out + n, "  OR ", 5);'),
 ("Anfuehrungszeichen nicht verdoppelt",
  "            if (*p == '\"' && n + 1 < out_size) out[n++] = '\"';", "            (void)0;"),
 ("Neuaufbau raeumt nicht auf",
  "    if (!index_clear(ix, err, err_size)) return false;", "    (void)0;"),
 ("Treffer laufen ueber den Puffer",
  "        if (n >= cap) break;", "        if (false) break;"),
]

def run(text):
    with tempfile.TemporaryDirectory() as d:
        src = os.path.join(d, "index_sqlite.c"); exe = os.path.join(d, "t")
        open(src, "w").write(text)
        b = subprocess.run(["cc","-std=c11","-Wall","-Wextra","-Wpedantic",
                            "-fsanitize=address,undefined","-g","-Isrc","-Itests",
                            "-DPDA_DATA_DIR=\"data\"", src] + DEPS +
                           ["-lsqlite3", "-o", exe], capture_output=True, text=True)
        if b.returncode != 0: return None, b.stderr
        env = dict(os.environ, TMPDIR=d)
        r = subprocess.run([exe], capture_output=True, timeout=60, env=env)
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
