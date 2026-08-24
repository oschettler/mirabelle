/* Datumsrechnung, siehe core/date.h.
 *
 * Datumscode ist berüchtigt dafür, an genau den Tagen zu versagen, die man
 * beim Schreiben nicht im Kopf hatte. Diese Datei sammelt sie: Schaltjahre,
 * Jahrhundertwenden, Monatsenden, Jahresgrenzen.
 */
#include "test.h"

#include <string.h>

#include "core/date.h"

static date D(int y, int m, int d)
{
    date x = { y, m, d };
    return x;
}

static bool same(date a, date b)
{
    return date_compare(a, b) == 0;
}

static void show(const char *what, date d)
{
    char iso[16];
    date_to_iso(d, iso, sizeof iso);
    printf("  %s: %s\n", what, iso);
}

/* --- Schaltjahre ------------------------------------------------------------------ */

TEST(leap_years_follow_all_three_rules)
{
    /* Alle vier Jahre - aber nicht alle hundert - aber doch alle vierhundert.
     * Die zweite Regel ist die, die gern vergessen wird, und die dritte die,
     * die man erst 2000 gemerkt hätte. */
    CHECK(date_is_leap_year(2024));
    CHECK(!date_is_leap_year(2025));
    CHECK(!date_is_leap_year(1900));
    CHECK(date_is_leap_year(2000));
    CHECK(!date_is_leap_year(2100));
    CHECK(date_is_leap_year(2400));
}

TEST(february_is_the_only_month_that_changes_length)
{
    CHECK_EQ(date_days_in_month(2025, 2), 28);
    CHECK_EQ(date_days_in_month(2024, 2), 29);
    CHECK_EQ(date_days_in_month(1900, 2), 28);
    CHECK_EQ(date_days_in_month(2000, 2), 29);

    CHECK_EQ(date_days_in_month(2025, 1), 31);
    CHECK_EQ(date_days_in_month(2025, 4), 30);
    CHECK_EQ(date_days_in_month(2025, 12), 31);

    /* Einen dreizehnten Monat gibt es nicht. */
    CHECK_EQ(date_days_in_month(2025, 0), 0);
    CHECK_EQ(date_days_in_month(2025, 13), 0);
}

TEST(a_date_that_does_not_exist_is_invalid)
{
    CHECK(date_valid(D(2024, 2, 29)));
    CHECK(!date_valid(D(2025, 2, 29)));
    CHECK(!date_valid(D(2025, 4, 31)));
    CHECK(!date_valid(D(2025, 13, 1)));
    CHECK(!date_valid(D(2025, 0, 1)));
    CHECK(!date_valid(D(2025, 1, 0)));
    CHECK(date_valid(D(2025, 12, 31)));
}

/* --- Wochentage --------------------------------------------------------------------- */

TEST(weekdays_are_right_for_days_we_can_check)
{
    /* Montag ist null. Nachgerechnet an Tagen, deren Wochentag feststeht. */
    CHECK_EQ(date_weekday(D(2026, 8, 24)), 0);   /* Montag */
    CHECK_EQ(date_weekday(D(2000, 1, 1)),  5);   /* Samstag */
    CHECK_EQ(date_weekday(D(2024, 2, 29)), 3);   /* Donnerstag, ein Schalttag */
    CHECK_EQ(date_weekday(D(1970, 1, 1)),  3);   /* Donnerstag */
    CHECK_EQ(date_weekday(D(2026, 1, 1)),  3);   /* Donnerstag */
}

TEST(weekdays_advance_by_one_a_day)
{
    /* Über einen Monatswechsel, einen Jahreswechsel und einen Schalttag
     * hinweg: an keiner dieser Stellen darf die Woche stolpern. */
    date d = D(2024, 2, 26);
    for (int i = 0; i < 400; i++) {
        int w    = date_weekday(d);
        date nxt = date_add_days(d, 1);
        int  wn  = date_weekday(nxt);

        if (wn != (w + 1) % 7) { show("Stolperstelle", d); CHECK(false); break; }
        d = nxt;
    }
}

/* --- Rechnen mit Tagen --------------------------------------------------------------- */

TEST(adding_days_crosses_month_and_year)
{
    CHECK(same(date_add_days(D(2025, 1, 31), 1),  D(2025, 2, 1)));
    CHECK(same(date_add_days(D(2025, 12, 31), 1), D(2026, 1, 1)));
    CHECK(same(date_add_days(D(2024, 2, 28), 1),  D(2024, 2, 29)));
    CHECK(same(date_add_days(D(2025, 2, 28), 1),  D(2025, 3, 1)));
}

