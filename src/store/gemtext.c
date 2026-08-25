/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "gemtext.h"

#include <string.h>

/* Sicherheit gegen zerschnittene Mehrbytezeichen: alle Bytes, an denen wir
 * hier trennen oder Präfixe erkennen (CR, LF, '=', '>', '#', '*', ' ', '\t',
 * '`'), liegen unter 0x80. Ein UTF-8-Folgebyte liegt immer im Bereich
 * 0x80..0xBF, kann also nie mit einem dieser Werte verwechselt werden - wir
 * schneiden nie mitten in einem Zeichen. */

static bool is_space(char c)
{
    return c == ' ' || c == '\t';
}

/* Erkennt eine Überschrift oder einen Listenpunkt oder ein Zitat: Präfix von
 * prefix_len Bytes weg, dann führenden Leerraum weg, der Rest bis Zeilenende
 * bleibt Inhalt (auch nachfolgender Leerraum). */
static void set_prefixed(gem_line *gl, gem_kind kind, const char *line,
                          size_t line_len, size_t prefix_len)
{
    gl->kind = kind;
    size_t i = prefix_len;
    while (i < line_len && is_space(line[i])) i++;
    gl->text = line + i;
    gl->text_len = line_len - i;
}

/* Erkennt eine Verweiszeile nach "=>". Liefert false, wenn danach nur
 * Leerraum folgt - dann ist die Zeile kein gültiger Verweis. */
static bool set_link(gem_line *gl, const char *line, size_t line_len)
{
    size_t i = 2;
    while (i < line_len && is_space(line[i])) i++;
    if (i == line_len) return false;

    size_t addr_start = i;
    while (i < line_len && !is_space(line[i])) i++;
    size_t addr_end = i;
    while (i < line_len && is_space(line[i])) i++;

    gl->kind = GEM_LINK;
    gl->url = line + addr_start;
    gl->url_len = addr_end - addr_start;
    if (i < line_len) {
        gl->text = line + i;
        gl->text_len = line_len - i;
    } else {
        gl->text = NULL;
        gl->text_len = 0;
    }
    return true;
}

/* Klassifiziert eine Zeile im normalen Zustand. gl ist vorher auf die
 * Textvoreinstellung gesetzt (ganze Zeile als GEM_TEXT). */
static void classify(gem_line *gl, const char *line, size_t line_len)
{
    if (line_len >= 2 && line[0] == '=' && line[1] == '>') {
        set_link(gl, line, line_len); /* liefert false: bleibt Text */
        return;
    }
    if (line_len >= 3 && line[0] == '#' && line[1] == '#' && line[2] == '#') {
        set_prefixed(gl, GEM_HEADING, line, line_len, 3);
        gl->level = 3;
        return;
    }
    if (line_len >= 2 && line[0] == '#' && line[1] == '#') {
        set_prefixed(gl, GEM_HEADING, line, line_len, 2);
        gl->level = 2;
        return;
    }
    if (line_len >= 1 && line[0] == '#') {
        set_prefixed(gl, GEM_HEADING, line, line_len, 1);
        gl->level = 1;
        return;
    }
    if (line_len >= 2 && line[0] == '*' && line[1] == ' ') {
        set_prefixed(gl, GEM_ITEM, line, line_len, 2);
        return;
    }
    if (line_len >= 1 && line[0] == '>') {
        set_prefixed(gl, GEM_QUOTE, line, line_len, 1);
        return;
    }
}

static bool is_fence(const char *line, size_t line_len)
{
    return line_len >= 3 && line[0] == '`' && line[1] == '`' && line[2] == '`';
}

int gemtext_parse(const char *text, size_t len, gem_line_fn fn, void *user)
{
    int count = 0;
    bool pre = false;
    size_t i = 0;

    while (i < len) {
        size_t start = i;
        size_t end = start;
        while (end < len && text[end] != '\n') end++;

        size_t line_len = end - start;
        if (line_len > 0 && text[start + line_len - 1] == '\r') line_len--;
        const char *line = text + start;

        i = (end < len) ? end + 1 : end;

        if (is_fence(line, line_len)) {
            pre = !pre;
            continue;
        }

        gem_line gl = {0};
        if (pre) {
            gl.kind = GEM_PRE;
            gl.text = line;
            gl.text_len = line_len;
        } else {
            gl.kind = GEM_TEXT;
            gl.text = line;
            gl.text_len = line_len;
            classify(&gl, line, line_len);
        }
        fn(&gl, user);
        count++;
    }

    return count;
}
