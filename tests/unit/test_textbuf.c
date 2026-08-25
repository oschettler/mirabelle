/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Das Textmodell aus M8: Puffer, Schreibmarke, Auswahl, Bewegen, Widerrufen.
 *
 * Der wichtigste Teil ist nicht, dass Text ankommt, sondern dass die
 * Schreibmarke nach jeder Änderung an der richtigen Stelle steht - deshalb
 * prüfen die meisten Tests hier Byte-Versätze, nicht nur den Inhalt.
 */

#include "test.h"

#include "ui/textbuf.h"

#include <string.h>

static textbuf *make(void)
{
    textbuf *tb = textbuf_create();
    return tb;
}

/* --- UTF-8 und Schreibmarke ------------------------------------------------- */

TEST(move_left_five_times_returns_to_start_after_umlaut_insert)
{
    textbuf *tb = make();
    REQUIRE(tb);

    textbuf_insert(tb, "Grüße");
    CHECK_EQ(textbuf_cursor(tb), 7);   /* G,r=1 Byte, ü,ß=2 Byte, e=1 Byte */

    for (int i = 0; i < 5; i++)
        textbuf_move_cursor(tb, MOVE_LEFT, false);

    CHECK_EQ(textbuf_cursor(tb), 0);

    textbuf_destroy(tb);
}

TEST(delete_back_removes_whole_umlaut_not_half)
{
    textbuf *tb = make();
    REQUIRE(tb);

    textbuf_insert(tb, "äöü");
    CHECK_EQ(textbuf_len(tb), 6);

    CHECK(textbuf_delete_back(tb));
    CHECK_EQ(textbuf_len(tb), 4);
    CHECK_STR(textbuf_text(tb), "äö");

    textbuf_destroy(tb);
}

TEST(set_cursor_inside_umlaut_snaps_to_start)
{
    textbuf *tb = make();
    REQUIRE(tb);

    textbuf_insert(tb, "ü");   /* Bytes 0,1 - Byte 1 ist das Folgebyte */
    textbuf_set_cursor(tb, 1, false);

    CHECK_EQ(textbuf_cursor(tb), 0);

    textbuf_destroy(tb);
}

TEST(move_across_multibyte_only_text)
{
    textbuf *tb = make();
    REQUIRE(tb);

    textbuf_set(tb, "😀😀");   /* zwei Codepunkte zu je vier Byte */
    CHECK_EQ(textbuf_cursor(tb), 8);

    textbuf_move_cursor(tb, MOVE_LEFT, false);
    CHECK_EQ(textbuf_cursor(tb), 4);
    textbuf_move_cursor(tb, MOVE_LEFT, false);
    CHECK_EQ(textbuf_cursor(tb), 0);

    textbuf_move_cursor(tb, MOVE_RIGHT, false);
    CHECK_EQ(textbuf_cursor(tb), 4);
    textbuf_move_cursor(tb, MOVE_RIGHT, false);
    CHECK_EQ(textbuf_cursor(tb), 8);

    textbuf_destroy(tb);
}

/* --- Auswahl ----------------------------------------------------------------- */

TEST(extend_selection_reports_sorted_range)
{
    textbuf *tb = make();
    REQUIRE(tb);

    textbuf_set(tb, "Hallo Welt");

    textbuf_set_cursor(tb, 5, false);
    textbuf_move_cursor(tb, MOVE_RIGHT, true);

    size_t from, to;
    textbuf_selection(tb, &from, &to);
    CHECK_EQ(from, 5);
    CHECK_EQ(to, 6);

    /* Über den Anker hinaus nach links: die Auswahl bleibt aufsteigend. */
    textbuf_set_cursor(tb, 5, false);
    textbuf_move_cursor(tb, MOVE_LEFT, true);

    textbuf_selection(tb, &from, &to);
    CHECK_EQ(from, 4);
    CHECK_EQ(to, 5);

    textbuf_destroy(tb);
}