TEST(subtracting_days_crosses_them_backwards)
{
    CHECK(same(date_add_days(D(2026, 1, 1), -1),  D(2025, 12, 31)));
    CHECK(same(date_add_days(D(2025, 3, 1), -1),  D(2025, 2, 28)));
    CHECK(same(date_add_days(D(2024, 3, 1), -1),  D(2024, 2, 29)));
}

TEST(adding_zero_days_changes_nothing)
{
    CHECK(same(date_add_days(D(2025, 6, 15), 0), D(2025, 6, 15)));
}

TEST(a_year_of_days_lands_a_year_later)
{
    CHECK(same(date_add_days(D(2025, 1, 1), 365), D(2026, 1, 1)));
    CHECK(same(date_add_days(D(2024, 1, 1), 366), D(2025, 1, 1)));   /* Schaltjahr */
}

TEST(days_and_back_is_the_same_day)
{
    /* Eine lange Reihe hin und wieder zurück - über Jahrhundertwenden hinweg,
     * wo die Schaltjahrregel die Ausnahme von der Ausnahme hat. */
    date starts[] = { D(1899, 12, 31), D(1900, 3, 1), D(2000, 2, 29), D(2100, 1, 1) };

    for (size_t i = 0; i < sizeof starts / sizeof starts[0]; i++)
        for (int n = -1000; n <= 1000; n += 37) {
            date there = date_add_days(starts[i], n);
            date back  = date_add_days(there, -n);
            if (!same(back, starts[i])) { show("Rundreise", starts[i]); CHECK(false); }
            CHECK(date_valid(there));
        }
}

/* --- Rechnen mit Monaten --------------------------------------------------------------- */

TEST(adding_months_keeps_the_day_where_it_can)
{
    CHECK(same(date_add_months(D(2025, 1, 15), 1),  D(2025, 2, 15)));
    CHECK(same(date_add_months(D(2025, 1, 15), 12), D(2026, 1, 15)));
    CHECK(same(date_add_months(D(2025, 3, 15), -1), D(2025, 2, 15)));
    CHECK(same(date_add_months(D(2025, 1, 15), -1), D(2024, 12, 15)));
}

TEST(a_month_later_never_lands_in_the_month_after_that)
{
    /* Der 31. Januar plus einen Monat ist der 28. Februar. Wer stattdessen
     * einunddreißig Tage rechnet, landet im März - und im Kalender springt die
     * Ansicht dann einen Monat zu weit. */
    CHECK(same(date_add_months(D(2025, 1, 31), 1), D(2025, 2, 28)));
    CHECK(same(date_add_months(D(2024, 1, 31), 1), D(2024, 2, 29)));
    CHECK(same(date_add_months(D(2025, 3, 31), 1), D(2025, 4, 30)));
    CHECK(same(date_add_months(D(2025, 5, 31), -1), D(2025, 4, 30)));
}

TEST(months_stay_between_one_and_twelve)
{
    for (int n = -30; n <= 30; n++) {
        date d = date_add_months(D(2025, 6, 10), n);
        if (d.month < 1 || d.month > 12) { show("Monat daneben", d); CHECK(false); }
        CHECK(date_valid(d));
    }
}

TEST(twelve_months_are_a_year_in_both_directions)
{
    CHECK(same(date_add_months(D(2025, 6, 10), 12),  D(2026, 6, 10)));
    CHECK(same(date_add_months(D(2025, 6, 10), -12), D(2024, 6, 10)));
    CHECK(same(date_add_months(D(2025, 1, 10), -13), D(2023, 12, 10)));
}

/* --- Vergleichen -------------------------------------------------------------------- */

TEST(comparing_looks_at_year_then_month_then_day)
{
    CHECK(date_compare(D(2025, 1, 1), D(2026, 1, 1)) < 0);
    CHECK(date_compare(D(2025, 2, 1), D(2025, 1, 1)) > 0);
    CHECK(date_compare(D(2025, 1, 2), D(2025, 1, 1)) > 0);
    CHECK_EQ(date_compare(D(2025, 1, 1), D(2025, 1, 1)), 0);

    /* Der Dezember des Vorjahres liegt vor dem Januar - eine Reihenfolge, die
     * eine Rechnung, die nur auf Tag und Monat schaut, umdreht. */
    CHECK(date_compare(D(2025, 12, 31), D(2026, 1, 1)) < 0);
}

/* --- Text --------------------------------------------------------------------------- */

TEST(iso_is_always_ten_characters)
{
    char out[16];

    date_to_iso(D(2026, 3, 5), out, sizeof out);
    CHECK_STR(out, "2026-03-05");
    CHECK_EQ((int)strlen(out), 10);

    date_to_iso(D(999, 12, 31), out, sizeof out);
    CHECK_STR(out, "0999-12-31");   /* aufgefüllt, sonst sortiert es falsch */
}

