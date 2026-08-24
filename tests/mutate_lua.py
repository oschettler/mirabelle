"""Mutationstest fuer die Lua-Anbindung. Direkt uebersetzt, mit Sanitizer.
Der Sollbildtest bleibt aussen vor - er braucht das Bausystem."""
import subprocess, pathlib, tempfile, os, shlex

LUA_CFLAGS = subprocess.run(["pkg-config","--cflags","--libs","lua5.4"],
                            capture_output=True, text=True).stdout.split()

DEPS = ["src/app/schema.c", "src/app/fieldkind.c",
        "src/core/i18n.c", "src/core/utf8.c", "src/core/keymap.c",
        "src/core/collate.c", "src/core/date.c",
        "src/store/record.c", "src/store/frontmatter.c", "src/store/gemtext.c",
        "src/store/query.c", "src/store/vault.c",
        "src/plat/plat_headless.c", "src/plat/plat_files_posix.c", "src/plat/expand.c",
        "src/ui/theme.c", "src/ui/widget.c", "src/ui/widget_list.c",
        "src/ui/widget_text.c", "src/ui/widget_scroll.c", "src/ui/scroll.c",
        "src/ui/textbuf.c",
        "src/gfx/bitmap.c", "src/gfx/pbm.c", "src/gfx/draw.c", "src/gfx/pattern.c",
        "src/gfx/font.c", "src/gfx/text.c", "build/font_system12.c",
        "tests/support/golden.c", "tests/unit/test_lua.c"]

SOURCES = ["src/lua/pdalua.c", "src/lua/pdalua_store.c", "src/lua/pdalua_schema.c"]

