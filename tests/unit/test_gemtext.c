/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "test.h"

#include "store/gemtext.h"

#include <string.h>

/* Sammelt die gemeldeten Zeilen als Kopien - die Zeiger darin bleiben
 * gültig, solange der ursprüngliche Text (ein String-Literal) im Test
 * lebt. */
typedef struct {
    gem_line lines[64];
    int      count;
} collector;

static void collect(const gem_line *line, void *user)
{
    collector *c = user;
    if (c->count < 64) c->lines[c->count] = *line;
    c->count++;
}

/* Vergleichshilfe: text ist nicht nullterminiert, also über text_len
 * vergleichen statt strcmp. */
static bool eq(const gem_line *l, const char *s)
{
    size_t n = strlen(s);
    return l->text_len == n && (n == 0 || memcmp(l->text, s, n) == 0);
}

static bool eq_url(const gem_line *l, const char *s)
{
    size_t n = strlen(s);
    return l->url_len == n && (n == 0 || memcmp(l->url, s, n) == 0);
}

TEST(gemtext_text_line)
{
    collector c = {0};
    const char *s = "Ein gewöhnlicher Satz.";
    int n = gemtext_parse(s, strlen(s), collect, &c);

    REQUIRE(n == 1);
    CHECK_EQ(c.lines[0].kind, GEM_TEXT);
    CHECK(eq(&c.lines[0], "Ein gewöhnlicher Satz."));
}

TEST(gemtext_empty_lines_reported)
{
    collector c = {0};
    const char *s = "Text\n\nNoch mehr Text\n";
    int n = gemtext_parse(s, strlen(s), collect, &c);

    REQUIRE(n == 3);
    CHECK(eq(&c.lines[0], "Text"));
    CHECK_EQ(c.lines[1].kind, GEM_TEXT);
    CHECK(eq(&c.lines[1], ""));
    CHECK(eq(&c.lines[2], "Noch mehr Text"));
}

TEST(gemtext_multiple_empty_lines_not_merged)
{
    collector c = {0};
    const char *s = "A\n\n\n\nB\n";
    int n = gemtext_parse(s, strlen(s), collect, &c);

    REQUIRE(n == 5);
    CHECK(eq(&c.lines[0], "A"));
    CHECK(eq(&c.lines[1], ""));
    CHECK(eq(&c.lines[2], ""));
    CHECK(eq(&c.lines[3], ""));
    CHECK(eq(&c.lines[4], "B"));
}

TEST(gemtext_heading_levels)
{
    collector c = {0};
    const char *s = "# Eins\n## Zwei\n### Drei\n";
    int n = gemtext_parse(s, strlen(s), collect, &c);

    REQUIRE(n == 3);
    CHECK_EQ(c.lines[0].kind, GEM_HEADING);
    CHECK_EQ(c.lines[0].level, 1);
    CHECK(eq(&c.lines[0], "Eins"));
    CHECK_EQ(c.lines[1].level, 2);
    CHECK(eq(&c.lines[1], "Zwei"));
    CHECK_EQ(c.lines[2].level, 3);
    CHECK(eq(&c.lines[2], "Drei"));
}

/* Reihenfolge der Prüfung ist verbindlich: "###" muss vor "#" erkannt
 * werden, sonst würde eine Ebene-3-Überschrift als Ebene 1 mit dem Text
 * "## Titel" fehlgedeutet. */
TEST(gemtext_heading_h3_not_h1)
{
    collector c = {0};
    const char *s = "### Titel\n";
    int n = gemtext_parse(s, strlen(s), collect, &c);

    REQUIRE(n == 1);
    CHECK_EQ(c.lines[0].level, 3);
    CHECK(eq(&c.lines[0], "Titel"));
}

TEST(gemtext_heading_without_space)
{
    collector c = {0};
    const char *s = "#Titel\n";
    int n = gemtext_parse(s, strlen(s), collect, &c);

    REQUIRE(n == 1);
    CHECK_EQ(c.lines[0].kind, GEM_HEADING);
    CHECK_EQ(c.lines[0].level, 1);
    CHECK(eq(&c.lines[0], "Titel"));
}

