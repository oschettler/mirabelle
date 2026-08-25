/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "utf8.h"

uint32_t utf8_next(const char **p)
{
    const unsigned char *s = (const unsigned char *)*p;
    unsigned char b0 = s[0];

    if (b0 == 0x00) return 0;

    if (b0 < 0x80) {
        *p += 1;
        return b0;
    }

    /* len ist die erwartete Bytelänge, lo2/hi2 der zulässige Bereich für das
     * zweite Byte - dort stecken die Sonderfälle für Überlänge, Surrogate
     * und die Obergrenze U+10FFFF. Alles außerhalb dieser Startbytebereiche
     * (0x80..0xBF, 0xC0, 0xC1, 0xF5..0xFF) ist sofort ungültig. */
    int len = 0;
    unsigned char lo2 = 0x80, hi2 = 0xBF;

    if (b0 >= 0xC2 && b0 <= 0xDF) {
        len = 2;
    } else if (b0 >= 0xE0 && b0 <= 0xEF) {
        len = 3;
        if (b0 == 0xE0) lo2 = 0xA0;       /* sonst überlang */
        else if (b0 == 0xED) hi2 = 0x9F;  /* sonst Surrogat U+D800..U+DFFF */
    } else if (b0 >= 0xF0 && b0 <= 0xF4) {
        len = 4;
        if (b0 == 0xF0) lo2 = 0x90;       /* sonst überlang */
        else if (b0 == 0xF4) hi2 = 0x8F;  /* sonst über U+10FFFF */
    } else {
        /* Folgebyte als Startbyte oder ungültiges Startbyte: genau ein
         * Byte überspringen, sonst würde eine Schleife nie terminieren. */
        *p += 1;
        return UTF8_REPLACEMENT;
    }

    unsigned char b1 = s[1];
    if (b1 == 0x00 || b1 < lo2 || b1 > hi2) {
        *p += 1;
        return UTF8_REPLACEMENT;
    }

    if (len == 2) {
        *p += 2;
        return ((uint32_t)(b0 & 0x1Fu) << 6) | (b1 & 0x3Fu);
    }

    unsigned char b2 = s[2];
    if (b2 == 0x00 || b2 < 0x80 || b2 > 0xBF) {
        *p += 1;
        return UTF8_REPLACEMENT;
    }

    if (len == 3) {
        *p += 3;
        return ((uint32_t)(b0 & 0x0Fu) << 12) | ((uint32_t)(b1 & 0x3Fu) << 6) |
               (b2 & 0x3Fu);
    }

    unsigned char b3 = s[3];
    if (b3 == 0x00 || b3 < 0x80 || b3 > 0xBF) {
        *p += 1;
        return UTF8_REPLACEMENT;
    }

    *p += 4;
    return ((uint32_t)(b0 & 0x07u) << 18) | ((uint32_t)(b1 & 0x3Fu) << 12) |
           ((uint32_t)(b2 & 0x3Fu) << 6) | (b3 & 0x3Fu);
}

uint32_t utf8_prev(const char *start, const char **p)
{
    const char *q = *p;
    if (q == start) return 0;

    /* Über Folgebytes rückwärts zum vermeintlichen Startbyte, höchstens vier
     * Bytes weit (länger ist keine gültige Folge). */
    const char *cand = q - 1;
    int steps = 1;
    while (cand > start && ((unsigned char)*cand & 0xC0) == 0x80 && steps < 4) {
        cand--;
        steps++;
    }

    if (((unsigned char)*cand & 0xC0) != 0x80) {
        /* Kandidat gefunden - probeweise vorwärts dekodieren. Landet die
         * Folge genau wieder bei q, war es tatsächlich der vorige Codepunkt. */
        const char *scan = cand;
        uint32_t cp = utf8_next(&scan);
        if (scan == q) {
            *p = cand;
            return cp;
        }
    }

    /* Kein passendes Startbyte innerhalb von vier Bytes: genau ein Byte
     * zurück, U+FFFD liefern - spiegelbildlich zu utf8_next. */
    *p = q - 1;
    return UTF8_REPLACEMENT;
}

int utf8_encode(uint32_t codepoint, char out[4])
{
    if (codepoint >= 0xD800u && codepoint <= 0xDFFFu) return 0;
    if (codepoint > 0x10FFFFu) return 0;

    unsigned char *o = (unsigned char *)out;

    if (codepoint <= 0x7Fu) {
        o[0] = (unsigned char)codepoint;
        return 1;
    }

    if (codepoint <= 0x7FFu) {
        o[0] = (unsigned char)(0xC0u | (codepoint >> 6));
        o[1] = (unsigned char)(0x80u | (codepoint & 0x3Fu));
        return 2;
    }

    if (codepoint <= 0xFFFFu) {
        o[0] = (unsigned char)(0xE0u | (codepoint >> 12));
        o[1] = (unsigned char)(0x80u | ((codepoint >> 6) & 0x3Fu));
        o[2] = (unsigned char)(0x80u | (codepoint & 0x3Fu));
        return 3;
    }

    o[0] = (unsigned char)(0xF0u | (codepoint >> 18));
    o[1] = (unsigned char)(0x80u | ((codepoint >> 12) & 0x3Fu));
    o[2] = (unsigned char)(0x80u | ((codepoint >> 6) & 0x3Fu));
    o[3] = (unsigned char)(0x80u | (codepoint & 0x3Fu));
    return 4;
}

size_t utf8_count(const char *s)
{
    size_t n = 0;
    const char *p = s;

    while (*p != '\0') {
        utf8_next(&p);
        n++;
    }

    return n;
}

int utf8_valid(const char *s)
{
    const char *p = s;

    while (*p != '\0') {
        const char *before = p;
        uint32_t cp = utf8_next(&p);

        /* utf8_next rückt bei jeder ungültigen Folge um genau ein Byte vor;
         * ein echtes, korrekt kodiertes U+FFFD braucht drei Bytes. Daran
         * unterscheidet sich eine erkannte Ungültigkeit von echtem Inhalt. */
        if (cp == UTF8_REPLACEMENT && p - before == 1) return 0;
    }

    return 1;
}