TEST(insert_replaces_existing_selection)
{
    textbuf *tb = make();
    REQUIRE(tb);

    textbuf_set(tb, "Hallo Welt");
    textbuf_set_cursor(tb, 0, false);
    textbuf_set_cursor(tb, 5, true);   /* "Hallo" ausgewählt */

    CHECK(textbuf_insert(tb, "Servus"));

    CHECK_STR(textbuf_text(tb), "Servus Welt");
    CHECK_EQ(textbuf_cursor(tb), 6);
    CHECK(!textbuf_has_selection(tb));

    textbuf_destroy(tb);
}

TEST(delete_removes_whole_selection)
{
    textbuf *tb = make();
    REQUIRE(tb);

    textbuf_set(tb, "Hallo Welt");
    textbuf_set_cursor(tb, 0, false);
    textbuf_set_cursor(tb, 6, true);   /* "Hallo " ausgewählt */

    CHECK(textbuf_delete_back(tb));
    CHECK_STR(textbuf_text(tb), "Welt");
    CHECK_EQ(textbuf_cursor(tb), 0);
    CHECK(!textbuf_has_selection(tb));

    textbuf_set(tb, "Hallo Welt");
    textbuf_set_cursor(tb, 5, false);
    textbuf_set_cursor(tb, 10, true);   /* " Welt" ausgewählt */

    CHECK(textbuf_delete_forward(tb));
    CHECK_STR(textbuf_text(tb), "Hallo");
    CHECK_EQ(textbuf_cursor(tb), 5);

    textbuf_destroy(tb);
}

TEST(select_all_selects_everything)
{
    textbuf *tb = make();
    REQUIRE(tb);

    textbuf_set(tb, "Hallo");
    textbuf_set_cursor(tb, 2, false);
    textbuf_select_all(tb);

    CHECK_EQ(textbuf_anchor(tb), 0);
    CHECK_EQ(textbuf_cursor(tb), 5);

    size_t from, to;
    textbuf_selection(tb, &from, &to);
    CHECK_EQ(from, 0);
    CHECK_EQ(to, 5);

    textbuf_destroy(tb);
}

/* --- Wortweise ----------------------------------------------------------------- */

TEST(move_word_right_skips_words_and_gaps)
{
    textbuf *tb = make();
    REQUIRE(tb);

    textbuf_set(tb, "Hallo Welt  und mehr");
    textbuf_set_cursor(tb, 0, false);

    textbuf_move_cursor(tb, MOVE_WORD_RIGHT, false);
    CHECK_EQ(textbuf_cursor(tb), 6);    /* Anfang von "Welt" */

    textbuf_move_cursor(tb, MOVE_WORD_RIGHT, false);
    CHECK_EQ(textbuf_cursor(tb), 12);   /* Anfang von "und", zwei Leerzeichen übersprungen */

    textbuf_move_cursor(tb, MOVE_WORD_RIGHT, false);
    CHECK_EQ(textbuf_cursor(tb), 16);   /* Anfang von "mehr" */

    textbuf_move_cursor(tb, MOVE_WORD_RIGHT, false);
    CHECK_EQ(textbuf_cursor(tb), 20);   /* Textende */

    textbuf_destroy(tb);
}

TEST(move_word_left_skips_words_and_gaps)
{
    textbuf *tb = make();
    REQUIRE(tb);

    textbuf_set(tb, "Hallo Welt  und mehr");
    textbuf_set_cursor(tb, 20, false);   /* Textende */

    textbuf_move_cursor(tb, MOVE_WORD_LEFT, false);
    CHECK_EQ(textbuf_cursor(tb), 16);   /* Anfang von "mehr" */

    textbuf_move_cursor(tb, MOVE_WORD_LEFT, false);
    CHECK_EQ(textbuf_cursor(tb), 12);   /* Anfang von "und" */

    textbuf_move_cursor(tb, MOVE_WORD_LEFT, false);
    CHECK_EQ(textbuf_cursor(tb), 6);    /* Anfang von "Welt" */

    textbuf_move_cursor(tb, MOVE_WORD_LEFT, false);
    CHECK_EQ(textbuf_cursor(tb), 0);    /* Anfang von "Hallo" */

    textbuf_destroy(tb);
}

