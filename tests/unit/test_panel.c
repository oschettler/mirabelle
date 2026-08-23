/* Panel: Feld, Layout und Fokusverwaltung aus M8.
 *
 * Die Geometrie wird nicht gegen feste Pixelwerte geprüft, sondern gegen
 * widget_measure - genau wie test_menu.c seine Formeln aus menu.c nachbaut,
 * statt Zahlen zu raten. Der Textkatalog bleibt NULL: T() liefert dann immer
 * den Schlüssel selbst zurück (siehe core/i18n.h), das reicht für Maße und
 * macht die Tests unabhängig von echten Sprachdateien.
 *
 * Für Verhalten, das echte Bedienelemente nicht zuverlässig zeigen -
 * insbesondere "ein Ereignis kommt beim fokussierten Element an" -, dient
 * ein kleines eigenes Testwidget (sink), das jedes Ereignis verschluckt und
 * mitzählt.
 */

#include "test.h"

#include <stdlib.h>

#include "ui/panel.h"

static theme make_theme(void)
{
    theme th;
    theme_defaults(&th);
    return th;
}

/* --- Ein Testwidget, das jedes Ereignis verschluckt und mitzählt --------- */

static void sink_measure(widget *w, int *pw, int *ph)
{
    (void)w;
    *pw = 12;
    *ph = 8;
}

static void sink_draw(const widget *w, gc *g)
{
    (void)w;
    (void)g;
}

static bool sink_event(widget *w, const event *e)
{
    (void)e;
    int *count = w->user;
    (*count)++;
    return true;
}

static void sink_destroy(widget *w)
{
    /* Gibt NUR das zurück, was diese Klasse zusätzlich belegt hat. Das Widget
     * selbst gibt widget_destroy() frei; ein free(w) hier wäre ein zweites
     * Freigeben (siehe widget.h). */
    free(w->user);
}

static const widget_class sink_class = {
    .name    = "test.sink",
    .measure = sink_measure,
    .draw    = sink_draw,
    .event   = sink_event,
    .destroy = sink_destroy,
};

static widget *sink_create(bool wants_focus)
{
    widget *w  = calloc(1, sizeof *w);
    int    *ct = calloc(1, sizeof *ct);

    w->cls         = &sink_class;
    w->enabled     = true;
    w->wants_focus = wants_focus;
    w->user        = ct;

    return w;
}

/* --- Feld: add, count, at -------------------------------------------------- */

TEST(panel_add_and_count_and_at)
{
    theme th = make_theme();
    panel *p = panel_create(&th, NULL);
    REQUIRE(p);

    widget *a = label_create(&th, NULL, "test.a");
    widget *b = label_create(&th, NULL, "test.b");
    REQUIRE(a && b);

    CHECK_EQ(panel_count(p), 0);
    CHECK(panel_add(p, a));
    CHECK(panel_add(p, b));
    CHECK_EQ(panel_count(p), 2);

    CHECK(panel_at(p, 0) == a);
    CHECK(panel_at(p, 1) == b);
    CHECK(panel_at(p, -1) == NULL);
    CHECK(panel_at(p, 2) == NULL);

    panel_destroy(p);
}

/* --- Layout: VSTACK --------------------------------------------------------- */

TEST(vstack_stacks_children_with_gap_and_equal_width)
{
    theme th = make_theme();
    panel *p = panel_create(&th, NULL);
    REQUIRE(p);
    panel_set_layout(p, LAYOUT_VSTACK, 4, 6);

    widget *a = label_create(&th, NULL, "test.short");
    widget *b = button_create(&th, NULL, "test.longer.button.label", "test.action");
    widget *c = checkbox_create(&th, NULL, "test.check", false);
    REQUIRE(a && b && c);
    REQUIRE(panel_add(p, a));
    REQUIRE(panel_add(p, b));
    REQUIRE(panel_add(p, c));

    int aw, ah, bw, bh, cw, ch;
    widget_measure(a, &aw, &ah);
    widget_measure(b, &bw, &bh);
    widget_measure(c, &cw, &ch);
    (void)aw; (void)bw; (void)cw;

    rect area = rect_make(0, 0, 300, 300);
    panel_layout(p, area);

    int expect_w = area.w - 2 * 6;

    CHECK_EQ(a->frame.x, area.x + 6);
    CHECK_EQ(a->frame.y, area.y);
    CHECK_EQ(a->frame.w, expect_w);
    CHECK_EQ(a->frame.h, ah);

    CHECK_EQ(b->frame.x, area.x + 6);
    CHECK_EQ(b->frame.y, a->frame.y + a->frame.h + 4);
    CHECK_EQ(b->frame.w, expect_w);
    CHECK_EQ(b->frame.h, bh);

    CHECK_EQ(c->frame.x, area.x + 6);
    CHECK_EQ(c->frame.y, b->frame.y + b->frame.h + 4);
    CHECK_EQ(c->frame.w, expect_w);
    CHECK_EQ(c->frame.h, ch);

    panel_destroy(p);
}