TEST(gemtext_item_line)
{
    collector c = {0};
    const char *s = "* Ein Listenpunkt\n";
    int n = gemtext_parse(s, strlen(s), collect, &c);

    REQUIRE(n == 1);
    CHECK_EQ(c.lines[0].kind, GEM_ITEM);
    CHECK(eq(&c.lines[0], "Ein Listenpunkt"));
}

TEST(gemtext_item_without_space_is_text)
{
    collector c = {0};
    const char *s = "*Stern\n";
    int n = gemtext_parse(s, strlen(s), collect, &c);

    REQUIRE(n == 1);
    CHECK_EQ(c.lines[0].kind, GEM_TEXT);
    CHECK(eq(&c.lines[0], "*Stern"));
}

TEST(gemtext_quote_line)
{
    collector c = {0};
    const char *s = ">Ein Zitat\n";
    int n = gemtext_parse(s, strlen(s), collect, &c);

    REQUIRE(n == 1);
    CHECK_EQ(c.lines[0].kind, GEM_QUOTE);
    CHECK(eq(&c.lines[0], "Ein Zitat"));
}

TEST(gemtext_link_with_name)
{
    collector c = {0};
    const char *s = "=> gemini://example.com/ Beispielseite\n";
    int n = gemtext_parse(s, strlen(s), collect, &c);

    REQUIRE(n == 1);
    CHECK_EQ(c.lines[0].kind, GEM_LINK);
    CHECK(eq_url(&c.lines[0], "gemini://example.com/"));
    CHECK(eq(&c.lines[0], "Beispielseite"));
}

TEST(gemtext_link_without_name)
{
    collector c = {0};
    const char *s = "=> gemini://example.com/\n";
    int n = gemtext_parse(s, strlen(s), collect, &c);

    REQUIRE(n == 1);
    CHECK_EQ(c.lines[0].kind, GEM_LINK);
    CHECK(eq_url(&c.lines[0], "gemini://example.com/"));
    CHECK(c.lines[0].text == NULL);
    CHECK_EQ(c.lines[0].text_len, 0);
}

TEST(gemtext_link_tab_separator)
{
    collector c = {0};
    const char *s = "=>\tgemini://example.com/\tName\n";
    int n = gemtext_parse(s, strlen(s), collect, &c);

    REQUIRE(n == 1);
    CHECK_EQ(c.lines[0].kind, GEM_LINK);
    CHECK(eq_url(&c.lines[0], "gemini://example.com/"));
    CHECK(eq(&c.lines[0], "Name"));
}

TEST(gemtext_link_name_with_spaces)
{
    collector c = {0};
    const char *s = "=> gemini://example.com/ Ein Name mit Leerzeichen\n";
    int n = gemtext_parse(s, strlen(s), collect, &c);

    REQUIRE(n == 1);
    CHECK_EQ(c.lines[0].kind, GEM_LINK);
    CHECK(eq_url(&c.lines[0], "gemini://example.com/"));
    CHECK(eq(&c.lines[0], "Ein Name mit Leerzeichen"));
}

TEST(gemtext_arrow_alone_is_text)
{
    collector c = {0};
    const char *s = "=>\n=>   \n";
    int n = gemtext_parse(s, strlen(s), collect, &c);

    REQUIRE(n == 2);
    CHECK_EQ(c.lines[0].kind, GEM_TEXT);
    CHECK(eq(&c.lines[0], "=>"));
    CHECK_EQ(c.lines[1].kind, GEM_TEXT);
    CHECK(eq(&c.lines[1], "=>   "));
}

TEST(gemtext_preformatted_block)
{
    collector c = {0};
    const char *s = "```\nZeile eins\nZeile zwei\n```\n";
    int n = gemtext_parse(s, strlen(s), collect, &c);

    REQUIRE(n == 2);
    CHECK_EQ(c.lines[0].kind, GEM_PRE);
    CHECK(eq(&c.lines[0], "Zeile eins"));
    CHECK_EQ(c.lines[1].kind, GEM_PRE);
    CHECK(eq(&c.lines[1], "Zeile zwei"));
}

