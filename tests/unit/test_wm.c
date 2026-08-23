/* Die Fensterverwaltung aus M6: Themadatei, z-Ordnung, Trefferprüfung,
 * Ziehen und das Zusammenspiel als Bild.
 *
 * plat.h wird nur für die Ereignisstruktur gebraucht, nicht für eine
 * Plattform - es wird nirgends plat_init() oder dergleichen aufgerufen.
 * Trefferpunkte werden aus den Themamaßen errechnet statt fest verdrahtet,
 * damit die Tests nicht heimlich die Umsetzung nachbauen, sondern denselben
 * Vertrag prüfen, den auch DESIGN.md beschreibt.
 */

#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx/bitmap.h"
#include "gfx/draw.h"
#include "plat/plat.h"
#include "support/golden.h"
#include "ui/theme.h"
#include "ui/wm.h"
#include "ui/window.h"

#ifndef PDA_DATA_DIR
#define PDA_DATA_DIR "data"
#endif

/* --- Thema laden -------------------------------------------------------- */

static theme load_test_theme(void)
{
    theme th;
    char  path[512], err[256] = "";

    snprintf(path, sizeof path, "%s/themes/desktop.theme", PDA_DATA_DIR);
    if (!theme_load(&th, path, err, sizeof err)) {
        printf("  Thema nicht ladbar: %s\n", err);
        theme_defaults(&th);
    }
    return th;
}

/* --- Punkte aus den Themamaßen errechnen, statt sie fest zu verdrahten --- */

static int close_box_center_x(const theme *th, rect frame)
{
    return frame.x + th->box_margin + th->close_box / 2;
}

static int close_box_center_y(const theme *th, rect frame)
{
    return frame.y + (th->titlebar_h - th->close_box) / 2 + th->close_box / 2;
}

static int grow_box_center_x(const theme *th, rect frame)
{
    return frame.x + frame.w - th->box_margin - th->grow_box / 2;
}

static int grow_box_center_y(const theme *th, rect frame)
{
    return frame.y + frame.h - th->box_margin - th->grow_box / 2;
}

static int titlebar_center_x(rect frame)
{
    return frame.x + frame.w / 2;
}

static int titlebar_center_y(const theme *th, rect frame)
{
    return frame.y + th->titlebar_h / 2;
}

static int content_point_x(rect frame)
{
    return frame.x + frame.w / 2;
}

static int content_point_y(const theme *th, rect frame)
{
    return frame.y + th->titlebar_h + 10;
}

/* --- Hilfsfunktionen für temporäre Dateien, wie in test_keymap.c --------- */

static void make_temp_path(char *buf, size_t bufsize, const char *name)
{
    const char *dir = getenv("TMPDIR");
    if (!dir || !*dir) dir = "/tmp";

    size_t len = strlen(dir);
    if (len > 0 && dir[len - 1] == '/')
        snprintf(buf, bufsize, "%s%s", dir, name);
    else
        snprintf(buf, bufsize, "%s/%s", dir, name);
}

static bool write_text_file(const char *path, const char *content)
{
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    bool ok = fputs(content, f) >= 0;
    return fclose(f) == 0 && ok;
}

static bool errbuf_has_location(const char *err, const char *path, int line)
{
    char prefix[300];
    snprintf(prefix, sizeof prefix, "%s:%d:", path, line);
    return strstr(err, prefix) != NULL;
}

/* --- theme_load ----------------------------------------------------------- */

TEST(theme_load_reads_real_file)
{
    theme th;
    char  path[512], err[256] = "";
    snprintf(path, sizeof path, "%s/themes/desktop.theme", PDA_DATA_DIR);

    CHECK(theme_load(&th, path, err, sizeof err));
    CHECK_EQ(err[0], '\0');
    CHECK_EQ(th.titlebar_h, 20);
    CHECK_EQ(th.border, 1);
    CHECK_EQ(th.close_box, 11);
    CHECK_EQ(th.grow_box, 15);
    CHECK_EQ(th.box_margin, 5);
    CHECK_EQ(th.stripe_gap, 2);
    CHECK_EQ(th.title_pad, 6);
    CHECK_EQ(th.hit_slop, 0);
    CHECK_EQ(th.min_w, 96);
    CHECK_EQ(th.min_h, 48);
    CHECK_STR(th.font, "system12");
}