/* --- Layout: HSTACK --------------------------------------------------------- */

TEST(hstack_places_children_side_by_side_with_gap_and_equal_height)
{
    theme th = make_theme();
    panel *p = panel_create(&th, NULL);
    REQUIRE(p);
    panel_set_layout(p, LAYOUT_HSTACK, 4, 6);

    widget *a = label_create(&th, NULL, "test.a");
    widget *b = button_create(&th, NULL, "test.button.key", "test.action");
    widget *c = checkbox_create(&th, NULL, "test.c", true);
    REQUIRE(a && b && c);
    REQUIRE(panel_add(p, a));
    REQUIRE(panel_add(p, b));
    REQUIRE(panel_add(p, c));

    int aw, ah, bw, bh, cw, ch;
    widget_measure(a, &aw, &ah);
    widget_measure(b, &bw, &bh);
    widget_measure(c, &cw, &ch);
    (void)ah; (void)bh; (void)ch;

    rect area = rect_make(0, 0, 400, 300);
    panel_layout(p, area);

    int expect_h = area.h - 2 * 6;

    CHECK_EQ(a->frame.x, area.x);
    CHECK_EQ(a->frame.y, area.y + 6);
    CHECK_EQ(a->frame.w, aw);
    CHECK_EQ(a->frame.h, expect_h);

    CHECK_EQ(b->frame.x, a->frame.x + a->frame.w + 4);
    CHECK_EQ(b->frame.y, area.y + 6);
    CHECK_EQ(b->frame.w, bw);
    CHECK_EQ(b->frame.h, expect_h);

    CHECK_EQ(c->frame.x, b->frame.x + b->frame.w + 4);
    CHECK_EQ(c->frame.y, area.y + 6);
    CHECK_EQ(c->frame.w, cw);
    CHECK_EQ(c->frame.h, expect_h);

    panel_destroy(p);
}

/* --- Layout: FORM ------------------------------------------------------------ */

TEST(form_left_column_matches_widest_even_indexed_item)
{
    theme th = make_theme();
    panel *p = panel_create(&th, NULL);
    REQUIRE(p);
    panel_set_layout(p, LAYOUT_FORM, 5, 8);

    widget *l0 = label_create(&th, NULL, "test.k");
    widget *f0 = label_create(&th, NULL, "test.field.one");
    widget *l1 = label_create(&th, NULL, "test.a.rather.much.longer.label.key.than.the.first");
    widget *f1 = label_create(&th, NULL, "test.field.two");
    REQUIRE(l0 && f0 && l1 && f1);
    REQUIRE(panel_add(p, l0));
    REQUIRE(panel_add(p, f0));
    REQUIRE(panel_add(p, l1));
    REQUIRE(panel_add(p, f1));

    int l0w, l0h, f0w, f0h, l1w, l1h, f1w, f1h;
    widget_measure(l0, &l0w, &l0h);
    widget_measure(f0, &f0w, &f0h);
    widget_measure(l1, &l1w, &l1h);
    widget_measure(f1, &f1w, &f1h);
    (void)f0w; (void)f1w;

    int left_w = l0w > l1w ? l0w : l1w;

    rect area = rect_make(0, 0, 400, 200);
    panel_layout(p, area);

    int content_x = area.x + 8;
    int content_w = area.w - 2 * 8;
    int right_w   = content_w - left_w - 5;
    int row0_h    = l0h > f0h ? l0h : f0h;
    int row1_h    = l1h > f1h ? l1h : f1h;

    CHECK_EQ(l0->frame.x, content_x);
    CHECK_EQ(l0->frame.y, area.y);
    CHECK_EQ(l0->frame.w, left_w);
    CHECK_EQ(l0->frame.h, row0_h);

    CHECK_EQ(f0->frame.x, content_x + left_w + 5);
    CHECK_EQ(f0->frame.y, area.y);
    CHECK_EQ(f0->frame.w, right_w);
    CHECK_EQ(f0->frame.h, row0_h);

    CHECK_EQ(l1->frame.x, content_x);
    CHECK_EQ(l1->frame.y, area.y + row0_h + 5);
    CHECK_EQ(l1->frame.w, left_w);
    CHECK_EQ(l1->frame.h, row1_h);

    CHECK_EQ(f1->frame.x, content_x + left_w + 5);
    CHECK_EQ(f1->frame.y, l1->frame.y);
    CHECK_EQ(f1->frame.w, right_w);
    CHECK_EQ(f1->frame.h, row1_h);

    panel_destroy(p);
}