/* Im vorformatierten Block wird kein Präfix erkannt, auch keine
 * Überschrift - alles ist GEM_PRE, unverändert. */
TEST(gemtext_preformatted_ignores_prefixes)
{
    collector c = {0};
    const char *s = "```\n# keine Überschrift\n=> auch kein Verweis\n```\n";
    int n = gemtext_parse(s, strlen(s), collect, &c);

    REQUIRE(n == 2);
    CHECK_EQ(c.lines[0].kind, GEM_PRE);
    CHECK(eq(&c.lines[0], "# keine Überschrift"));
    CHECK_EQ(c.lines[1].kind, GEM_PRE);
    CHECK(eq(&c.lines[1], "=> auch kein Verweis"));
}

TEST(gemtext_preformatted_hint_text_discarded)
{
    collector c = {0};
    const char *s = "```ein Hinweis für den Darsteller\ncode\n```\n";
    int n = gemtext_parse(s, strlen(s), collect, &c);

    REQUIRE(n == 1);
    CHECK_EQ(c.lines[0].kind, GEM_PRE);
    CHECK(eq(&c.lines[0], "code"));
}

TEST(gemtext_preformatted_unclosed_block)
{
    collector c = {0};
    const char *s = "```\nZeile eins\nZeile zwei";
    int n = gemtext_parse(s, strlen(s), collect, &c);

    REQUIRE(n == 2);
    CHECK_EQ(c.lines[0].kind, GEM_PRE);
    CHECK(eq(&c.lines[0], "Zeile eins"));
    CHECK_EQ(c.lines[1].kind, GEM_PRE);
    CHECK(eq(&c.lines[1], "Zeile zwei"));
}

TEST(gemtext_crlf_and_lf_mixed)
{
    collector c = {0};
    const char *s = "# Titel\r\nText mit LF\n> Zitat\r\n";
    int n = gemtext_parse(s, strlen(s), collect, &c);

    REQUIRE(n == 3);
    CHECK_EQ(c.lines[0].kind, GEM_HEADING);
    CHECK(eq(&c.lines[0], "Titel"));
    CHECK_EQ(c.lines[1].kind, GEM_TEXT);
    CHECK(eq(&c.lines[1], "Text mit LF"));
    CHECK_EQ(c.lines[2].kind, GEM_QUOTE);
    CHECK(eq(&c.lines[2], "Zitat"));
}

TEST(gemtext_last_line_without_newline)
{
    collector c = {0};
    const char *s = "Erste Zeile\nLetzte Zeile ohne Umbruch";
    int n = gemtext_parse(s, strlen(s), collect, &c);

    REQUIRE(n == 2);
    CHECK(eq(&c.lines[0], "Erste Zeile"));
    CHECK(eq(&c.lines[1], "Letzte Zeile ohne Umbruch"));
}

TEST(gemtext_empty_text)
{
    collector c = {0};
    int n = gemtext_parse("", 0, collect, &c);

    CHECK_EQ(n, 0);
    CHECK_EQ(c.count, 0);
}

TEST(gemtext_only_newline)
{
    collector c = {0};
    const char *s = "\n";
    int n = gemtext_parse(s, strlen(s), collect, &c);

    REQUIRE(n == 1);
    CHECK_EQ(c.lines[0].kind, GEM_TEXT);
    CHECK(eq(&c.lines[0], ""));
}

TEST(gemtext_utf8_content)
{
    collector c = {0};
    const char *s = "# Überschrift öäü\n"
                     "* Listenpunkt größer\n"
                     "> Zitat müßig\n"
                     "=> gemini://example.com/straße Straße\n"
                     "Satz mit Grüßen\n";
    int n = gemtext_parse(s, strlen(s), collect, &c);

    REQUIRE(n == 5);
    CHECK(eq(&c.lines[0], "Überschrift öäü"));
    CHECK(eq(&c.lines[1], "Listenpunkt größer"));
    CHECK(eq(&c.lines[2], "Zitat müßig"));
    CHECK(eq_url(&c.lines[3], "gemini://example.com/straße"));
    CHECK(eq(&c.lines[3], "Straße"));
    CHECK(eq(&c.lines[4], "Satz mit Grüßen"));
}

