#include "test.h"

#include "core/utf8.h"

#include <string.h>

/* Rundlauf über eine Reihe von Codepunkten verschiedener Bytelänge:
 * encode, dann next, und der Codepunkt kommt heil samt korrekter
 * Bytelänge zurück. */
TEST(utf8_roundtrip_codepoints)
{
    struct {
        uint32_t cp;
        int len;
    } cases[] = {
        {0x41,    1},  /* A   */
        {0xE4,    2},  /* ä   */
        {0xF6,    2},  /* ö   */
        {0xFC,    2},  /* ü   */
        {0xDF,    2},  /* ß   */
        {0xC4,    2},  /* Ä   */
        {0x20AC,  3},  /* €   */
        {0x1F600, 4},  /* 😀  */
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        char buf[5] = {0};
        int n = utf8_encode(cases[i].cp, buf);
        CHECK_EQ(n, cases[i].len);
        buf[n] = '\0';

        const char *p = buf;
        uint32_t cp = utf8_next(&p);
        CHECK_EQ(cp, cases[i].cp);
        CHECK_EQ(p - buf, cases[i].len);
    }
}

/* Die genauen Bytes für ä und € - nicht nur der Rundlauf, sondern die
 * tatsächliche Kodierung. */
TEST(utf8_encode_exact_bytes)
{
    char buf[4];
    int n;

    n = utf8_encode(0xE4, buf);  /* ä */
    CHECK_EQ(n, 2);
    CHECK_EQ((unsigned char)buf[0], 0xC3);
    CHECK_EQ((unsigned char)buf[1], 0xA4);

    n = utf8_encode(0x20AC, buf);  /* € */
    CHECK_EQ(n, 3);
    CHECK_EQ((unsigned char)buf[0], 0xE2);
    CHECK_EQ((unsigned char)buf[1], 0x82);
    CHECK_EQ((unsigned char)buf[2], 0xAC);
}

TEST(utf8_count_gruesse)
{
    const char *s = "Grüße";
    CHECK_EQ(utf8_count(s), 5);
    CHECK_EQ(strlen(s), 7);
}

TEST(utf8_next_at_end_of_string)
{
    const char *s = "A";
    const char *p = s + 1;  /* zeigt schon auf das Nullbyte */
    uint32_t cp = utf8_next(&p);
    CHECK_EQ(cp, 0);
    CHECK(p == s + 1);
}

/* Rückwärtslauf über eine Kette mit Umlauten liefert dieselben Codepunkte
 * wie der Vorwärtslauf, nur umgekehrt, und endet mit 0 am Anfang. */
TEST(utf8_prev_matches_forward_reversed)
{
    const char *s = "Grüße";
    uint32_t forward[16];
    size_t n = 0;

    const char *p = s;
    uint32_t cp;
    while ((cp = utf8_next(&p)) != 0)
        forward[n++] = cp;
    CHECK_EQ(n, 5);

    const char *q = s + strlen(s);
    size_t i = n;
    while (q != s) {
        cp = utf8_prev(s, &q);
        i--;
        CHECK_EQ(cp, forward[i]);
    }
    CHECK_EQ(i, 0);

    const char *before = q;
    cp = utf8_prev(s, &q);
    CHECK_EQ(cp, 0);
    CHECK(q == before);
}

TEST(utf8_next_overlong_2byte)
{
    const char *p = "\xC0\x80";
    const char *before = p;
    uint32_t cp = utf8_next(&p);
    CHECK_EQ(cp, UTF8_REPLACEMENT);
    CHECK_EQ(p - before, 1);
}

TEST(utf8_next_overlong_3byte)
{
    const char *p = "\xE0\x80\x80";
    const char *before = p;
    uint32_t cp = utf8_next(&p);
    CHECK_EQ(cp, UTF8_REPLACEMENT);
    CHECK_EQ(p - before, 1);
}

TEST(utf8_next_overlong_4byte)
{
    const char *p = "\xF0\x80\x80\x80";
    const char *before = p;
    uint32_t cp = utf8_next(&p);
    CHECK_EQ(cp, UTF8_REPLACEMENT);
    CHECK_EQ(p - before, 1);
}