TEST(form_allows_odd_count_last_item_spans_full_width)
{
    theme th = make_theme();
    panel *p = panel_create(&th, NULL);
    REQUIRE(p);
    panel_set_layout(p, LAYOUT_FORM, 5, 8);

    widget *l0   = label_create(&th, NULL, "test.k1");
    widget *f0   = label_create(&th, NULL, "test.value.one");
    widget *lone = label_create(&th, NULL, "test.lone.spans.full.width");
    REQUIRE(l0 && f0 && lone);
    REQUIRE(panel_add(p, l0));
    REQUIRE(panel_add(p, f0));
    REQUIRE(panel_add(p, lone));

    int l0w, l0h, f0w, f0h, lonew, loneh;
    widget_measure(l0, &l0w, &l0h);
    widget_measure(f0, &f0w, &f0h);
    widget_measure(lone, &lonew, &loneh);
    (void)l0w; (void)f0w; (void)lonew;

    rect area = rect_make(0, 0, 400, 200);
    panel_layout(p, area);

    int row0_h = l0h > f0h ? l0h : f0h;

    CHECK_EQ(lone->frame.x, area.x + 8);
    CHECK_EQ(lone->frame.y, area.y + row0_h + 5);
    CHECK_EQ(lone->frame.w, area.w - 2 * 8);
    CHECK_EQ(lone->frame.h, loneh);

    panel_destroy(p);
}

/* --- panel_measure gegen panel_layout ---------------------------------------- */

TEST(measure_matches_what_layout_needs_for_vstack)
{
    theme th = make_theme();
    panel *p = panel_create(&th, NULL);
    REQUIRE(p);
    panel_set_layout(p, LAYOUT_VSTACK, 3, 7);

    widget *a = label_create(&th, NULL, "test.short.key");
    widget *b = label_create(&th, NULL, "test.a.rather.much.longer.key.here");
    REQUIRE(a && b);
    REQUIRE(panel_add(p, a));
    REQUIRE(panel_add(p, b));

    int aw, ah, bw, bh;
    widget_measure(a, &aw, &ah);
    widget_measure(b, &bw, &bh);

    int pw, ph;
    panel_measure(p, &pw, &ph);

    int expect_w = (aw > bw ? aw : bw) + 2 * 7;
    int expect_h = ah + bh + 3;
    CHECK_EQ(pw, expect_w);
    CHECK_EQ(ph, expect_h);

    rect area = rect_make(5, 5, pw, ph);
    panel_layout(p, area);

    int content_w = pw - 2 * 7;
    CHECK_EQ(a->frame.w, content_w);
    CHECK_EQ(b->frame.w, content_w);
    CHECK(a->frame.w >= aw);
    CHECK(b->frame.w >= bw);
    CHECK_EQ(a->frame.y, area.y);
    CHECK_EQ(b->frame.y, a->frame.y + a->frame.h + 3);
    CHECK_EQ(b->frame.y + b->frame.h, area.y + ph);

    panel_destroy(p);
}

/* --- Fokus: Tab und Umschalt+Tab ---------------------------------------------- */

TEST(tab_moves_forward_through_focusable_items_and_wraps)
{
    theme th = make_theme();
    panel *p = panel_create(&th, NULL);
    REQUIRE(p);
    panel_set_layout(p, LAYOUT_VSTACK, 2, 2);

    widget *lbl = label_create(&th, NULL, "test.label");
    widget *cb  = checkbox_create(&th, NULL, "test.cb", false);
    widget *btn = button_create(&th, NULL, "test.btn", "test.action");
    REQUIRE(lbl && cb && btn);
    REQUIRE(panel_add(p, lbl));
    REQUIRE(panel_add(p, cb));
    REQUIRE(panel_add(p, btn));

    CHECK(panel_focus(p) == NULL);

    event tab = { .kind = EV_KEY_DOWN, .key = KEY_TAB };

    CHECK(panel_event(p, &tab, NULL));
    CHECK(panel_focus(p) == cb);

    CHECK(panel_event(p, &tab, NULL));
    CHECK(panel_focus(p) == btn);

    /* lbl nimmt keinen Fokus an - der Umbruch springt direkt zu cb zurück. */
    CHECK(panel_event(p, &tab, NULL));
    CHECK(panel_focus(p) == cb);

    panel_destroy(p);
}