TEST(move_word_at_text_boundaries_is_safe)
{
    textbuf *tb = make();
    REQUIRE(tb);

    textbuf_set(tb, "Wort");

    textbuf_set_cursor(tb, 0, false);
    textbuf_move_cursor(tb, MOVE_WORD_LEFT, false);
    CHECK_EQ(textbuf_cursor(tb), 0);

    textbuf_set_cursor(tb, 4, false);
    textbuf_move_cursor(tb, MOVE_WORD_RIGHT, false);
    CHECK_EQ(textbuf_cursor(tb), 4);

    textbuf_destroy(tb);
}

/* --- Zeilen und Spalten ---------------------------------------------------------- */

/* Der Test, an dem die meisten Editoren scheitern: über eine kurze Zeile
 * hinweg und wieder in eine lange - die Wunschspalte muss die kurze Zeile
 * überleben. */
/* Die Wunschspalte gilt nur für eine ununterbrochene Kette aus Auf und Ab.
 * Jede andere Bewegung setzt sie zurück - sonst springt die Marke beim
 * nächsten Auf oder Ab in eine Spalte, in der sie längst nicht mehr steht. */
TEST(any_other_move_forgets_the_desired_column)
{
    textbuf *tb = make();
    REQUIRE(tb);

    textbuf_set(tb, "abcdefghij\nklmnopqrst\nuvwxyz");

    /* In Zeile 0, Spalte 8. */
    textbuf_set_cursor(tb, 8, false);
    textbuf_move_cursor(tb, MOVE_DOWN, false);   /* Zeile 1, Spalte 8 */
    CHECK_EQ(textbuf_cursor(tb), 11 + 8);

    /* Jetzt seitlich an den Zeilenanfang - das muss die Wunschspalte löschen. */
    textbuf_move_cursor(tb, MOVE_LINE_START, false);
    CHECK_EQ(textbuf_cursor(tb), 11u);

    /* Ein Schritt nach unten landet nun in Spalte 0, nicht in der alten 8. */
    textbuf_move_cursor(tb, MOVE_DOWN, false);
    CHECK_EQ(textbuf_cursor(tb), 22u);

    /* Dasselbe für eine Bewegung um ein Zeichen. */
    textbuf_set_cursor(tb, 8, false);
    textbuf_move_cursor(tb, MOVE_DOWN, false);
    textbuf_move_cursor(tb, MOVE_LEFT, false);   /* Zeile 1, Spalte 7 */
    textbuf_move_cursor(tb, MOVE_DOWN, false);
    CHECK_EQ(textbuf_cursor(tb), 22u + 6u);      /* Zeile 2 ist nur 6 lang */

    textbuf_destroy(tb);
}

TEST(move_up_down_preserves_desired_column_across_short_line)
{
    textbuf *tb = make();
    REQUIRE(tb);

    /* Zeile 0: "Hallo Welt" (10 Zeichen), Zeile 1: "hi" (kurz),
     * Zeile 2: "Auf Wiedersehen" (lang). */
    textbuf_set(tb, "Hallo Welt\nhi\nAuf Wiedersehen");
    textbuf_set_cursor(tb, 7, false);   /* Spalte 7, das 'e' von "Welt" */

    textbuf_move_cursor(tb, MOVE_DOWN, false);
    CHECK_EQ(textbuf_cursor(tb), 13);   /* ans Ende von "hi" geklemmt, Spalte 7 bleibt gemerkt */

    textbuf_move_cursor(tb, MOVE_DOWN, false);
    CHECK_EQ(textbuf_cursor(tb), 21);   /* wieder Spalte 7, nicht Spalte 2 */

    textbuf_move_cursor(tb, MOVE_UP, false);
    CHECK_EQ(textbuf_cursor(tb), 13);

    textbuf_move_cursor(tb, MOVE_UP, false);
    CHECK_EQ(textbuf_cursor(tb), 7);    /* zurück auf die ursprüngliche Spalte */

    textbuf_destroy(tb);
}

