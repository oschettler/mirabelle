# SPDX-License-Identifier: GPL-3.0-or-later
"""Mutationstest fuer src/app/browser.c. Direkt uebersetzt, mit Sanitizer.
Der Sollbildtest bleibt aussen vor - er braucht das Bausystem."""
import subprocess, pathlib, tempfile, os

LUA_CFLAGS = subprocess.run(["pkg-config","--cflags","--libs","lua5.4"],
                            capture_output=True, text=True).stdout.split()

SRC  = pathlib.Path("src/app/browser.c")
DEPS = ["src/app/schema.c", "src/app/fieldkind.c", "src/app/monthview.c",
        "src/core/date.c",
        "src/core/i18n.c", "src/core/utf8.c", "src/core/keymap.c", "src/core/lines.c",
        "src/core/collate.c",
        "src/store/record.c", "src/store/frontmatter.c", "src/store/gemtext.c",
        "src/store/query.c", "src/store/vault.c",
        "src/plat/plat_headless.c", "src/plat/plat_files_posix.c", "src/plat/expand.c",
        "src/plat/plat_net_posix.c", "src/app/gemview.c",
        "src/ui/theme.c", "src/ui/widget.c", "src/ui/widget_list.c",
        "src/ui/widget_text.c", "src/ui/caret.c", "src/ui/widget_scroll.c", "src/ui/scroll.c",
        "src/ui/textbuf.c", "src/ui/panel.c",
        "src/gfx/bitmap.c", "src/gfx/pbm.c", "src/gfx/draw.c", "src/gfx/pattern.c",
        "src/gfx/font.c", "src/gfx/text.c", "build/font_system12.c",
        "tests/support/golden.c", "src/net/spartan.c", "src/net/plat_transport.c",
        "src/lua/pdalua.c", "src/lua/pdalua_store.c",
        "src/lua/pdalua_schema.c", "src/lua/pdalua_apps.c",
        "src/lua/pdalua_net.c", "src/lua/pdalua_widgets.c",
        "tests/unit/test_browser.c"]
orig = SRC.read_text(encoding="utf-8")