TEST(shift_tab_moves_backward_through_focusable_items_and_wraps)
{
    theme th = make_theme();
    panel *p = panel_create(&th, NULL);
    REQUIRE(p);
    panel_set_layout(p, LAYOUT_VSTACK, 2, 2);

    widget *lbl = label_create(&th, NULL, "test.label");
    widget *cb  = checkbox_create(&th, NULL, "test.cb", false);
    widget *btn = button_create(&th, NULL, "test.btn", "test.action");
    REQUIRE(lbl && cb && btn);
    REQUIRE(panel_add(p, lbl));
    REQUIRE(panel_add(p, cb));
    REQUIRE(panel_add(p, btn));

    event shift_tab = { .kind = EV_KEY_DOWN, .key = KEY_TAB, .mods = MOD_SHIFT };

    CHECK(panel_event(p, &shift_tab, NULL));
    CHECK(panel_focus(p) == btn);

    CHECK(panel_event(p, &shift_tab, NULL));
    CHECK(panel_focus(p) == cb);

    CHECK(panel_event(p, &shift_tab, NULL));
    CHECK(panel_focus(p) == btn);

    panel_destroy(p);
}

TEST(items_without_wants_focus_are_skipped)
{
    theme th = make_theme();
    panel *p = panel_create(&th, NULL);
    REQUIRE(p);
    panel_set_layout(p, LAYOUT_VSTACK, 2, 2);

    widget *l1 = label_create(&th, NULL, "test.l1");
    widget *cb = checkbox_create(&th, NULL, "test.cb", false);
    widget *l2 = label_create(&th, NULL, "test.l2");
    REQUIRE(l1 && cb && l2);
    REQUIRE(panel_add(p, l1));
    REQUIRE(panel_add(p, cb));
    REQUIRE(panel_add(p, l2));

    panel_focus_next(p);
    CHECK(panel_focus(p) == cb);

    panel_focus_next(p);
    CHECK(panel_focus(p) == cb);   /* einziges fokussierbares Element */

    panel_destroy(p);
}

TEST(disabled_items_are_skipped)
{
    theme th = make_theme();
    panel *p = panel_create(&th, NULL);
    REQUIRE(p);
    panel_set_layout(p, LAYOUT_VSTACK, 2, 2);

    widget *cb1 = checkbox_create(&th, NULL, "test.cb1", false);
    widget *cb2 = checkbox_create(&th, NULL, "test.cb2", false);
    REQUIRE(cb1 && cb2);
    cb2->enabled = false;
    REQUIRE(panel_add(p, cb1));
    REQUIRE(panel_add(p, cb2));

    panel_focus_next(p);
    CHECK(panel_focus(p) == cb1);

    panel_focus_next(p);
    CHECK(panel_focus(p) == cb1);   /* cb2 ist gesperrt */

    panel_destroy(p);
}

TEST(panel_without_focusable_item_does_not_hang_on_tab)
{
    theme th = make_theme();
    panel *p = panel_create(&th, NULL);
    REQUIRE(p);
    panel_set_layout(p, LAYOUT_VSTACK, 2, 2);

    widget *l1 = label_create(&th, NULL, "test.l1");
    widget *l2 = label_create(&th, NULL, "test.l2");
    REQUIRE(l1 && l2);
    REQUIRE(panel_add(p, l1));
    REQUIRE(panel_add(p, l2));

    panel_focus_next(p);
    CHECK(panel_focus(p) == NULL);

    panel_focus_prev(p);
    CHECK(panel_focus(p) == NULL);

    panel_destroy(p);
}