TEST(move_up_down_noop_at_first_and_last_line)
{
    textbuf *tb = make();
    REQUIRE(tb);

    textbuf_set(tb, "eins\nzwei\ndrei");

    textbuf_set_cursor(tb, 2, false);
    textbuf_move_cursor(tb, MOVE_UP, false);
    CHECK_EQ(textbuf_cursor(tb), 2);

    textbuf_set_cursor(tb, 12, false);
    textbuf_move_cursor(tb, MOVE_DOWN, false);
    CHECK_EQ(textbuf_cursor(tb), 12);

    textbuf_destroy(tb);
}

TEST(column_counted_in_codepoints_not_bytes)
{
    textbuf *tb = make();
    REQUIRE(tb);

    /* Zeile 0: "grüße" (5 Codepunkte, 7 Byte). Zeile 1: "äöü mehr". */
    textbuf_set(tb, "grüße\näöü mehr");
    textbuf_set_cursor(tb, 6, false);   /* Codepunkt-Spalte 4, das 'e' */

    textbuf_move_cursor(tb, MOVE_DOWN, false);

    /* Bytebasiert läge Spalte 4 mitten im ü; codepunktbasiert liegt sie
     * genau am Anfang von "mehr", nach ä,ö,ü und dem Leerzeichen. */
    CHECK_EQ(textbuf_cursor(tb), 15);

    textbuf_destroy(tb);
}

/* --- Widerrufen ------------------------------------------------------------------- */

TEST(undo_merges_consecutive_typing_into_one_step)
{
    textbuf *tb = make();
    REQUIRE(tb);

    textbuf_insert(tb, "a");
    textbuf_insert(tb, "b");
    textbuf_insert(tb, "c");
    CHECK_STR(textbuf_text(tb), "abc");

    CHECK(textbuf_undo(tb));
    CHECK_STR(textbuf_text(tb), "");
    CHECK(!textbuf_can_undo(tb));

    textbuf_destroy(tb);
}

TEST(break_undo_separates_following_edits)
{
    textbuf *tb = make();
    REQUIRE(tb);

    textbuf_insert(tb, "a");
    textbuf_break_undo(tb);
    textbuf_insert(tb, "b");

    CHECK_STR(textbuf_text(tb), "ab");

    CHECK(textbuf_undo(tb));
    CHECK_STR(textbuf_text(tb), "a");

    CHECK(textbuf_undo(tb));
    CHECK_STR(textbuf_text(tb), "");

    CHECK(!textbuf_can_undo(tb));

    textbuf_destroy(tb);
}

TEST(undo_redo_roundtrip_restores_same_text)
{
    textbuf *tb = make();
    REQUIRE(tb);

    textbuf_insert(tb, "Hallo");
    textbuf_break_undo(tb);
    textbuf_insert(tb, " Welt");
    textbuf_break_undo(tb);
    textbuf_delete_back(tb);   /* entfernt das 't' */

    CHECK_STR(textbuf_text(tb), "Hallo Wel");

    CHECK(textbuf_undo(tb));
    CHECK(textbuf_undo(tb));
    CHECK(textbuf_undo(tb));
    CHECK_STR(textbuf_text(tb), "");
    CHECK(!textbuf_can_undo(tb));

    CHECK(textbuf_redo(tb));
    CHECK(textbuf_redo(tb));
    CHECK(textbuf_redo(tb));
    CHECK_STR(textbuf_text(tb), "Hallo Wel");
    CHECK(!textbuf_can_redo(tb));

    textbuf_destroy(tb);
}

TEST(new_edit_after_undo_clears_redo_stack)
{
    textbuf *tb = make();
    REQUIRE(tb);

    textbuf_insert(tb, "a");
    CHECK(textbuf_undo(tb));
    CHECK(textbuf_can_redo(tb));

    textbuf_insert(tb, "x");
    CHECK(!textbuf_can_redo(tb));

    textbuf_destroy(tb);
}

