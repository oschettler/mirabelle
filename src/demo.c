/* Die Vorführung aus M4 bis M6.
 *
 * Sie öffnet drei Fenster und füllt deren Inhalt; Rahmen, z-Ordnung,
 * Aktivierung und Ziehen macht die Fensterverwaltung. Was hier bleibt, ist
 * das, was eine Anwendung wirklich tut: Inhalt zeichnen und auf Ereignisse
 * reagieren, die die Verwaltung nicht selbst verbraucht hat.
 *
 * Der getippte Text wird angezeigt, und das ist Absicht. Wer auf einer
 * deutschen Tastatur ä eingibt, über AltGr ein € oder über die Tottaste ´
 * gefolgt von a ein á, sieht sofort, ob Entscheidung D-2 trägt.
 */

#include "demo.h"

#include <stdio.h>
#include <string.h>

#include "gfx/pattern.h"
#include "gfx/text.h"
#include "ui/widget.h"

extern const font system12;

/* Die Menüs sind Daten. Jeder Eintrag nennt nur einen Katalogschlüssel für
 * seinen Text und den Namen einer Aktion - das Kürzel daneben erzeugt die
 * Leiste selbst aus der Belegungsdatei. */
static const menu_item file_items[] = {
    { "menu.file.new",   "record.new"   },
    { "menu.file.open",  "record.open"  },
    { "menu.file.save",  "record.save"  },
    { NULL,              NULL           },
    { "menu.file.close", "window.close" },
    { "menu.file.quit",  "app.quit"     },
};

static const menu_item edit_items[] = {
    { "menu.edit.undo",  "edit.undo"  },
    { "menu.edit.redo",  "edit.redo"  },
    { NULL,              NULL         },
    { "menu.edit.cut",   "edit.cut"   },
    { "menu.edit.copy",  "edit.copy"  },
    { "menu.edit.paste", "edit.paste" },
};

static const menu_item view_items[] = {
    { "menu.view.tasks",    "app.tasks"    },
    { "menu.view.calendar", "app.calendar" },
    { "menu.view.contacts", "app.contacts" },
    { "menu.view.notes",    "app.notes"    },
};

static const menu_item special_items[] = {
    { "menu.special.search", "search.open" },
    { NULL,                  NULL          },
    { "menu.special.about",  "app.about"   },
};

static const menu demo_menus[] = {
    { "menu.file",    file_items,    6 },
    { "menu.edit",    edit_items,    6 },
    { "menu.view",    view_items,    4 },
    { "menu.special", special_items, 3 },
};

/* Einsetzen ist gesperrt, solange nichts in der Zwischenablage liegt. Der
 * Eintrag wird dadurch grau gerastert und lässt sich nicht auslösen. */
static bool demo_is_enabled(const char *action, void *user)
{
    (void)user;
    return !(action && strcmp(action, "edit.paste") == 0);
}

/* Die Fensterverwaltung schließt ein Fenster selbst, sobald der Nutzer das
 * Schließfeld trifft. Ohne diese Rückmeldung zeigten w_desk und w_keys danach
 * ins Leere, und der nächste Zeichenlauf griff auf freigegebenen Speicher zu. */
static void demo_window_closed(window *w, void *user)
{
    demo_state *st = user;
    if (w == st->w_desk) st->w_desk = NULL;
    if (w == st->w_keys) st->w_keys = NULL;
}

bool demo_init(demo_state *st, const keymap *km, const catalog *cat,
               const theme *th, int screen_w, int screen_h)
{
    static theme fallback;
    if (!th) {
        theme_defaults(&fallback);
        th = &fallback;
    }

    memset(st, 0, sizeof *st);
    st->km      = km;
    st->cat     = cat;
    st->th      = *th;   /* kopieren, nicht zeigen - siehe demo.h */
    st->running = true;

    st->m = wm_create(th, screen_w, screen_h);
    if (!st->m) return false;

    st->mb = menubar_create(demo_menus,
                            (int)(sizeof demo_menus / sizeof demo_menus[0]),
                            cat, km, th);
    if (!st->mb) {
        demo_free(st);
        return false;
    }
    menubar_set_enabled_fn(st->mb, demo_is_enabled, NULL);

    /* Unterhalb der Menüleiste: die liegt immer oben und würde sonst
     * Titelleisten verdecken. */
    int top = menubar_height(st->mb) + 20;

    st->w_keys = wm_open(st->m, rect_make(screen_w / 2 - 250, top, 240, 150),
                         T(cat, "window.keys"), WIN_NORMAL);
    st->w_desk = wm_open(st->m, rect_make(screen_w / 2 - 80, top + 40, 330, 250),
                         T(cat, "window.desk"), WIN_NORMAL);

    if (!st->w_keys || !st->w_desk) {
        demo_free(st);
        return false;
    }

    window_set_on_close(st->w_keys, demo_window_closed, st);
    window_set_on_close(st->w_desk, demo_window_closed, st);

    /* Ein echtes Formular im Schreibtischfenster: Beschriftung links, Element
     * rechts. Tab wandert durch die Elemente, der Fokus wohnt im Panel. */
    st->form = panel_create(th, cat);
    if (!st->form) {
        demo_free(st);
        return false;
    }
    panel_set_layout(st->form, LAYOUT_FORM, 6, 8);

    st->notes = text_area_create(th, cat);

    if (!panel_add(st->form, label_create(th, cat, "form.name")) ||
        !panel_add(st->form, text_field_create(th, cat)) ||
        !panel_add(st->form, label_create(th, cat, "form.status")) ||
        !panel_add(st->form, checkbox_create(th, cat, "form.done", false)) ||
        !panel_add(st->form, label_create(th, cat, "form.notes")) ||
        !st->notes || !panel_add(st->form, st->notes)) {
        demo_free(st);
        return false;
    }

    /* Der Rollbalken zum Notizfeld. Er steht nicht im Panel, sondern daneben
     * am Fensterrand - so hatte es System 1, und so muss das Layout nichts
     * von ihm wissen. Er zeigt auf das Bildlaufmodell des Felds; abgeglichen
     * wird nichts, es ist dieselbe Zahl.
     *
     * Nach panel_add zeigt st->notes auf dasselbe Widget wie zuvor, aber sein
     * Thema hängt jetzt am Panel - deshalb erst hier fragen. */
    st->notes_bar = scrollbar_create(&st->th, cat, SCROLLBAR_VERTICAL,
                                     text_widget_scroll(st->notes));
    if (!st->notes_bar) {
        demo_free(st);
        return false;
    }

    panel_focus_next(st->form);
    return true;
}