TEST(at_most_one_widget_is_focused)
{
    theme th = make_theme();
    panel *p = panel_create(&th, NULL);
    REQUIRE(p);
    panel_set_layout(p, LAYOUT_VSTACK, 2, 2);

    widget *cb1 = checkbox_create(&th, NULL, "test.cb1", false);
    widget *cb2 = checkbox_create(&th, NULL, "test.cb2", false);
    widget *cb3 = checkbox_create(&th, NULL, "test.cb3", false);
    REQUIRE(cb1 && cb2 && cb3);
    REQUIRE(panel_add(p, cb1));
    REQUIRE(panel_add(p, cb2));
    REQUIRE(panel_add(p, cb3));

    panel_focus_next(p);
    panel_focus_next(p);
    panel_focus_next(p);

    int focused_count = 0;
    for (int i = 0; i < panel_count(p); i++)
        if (panel_at(p, i)->focused) focused_count++;
    CHECK_EQ(focused_count, 1);

    panel_set_focus(p, cb1);
    focused_count = 0;
    for (int i = 0; i < panel_count(p); i++)
        if (panel_at(p, i)->focused) focused_count++;
    CHECK_EQ(focused_count, 1);
    CHECK(cb1->focused);

    panel_destroy(p);
}

/* --- Maus --------------------------------------------------------------------- */

TEST(click_on_widget_sets_focus)
{
    theme th = make_theme();
    panel *p = panel_create(&th, NULL);
    REQUIRE(p);
    panel_set_layout(p, LAYOUT_VSTACK, 2, 2);

    widget *cb1 = checkbox_create(&th, NULL, "test.cb1", false);
    widget *cb2 = checkbox_create(&th, NULL, "test.cb2", false);
    REQUIRE(cb1 && cb2);
    REQUIRE(panel_add(p, cb1));
    REQUIRE(panel_add(p, cb2));

    panel_layout(p, rect_make(0, 0, 200, 200));

    event click = {
        .kind   = EV_MOUSE_DOWN,
        .button = 1,
        .x      = cb2->frame.x + 1,
        .y      = cb2->frame.y + 1,
    };
    CHECK(panel_event(p, &click, NULL));
    CHECK(panel_focus(p) == cb2);

    panel_destroy(p);
}

TEST(click_outside_any_widget_does_not_change_focus)
{
    theme th = make_theme();
    panel *p = panel_create(&th, NULL);
    REQUIRE(p);
    panel_set_layout(p, LAYOUT_VSTACK, 2, 2);

    widget *cb1 = checkbox_create(&th, NULL, "test.cb1", false);
    REQUIRE(cb1);
    REQUIRE(panel_add(p, cb1));

    panel_layout(p, rect_make(0, 0, 200, 200));
    panel_set_focus(p, cb1);

    event click = { .kind = EV_MOUSE_DOWN, .button = 1, .x = -50, .y = -50 };
    CHECK(!panel_event(p, &click, NULL));
    CHECK(panel_focus(p) == cb1);

    panel_destroy(p);
}

/* --- Aktionen: Knöpfe ----------------------------------------------------------
 *
 * widget.h bietet keine Möglichkeit, von einem widget* auf den Aktionsnamen
 * zurückzuschließen, den button_create beim Erzeugen entgegengenommen hat -
 * etwa ein `const char *button_action(const widget *w)`. panel_event kann
 * *out_action deshalb heute nicht befüllen, obwohl ein Knopf feuert (siehe
 * panel.c, dieselbe Stelle). Der folgende Test hält genau diese Grenze fest,
 * statt sie zu verschweigen: der Klick wird verarbeitet, der Aktionsname
 * bleibt aus.
 */
TEST(button_press_reports_its_action_name)
{
    theme th = make_theme();
    panel *p = panel_create(&th, NULL);
    REQUIRE(p);
    panel_set_layout(p, LAYOUT_VSTACK, 2, 2);

    widget *btn = button_create(&th, NULL, "test.btn", "test.action");
    REQUIRE(btn);
    REQUIRE(panel_add(p, btn));

    panel_layout(p, rect_make(0, 0, 200, 200));
    panel_set_focus(p, btn);

    event down = { .kind = EV_MOUSE_DOWN, .button = 1,
                   .x = btn->frame.x + 1, .y = btn->frame.y + 1 };

    const char *action = "unberührt";
    CHECK(panel_event(p, &down, &action));
    CHECK_STR(action, "test.action");

    /* Der Merker ist danach zurückgesetzt: dasselbe Ereignis noch einmal
     * meldet nicht erneut, ohne dass der Knopf wieder gedrückt wurde. */
    action = "unberührt";
    event move = { .kind = EV_MOUSE_MOVE, .x = 999, .y = 999 };
    panel_event(p, &move, &action);
    CHECK(action == NULL);

    panel_destroy(p);
}

