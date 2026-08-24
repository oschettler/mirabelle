/* Das Bildlaufmodell aus M8.
 *
 * Reine Arithmetik, deshalb keine Bitmap und kein Sollbild in dieser Datei.
 * Was hier stimmt, stimmt für Liste, Textfeld und Rollbalken gleichermaßen -
 * das ist der Zweck des getrennten Modells.
 */
#include "test.h"

#include "ui/scroll.h"

/* Ein Modell mit Umfang und Seitengröße, value auf 0. */
static scrollmodel model(int total, int page)
{
    scrollmodel m = { 0, 0, 0 };
    scroll_set(&m, total, page);
    return m;
}

/* --- Grenzen ---------------------------------------------------------------- */

TEST(max_leaves_the_last_unit_visible)
{
    scrollmodel m = model(100, 10);
    CHECK_EQ(scroll_max(&m), 90);

    /* Bei value = 90 sind die Einheiten 90 bis 99 zu sehen, die letzte also
     * gerade noch. Ein Schritt weiter wäre eine leere Zeile am Ende. */
    scroll_to(&m, 90);
    CHECK_EQ(m.value + m.page, m.total);
}

TEST(max_is_zero_when_everything_fits)
{
    scrollmodel m = model(5, 10);
    CHECK_EQ(scroll_max(&m), 0);
    CHECK(!scroll_needed(&m));

    scroll_to(&m, 3);
    CHECK_EQ(m.value, 0);
}

TEST(needed_only_when_something_is_hidden)
{
    scrollmodel a = model(10, 10);
    CHECK(!scroll_needed(&a));

    scrollmodel b = model(11, 10);
    CHECK(scroll_needed(&b));
}

TEST(negative_input_becomes_zero)
{
    scrollmodel m = model(-5, -3);
    CHECK_EQ(m.total, 0);
    CHECK_EQ(m.page, 0);
    CHECK_EQ(scroll_max(&m), 0);
    CHECK(!scroll_needed(&m));
}

TEST(moves_clamp_instead_of_wrapping)
{
    scrollmodel m = model(20, 5);

    scroll_by(&m, -1);
    CHECK_EQ(m.value, 0);      /* nicht ans Ende gesprungen */

    scroll_to(&m, 15);
    scroll_by(&m, 1);
    CHECK_EQ(m.value, 15);     /* nicht an den Anfang gesprungen */
}

TEST(pages_move_by_page_size)
{
    scrollmodel m = model(100, 10);

    scroll_pages(&m, 1);
    CHECK_EQ(m.value, 10);
    scroll_pages(&m, 2);
    CHECK_EQ(m.value, 30);
    scroll_pages(&m, -1);
    CHECK_EQ(m.value, 20);
}

TEST(page_move_advances_even_before_the_first_layout)
{
    /* Vor dem ersten Layout ist die Höhe null und damit auch page. Ein Klick
     * in die Rinne muss trotzdem etwas bewirken, sonst hängt die Ansicht. */
    scrollmodel m = model(100, 0);
    scroll_pages(&m, 1);
    CHECK_EQ(m.value, 1);
}

/* --- Umfang ändern ---------------------------------------------------------- */

TEST(set_keeps_the_position_where_it_can)
{
    scrollmodel m = model(100, 10);
    scroll_to(&m, 40);

    scroll_set(&m, 100, 20);   /* Fenster größer gezogen */
    CHECK_EQ(m.value, 40);     /* nicht wieder oben gelandet */
}

TEST(set_pulls_the_position_back_when_content_shrinks)
{
    scrollmodel m = model(100, 10);
    scroll_to(&m, 90);

    scroll_set(&m, 20, 10);
    CHECK_EQ(m.value, 10);     /* das neue Maximum */
}

/* --- Auswahl im Blick behalten ---------------------------------------------- */

TEST(reveal_scrolls_up_to_an_index_above_the_view)
{
    scrollmodel m = model(100, 10);
    scroll_to(&m, 50);

    CHECK(scroll_reveal(&m, 20));
    CHECK_EQ(m.value, 20);     /* der Index steht ganz oben */
}

TEST(reveal_scrolls_down_just_far_enough)
{
    scrollmodel m = model(100, 10);

    CHECK(scroll_reveal(&m, 12));
    CHECK_EQ(m.value, 3);      /* 3 bis 12 - der Index ist die letzte Zeile */
}