void demo_free(demo_state *st)
{
    /* Der Balken gehört nicht dem Panel - es kennt ihn gar nicht - also gibt
     * ihn die Anwendung frei. Und zwar vor dem Panel: er zeigt auf ein Modell,
     * das im Notizfeld liegt. */
    if (st->notes_bar) widget_destroy(st->notes_bar);
    st->notes_bar = NULL;
    st->notes     = NULL;

    if (st->form) panel_destroy(st->form);
    st->form = NULL;

    if (st->dlg) dialog_close(st->dlg);
    if (st->mb)  menubar_free(st->mb);
    if (st->m)   wm_destroy(st->m);

    st->dlg    = NULL;
    st->mb     = NULL;
    st->m      = NULL;
    st->w_keys = NULL;
    st->w_desk = NULL;
}

/* Führt eine Aktion aus, gleich ob sie aus einem Menü oder von einem Kürzel
 * kommt. Beide Wege landen hier - sonst könnten sie auseinanderlaufen. */
static void demo_run_action(demo_state *st, const char *action)
{
    if (!action) return;
    snprintf(st->last_action, sizeof st->last_action, "%s", action);

    if (strcmp(action, "app.quit") == 0) {
        /* Nicht sofort beenden, sondern modal nachfragen - das zeigt den
         * Dialog im Zusammenspiel. */
        const char *args[] = { T(st->cat, "window.desk") };
        const char *btns[] = { "button.cancel", "button.discard" };
        st->dlg = dialog_open(st->m, st->cat, "dialog.discard.body",
                              args, 1, btns, 2);
        if (!st->dlg) st->running = false;
    } else if (strcmp(action, "window.close") == 0) {
        window *w = wm_active(st->m);
        if (w) wm_close(st->m, w);   /* die Rückmeldung räumt die Zeiger auf */
    }
}