/* Das Panel reicht seine Elemente durch, ohne ihre Klassen zu kennen, und
 * fragt nach jedem Ereignis ALLE auf "wurde gedrückt" ab. Diese Abfrage muss
 * eine Beschriftung aushalten - sonst liest sie hinter deren Struktur hinaus.
 * Genau das hat der Sanitizer beim Zusammenbau von M8 gefunden, während der
 * gewöhnliche Bau grün blieb. */
TEST(pressing_query_is_safe_on_non_buttons)
{
    theme th = make_theme();
    panel *p = panel_create(&th, NULL);
    REQUIRE(p);
    panel_set_layout(p, LAYOUT_VSTACK, 2, 2);

    widget *lab = label_create(&th, NULL, "test.label");
    widget *chk = checkbox_create(&th, NULL, "test.check", false);
    widget *btn = button_create(&th, NULL, "test.btn", "test.action");
    REQUIRE(lab && chk && btn);
    REQUIRE(panel_add(p, lab));
    REQUIRE(panel_add(p, chk));
    REQUIRE(panel_add(p, btn));

    CHECK(!button_was_pressed(lab));
    CHECK(!button_was_pressed(chk));
    CHECK(button_action(lab) == NULL);
    CHECK(button_action(chk) == NULL);
    CHECK_STR(button_action(btn), "test.action");

    panel_layout(p, rect_make(0, 0, 200, 200));
    event move = { .kind = EV_MOUSE_MOVE, .x = 5, .y = 5 };
    const char *action = NULL;
    panel_event(p, &move, &action);      /* fragt intern alle drei ab */

    panel_destroy(p);
}


TEST(event_without_trigger_leaves_out_action_null)
{
    theme th = make_theme();
    panel *p = panel_create(&th, NULL);
    REQUIRE(p);
    panel_set_layout(p, LAYOUT_VSTACK, 2, 2);

    widget *btn = button_create(&th, NULL, "test.btn", "test.action");
    REQUIRE(btn);
    REQUIRE(panel_add(p, btn));
    panel_set_focus(p, btn);

    const char *action = "unberührt";
    event       tab     = { .kind = EV_KEY_DOWN, .key = KEY_TAB };
    panel_event(p, &tab, &action);
    CHECK(action == NULL);

    panel_destroy(p);
}

/* --- Ereignisweiterleitung mit dem eigenen Testwidget -------------------------- */

TEST(an_event_sink_widget_consumes_every_forwarded_event)
{
    theme th = make_theme();
    panel *p = panel_create(&th, NULL);
    REQUIRE(p);
    panel_set_layout(p, LAYOUT_VSTACK, 2, 2);

    widget *sink = sink_create(true);
    REQUIRE(sink);
    REQUIRE(panel_add(p, sink));
    panel_set_focus(p, sink);

    event key = { .kind = EV_KEY_DOWN, .key = 'x' };
    CHECK(panel_event(p, &key, NULL));
    CHECK_EQ(*(int *)sink->user, 1);

    CHECK(panel_event(p, &key, NULL));
    CHECK_EQ(*(int *)sink->user, 2);

    panel_destroy(p);
}

int main(void)
{
    RUN(panel_add_and_count_and_at);

    RUN(vstack_stacks_children_with_gap_and_equal_width);
    RUN(hstack_places_children_side_by_side_with_gap_and_equal_height);
    RUN(form_left_column_matches_widest_even_indexed_item);
    RUN(form_allows_odd_count_last_item_spans_full_width);
    RUN(measure_matches_what_layout_needs_for_vstack);

    RUN(tab_moves_forward_through_focusable_items_and_wraps);
    RUN(shift_tab_moves_backward_through_focusable_items_and_wraps);
    RUN(items_without_wants_focus_are_skipped);
    RUN(disabled_items_are_skipped);
    RUN(panel_without_focusable_item_does_not_hang_on_tab);
    RUN(at_most_one_widget_is_focused);

    RUN(click_on_widget_sets_focus);
    RUN(click_outside_any_widget_does_not_change_focus);

    RUN(button_press_reports_its_action_name);
    RUN(pressing_query_is_safe_on_non_buttons);
    RUN(event_without_trigger_leaves_out_action_null);

    RUN(an_event_sink_widget_consumes_every_forwarded_event);

    return test_summary();
}