TEST(theme_load_rejects_unknown_name)
{
    char path[256];
    make_temp_path(path, sizeof path, "pda_test_theme_unknown.theme");

    const char *content =
        "titlebar_h   20\n"
        "unsinn       5\n";
    CHECK(write_text_file(path, content));

    theme th;
    char  err[256];
    CHECK(!theme_load(&th, path, err, sizeof err));
    CHECK(errbuf_has_location(err, path, 2));

    remove(path);
}

TEST(theme_load_rejects_unreadable_number)
{
    char path[256];
    make_temp_path(path, sizeof path, "pda_test_theme_badnum.theme");

    const char *content =
        "titlebar_h   20\n"
        "border       eins\n";
    CHECK(write_text_file(path, content));

    theme th;
    char  err[256];
    CHECK(!theme_load(&th, path, err, sizeof err));
    CHECK(errbuf_has_location(err, path, 2));

    remove(path);
}

/* --- Öffnen, z-Ordnung, Aktivierung --------------------------------------- */

TEST(wm_open_activates_and_orders)
{
    theme th = load_test_theme();
    wm   *m  = wm_create(&th, 400, 300);
    REQUIRE(m);

    window *a = wm_open(m, rect_make(10, 10, 100, 60), "A", WIN_NORMAL);
    REQUIRE(a);
    CHECK(wm_active(m) == a);
    CHECK_EQ(wm_count(m), 1);
    CHECK(wm_at_z(m, 0) == a);

    window *b = wm_open(m, rect_make(150, 10, 100, 60), "B", WIN_NORMAL);
    REQUIRE(b);
    CHECK(wm_active(m) == b);
    CHECK_EQ(wm_count(m), 2);
    CHECK(wm_at_z(m, 0) == a);
    CHECK(wm_at_z(m, 1) == b);
    CHECK(wm_at_z(m, 2) == NULL);

    wm_destroy(m);
}

TEST(click_on_back_window_raises_it)
{
    theme th = load_test_theme();
    wm   *m  = wm_create(&th, 400, 300);
    REQUIRE(m);

    rect    fa = rect_make(10, 10, 100, 60);
    window *a  = wm_open(m, fa, "A", WIN_NORMAL);
    window *b  = wm_open(m, rect_make(150, 10, 100, 60), "B", WIN_NORMAL);
    REQUIRE(a && b);
    CHECK(wm_active(m) == b);

    event e = { .kind = EV_MOUSE_DOWN, .button = 1,
                .x = content_point_x(fa), .y = content_point_y(&th, fa) };
    bool consumed = wm_event(m, &e);

    CHECK(consumed);   /* A war nicht aktiv: der Klick hat nur aktiviert */
    CHECK(wm_active(m) == a);
    CHECK(wm_at_z(m, 1) == a);
    CHECK(wm_at_z(m, 0) == b);

    wm_destroy(m);
}

/* --- wm_hit ---------------------------------------------------------------- */

/* Die definierende Eigenschaft überlappender Fenster: im gemeinsamen Bereich
 * trifft der Klick das vordere. Ohne diesen Test bliebe unbemerkt, wenn die
 * Trefferprüfung in der falschen Richtung liefe - gezeichnet wird von hinten
 * nach vorn, getroffen aber von vorn nach hinten. */
/* Der Eigentümer muss erfahren, wenn sein Fenster verschwindet - die
 * Verwaltung schließt es beim Klick auf das Schließfeld selbst. Ohne diese
 * Rückmeldung zeigt ein Anwendungszeiger danach ins Leere. */