TEST(utf8_next_surrogate)
{
    const char *p = "\xED\xA0\x80";
    const char *before = p;
    uint32_t cp = utf8_next(&p);
    CHECK_EQ(cp, UTF8_REPLACEMENT);
    CHECK_EQ(p - before, 1);
}

TEST(utf8_next_above_max_via_f4)
{
    const char *p = "\xF4\x90\x80\x80";
    const char *before = p;
    uint32_t cp = utf8_next(&p);
    CHECK_EQ(cp, UTF8_REPLACEMENT);
    CHECK_EQ(p - before, 1);
}

TEST(utf8_next_invalid_startbyte_f5)
{
    const char *p = "\xF5\x80\x80\x80";
    const char *before = p;
    uint32_t cp = utf8_next(&p);
    CHECK_EQ(cp, UTF8_REPLACEMENT);
    CHECK_EQ(p - before, 1);
}

TEST(utf8_next_continuation_byte_as_start)
{
    const char *p = "\x80";
    const char *before = p;
    uint32_t cp = utf8_next(&p);
    CHECK_EQ(cp, UTF8_REPLACEMENT);
    CHECK_EQ(p - before, 1);
}

TEST(utf8_next_truncated_2byte)
{
    const char *p = "\xC3";
    const char *before = p;
    uint32_t cp = utf8_next(&p);
    CHECK_EQ(cp, UTF8_REPLACEMENT);
    CHECK_EQ(p - before, 1);
}

TEST(utf8_next_truncated_3byte)
{
    const char *p = "\xE2\x82";
    const char *before = p;
    uint32_t cp = utf8_next(&p);
    CHECK_EQ(cp, UTF8_REPLACEMENT);
    CHECK_EQ(p - before, 1);
}

/* Eine Bytefolge aus lauter ungültigen Bytes: die Schleife muss terminieren
 * und je Byte genau einen Codepunkt liefern. */
TEST(utf8_invalid_bytes_loop_terminates)
{
    const char *s = "\x80\xFF\xC0\xED\xA0\xF5\x81\xBF\xC1\x80";
    size_t nbytes = strlen(s);

    const char *p = s;
    size_t n = 0;
    uint32_t cp;
    while ((cp = utf8_next(&p)) != 0) {
        CHECK_EQ(cp, UTF8_REPLACEMENT);
        n++;
    }
    CHECK_EQ(n, nbytes);
}

TEST(utf8_encode_rejects_surrogates_and_overflow)
{
    char buf[4];
    CHECK_EQ(utf8_encode(0xD800, buf), 0);
    CHECK_EQ(utf8_encode(0xDFFF, buf), 0);
    CHECK_EQ(utf8_encode(0x110000, buf), 0);
}

TEST(utf8_valid_checks)
{
    CHECK_EQ(utf8_valid("Grüße"), 1);
    CHECK_EQ(utf8_valid(""), 1);
    CHECK_EQ(utf8_valid("A\xC3\xA4""B"), 1);  /* AäB */
    CHECK_EQ(utf8_valid("\xC0\x80"), 0);
    CHECK_EQ(utf8_valid("A\xED\xA0\x80"), 0);
    CHECK_EQ(utf8_valid("\xC3"), 0);
}

int main(void)
{
    RUN(utf8_roundtrip_codepoints);
    RUN(utf8_encode_exact_bytes);
    RUN(utf8_count_gruesse);
    RUN(utf8_next_at_end_of_string);
    RUN(utf8_prev_matches_forward_reversed);
    RUN(utf8_next_overlong_2byte);
    RUN(utf8_next_overlong_3byte);
    RUN(utf8_next_overlong_4byte);
    RUN(utf8_next_surrogate);
    RUN(utf8_next_above_max_via_f4);
    RUN(utf8_next_invalid_startbyte_f5);
    RUN(utf8_next_continuation_byte_as_start);
    RUN(utf8_next_truncated_2byte);
    RUN(utf8_next_truncated_3byte);
    RUN(utf8_invalid_bytes_loop_terminates);
    RUN(utf8_encode_rejects_surrogates_and_overflow);
    RUN(utf8_valid_checks);
    return test_summary();
}