MUTS = [
 ("pdalua.c", "gefaehrliche Bibliotheken offen",
  '{ LUA_MATHLIBNAME, luaopen_math  },', '{ LUA_MATHLIBNAME, luaopen_math  },\n        { LUA_OSLIBNAME, luaopen_os },'),
 ("pdalua.c", "Luas print bleibt print",
  '    lua_getglobal(L, "print");\n    lua_setglobal(L, "log");', '    (void)0;'),
 ("pdalua.c", "unbekanntes Muster egal",
  '    return luaL_error(L, "unbekanntes Muster \'%s\'", name);', '    return 0;'),
 ("pdalua.c", "unbekannter Modus egal",
  '    return luaL_error(L, "unbekannter Modus \'%s\'", name);', '    return 0;'),
 ("pdalua.c", "Muster wird nicht gesetzt",
  '            if (g) g->pat = *PATTERNS[i].pat;', '            (void)g;'),
 ("pdalua.c", "Modus wird nicht gesetzt",
  '            if (g) g->mode = MODES[i].mode;', '            (void)g;'),
 ("pdalua.c", "print ohne Grundlinie",
  '    gfx_text(g, &system12, x, y + system12.ascent, s);', '    gfx_text(g, &system12, x, y, s);'),
 ("pdalua.c", "Katalog wird nicht durchgereicht",
  '        lua_pushstring(L, T(c, key));', '        lua_pushstring(L, key);'),
 ("pdalua.c", "Platzhalter werden nicht gesetzt",
  '    if (!Tf(c, key, out, sizeof out, args, argc))', '    if (!Tf(c, key, out, sizeof out, args, 0))'),
 ("pdalua.c", "Fehler kommt ohne Meldung",
  '    if (err && err_size) snprintf(err, err_size, "%s", msg ? msg : "unbekannter Fehler");', '    if (err && err_size) err[0] = 0;'),
 ("pdalua.c", "Zeichnen ohne Ziel stuerzt ab",
  '#define GC_OR_RETURN(L)                        \\\n    gc *g = current_gc(L);                     \\\n    if (!g) return 0',
  '#define GC_OR_RETURN(L)                        \\\n    gc *g = current_gc(L);                     \\\n    if (!g) g = current_gc(L)'),

 ("pdalua_store.c", "store ohne Vault trotzdem da",
  '    if (!v) {\n        lua_pushnil(L);\n        lua_setglobal(L, "store");\n        return;\n    }', '    (void)0;'),
 ("pdalua_store.c", "Skalar wird zum Feld",
  '        if (count == 1) {', '        if (false) {'),
 ("pdalua_store.c", "Listenfeld wird zum Skalar",
  '        int count = frontmatter_list_count(fm, key);', '        int count = 1;'),
 ("pdalua_store.c", "Koerper fehlt",
  '    lua_pushstring(L, record_body(rec));\n    lua_setfield(L, -2, "body");', '    (void)0;'),
 ("pdalua_store.c", "Schluessel unsortiert",
  '    qsort(keys, (size_t)count, sizeof keys[0], keyname_cmp);', '    (void)keyname_cmp;'),
 ("pdalua_store.c", "Zeilenumbruch erlaubt",
  '                if (strchr(v, \'\\n\')) {', '                if (false) {'),
 ("pdalua_store.c", "Filter greift nicht",
  '                query_where(q, k, QF_EQUALS, v);', '                (void)k;'),
 ("pdalua_store.c", "reservierte Namen als Filter",
  '            if (v && strcmp(k, "text") != 0 && strcmp(k, "sort") != 0 &&\n                strcmp(k, "desc") != 0)', '            if (v)'),
 ("pdalua_store.c", "Volltext wird uebergangen",
  '    if (lua_isstring(L, -1)) query_text(q, lua_tostring(L, -1));', '    (void)0;'),
 ("pdalua_store.c", "Sortierung wird uebergangen",
  '    if (lua_isstring(L, -1)) query_order(q, lua_tostring(L, -1), desc);', '    (void)desc;'),
 ("pdalua_store.c", "Treffer nicht sortiert",
  '        while (j >= 0 && query_compare(q, hits[j].rec, h.rec, sort) > 0) {', '        while (false) {'),
 ("pdalua_store.c", "Suche ohne Faltung",
  '        if (!query_matches(q, rec, search)) { record_free(rec); continue; }', '        if (!query_matches(q, rec, NULL)) { record_free(rec); continue; }'),
 ("pdalua_store.c", "fehlender Datensatz wirft",
  '        lua_pushnil(L);\n        return 1;\n    }\n\n    push_record', '        return luaL_error(L, "gibt es nicht");\n    }\n\n    push_record'),

 ("pdalua_schema.c", "Schlussprueufung faellt weg",
  '    if (!schema_check(&tmp, path, err, err_size)) return false;', '    (void)0;'),
 ("pdalua_schema.c", "keine Tabelle wird angenommen",
  '    if (!lua_istable(L, -1)) {\n        snprintf(err, err_size, "%s: die Datei gibt keine Tabelle zurück", path);', '    if (false) {\n        snprintf(err, err_size, "%s: die Datei gibt keine Tabelle zurück", path);'),
 ("pdalua_schema.c", "unbekannter Feldtyp erlaubt",
  '    if (!found) {\n        snprintf(err, err_size, "Feld \'%s\': unbekannter Feldtyp \'%s\'", f->name, kind);\n        return false;\n    }', '    (void)found;'),
 ("pdalua_schema.c", "required wird nicht gelesen",
  '    f->required = get_bool(L, tbl, "required");', '    f->required = false;'),
 ("pdalua_schema.c", "values werden nicht gelesen",
  '    return get_list(L, tbl, "values", f->values, SCHEMA_VALUES_MAX,\n                    &f->value_count, err, err_size);', '    return true;'),
 ("pdalua_schema.c", "Zahlen als Namen erlaubt",
  '    const char *s = lua_type(L, -1) == LUA_TSTRING ? lua_tostring(L, -1) : NULL;', '    const char *s = lua_tostring(L, -1);'),
 ("pdalua_schema.c", "Auswahlwerte muessen Text sein",
  '        const char *s = lua_tostring(L, -1);\n\n        if (!s || strlen(s) >= SCHEMA_NAME_MAX) {', '        const char *s = lua_type(L, -1) == LUA_TSTRING ? lua_tostring(L, -1) : NULL;\n\n        if (!s || strlen(s) >= SCHEMA_NAME_MAX) {'),
 ("pdalua_schema.c", "Dopplung erlaubt",
  '        if (schema_field_by_name(s, s->fields[s->field_count].name)) {', '        if (false) {'),
 ("pdalua_schema.c", "Stapel wird nicht aufgeraeumt",
  '    lua_settop(L, base);\n\n    if (!ok) {', '    if (!ok) {'),
 ("pdalua_schema.c", "Ansicht wird nicht gelesen",
  '    } else if (strcmp(view, "month") == 0) {', '    } else if (false) {'),
]

def run(overrides):
    with tempfile.TemporaryDirectory() as d:
        srcs = []
        for src in SOURCES:
            name = os.path.basename(src)
            text = overrides.get(name, pathlib.Path(src).read_text(encoding="utf-8"))
            out  = os.path.join(d, name)
            open(out, "w").write(text)
            srcs.append(out)

        exe = os.path.join(d, "t")
        cmd = (["cc","-std=c11","-Wall","-Wextra","-Wpedantic",
                "-fsanitize=address,undefined","-g","-Isrc","-Itests",
                "-DPDA_DATA_DIR=\"data\"", "-DPDA_GOLDEN_DIR=\"tests/golden\""]
               + srcs + DEPS + LUA_CFLAGS + ["-o", exe])
        b = subprocess.run(cmd, capture_output=True, text=True)
        if b.returncode != 0: return None, b.stderr

        env = dict(os.environ, TMPDIR=d)
        r = subprocess.run([exe], capture_output=True, timeout=90, env=env)
        return r.returncode, r.stdout.decode("utf-8", "replace")

rc, out = run({})
assert rc == 0, "Der unveraenderte Stand muss gruen sein:\n" + str(out)

survivors = []
for fname, name, a, b in MUTS:
    src = "src/lua/" + fname
    orig = pathlib.Path(src).read_text(encoding="utf-8")
    assert a in orig, "Muster nicht gefunden (" + fname + "): " + name
    try:
        rc, out = run({fname: orig.replace(a, b, 1)})
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