static window *g_closed;
static int     g_closed_count;

static void note_close(window *w, void *user)
{
    g_closed = w;
    *(int *)user += 1;
}

TEST(close_notifies_the_owner)
{
    theme th = load_test_theme();
    wm   *m  = wm_create(&th, 400, 300);
    REQUIRE(m);

    window *w = wm_open(m, rect_make(10, 10, 120, 90), "A", WIN_NORMAL);
    REQUIRE(w);

    g_closed       = NULL;
    g_closed_count = 0;
    window_set_on_close(w, note_close, &g_closed_count);

    wm_close(m, w);
    CHECK(g_closed == w);
    CHECK_EQ(g_closed_count, 1);
    CHECK_EQ(wm_count(m), 0);

    wm_destroy(m);
}

/* Auch beim Abräumen der ganzen Verwaltung, sonst müsste der Eigentümer zwei
 * verschiedene Wege kennen. */
TEST(destroy_notifies_owners_of_remaining_windows)
{
    theme th = load_test_theme();
    wm   *m  = wm_create(&th, 400, 300);
    REQUIRE(m);

    int count = 0;
    window *a = wm_open(m, rect_make(10, 10, 100, 60), "A", WIN_NORMAL);
    window *b = wm_open(m, rect_make(50, 50, 100, 60), "B", WIN_NORMAL);
    REQUIRE(a && b);
    window_set_on_close(a, note_close, &count);
    window_set_on_close(b, note_close, &count);

    wm_destroy(m);
    CHECK_EQ(count, 2);
}

/* Der Weg, an dem es wirklich krachte: Klick auf das Schließfeld. */
TEST(close_box_click_notifies_the_owner)
{
    theme th = load_test_theme();
    wm   *m  = wm_create(&th, 400, 300);
    REQUIRE(m);

    rect    fa = rect_make(20, 20, 160, 120);
    window *w  = wm_open(m, fa, "A", WIN_NORMAL);
    REQUIRE(w);

    int count = 0;
    g_closed  = NULL;
    window_set_on_close(w, note_close, &count);

    event e = { .kind = EV_MOUSE_DOWN, .button = 1,
                .x = close_box_center_x(&th, fa),
                .y = close_box_center_y(&th, fa) };
    CHECK(wm_event(m, &e));

    CHECK(g_closed == w);
    CHECK_EQ(count, 1);
    CHECK_EQ(wm_count(m), 0);

    wm_destroy(m);
}

TEST(overlap_is_hit_front_to_back)
{
    theme th = load_test_theme();
    wm   *m  = wm_create(&th, 400, 300);
    REQUIRE(m);

    rect    fa = rect_make(20, 20, 160, 120);
    rect    fb = rect_make(100, 60, 160, 120);   /* überlappt a */
    window *a  = wm_open(m, fa, "A", WIN_NORMAL);
    window *b  = wm_open(m, fb, "B", WIN_NORMAL);
    REQUIRE(a && b);

    /* Ein Punkt, der in beiden Rahmen liegt. */
    int x = 130, y = 100;
    CHECK(rect_contains(fa, x, y));
    CHECK(rect_contains(fb, x, y));

    hit_part part = HIT_NONE;
    CHECK(wm_hit(m, x, y, &part) == b);

    /* Nach dem Hervorholen von a trifft derselbe Punkt a. */
    wm_activate(m, a);
    CHECK(wm_hit(m, x, y, &part) == a);

    /* Und ein Punkt, der nur in b liegt, trifft weiterhin b. */
    CHECK(wm_hit(m, 250, 170, &part) == b);

    wm_destroy(m);
}

