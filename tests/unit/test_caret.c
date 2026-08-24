/* Der Takt der Schreibmarke.
 *
 * Reine Arithmetik über der Uhr, deshalb kein Bild in dieser Datei. Geprüft
 * wird vor allem eines: dass ein Programm, das nie tickt, immer dasselbe
 * sieht. Daran hängen sämtliche Sollbilder mit einem Textfeld darin.
 */
#include "test.h"

#include "ui/caret.h"

/* --- Ohne Uhr ---------------------------------------------------------------- */

TEST(without_a_clock_the_caret_stands_still_and_shows)
{
    caret_reset();

    /* Kein caret_tick(): so laufen Tests, Sollbilder und der Bildabzug. */
    CHECK(caret_on());
    CHECK(caret_on());
}

/* --- Der Takt ---------------------------------------------------------------- */

TEST(half_a_period_turns_it_over)
{
    caret_reset();

    /* Der erste Tick setzt nur den Anfang - sonst hinge die Phase davon ab,
     * wie lange das Programm zum Starten gebraucht hat. */
    caret_tick(1000);
    CHECK(caret_on());

    caret_tick(1000 + CARET_BLINK_MS - 1);
    CHECK(caret_on());

    caret_tick(1000 + CARET_BLINK_MS);
    CHECK(!caret_on());

    caret_tick(1000 + 2 * CARET_BLINK_MS);
    CHECK(caret_on());
}

TEST(many_small_ticks_give_the_same_phase_as_one_big_one)
{
    caret_reset();
    caret_tick(0);
    /* Schrittweite so gewählt, dass sie die Halbperiode teilt: sonst läge das
     * Ergebnis um einen Schritt daneben, und der Test prüfte die Schrittweite
     * statt der Phase. */
    for (uint32_t t = 0; t <= 3 * CARET_BLINK_MS; t += 100) caret_tick(t);
    bool schrittweise = caret_on();

    caret_reset();
    caret_tick(0);
    caret_tick(3 * CARET_BLINK_MS);
    CHECK_EQ(caret_on(), schrittweise);

    /* Drei halbe Perioden weiter ist sie aus. */
    CHECK(!caret_on());
}

TEST(a_jump_in_time_costs_no_loop)
{
    caret_reset();
    caret_tick(0);

    /* Ein Haltepunkt im Debugger oder ein zugeklappter Deckel: hier springt
     * die Uhr um Tage. Das darf nicht Millionen Schritte kosten. */
    caret_tick(3600u * 1000u);          /* eine Stunde = 7200 Halbperioden */
    CHECK(caret_on());                  /* gerade Anzahl, also unverändert */

    caret_tick(3600u * 1000u + CARET_BLINK_MS);
    CHECK(!caret_on());
}

TEST(the_clock_may_wrap_around)
{
    caret_reset();

    /* plat_ticks_ms() zählt in 32 Bit und läuft nach 49 Tagen über. Der
     * Schritt über die Grenze ist kein besonderer Schritt. */
    uint32_t vorher = 0xFFFFFFFFu - 100u;
    caret_tick(vorher);
    CHECK(caret_on());

    caret_tick(vorher + CARET_BLINK_MS);   /* läuft über */
    CHECK(!caret_on());
}

TEST(a_reset_starts_the_beat_over)
{
    caret_reset();
    caret_tick(0);
    caret_tick(CARET_BLINK_MS);
    CHECK(!caret_on());

    /* Nach dem Zurücksetzen zählt der nächste Tick wieder nur als Anfang -
     * gleich, welche Zahl die Uhr gerade zeigt. Sonst hinge die Phase davon
     * ab, wie lange das Programm vorher lief. */
    caret_reset();
    caret_tick(CARET_BLINK_MS);
    CHECK(caret_on());

    caret_tick(2 * CARET_BLINK_MS);
    CHECK(!caret_on());
}

TEST(the_beat_does_not_drift_over_a_minute)
{
    caret_reset();

    /* Bilder kommen alle 16 ms, und 16 teilt 500 nicht. Wer die Phase bei
     * jedem Umschlag auf die aktuelle Zeit setzt, verliert bei jedem Takt ein
     * paar Millisekunden - nach einer Minute fehlt ein ganzer Schlag. */
    const uint32_t frame = 16, dauer = 60u * 1000u;

    caret_tick(0);
    bool vorher = caret_on();
    int  wechsel = 0;

    for (uint32_t t = frame; t <= dauer; t += frame) {
        caret_tick(t);
        if (caret_on() != vorher) {
            wechsel++;
            vorher = caret_on();
        }
    }

    CHECK_EQ(wechsel, (int)(dauer / CARET_BLINK_MS));
}

/* --- Nach einer Eingabe ------------------------------------------------------ */

TEST(typing_makes_the_caret_show_again)
{
    caret_reset();
    caret_tick(0);
    caret_tick(CARET_BLINK_MS);
    CHECK(!caret_on());

    /* Ein Anschlag: sofort sichtbar, ohne auf den nächsten Tick zu warten. */
    caret_wake();
    CHECK(caret_on());

    /* Und der Takt fängt von vorn an, nicht mitten in der dunklen Hälfte. */
    caret_tick(CARET_BLINK_MS + 10);
    CHECK(caret_on());

    caret_tick(CARET_BLINK_MS + 10 + CARET_BLINK_MS - 1);
    CHECK(caret_on());

    caret_tick(CARET_BLINK_MS + 10 + CARET_BLINK_MS);
    CHECK(!caret_on());
}

int main(void)
{
    RUN(without_a_clock_the_caret_stands_still_and_shows);

    RUN(half_a_period_turns_it_over);
    RUN(many_small_ticks_give_the_same_phase_as_one_big_one);
    RUN(a_jump_in_time_costs_no_loop);
    RUN(the_clock_may_wrap_around);
    RUN(a_reset_starts_the_beat_over);
    RUN(the_beat_does_not_drift_over_a_minute);

    RUN(typing_makes_the_caret_show_again);

    return test_summary();
}
