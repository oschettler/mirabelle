"""Mutationstest fuer src/app/shell.c. Direkt uebersetzt, mit Sanitizer.

Drei Mutationen ueberleben, und alle drei aus gutem Grund:

  "auch Verzeichnisse als Schema" - ein Verzeichnis laesst sich zwar oeffnen,
  aber nicht als Schema lesen; schema_load weist es ohnehin ab. Die Pruefung
  auf is_dir spart nur den Versuch.

  "Schliessen raeumt nicht auf" - das waere ein Speicherleck, kein falsches
  Ergebnis. Auf macOS/arm64 gibt es keinen LeakSanitizer, also sieht der
  Testlauf es nicht. Auf einer Plattform mit -fsanitize=leak faellt es auf.

  "Klick ausserhalb des Fensters" - der Inhaltsbereich wird nur verschoben,
  nicht verzerrt. Ein Punkt ausserhalb des Fensters liegt nach der
  Verschiebung zwangslaeufig ausserhalb des Widgets, und das weist ihn selbst
  ab. Die Pruefung spart den Weg dorthin.
"""
import subprocess, pathlib, tempfile, os

SRC  = pathlib.Path("src/app/shell.c")
DEPS = ["src/app/schema.c", "src/app/fieldkind.c", "src/app/browser.c",
        "src/app/monthview.c", "src/app/gemview.c",
        "src/core/i18n.c", "src/core/utf8.c", "src/core/keymap.c", "src/core/lines.c",
        "src/core/collate.c", "src/core/date.c",
        "src/store/record.c", "src/store/frontmatter.c", "src/store/gemtext.c",
        "src/store/query.c", "src/store/vault.c",
        "src/plat/plat_headless.c", "src/plat/plat_files_posix.c",
        "src/plat/plat_net_posix.c", "src/plat/expand.c",
        "src/ui/theme.c", "src/ui/widget.c", "src/ui/widget_list.c",
        "src/ui/widget_text.c", "src/ui/caret.c", "src/ui/widget_scroll.c", "src/ui/scroll.c",
        "src/ui/textbuf.c", "src/ui/panel.c", "src/ui/window.c", "src/ui/wm.c",
        "src/ui/menu.c", "src/ui/dialog.c",
        "src/gfx/bitmap.c", "src/gfx/pbm.c", "src/gfx/draw.c", "src/gfx/pattern.c",
        "src/gfx/font.c", "src/gfx/text.c", "build/font_system12.c",
        "tests/support/golden.c", "tests/unit/test_shell.c"]
orig = SRC.read_text(encoding="utf-8")