/* Gezeichnet wird umgekehrt: das vordere Fenster überdeckt das hintere. */
TEST(overlap_is_drawn_back_to_front)
{
    theme th = load_test_theme();
    wm   *m  = wm_create(&th, 200, 150);
    REQUIRE(m);

    window *a = wm_open(m, rect_make(10, 10, 120, 90), "A", WIN_NORMAL);
    window *b = wm_open(m, rect_make(60, 40, 120, 90), "B", WIN_NORMAL);
    REQUIRE(a && b);

    bitmap fb;
    REQUIRE(bitmap_init(&fb, 200, 150));
    gc g;
    gc_init(&g, &fb);
    wm_draw(m, &g);

    /* Entscheidend ist der Bereich, in dem sich beide überdecken: dort liegt
     * die gestreifte Titelleiste von b über dem weißen Inhalt von a. Wird in
     * der falschen Richtung gezeichnet, steht dort a und der Bereich ist leer.
     * Deshalb wird nur innerhalb von a gemessen, nicht daneben. */
    int ink = 0, cells = 0;
    for (int x = 70; x < 125; x++) {
        for (int y = 43; y < 40 + th.titlebar_h - 3; y++) {
            ink += bitmap_get(&fb, x, y);
            cells++;
        }
    }

    /* Gestreift heißt: ungefähr jede zweite Zeile ist schwarz. */
    CHECK(cells > 300);
    CHECK(ink > cells / 4);

    bitmap_free(&fb);
    wm_destroy(m);
}

TEST(wm_hit_returns_correct_parts)
{
    theme th = load_test_theme();
    wm   *m  = wm_create(&th, 400, 300);
    REQUIRE(m);

    rect    frame = rect_make(10, 10, 150, 100);
    window *w     = wm_open(m, frame, "Fenster", WIN_NORMAL);
    REQUIRE(w);

    hit_part part;
    window  *hit;

    hit = wm_hit(m, titlebar_center_x(frame), titlebar_center_y(&th, frame), &part);
    CHECK(hit == w);
    CHECK_EQ(part, HIT_TITLEBAR);

    hit = wm_hit(m, close_box_center_x(&th, frame), close_box_center_y(&th, frame), &part);
    CHECK(hit == w);
    CHECK_EQ(part, HIT_CLOSE_BOX);

    hit = wm_hit(m, grow_box_center_x(&th, frame), grow_box_center_y(&th, frame), &part);
    CHECK(hit == w);
    CHECK_EQ(part, HIT_GROW_BOX);

    hit = wm_hit(m, content_point_x(frame), content_point_y(&th, frame), &part);
    CHECK(hit == w);
    CHECK_EQ(part, HIT_CONTENT);

    wm_destroy(m);
}

TEST(fields_only_hit_when_active)
{
    theme th = load_test_theme();
    wm   *m  = wm_create(&th, 400, 300);
    REQUIRE(m);

    rect    fa = rect_make(10, 10, 150, 100);
    window *a  = wm_open(m, fa, "A", WIN_NORMAL);
    REQUIRE(a);
    /* B liegt weit weg von A, deckt dessen Felder also nicht ab - macht A
     * aber inaktiv. */
    window *b = wm_open(m, rect_make(300, 300, 120, 80), "B", WIN_NORMAL);
    REQUIRE(b);
    CHECK(wm_active(m) == b);   /* B ist jetzt aktiv, A nicht mehr */

    hit_part part;

    window *hit = wm_hit(m, close_box_center_x(&th, fa), close_box_center_y(&th, fa), &part);
    CHECK(hit == a);
    CHECK(part != HIT_CLOSE_BOX);
    CHECK_EQ(part, HIT_TITLEBAR);   /* das Feld zählt nicht, die Titelleiste schon */

    hit = wm_hit(m, grow_box_center_x(&th, fa), grow_box_center_y(&th, fa), &part);
    CHECK(hit == a);
    CHECK(part != HIT_GROW_BOX);
    CHECK_EQ(part, HIT_CONTENT);

    wm_destroy(m);
}

