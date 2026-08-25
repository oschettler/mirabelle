/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Der Testläufer des Projekts. Bewusst klein und ohne Magie:
 * Ein Test ist eine Funktion, main() zählt sie auf. Mehr braucht es nicht.
 *
 *     TEST(bitmap_pset) { CHECK_EQ(1, 1); }
 *     int main(void) { RUN(bitmap_pset); return test_summary(); }
 */
#ifndef PDA_TEST_H
#define PDA_TEST_H

#include <stdio.h>
#include <string.h>

static const char *test_current = "";
static int test_count;
static int test_failed;
static int test_failed_here;

#define TEST(name) static void name(void)

#define RUN(fn)                                                               \
    do {                                                                      \
        test_current = #fn;                                                   \
        test_failed_here = 0;                                                 \
        test_count++;                                                         \
        fn();                                                                 \
        if (test_failed_here) {                                               \
            test_failed++;                                                    \
            printf("FEHLER  %s\n", #fn);                                      \
        } else {                                                              \
            printf("ok      %s\n", #fn);                                      \
        }                                                                     \
    } while (0)

#define TEST_FAIL(...)                                                        \
    do {                                                                      \
        test_failed_here = 1;                                                 \
        printf("  %s:%d in %s: ", __FILE__, __LINE__, test_current);          \
        printf(__VA_ARGS__);                                                  \
        printf("\n");                                                         \
    } while (0)

#define CHECK(cond)                                                           \
    do {                                                                      \
        if (!(cond)) TEST_FAIL("%s ist nicht wahr", #cond);                    \
    } while (0)

/* Wie CHECK, bricht den Test aber ab. Für Bedingungen, ohne die die folgenden
 * Zeilen abstürzen würden - typisch ein Zeiger, der nicht NULL sein darf. Ein
 * abstürzender Test ist schlimmer als ein fehlschlagender: er verschluckt alle
 * Ergebnisse, die nach ihm kämen. */
#define REQUIRE(cond)                                                         \
    do {                                                                      \
        if (!(cond)) {                                                        \
            TEST_FAIL("%s ist nicht wahr, Test abgebrochen", #cond);          \
            return;                                                           \
        }                                                                     \
    } while (0)

#define CHECK_EQ(got, want)                                                   \
    do {                                                                      \
        long long g_ = (long long)(got), w_ = (long long)(want);              \
        if (g_ != w_)                                                         \
            TEST_FAIL("%s: erwartet %lld, bekommen %lld", #got, w_, g_);       \
    } while (0)

#define CHECK_STR(got, want)                                                  \
    do {                                                                      \
        const char *g_ = (got), *w_ = (want);                                 \
        if (g_ == NULL || w_ == NULL || strcmp(g_, w_) != 0)                   \
            TEST_FAIL("%s: erwartet \"%s\", bekommen \"%s\"", #got,            \
                      w_ ? w_ : "(null)", g_ ? g_ : "(null)");                 \
    } while (0)

#define CHECK_MEM(got, want, n)                                               \
    do {                                                                      \
        if (memcmp((got), (want), (size_t)(n)) != 0)                          \
            TEST_FAIL("%s: %zu Bytes unterscheiden sich", #got, (size_t)(n));  \
    } while (0)

static int test_summary(void)
{
    printf("\n%d Tests, %d Fehler\n", test_count, test_failed);
    return test_failed == 0 ? 0 : 1;
}

#endif /* PDA_TEST_H */