TEST(reveal_leaves_a_visible_index_alone)
{
    scrollmodel m = model(100, 10);
    scroll_to(&m, 30);

    CHECK(!scroll_reveal(&m, 35));
    CHECK_EQ(m.value, 30);

    /* Auch die Ränder des Sichtfensters gelten als sichtbar. */
    CHECK(!scroll_reveal(&m, 30));
    CHECK(!scroll_reveal(&m, 39));
    CHECK_EQ(m.value, 30);
}

TEST(reveal_moves_by_one_for_the_first_hidden_index)
{
    /* Bei value = 30 und zehn Zeilen sind 30 bis 39 zu sehen. Der Index 40 ist
     * der erste, der es nicht mehr ist, und genau an dieser Kante entscheidet
     * sich, ob die Grenze richtig gezogen ist: 39 darf nichts bewegen, 40 muss
     * um genau eine Zeile weiterrücken. */
    scrollmodel m = model(100, 10);
    scroll_to(&m, 30);

    CHECK(scroll_reveal(&m, 40));
    CHECK_EQ(m.value, 31);
}

TEST(reveal_on_empty_content_does_nothing)
{
    scrollmodel m = model(0, 10);
    CHECK(!scroll_reveal(&m, 5));
    CHECK_EQ(m.value, 0);
}

/* --- Der Schieber ------------------------------------------------------------ */

TEST(thumb_length_shows_the_visible_share)
{
    scrollmodel m = model(100, 25);   /* ein Viertel ist zu sehen */

    int pos, len;
    scroll_thumb(&m, 200, 8, &pos, &len);
    CHECK_EQ(len, 50);                /* ein Viertel der Rinne */
    CHECK_EQ(pos, 0);
}

TEST(thumb_fills_the_track_when_nothing_scrolls)
{
    scrollmodel m = model(5, 10);

    int pos, len;
    scroll_thumb(&m, 200, 8, &pos, &len);
    CHECK_EQ(pos, 0);
    CHECK_EQ(len, 200);
}

TEST(thumb_length_rounds_instead_of_truncating)
{
    /* Zwei von sieben Einheiten auf einer Rinne von 100 Pixeln sind 28,57
     * Pixel. Abgeschnitten wären es 28. Über eine einzelne Länge ist das
     * gleichgültig, aber dieselbe Rechnung trägt auch die Schieberposition,
     * und dort summiert sich das Abschneiden zu einem sichtbaren Versatz. */
    scrollmodel m = model(7, 2);

    int len;
    scroll_thumb(&m, 100, 8, NULL, &len);
    CHECK_EQ(len, 29);
}

TEST(thumb_stays_grabbable_in_a_very_long_list)
{
    scrollmodel m = model(10000, 10);

    int len;
    scroll_thumb(&m, 100, 16, NULL, &len);
    CHECK_EQ(len, 16);                /* rechnerisch wäre er 0 Pixel hoch */
}

TEST(thumb_reaches_both_ends_of_the_track)
{
    scrollmodel m = model(100, 25);
    int track = 200, min = 8, pos, len;

    scroll_thumb(&m, track, min, &pos, &len);
    CHECK_EQ(pos, 0);

    scroll_to(&m, scroll_max(&m));
    scroll_thumb(&m, track, min, &pos, &len);
    CHECK_EQ(pos + len, track);       /* bündig am unteren Ende */
}

TEST(thumb_position_grows_with_the_value)
{
    scrollmodel m = model(100, 10);
    int last = -1;

    for (int v = 0; v <= scroll_max(&m); v++) {
        scroll_to(&m, v);
        int pos;
        scroll_thumb(&m, 180, 16, &pos, NULL);
        CHECK(pos >= last);
        last = pos;
    }
}

TEST(thumb_on_an_unmeasured_track_is_empty)
{
    scrollmodel m = model(100, 10);

    int pos = 7, len = 7;
    scroll_thumb(&m, 0, 16, &pos, &len);
    CHECK_EQ(pos, 0);
    CHECK_EQ(len, 0);
}

/* --- Ziehen ------------------------------------------------------------------ */

TEST(dragging_to_the_ends_gives_the_extreme_values)
{
    scrollmodel m = model(100, 10);
    int track = 180, min = 16;
    int len;
    scroll_thumb(&m, track, min, NULL, &len);

    CHECK_EQ(scroll_value_at(&m, track, min, 0), 0);
    CHECK_EQ(scroll_value_at(&m, track, min, track - len), scroll_max(&m));
}