TEST(hit_slop_grows_hit_area)
{
    theme plain = load_test_theme();
    theme slop  = plain;
    slop.hit_slop = 6;

    wm *m1 = wm_create(&plain, 400, 300);
    wm *m2 = wm_create(&slop, 400, 300);
    REQUIRE(m1 && m2);

    rect    frame = rect_make(10, 10, 150, 100);
    window *w1    = wm_open(m1, frame, "A", WIN_CLOSABLE);
    window *w2    = wm_open(m2, frame, "A", WIN_CLOSABLE);
    REQUIRE(w1 && w2);

    /* Knapp links neben dem gezeichneten Schließfeld. */
    int x = frame.x + plain.box_margin - 3;
    int y = close_box_center_y(&plain, frame);

    hit_part part;
    window  *hit1 = wm_hit(m1, x, y, &part);
    CHECK(hit1 == w1);
    CHECK(part != HIT_CLOSE_BOX);   /* ohne hit_slop kein Treffer auf das Feld */

    window *hit2 = wm_hit(m2, x, y, &part);
    CHECK(hit2 == w2);
    CHECK_EQ(part, HIT_CLOSE_BOX);   /* mit hit_slop=6 schon */

    wm_destroy(m1);
    wm_destroy(m2);
}

TEST(click_close_box_closes_window)
{
    theme th = load_test_theme();
    wm   *m  = wm_create(&th, 400, 300);
    REQUIRE(m);

    window *a  = wm_open(m, rect_make(10, 10, 100, 60), "A", WIN_NORMAL);
    rect    fb = rect_make(150, 10, 100, 60);
    window *b  = wm_open(m, fb, "B", WIN_NORMAL);
    REQUIRE(a && b);
    CHECK(wm_active(m) == b);

    event e = { .kind = EV_MOUSE_DOWN, .button = 1,
                .x = close_box_center_x(&th, fb), .y = close_box_center_y(&th, fb) };
    CHECK(wm_event(m, &e));

    CHECK_EQ(wm_count(m), 1);
    CHECK(wm_active(m) == a);
    CHECK(wm_at_z(m, 0) == a);

    wm_destroy(m);
}

TEST(click_in_content_of_active_window_returns_false)
{
    theme th = load_test_theme();
    wm   *m  = wm_create(&th, 400, 300);
    REQUIRE(m);

    rect    fa = rect_make(10, 10, 100, 60);
    window *a  = wm_open(m, fa, "A", WIN_NORMAL);
    REQUIRE(a);
    CHECK(wm_active(m) == a);   /* wm_open aktiviert sofort */

    event e = { .kind = EV_MOUSE_DOWN, .button = 1,
                .x = content_point_x(fa), .y = content_point_y(&th, fa) };
    bool consumed = wm_event(m, &e);

    CHECK(!consumed);   /* A war schon aktiv: die Anwendung soll den Klick sehen */

    wm_destroy(m);
}

/* --- Ziehen: Verschieben und Vergrößern ------------------------------------ */

TEST(drag_move_commits_only_on_mouse_up)
{
    theme th = load_test_theme();
    wm   *m  = wm_create(&th, 400, 300);
    REQUIRE(m);

    rect    frame = rect_make(10, 10, 150, 100);
    window *a     = wm_open(m, frame, "A", WIN_MOVABLE);
    REQUIRE(a);

    int tx = titlebar_center_x(frame);
    int ty = titlebar_center_y(&th, frame);

    event down = { .kind = EV_MOUSE_DOWN, .button = 1, .x = tx, .y = ty };
    CHECK(wm_event(m, &down));
    CHECK(wm_is_dragging(m));

    rect still = window_frame(a);
    CHECK_EQ(still.x, frame.x);
    CHECK_EQ(still.y, frame.y);

    event move = { .kind = EV_MOUSE_MOVE, .x = tx + 15, .y = ty + 30 };
    CHECK(wm_event(m, &move));
    CHECK(wm_is_dragging(m));

    /* Während des Ziehens bewegt sich das Fenster nicht - nur der Umriss. */
    rect still2 = window_frame(a);
    CHECK_EQ(still2.x, frame.x);
    CHECK_EQ(still2.y, frame.y);

    event up = { .kind = EV_MOUSE_UP, .x = tx + 15, .y = ty + 30 };
    CHECK(wm_event(m, &up));
    CHECK(!wm_is_dragging(m));

    rect moved = window_frame(a);
    CHECK_EQ(moved.x, frame.x + 15);
    CHECK_EQ(moved.y, frame.y + 30);
    CHECK_EQ(moved.w, frame.w);
    CHECK_EQ(moved.h, frame.h);

    wm_destroy(m);
}