TEST(undo_restores_cursor_and_selection)
{
    textbuf *tb = make();
    REQUIRE(tb);

    textbuf_set(tb, "Hallo Welt");   /* leert die Geschichte, Marke am Ende */

    textbuf_set_cursor(tb, 0, false);
    textbuf_set_cursor(tb, 5, true);   /* "Hallo" ausgewählt, Anker 0, Marke 5 */

    textbuf_insert(tb, "Servus");   /* ersetzt die Auswahl */
    CHECK_STR(textbuf_text(tb), "Servus Welt");

    /* EIN Widerruf, denn für den Nutzer war es EINE Handlung. Intern sind es
     * zwei Schritte, eine Löschung und eine Einfügung; sie sind aneinander
     * gebunden und werden zusammen zurückgenommen. Sonst stünde dazwischen
     * " Welt" auf dem Schirm - ein Text, den nie jemand geschrieben hat. */
    CHECK(textbuf_undo(tb));
    CHECK_STR(textbuf_text(tb), "Hallo Welt");
    CHECK_EQ(textbuf_anchor(tb), 0);
    CHECK_EQ(textbuf_cursor(tb), 5);
    CHECK(textbuf_has_selection(tb));

    /* Und zurück: ein Wiederholen führt genauso in einem Schritt zurück. */
    CHECK(textbuf_redo(tb));
    CHECK_STR(textbuf_text(tb), "Servus Welt");
    CHECK(!textbuf_can_redo(tb));

    CHECK(textbuf_undo(tb));
    CHECK_STR(textbuf_text(tb), "Hallo Welt");

    textbuf_destroy(tb);
}

/* --- Ränder ------------------------------------------------------------------------- */

TEST(empty_buffer_moves_and_deletes_are_safe)
{
    textbuf *tb = make();
    REQUIRE(tb);

    static const textbuf_move all[] = {
        MOVE_LEFT, MOVE_RIGHT, MOVE_WORD_LEFT, MOVE_WORD_RIGHT,
        MOVE_LINE_START, MOVE_LINE_END, MOVE_UP, MOVE_DOWN,
        MOVE_TEXT_START, MOVE_TEXT_END
    };

    for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); i++) {
        textbuf_move_cursor(tb, all[i], false);
        CHECK_EQ(textbuf_cursor(tb), 0);
    }

    CHECK(!textbuf_delete_back(tb));
    CHECK(!textbuf_delete_forward(tb));
    CHECK_EQ(textbuf_len(tb), 0);
    CHECK_STR(textbuf_text(tb), "");

    textbuf_destroy(tb);
}

TEST(textbuf_set_clears_undo_history)
{
    textbuf *tb = make();
    REQUIRE(tb);

    textbuf_insert(tb, "abc");
    CHECK(textbuf_can_undo(tb));

    textbuf_set(tb, "xyz");
    CHECK(!textbuf_can_undo(tb));
    CHECK(!textbuf_can_redo(tb));

    textbuf_destroy(tb);
}

int main(void)
{
    RUN(move_left_five_times_returns_to_start_after_umlaut_insert);
    RUN(delete_back_removes_whole_umlaut_not_half);
    RUN(set_cursor_inside_umlaut_snaps_to_start);
    RUN(move_across_multibyte_only_text);

    RUN(extend_selection_reports_sorted_range);
    RUN(insert_replaces_existing_selection);
    RUN(delete_removes_whole_selection);
    RUN(select_all_selects_everything);

    RUN(move_word_right_skips_words_and_gaps);
    RUN(move_word_left_skips_words_and_gaps);
    RUN(move_word_at_text_boundaries_is_safe);

    RUN(any_other_move_forgets_the_desired_column);
    RUN(move_up_down_preserves_desired_column_across_short_line);
    RUN(move_up_down_noop_at_first_and_last_line);
    RUN(column_counted_in_codepoints_not_bytes);

    RUN(undo_merges_consecutive_typing_into_one_step);
    RUN(break_undo_separates_following_edits);
    RUN(undo_redo_roundtrip_restores_same_text);
    RUN(new_edit_after_undo_clears_redo_stack);
    RUN(undo_restores_cursor_and_selection);

    RUN(empty_buffer_moves_and_deletes_are_safe);
    RUN(textbuf_set_clears_undo_history);

    return test_summary();
}