MUTS = [
 ("nur die erste Schemadatei", "    for (int i = 0; i < n && s->app_count < APPS_MAX; i++) {\n        if (entries[i].is_dir) continue;", "    for (int i = 0; i < 1; i++) {\n        if (entries[i].is_dir) continue;"),
 ("auch Verzeichnisse als Schema", "        if (entries[i].is_dir) continue;", "        (void)0;"),
 ("jede Datei als Schema",      '        if (!ends_with(entries[i].name, ".schema")) continue;', "        (void)0;"),
 ("kaputtes Schema zaehlt mit", "        if (!schema_load(&a->sch, path, msg, sizeof msg)) {", "        if (schema_load(&a->sch, path, msg, sizeof msg), false) {"),
 ("Fehler wird nicht gemeldet", "            snprintf(s->last_error, sizeof s->last_error, \"%s\", msg);\n            continue;", "            continue;"),
 ("ohne Schema kein Fehler",    "    if (s->app_count == 0) {\n        snprintf(err, err_size, \"%s: kein brauchbares Schema\", dir);\n        return false;\n    }", "    (void)0;"),
 ("unsortierte Reihenfolge",    "    qsort(s->apps, (size_t)s->app_count, sizeof s->apps[0], by_name);", "    (void)by_name;"),
 ("Skripte ohne Titel zaehlen", "        if (!title) continue;", "        (void)0;"),
 ("Skripte werden nicht geladen", "    load_scripts(s);", "    (void)0;"),
 ("Menue nimmt falsche Namen",  "        s->app_items[i].action = s->apps[i].label;", "        s->app_items[i].action = \"app.quit\";"),
 ("Fenster wird nicht gemerkt", "    if (a->win) {\n        wm_activate(s->wm, a->win);\n        return true;\n    }", "    if (false) {\n        wm_activate(s->wm, a->win);\n        return true;\n    }"),
 ("Rueckruf beim Schliessen fehlt", "    window_set_on_close(a->win, on_window_closed, s);", "    (void)on_window_closed;"),
 ("Schliessen raeumt nicht auf", "            widget_destroy(s->apps[i].bar);\n            browser_destroy(s->apps[i].br);\n            s->apps[i].bar = NULL;", "            s->apps[i].bar = NULL;"),
 ("Index ausserhalb erlaubt",   "    if (index < 0 || index >= s->app_count) {\n        snprintf(err, err_size, \"diese Anwendung gibt es nicht\");\n        return false;\n    }", "    if (index < 0 || index >= s->app_count) return true;"),
 ("Skriptfenster bekommt Browser", "    if (a->kind == APP_SCRIPT) {\n        if (err && err_size) err[0] = '\\0';\n        return true;\n    }", "    (void)0;"),
 ("Rollbalken fehlt",           "        a->bar = scrollbar_create(&s->th, s->cfg.catalog, SCROLLBAR_VERTICAL,\n                                  list_scroll(ov));", "        (void)ov;"),
 ("Beenden beendet nicht",      "    if (strcmp(action, \"app.quit\") == 0) {\n        s->running = false;\n        return;\n    }", "    if (strcmp(action, \"app.quit\") == 0) return;"),
 ("Anwendung wird nicht geoeffnet", "        if (strcmp(action, s->apps[i].label) != 0) continue;", "        continue;"),
 ("Schliessen schliesst nicht", "    if (strcmp(action, \"window.close\") == 0) {\n        wm_close(s->wm, a->win);\n        return;\n    }", "    if (strcmp(action, \"window.close\") == 0) return;"),
 ("Skript ohne Browser stuerzt ab", "    if (!a->br) return;", "    (void)0;"),
 ("Quit-Ereignis wird uebergangen", "    if (e->kind == EV_QUIT) {\n        s->running = false;\n        return;\n    }", "    if (e->kind == EV_QUIT) return;"),
 ("Kuerzel nur global",         "        const char *action = keymap_lookup(s->cfg.keymap, e->key, e->mods, inner);\n        if (!action)\n            action = keymap_lookup(s->cfg.keymap, e->key, e->mods, \"app\");", "        const char *action = keymap_lookup(s->cfg.keymap, e->key, e->mods, NULL);"),
 ("Kuerzel im Formular wie in der Liste", "        if (cur && cur->br && browser_view_of(cur->br) == BROWSE_FORM)\n            inner = \"form\";", "        (void)cur;"),
 ("Widgettasten werden geschluckt", "        if (action && (shell_handles(action) || is_app_label(s, action))) {", "        if (action) {"),
 ("Schalenaktionen werden durchgereicht", "        if (action && (shell_handles(action) || is_app_label(s, action))) {", "        if (false) {"),
 ("Return oeffnet nicht",         "    else if (strcmp(action, \"list.open\") == 0)      ok = browser_open_selected(a->br, msg, sizeof msg);", "    else if (strcmp(action, \"list.open\") == 0)      ok = true;"),
 ("Esc bricht nicht ab",          "    else if (strcmp(action, \"form.cancel\") == 0)  { browser_cancel(a->br); return; }", "    else if (strcmp(action, \"form.cancel\") == 0)  { return; }"),
 ("Return im Formular oeffnet",   "    else if (strcmp(action, \"form.accept\") == 0)    ok = browser_save(a->br, msg, sizeof msg);", "    else if (strcmp(action, \"form.accept\") == 0)    ok = browser_open_selected(a->br, msg, sizeof msg);"),
 ("Menue bekommt keine Ereignisse", "    if (menubar_event(s->mb, e, s->cfg.screen_w, &action)) {", "    if (false) {"),
 ("Skript bekommt keine Ereignisse", "                if (s->cfg.scripts->event &&\n                    s->cfg.scripts->event(s->cfg.scripts->user, a->script, &local))\n                    return;", "                (void)0;"),
 ("Skript wird nicht gezeichnet", "            if (s->cfg.scripts->draw)   s->cfg.scripts->draw(s->cfg.scripts->user, a->script,\n                                                             &wg, cr.w, cr.h);", "            (void)cr;"),
 ("Skript wird nicht gerechnet", "            if (s->cfg.scripts->update) s->cfg.scripts->update(s->cfg.scripts->user, a->script);", "            (void)0;"),
 ("Fensterverwaltung bekommt nichts", "    wm_event(s->wm, e);\n}", "    (void)0;\n}"),
 ("Klick ausserhalb des Fensters", "        if (!positional || rect_contains(cr, e->x, e->y)) {", "        if (true) {"),
 ("Rollbalken bekommt nichts",  "            if (a->bar && widget_event(a->bar, &local)) return;", "            (void)0;"),
 ("Doppelklick oeffnet nicht",  "                if (browser_was_opened(a->br)) {", "                if (false) {"),
 ("Fenster laeuft aus dem Bild", "    int h = s->cfg.screen_h - top - offset - 24;", "    int h = s->cfg.screen_h - top - 24;"),
]

def run(text):
    with tempfile.TemporaryDirectory() as d:
        src = os.path.join(d, "shell.c"); exe = os.path.join(d, "t")
        open(src, "w").write(text)
        b = subprocess.run(["cc","-std=c11","-Wall","-Wextra","-Wpedantic",
                            "-fsanitize=address,undefined","-g","-Isrc","-Itests",
                            "-DPDA_DATA_DIR=\"data\"",
                            "-DPDA_GOLDEN_DIR=\"tests/golden\"", src] + DEPS +
                           ["-o", exe], capture_output=True, text=True)
        if b.returncode != 0: return None, b.stderr
        env = dict(os.environ, TMPDIR=d)
        r = subprocess.run([exe], capture_output=True, timeout=90, env=env)
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