TEST(drag_move_keeps_titlebar_onscreen)
{
    theme th = load_test_theme();
    wm   *m  = wm_create(&th, 100, 80);
    REQUIRE(m);

    rect    frame = rect_make(10, 10, 60, 40);
    window *a     = wm_open(m, frame, "A", WIN_MOVABLE);
    REQUIRE(a);

    int tx = titlebar_center_x(frame);
    int ty = titlebar_center_y(&th, frame);

    event down = { .kind = EV_MOUSE_DOWN, .button = 1, .x = tx, .y = ty };
    CHECK(wm_event(m, &down));
    event move = { .kind = EV_MOUSE_MOVE, .x = tx - 500, .y = ty - 500 };
    CHECK(wm_event(m, &move));
    event up = { .kind = EV_MOUSE_UP, .x = tx - 500, .y = ty - 500 };
    CHECK(wm_event(m, &up));

    rect r1 = window_frame(a);
    CHECK_EQ(r1.x, 0);
    CHECK_EQ(r1.y, 0);

    /* Und in die andere Richtung: weit über den rechten/unteren Rand hinaus. */
    int tx2 = titlebar_center_x(r1);
    int ty2 = titlebar_center_y(&th, r1);

    event down2 = { .kind = EV_MOUSE_DOWN, .button = 1, .x = tx2, .y = ty2 };
    CHECK(wm_event(m, &down2));
    event move2 = { .kind = EV_MOUSE_MOVE, .x = tx2 + 500, .y = ty2 + 500 };
    CHECK(wm_event(m, &move2));
    event up2 = { .kind = EV_MOUSE_UP, .x = tx2 + 500, .y = ty2 + 500 };
    CHECK(wm_event(m, &up2));

    rect r2 = window_frame(a);
    CHECK_EQ(r2.x, 100 - frame.w);
    CHECK_EQ(r2.y, 80 - th.titlebar_h);

    wm_destroy(m);
}

TEST(drag_resize_enforces_min_size)
{
    theme th = load_test_theme();
    wm   *m  = wm_create(&th, 400, 300);
    REQUIRE(m);

    rect    frame = rect_make(10, 10, 150, 100);
    window *a     = wm_open(m, frame, "A", WIN_RESIZABLE);
    REQUIRE(a);

    int gx = grow_box_center_x(&th, frame);
    int gy = grow_box_center_y(&th, frame);

    event down = { .kind = EV_MOUSE_DOWN, .button = 1, .x = gx, .y = gy };
    CHECK(wm_event(m, &down));
    CHECK(wm_is_dragging(m));

    /* Weit ins Negative ziehen: die Mindestgröße greift. */
    event move = { .kind = EV_MOUSE_MOVE, .x = gx - 200, .y = gy - 200 };
    CHECK(wm_event(m, &move));
    event up = { .kind = EV_MOUSE_UP, .x = gx - 200, .y = gy - 200 };
    CHECK(wm_event(m, &up));

    rect r = window_frame(a);
    CHECK_EQ(r.w, th.min_w);
    CHECK_EQ(r.h, th.min_h);
    CHECK_EQ(r.x, frame.x);   /* die Ecke oben links bleibt fest */
    CHECK_EQ(r.y, frame.y);

    wm_destroy(m);
}

/* --- Modale Fenster --------------------------------------------------------- */