TEST(gemtext_full_document)
{
    collector c = {0};
    const char *s =
        "# Überschrift\n"
        "Ein normaler Satz.\n"
        "\n"
        "## Unterabschnitt\n"
        "* Erster Punkt\n"
        "* Zweiter Punkt\n"
        "=> gemini://example.com/ Ein Verweis\n"
        "=> gemini://example.com/ohne-name\n"
        "> Ein weises Zitat\n"
        "```alt\n"
        "vorformatierter Text\n"
        "  mit Einrückung\n"
        "```\n"
        "Zum Schluss noch Text.\n";
    int n = gemtext_parse(s, strlen(s), collect, &c);

    REQUIRE(n == 12);

    CHECK_EQ(c.lines[0].kind, GEM_HEADING);
    CHECK_EQ(c.lines[0].level, 1);
    CHECK(eq(&c.lines[0], "Überschrift"));

    CHECK_EQ(c.lines[1].kind, GEM_TEXT);
    CHECK(eq(&c.lines[1], "Ein normaler Satz."));

    CHECK_EQ(c.lines[2].kind, GEM_TEXT);
    CHECK(eq(&c.lines[2], ""));

    CHECK_EQ(c.lines[3].kind, GEM_HEADING);
    CHECK_EQ(c.lines[3].level, 2);
    CHECK(eq(&c.lines[3], "Unterabschnitt"));

    CHECK_EQ(c.lines[4].kind, GEM_ITEM);
    CHECK(eq(&c.lines[4], "Erster Punkt"));

    CHECK_EQ(c.lines[5].kind, GEM_ITEM);
    CHECK(eq(&c.lines[5], "Zweiter Punkt"));

    CHECK_EQ(c.lines[6].kind, GEM_LINK);
    CHECK(eq_url(&c.lines[6], "gemini://example.com/"));
    CHECK(eq(&c.lines[6], "Ein Verweis"));

    CHECK_EQ(c.lines[7].kind, GEM_LINK);
    CHECK(eq_url(&c.lines[7], "gemini://example.com/ohne-name"));
    CHECK(c.lines[7].text == NULL);

    CHECK_EQ(c.lines[8].kind, GEM_QUOTE);
    CHECK(eq(&c.lines[8], "Ein weises Zitat"));

    CHECK_EQ(c.lines[9].kind, GEM_PRE);
    CHECK(eq(&c.lines[9], "vorformatierter Text"));

    CHECK_EQ(c.lines[10].kind, GEM_PRE);
    CHECK(eq(&c.lines[10], "  mit Einrückung"));

    CHECK_EQ(c.lines[11].kind, GEM_TEXT);
    CHECK(eq(&c.lines[11], "Zum Schluss noch Text."));
}

int main(void)
{
    RUN(gemtext_text_line);
    RUN(gemtext_empty_lines_reported);
    RUN(gemtext_multiple_empty_lines_not_merged);
    RUN(gemtext_heading_levels);
    RUN(gemtext_heading_h3_not_h1);
    RUN(gemtext_heading_without_space);
    RUN(gemtext_item_line);
    RUN(gemtext_item_without_space_is_text);
    RUN(gemtext_quote_line);
    RUN(gemtext_link_with_name);
    RUN(gemtext_link_without_name);
    RUN(gemtext_link_tab_separator);
    RUN(gemtext_link_name_with_spaces);
    RUN(gemtext_arrow_alone_is_text);
    RUN(gemtext_preformatted_block);
    RUN(gemtext_preformatted_ignores_prefixes);
    RUN(gemtext_preformatted_hint_text_discarded);
    RUN(gemtext_preformatted_unclosed_block);
    RUN(gemtext_crlf_and_lf_mixed);
    RUN(gemtext_last_line_without_newline);
    RUN(gemtext_empty_text);
    RUN(gemtext_only_newline);
    RUN(gemtext_utf8_content);
    RUN(gemtext_full_document);
    return test_summary();
}
