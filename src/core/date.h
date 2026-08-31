/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Datumsrechnung auf dem bürgerlichen Kalender.
 *
 * Ein Datum ist hier drei Zahlen, und im Speicher ist es die Zeichenkette
 * JJJJ-MM-TT. Beides zusammen ist Absicht: als Zeichenkette sortiert und
 * vergleicht es sich ohne Umrechnung (query.h), als drei Zahlen lässt sich
 * damit rechnen.
 *
 * Keine Zeitzone, keine Uhrzeit, kein Zeitstempel. Ein Termin am 15. März ist
 * am 15. März, egal wo jemand steht - und ein Taschencomputer, der um
 * Mitternacht die Zeitzone wechselt, soll nicht plötzlich einen Tag verschoben
 * sein.
 *
 * Der Kalender ist der gregorianische, ohne Rücksicht auf seine Einführung:
 * für 1582 rechnet diese Datei falsch, und das ist in Ordnung. Ein Adressbuch
 * braucht keine Geschichtswissenschaft.
 */
#ifndef PDA_CORE_DATE_H
#define PDA_CORE_DATE_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    int year;    /* vierstellig */
    int month;   /* 1 bis 12 */
    int day;     /* 1 bis 31, gültig für diesen Monat */
} date;

/* true, wenn das Datum es wirklich gibt - der 30. Februar also nicht. */
bool date_valid(date d);

/* Das heutige Datum nach der Systemuhr, ohne Zeitzone - wie überall in dieser
 * Datei gibt es keine Uhrzeit, nur den Tag. */
date date_today(void);

bool date_is_leap_year(int year);
int  date_days_in_month(int year, int month);

/* Wochentag: 0 ist Montag, 6 ist Sonntag.
 *
 * Montag als Null, weil `week.start` im Katalog so zählt und weil in
 * Mitteleuropa die Woche am Montag beginnt. Wer den Sonntag vorn haben will,
 * ändert eine Katalogzeile, keine Rechnung. */
int date_weekday(date d);

/* Verschiebt um days Tage, vorwärts oder rückwärts, über Monats- und
 * Jahresgrenzen hinweg. */
date date_add_days(date d, int days);

/* Verschiebt um months Monate. Der Tag wird auf den letzten des Zielmonats
 * gekürzt, wenn es ihn dort nicht gibt: der 31. Januar plus einen Monat ist der
 * 28. Februar, nicht der 3. März. Anders wäre „einen Monat weiter" im Kalender
 * nicht mehr das, was der Nutzer meint. */
date date_add_months(date d, int months);

/* Wie strcmp: kleiner null, wenn a vor b liegt. */
int date_compare(date a, date b);

/* JJJJ-MM-TT, immer genau zehn Zeichen. out braucht 11 Bytes. */
void date_to_iso(date d, char *out, size_t out_size);

/* Liest JJJJ-MM-TT. false, wenn es nicht genau so dasteht oder das Datum es
 * nicht gibt. */
bool date_from_iso(const char *iso, date *out);

#endif /* PDA_CORE_DATE_H */