TEST(modal_window_swallows_clicks_outside)
{
    theme th = load_test_theme();
    wm   *m  = wm_create(&th, 400, 300);
    REQUIRE(m);

    rect    fa = rect_make(10, 10, 150, 100);
    window *a  = wm_open(m, fa, "A", WIN_NORMAL);
    window *b  = wm_open(m, rect_make(200, 10, 100, 80), "B", WIN_MODAL);
    REQUIRE(a && b);
    CHECK(wm_active(m) == b);

    int cx = content_point_x(fa);
    int cy = content_point_y(&th, fa);

    hit_part part;
    CHECK(wm_hit(m, cx, cy, &part) == NULL);
    CHECK_EQ(part, HIT_NONE);

    event e = { .kind = EV_MOUSE_DOWN, .button = 1, .x = cx, .y = cy };
    CHECK(wm_event(m, &e));   /* verschluckt */

    CHECK(wm_active(m) == b);   /* unverändert */
    CHECK_EQ(wm_count(m), 2);

    wm_destroy(m);
}

/* --- Sollbilder -------------------------------------------------------------- */

TEST(golden_wm_three_windows)
{
    theme th = load_test_theme();
    wm   *m  = wm_create(&th, 190, 140);
    REQUIRE(m);

    wm_open(m, rect_make(10, 10, 100, 70), "Eins", WIN_NORMAL);
    wm_open(m, rect_make(40, 30, 100, 70), "Zwei", WIN_NORMAL);
    wm_open(m, rect_make(70, 50, 100, 70), "Drei", WIN_NORMAL);

    bitmap fb;
    REQUIRE(bitmap_init(&fb, 190, 140));
    gc g;
    gc_init(&g, &fb);
    wm_draw(m, &g);

    CHECK(golden_check("wm_three_windows", &fb));

    bitmap_free(&fb);
    wm_destroy(m);
}

TEST(golden_wm_drag_outline)
{
    theme th = load_test_theme();
    wm   *m  = wm_create(&th, 150, 100);
    REQUIRE(m);

    rect    frame = rect_make(10, 10, 80, 50);
    window *w     = wm_open(m, frame, "Zieh", WIN_MOVABLE);
    REQUIRE(w);

    int tx = titlebar_center_x(frame);
    int ty = titlebar_center_y(&th, frame);

    event down = { .kind = EV_MOUSE_DOWN, .button = 1, .x = tx, .y = ty };
    wm_event(m, &down);
    event move = { .kind = EV_MOUSE_MOVE, .x = tx + 30, .y = ty + 20 };
    wm_event(m, &move);
    REQUIRE(wm_is_dragging(m));

    bitmap fb;
    REQUIRE(bitmap_init(&fb, 150, 100));
    gc g;
    gc_init(&g, &fb);
    wm_draw(m, &g);

    CHECK(golden_check("wm_drag_outline", &fb));

    bitmap_free(&fb);
    wm_destroy(m);
}

int main(void)
{
    RUN(theme_load_reads_real_file);
    RUN(theme_load_rejects_unknown_name);
    RUN(theme_load_rejects_unreadable_number);

    RUN(wm_open_activates_and_orders);
    RUN(click_on_back_window_raises_it);

    RUN(close_notifies_the_owner);
    RUN(destroy_notifies_owners_of_remaining_windows);
    RUN(close_box_click_notifies_the_owner);
    RUN(overlap_is_hit_front_to_back);
    RUN(overlap_is_drawn_back_to_front);
    RUN(wm_hit_returns_correct_parts);
    RUN(fields_only_hit_when_active);
    RUN(hit_slop_grows_hit_area);
    RUN(click_close_box_closes_window);
    RUN(click_in_content_of_active_window_returns_false);

    RUN(drag_move_commits_only_on_mouse_up);
    RUN(drag_move_keeps_titlebar_onscreen);
    RUN(drag_resize_enforces_min_size);

    RUN(modal_window_swallows_clicks_outside);

    RUN(golden_wm_three_windows);
    RUN(golden_wm_drag_outline);

    return test_summary();
}