TEST(dragging_past_the_ends_clamps)
{
    scrollmodel m = model(100, 10);

    CHECK_EQ(scroll_value_at(&m, 180, 16, -50), 0);
    CHECK_EQ(scroll_value_at(&m, 180, 16, 999), scroll_max(&m));
}

TEST(drag_and_draw_agree_when_the_track_is_long_enough)
{
    /* 40 mögliche Werte auf einer Rinne, die deutlich mehr Pixel hat: dann
     * muss jeder Wert seine eigene Schieberposition haben und aus ihr wieder
     * hervorgehen. Wäre eine der beiden Richtungen schief gerundet, driftete
     * der Schieber unter dem Zeiger weg. */
    scrollmodel m = model(50, 10);
    int track = 300, min = 16;

    for (int v = 0; v <= scroll_max(&m); v++) {
        scroll_to(&m, v);
        int pos;
        scroll_thumb(&m, track, min, &pos, NULL);
        CHECK_EQ(scroll_value_at(&m, track, min, pos), v);
    }
}

TEST(drag_and_draw_agree_on_numbers_that_do_not_divide_evenly)
{
    /* Wie oben, aber mit Zahlen, die nirgends glatt aufgehen: 7 von 30
     * Einheiten auf 137 Pixeln. Geht eine der beiden Richtungen zur falschen
     * Seite, driftet der Schieber unter dem Zeiger weg - mit glatten Zahlen
     * fiele das nicht auf, weil dort beide Rundungsarten dasselbe liefern. */
    scrollmodel m = model(30, 7);
    int track = 137, min = 10;

    for (int v = 0; v <= scroll_max(&m); v++) {
        scroll_to(&m, v);
        int pos;
        scroll_thumb(&m, track, min, &pos, NULL);
        CHECK_EQ(scroll_value_at(&m, track, min, pos), v);
    }
}

TEST(dragging_a_track_that_cannot_move_keeps_the_value)
{
    /* Untergrenze so groß wie die Rinne: der Schieber füllt sie ganz und kann
     * nichts mehr ausdrücken. Dann darf ein Zug nicht an den Anfang werfen. */
    scrollmodel m = model(100, 10);
    scroll_to(&m, 42);
    CHECK_EQ(scroll_value_at(&m, 40, 40, 0), 42);
}

TEST(dragging_without_content_gives_zero)
{
    scrollmodel m = model(5, 10);
    CHECK_EQ(scroll_value_at(&m, 180, 16, 90), 0);
}

int main(void)
{
    RUN(max_leaves_the_last_unit_visible);
    RUN(max_is_zero_when_everything_fits);
    RUN(needed_only_when_something_is_hidden);
    RUN(negative_input_becomes_zero);
    RUN(moves_clamp_instead_of_wrapping);
    RUN(pages_move_by_page_size);
    RUN(page_move_advances_even_before_the_first_layout);

    RUN(set_keeps_the_position_where_it_can);
    RUN(set_pulls_the_position_back_when_content_shrinks);

    RUN(reveal_scrolls_up_to_an_index_above_the_view);
    RUN(reveal_scrolls_down_just_far_enough);
    RUN(reveal_leaves_a_visible_index_alone);
    RUN(reveal_moves_by_one_for_the_first_hidden_index);
    RUN(reveal_on_empty_content_does_nothing);

    RUN(thumb_length_shows_the_visible_share);
    RUN(thumb_fills_the_track_when_nothing_scrolls);
    RUN(thumb_length_rounds_instead_of_truncating);
    RUN(thumb_stays_grabbable_in_a_very_long_list);
    RUN(thumb_reaches_both_ends_of_the_track);
    RUN(thumb_position_grows_with_the_value);
    RUN(thumb_on_an_unmeasured_track_is_empty);

    RUN(dragging_to_the_ends_gives_the_extreme_values);
    RUN(dragging_past_the_ends_clamps);
    RUN(drag_and_draw_agree_when_the_track_is_long_enough);
    RUN(drag_and_draw_agree_on_numbers_that_do_not_divide_evenly);
    RUN(dragging_a_track_that_cannot_move_keeps_the_value);
    RUN(dragging_without_content_gives_zero);

    return test_summary();
}
