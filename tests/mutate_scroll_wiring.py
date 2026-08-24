import subprocess, pathlib, tempfile, os, shutil

CASES = [
 ("src/ui/widget_list.c", [
  ("Liste zieht vor dem Zeichnen nicht nach", "    list_sync(lw);\n\n    g->pat  = PAT_WHITE;", "    g->pat  = PAT_WHITE;"),
  ("Liste zieht vor dem Ereignis nicht nach", "    list_sync(lw);\n\n    switch (e->kind) {", "    switch (e->kind) {"),
  ("Seitengroesse statt Umfang",              "scroll_set(&lw->sc, lw->count, visible_rows(&lw->base));", "scroll_set(&lw->sc, lw->count, lw->count);"),
  ("Auswahl wird nicht sichtbar gemacht",     "    scroll_reveal(&lw->sc, lw->selected);", "    (void)0;"),
  ("Rad falsch herum",                        "scroll_by(&lw->sc, -e->wheel);", "scroll_by(&lw->sc, e->wheel);"),
  ("list_scroll zieht nicht nach",            "    list_widget *lw = (list_widget *)w;\n    list_sync(lw);\n    return &lw->sc;", "    list_widget *lw = (list_widget *)w;\n    return &lw->sc;"),
 ]),
 ("src/ui/widget_text.c", "src/ui/caret.c", [

  ("Schreibmarke bleibt unsichtbar",    "    scroll_reveal(&tw->sc, text_area_line_at(tw, textbuf_cursor(tw->buf)));", "    (void)0;"),
  ("Rad im einzeiligen Feld",           "        if (!tw->multiline) return false;\n        if (!rect_contains(w->frame, e->x, e->y)) return false;", "        if (!rect_contains(w->frame, e->x, e->y)) return false;"),
  ("Rad ausserhalb des Felds",          "        if (!tw->multiline) return false;\n        if (!rect_contains(w->frame, e->x, e->y)) return false;", "        if (!tw->multiline) return false;"),
  ("Rad falsch herum",                  "scroll_by(&tw->sc, -e->wheel);", "scroll_by(&tw->sc, e->wheel);"),
  ("Umbruch zieht Modell nur beim Neuumbruch nach",
   "    }\n\n    text_area_sync_scroll(tw);\n}",
   "        text_area_sync_scroll(tw);\n    }\n}"),
  ("Umbruch zieht Modell gar nicht nach",
   "    }\n\n    text_area_sync_scroll(tw);\n}",
   "    }\n}"),
 ]),
]

TESTS = ["scrollbar", "list", "text_widget", "panel", "widget"]

def run_suite():
    b = subprocess.run(["cmake","--build","build"], capture_output=True, text=True)
    if b.returncode != 0:
        return None
    ok = True
    for t in TESTS:
        r = subprocess.run(["./build/test_" + t], capture_output=True, text=True)
        if r.returncode != 0: ok = False
    return ok

survivors = []
for path, muts in CASES:
    p = pathlib.Path(path)
    orig = p.read_text(encoding="utf-8")
    for name, a, b in muts:
        assert a in orig, "Muster nicht gefunden: " + name
        p.write_text(orig.replace(a, b, 1), encoding="utf-8")
        # Objektdatei wegwerfen, sonst haelt make sie fuer aktuell.
        for o in pathlib.Path("build/CMakeFiles/pda_ui.dir/src/ui").glob("*.o"):
            o.unlink()
        for t in TESTS:
            e = pathlib.Path("build/test_" + t)
            if e.exists(): e.unlink()
        ok = run_suite()
        if ok is None:
            print("BAUT NICHT  " + name); survivors.append(name)
        elif ok:
            print("UEBERLEBT   " + name); survivors.append(name)
        else:
            print("erkannt     " + name)
        p.write_text(orig, encoding="utf-8")

for o in pathlib.Path("build/CMakeFiles/pda_ui.dir/src/ui").glob("*.o"):
    o.unlink()
subprocess.run(["cmake","--build","build"], capture_output=True)
print()
print("Ueberlebende:", survivors if survivors else "keine")