TEST(iso_round_trips)
{
    const char *isos[] = { "2026-03-05", "2024-02-29", "1970-01-01", "2100-12-31" };

    for (size_t i = 0; i < sizeof isos / sizeof isos[0]; i++) {
        date d;
        REQUIRE(date_from_iso(isos[i], &d));

        char back[16];
        date_to_iso(d, back, sizeof back);
        CHECK_STR(back, isos[i]);
    }
}

TEST(anything_that_is_not_iso_is_refused)
{
    date d;
    const char *bad[] = {
        "2026-3-5",      /* ohne führende Null */
        "2026/03/05",    /* falsches Trennzeichen */
        "05.03.2026",    /* die Anzeigeform, nicht die Speicherform */
        "2026-03-05x",   /* hinten steht noch etwas */
        "20a6-03-05",    /* ein Buchstabe in der Jahreszahl - alle Trennzeichen
                          * stimmen, und ohne Ziffernprüfung käme ein Jahr
                          * heraus, das niemand eingegeben hat */
        "2026-0a-05",
        "2026-03-0",     /* zu kurz */
        "2025-02-29",    /* das Datum gibt es nicht */
        "2026-13-01",
        "",
    };

    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        if (date_from_iso(bad[i], &d)) printf("  angenommen: „%s“\n", bad[i]);
        CHECK(!date_from_iso(bad[i], &d));
    }

    CHECK(!date_from_iso(NULL, &d));
}

TEST(a_refused_date_leaves_the_target_alone)
{
    date d = D(2025, 6, 10);

    CHECK(!date_from_iso("Unsinn", &d));
    CHECK(same(d, D(2025, 6, 10)));

    /* Der interessantere Fall: die Form stimmt, das Datum gibt es trotzdem
     * nicht. Wer erst schreibt und dann prüft, hat das Ziel schon verdorben,
     * wenn er false zurückgibt. */
    CHECK(!date_from_iso("2025-02-29", &d));
    CHECK(same(d, D(2025, 6, 10)));
}

TEST(the_first_years_of_the_calendar_still_add_up)
{
    /* Solche Daten kommen in einem Terminkalender nicht vor. Die Rechnung soll
     * trotzdem stimmen: eine Funktion, die für alle Eingaben richtig ist, ist
     * besser als eine mit einem dokumentierten Loch - und der Nullpunkt der
     * Tageszählung liegt genau hier, also ist es die Stelle, an der eine
     * fehlende Vorzeichenbehandlung auffällt. */
    CHECK(date_weekday(D(0, 3, 1)) == 2);        /* der Nullpunkt: ein Mittwoch */

    /* Vor dem Nullpunkt wird die Tageszahl negativ. */
    CHECK_EQ(date_weekday(D(0, 2, 29)), 1);      /* der Tag davor, ein Dienstag */
    CHECK_EQ(date_weekday(D(0, 1, 1)), 5);

    for (int i = 0; i < 40; i++) {
        date d = date_add_days(D(0, 3, 1), -i);
        int  w = date_weekday(d);
        if (w < 0 || w > 6) { show("Wochentag daneben", d); CHECK(false); }
    }

    /* Und die Monatsrechnung darf hier nicht in den Monat null laufen. */
    for (int n = -1; n >= -30; n--) {
        date d = date_add_months(D(0, 6, 15), n);
        if (d.month < 1 || d.month > 12) { show("Monat daneben", d); CHECK(false); }
    }
}

int main(void)
{
    RUN(leap_years_follow_all_three_rules);
    RUN(february_is_the_only_month_that_changes_length);
    RUN(a_date_that_does_not_exist_is_invalid);

    RUN(weekdays_are_right_for_days_we_can_check);
    RUN(weekdays_advance_by_one_a_day);

    RUN(adding_days_crosses_month_and_year);
    RUN(subtracting_days_crosses_them_backwards);
    RUN(adding_zero_days_changes_nothing);
    RUN(a_year_of_days_lands_a_year_later);
    RUN(days_and_back_is_the_same_day);

    RUN(adding_months_keeps_the_day_where_it_can);
    RUN(a_month_later_never_lands_in_the_month_after_that);
    RUN(months_stay_between_one_and_twelve);
    RUN(twelve_months_are_a_year_in_both_directions);

    RUN(comparing_looks_at_year_then_month_then_day);

    RUN(iso_is_always_ten_characters);
    RUN(iso_round_trips);
    RUN(anything_that_is_not_iso_is_refused);
    RUN(a_refused_date_leaves_the_target_alone);
    RUN(the_first_years_of_the_calendar_still_add_up);

    return test_summary();
}