MUTS = [
 ("Gemtext ist ein Feld statt der Koerper", "    if (f->kind == FIELD_GEMTEXT) return record_body(rec);", "    (void)0;"),
 ("Spalten kommen nicht aus dem Schema",    "    for (int i = 0; i < b->s->column_count; i++) {", "    for (int i = 0; i < 1; i++) {"),
 ("Spalten ohne Anzeigeform",               "        fieldkind_of(f)->format(f, b->cat, field_value(f, rec), shown, sizeof shown);", "        snprintf(shown, sizeof shown, \"%s\", field_value(f, rec));"),
 ("mehrzeilige Spalte bricht die Zeile",    "        char *nl = strchr(shown, '\\n');\n        if (nl) *nl = '\\0';", "        (void)0;"),
 ("Filter wird nicht gesetzt",              "    if (b->filter[0]) query_text(&q, b->filter);", "    (void)0;"),
 ("Filter greift nicht",                    "        if (!query_matches(&q, rec, b->search)) {", "        if (false) {"),
 ("Suche ohne Faltung",                     "        if (!query_matches(&q, rec, b->search)) {", "        if (!query_matches(&q, rec, NULL)) {"),
 ("Sortierung kommt nicht aus dem Schema",  "    query_order(&q, b->s->sort, b->s->sort_desc);", "    query_order(&q, \"id\", false);"),
 ("Sortierung ohne Faltung",                "    g_sort  = b->sort;", "    g_sort  = NULL;"),
 ("gar nicht sortiert",                     "    qsort(keep, (size_t)kept, sizeof keep[0], entry_cmp);", "    (void)entry_cmp;"),
 ("Liste bekommt keine Kopie",              "    if (!list_set_items_copy(b->list, rows, kept)) {", "    list_set_items(b->list, rows, kept);\n    if (false) {"),
 ("Auswahl ausserhalb wird nicht geprueft", "    if (i < 0 || i >= b->count) return NULL;", "    if (false) return NULL;"),
 ("Kennung geht beim Speichern verloren",   "    if (b->editing[0])\n        n += (size_t)snprintf(out + n, out_size - n, \"id: %s\\n\", b->editing);", "    (void)0;"),
 ("Pflichtfeld wird nicht geprueft",        "        if (f->required && !value[0]) {", "        if (false) {"),
 ("Eingabefehler wird verschluckt",         "        if (!fieldkind_of(f)->read(f, b->cat, b->widgets[i], value, sizeof value)) {", "        if (fieldkind_of(f)->read(f, b->cat, b->widgets[i], value, sizeof value), false) {"),
 ("Koerper landet im Front Matter",         "        if (f->kind == FIELD_GEMTEXT) { body = value; continue; }", "        (void)body;"),
 ("Zeilenumbruch im Skalar erlaubt",        "        if (strchr(value, '\\n')) {", "        if (false) {"),
 ("leere Felder werden geschrieben",        "        if (!value[0]) continue;      /* leere Felder gar nicht erst schreiben */", "        (void)0;"),
 ("Formular bleibt nach Speichern offen",   "    form_close(b);\n    b->view = BROWSE_LIST;\n\n    if (!browser_reload(b, err, err_size)) return false;", "    if (!browser_reload(b, err, err_size)) return false;"),
 ("nach Speichern keine Neuwahl",           "        if (strcmp(b->ids[i], id) == 0) { browser_select(b, i); break; }", "        if (false) { browser_select(b, i); break; }"),
 ("Loeschen ohne Auswahl erlaubt",          "        if (err && err_size) snprintf(err, err_size, \"nichts ausgewählt\");\n        return false;\n    }\n\n    char keep", "        if (err && err_size) snprintf(err, err_size, \"nichts ausgewählt\");\n        return true;\n    }\n\n    char keep"),
 ("Oeffnen ohne Auswahl erlaubt",           "    const char *id = browser_selected_id(b);\n    if (!id) {\n        if (err && err_size) snprintf(err, err_size, \"nichts ausgewählt\");\n        return false;\n    }\n\n    record *rec", "    const char *id = browser_selected_id(b);\n    if (!id) return true;\n\n    record *rec"),
 ("Formular zeigt nicht die Schemafelder",  "    for (int i = 0; i < b->s->form_count; i++) {\n        const schema_field *f = schema_field_by_name(b->s, b->s->form[i]);", "    for (int i = 0; i < 1; i++) {\n        const schema_field *f = schema_field_by_name(b->s, b->s->form[i]);"),
 ("Formular wird nicht gefuellt",           "        if (rec) ops->write(f, b->cat, w, field_value(f, rec));", "        (void)rec;"),
 ("Ansicht wechselt nicht",                 "    b->view = BROWSE_FORM;", "    (void)0;"),
 ("Abbrechen bleibt im Formular",           "    form_close(b);\n    b->view = BROWSE_LIST;\n}", "    form_close(b);\n}"),
 ("Feldzugriff im Formular ohne Namen",     "        if (strcmp(b->s->form[i], field) == 0) return b->widgets[i];", "        if (i == 0) return b->widgets[i];"),

 ("Ansicht kommt nicht aus dem Schema",     "    if (s->view == VIEW_MONTH) {", "    if (false) {"),
 ("Marken werden nicht eingetragen",        "        if (b->has_day[i]) monthview_mark(b->month, b->days[i]);", "        (void)i;"),
 ("Marken werden nicht geraeumt",           "    monthview_clear_marks(b->month);\n    for (int i = 0", "    for (int i = 0"),
 ("Marken nach einem Ereignis veraltet",    "    if (used) refresh_marks(b);", "    (void)0;"),
 ("Tag wird nicht gelesen",                 "        b->has_day[i] = daysrc &&\n                        date_from_iso(field_value(daysrc, keep[i].rec), &b->days[i]);", "        b->has_day[i] = false;"),
 ("Datumsfeld kommt nicht aus dem Schema",  "                               ? schema_field_by_name(b->s, b->s->view_field)", "                               ? schema_field_by_name(b->s, b->s->sort)"),
 ("Auswahl im Raster ohne Tagesvergleich",  "        if (b->has_day[i] && date_compare(b->days[i], sel) == 0) return i;", "        if (b->has_day[i]) return i;"),
 ("neuer Termin ohne Tag",                  "            fieldkind_of(f)->write(f, b->cat, w, iso);", "            (void)iso;"),
]

def run(text):
    with tempfile.TemporaryDirectory() as d:
        src = os.path.join(d, "browser.c"); exe = os.path.join(d, "t")
        open(src, "w").write(text)
        b = subprocess.run(["cc","-std=c11","-Wall","-Wextra","-Wpedantic",
                            "-fsanitize=address,undefined","-g","-Isrc","-Itests",
                            "-DPDA_DATA_DIR=\"data\"",
                            "-DPDA_GOLDEN_DIR=\"tests/golden\"", src] + DEPS + LUA_CFLAGS +
                           ["-o", exe], capture_output=True, text=True)
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
