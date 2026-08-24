/* Siehe date.h für den Vertrag.
 *
 * Gerechnet wird über die Tageszahl seit einem festen Nullpunkt: einmal
 * hinein, einmal heraus, dazwischen gewöhnliche Arithmetik. Das erspart die
 * Schleifen über Monate, die man sonst schreibt - und mit ihnen die Fehler an
 * den Jahresgrenzen, die man dabei macht.
 *
 * Der Nullpunkt ist der 1. März des Jahres 0. Das ist kein Datum, das jemand
 * feiert, sondern ein Rechentrick: legt man den März an den Anfang des Jahres,
 * steht der Schalttag am Ende, und die Monatslängen folgen einer einfachen
 * Formel, statt eine Tabelle mit einem Sonderfall zu brauchen.
 */
#include "core/date.h"

#include <stdio.h>

bool date_is_leap_year(int year)
{
    if (year % 4 != 0)   return false;
    if (year % 100 != 0) return true;
    return year % 400 == 0;
}

int date_days_in_month(int year, int month)
{
    static const int len[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (month < 1 || month > 12) return 0;
    if (month == 2 && date_is_leap_year(year)) return 29;
    return len[month - 1];
}

bool date_valid(date d)
{
    if (d.month < 1 || d.month > 12) return false;
    if (d.day < 1) return false;
    return d.day <= date_days_in_month(d.year, d.month);
}

/* --- Tageszahl -------------------------------------------------------------------
 *
 * Nach Howard Hinnants "days from civil". Der Jahresanfang wird auf den März
 * gelegt (era-basiert), damit der Schalttag ans Jahresende rückt.
 */

static long to_days(date d)
{
    int  y   = d.year - (d.month <= 2 ? 1 : 0);
    long era = (y >= 0 ? y : y - 399) / 400;
    long yoe = y - era * 400;                                  /* 0 bis 399 */
    long doy = (153 * (d.month + (d.month > 2 ? -3 : 9)) + 2) / 5 + d.day - 1;
    long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;          /* 0 bis 146096 */

    return era * 146097 + doe;
}

static date from_days(long z)
{
    long era = (z >= 0 ? z : z - 146096) / 146097;
    long doe = z - era * 146097;
    long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long y   = yoe + era * 400;
    long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    long mp  = (5 * doy + 2) / 153;
    long d   = doy - (153 * mp + 2) / 5 + 1;
    long m   = mp + (mp < 10 ? 3 : -9);

    date out;
    out.year  = (int)(y + (m <= 2 ? 1 : 0));
    out.month = (int)m;
    out.day   = (int)d;
    return out;
}

int date_weekday(date d)
{
    /* Der Nullpunkt der Tageszahl - der 1. März 0 - fällt auf einen Mittwoch.
     * Mittwoch ist bei Montag als Null die Zwei, also der Versatz. */
    long z = to_days(d) + 2;
    long w = z % 7;
    return (int)(w < 0 ? w + 7 : w);
}

date date_add_days(date d, int days)
{
    return from_days(to_days(d) + days);
}

date date_add_months(date d, int months)
{
    long m0 = (long)d.year * 12 + (d.month - 1) + months;

    date out;
    out.year  = (int)(m0 / 12);
    out.month = (int)(m0 % 12) + 1;

    /* Bei negativen Jahren rundet die Division zur Null hin, und der Monat
     * käme null oder negativ heraus. */
    if (out.month < 1) {
        out.month += 12;
        out.year  -= 1;
    }

    int last = date_days_in_month(out.year, out.month);
    out.day  = d.day < last ? d.day : last;
    return out;
}

int date_compare(date a, date b)
{
    if (a.year  != b.year)  return a.year  < b.year  ? -1 : 1;
    if (a.month != b.month) return a.month < b.month ? -1 : 1;
    if (a.day   != b.day)   return a.day   < b.day   ? -1 : 1;
    return 0;
}

void date_to_iso(date d, char *out, size_t out_size)
{
    snprintf(out, out_size, "%04d-%02d-%02d", d.year, d.month, d.day);
}

bool date_from_iso(const char *iso, date *out)
{
    if (!iso) return false;

    /* Genau zehn Zeichen in genau dieser Form. Alles andere abzulehnen ist
     * hier richtig: das Format ist unser eigenes, es kommt aus unseren
     * Dateien, und was davon abweicht, ist ein Fehler und keine Variante. */
    for (int i = 0; i < 10; i++) {
        if (!iso[i]) return false;
        bool digit = (i != 4 && i != 7);
        if (digit && (iso[i] < '0' || iso[i] > '9')) return false;
        if (!digit && iso[i] != '-') return false;
    }
    if (iso[10]) return false;

    date d;
    d.year  = (iso[0]-'0')*1000 + (iso[1]-'0')*100 + (iso[2]-'0')*10 + (iso[3]-'0');
    d.month = (iso[5]-'0')*10 + (iso[6]-'0');
    d.day   = (iso[8]-'0')*10 + (iso[9]-'0');

    if (!date_valid(d)) return false;
    *out = d;
    return true;
}