void demo_event(demo_state *st, const event *e)
{
    if (e->kind == EV_QUIT) {
        st->running = false;
        return;
    }

    /* Ein offener Dialog hat Vorrang vor allem anderen. */
    if (st->dlg) {
        dialog_event(st->dlg, e);

        int r = dialog_result(st->dlg);
        if (r != DIALOG_OPEN) {
            dialog_close(st->dlg);
            st->dlg = NULL;
            if (r == 1) st->running = false;   /* verworfen: beenden */
        }
        return;
    }

    /* Dann die Menüleiste: sie liegt oben und bekommt deshalb den Klick
     * zuerst. Was sie nicht will, reicht sie weiter. */
    if (st->mb) {
        const char *action = NULL;
        if (menubar_event(st->mb, e, 800, &action)) {
            demo_run_action(st, action);
            return;
        }
    }

    /* Tastatur und Klicks im Schreibtischfenster gehen an das Formular, wenn
     * dieses Fenster aktiv ist. Was es nicht will, geht weiter. */
    if (st->form && st->w_desk && wm_active(st->m) == st->w_desk) {
        rect cr = window_content_rect(st->w_desk);
        event local = *e;
        bool  positional = (e->kind == EV_MOUSE_DOWN || e->kind == EV_MOUSE_UP ||
                            e->kind == EV_MOUSE_MOVE || e->kind == EV_WHEEL);

        if (positional) {
            if (!rect_contains(cr, e->x, e->y)) goto to_wm;
            local.x -= cr.x;
            local.y -= cr.y;
        }

        /* Der Balken zuerst: er liegt neben dem Formular, und ein Klick auf
         * ihn ist keiner ins Formular. */
        if (st->notes_bar && widget_event(st->notes_bar, &local)) return;

        const char *act = NULL;
        if (panel_event(st->form, &local, &act)) {
            if (act) demo_run_action(st, act);
            return;
        }
    }

to_wm:
    /* Erst die Fensterverwaltung. Was sie verbraucht hat - Aktivieren,
     * Verschieben, Vergrößern, Schließen - sieht die Anwendung nie. */
    if (st->m && wm_event(st->m, e)) {
        if (e->kind == EV_MOUSE_DOWN) {
            st->click_x     = e->x;
            st->click_y     = e->y;
            st->click_count = e->clicks;
        }
        return;
    }

    switch (e->kind) {
    case EV_MOUSE_DOWN:
        st->click_x     = e->x;
        st->click_y     = e->y;
        st->click_count = e->clicks;
        break;

    case EV_KEY_DOWN: {
        /* Erst die Belegung fragen. Nur was dort nicht steht, wird hier von
         * Hand behandelt - und das ist bewusst wenig. */
        const char *action = st->km
            ? keymap_lookup(st->km, e->key, e->mods, "app") : NULL;

        if (action && strcmp(action, "menu.enter") == 0) {
            if (st->mb) menubar_enter(st->mb);
            break;
        }
        if (action) {
            demo_run_action(st, action);
            break;
        }

        if (e->key == KEY_ESCAPE) st->running = false;
        break;
    }

    default:
        break;
    }
}

/* Zeigt die Kürzel aus der Belegung. Sie kommen aus derselben Datei wie die
 * Auswertung; was hier steht, kann deshalb gar nicht von dem abweichen, was
 * die Taste tut. */
static void draw_keys_window(const demo_state *st, window *w)
{
    static const char *shown[] = { "record.new", "record.save", "search.open",
                                   "edit.undo", "edit.redo", "app.quit" };
    gc g;
    window_gc(w, &g);
    g.pat  = PAT_WHITE;
    g.mode = GFX_COPY;
    gfx_clear(&g);
    g.pat = PAT_BLACK;

    int y = 4 + system12.ascent;
    for (size_t i = 0; i < sizeof shown / sizeof shown[0]; i++) {
        char line[96], keys[32];
        if (st->km && keymap_describe(st->km, shown[i], keys, sizeof keys))
            snprintf(line, sizeof line, "%-13s %s", shown[i], keys);
        else
            snprintf(line, sizeof line, "%-13s -", shown[i]);
        gfx_text(&g, &system12, 8, y, line);
        y += 13;
    }
}

static void draw_desk_window(const demo_state *st, window *w)
{
    gc g;
    window_gc(w, &g);
    g.pat  = PAT_WHITE;
    g.mode = GFX_COPY;
    gfx_clear(&g);
    g.pat = PAT_BLACK;

    rect cr = window_content_rect(w);
    int  y  = 4 + system12.ascent;
    gfx_text(&g, &system12, 8, y, T(st->cat, "demo.greeting"));
    y += 16;

    char info[128];
    const char *action = st->last_action[0]
        ? st->last_action : T(st->cat, "demo.action.none");
    const char *one[] = { action };
    if (Tf(st->cat, "demo.action", info, sizeof info, one, 1))
        gfx_text(&g, &system12, 8, y, info);
    y += 16;

    /* Das Formular bekommt den Rest des Fensters. Das Layout muss vor dem
     * Zeichnen laufen, weil sich die Fenstergröße geändert haben kann. */
    if (st->form) {
        /* Der Rollbalken bekommt seinen Platz vom Layout abgezogen, statt sich
         * über das Formular zu legen: sonst liefe der Text unter ihm weiter
         * und würde für eine Breite umgebrochen, die gar nicht zu sehen ist.
         * Ein Pixel Überlappung, damit beide sich die Randlinie teilen. */
        int  bar_w = st->notes_bar ? st->notes_bar->th->scrollbar_w : 0;
        int  y0    = y - system12.ascent;
        rect area  = rect_make(0, y0, cr.w - bar_w + 1, cr.h - y0);

        panel_layout(st->form, area);
        panel_draw(st->form, &g);

        if (st->notes_bar && st->notes) {
            st->notes_bar->frame = rect_make(area.x + area.w - 1,
                                             st->notes->frame.y,
                                             bar_w, st->notes->frame.h);
            widget_draw(st->notes_bar, &g);
        }
    }
}

void demo_draw(demo_state *st, gc *g)
{
    if (!st->m) return;

    if (st->w_keys) draw_keys_window(st, st->w_keys);
    if (st->w_desk) draw_desk_window(st, st->w_desk);
    if (st->dlg)    dialog_draw(st->dlg);

    wm_draw(st->m, g);

    /* Die Menüleiste zuletzt: sie liegt immer über allem. */
    if (st->mb) menubar_draw(st->mb, g, g->dst->w);
}
