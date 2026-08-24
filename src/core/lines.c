/* Siehe lines.h für den Vertrag. */
#include "core/lines.h"

#include <stdarg.h>
#include <string.h>

bool lines_open(linereader *r, const char *path, char *err, size_t err_size)
{
    memset(r, 0, sizeof *r);
    r->path = path;

    r->fp = fopen(path, "rb");
    if (!r->fp) return lines_fail_file(path, err, err_size, "nicht lesbar");

    if (err && err_size) err[0] = '\0';
    return true;
}

void lines_close(linereader *r)
{
    if (r->fp) fclose(r->fp);
    r->fp = NULL;
}

bool lines_next(linereader *r)
{
    while (fgets(r->buf, sizeof r->buf, r->fp)) {
        r->line++;
        r->count = 0;

        /* Ein `#` beendet die Zeile, gleich wo es steht. Das ist die eine
         * Regel, die alle drei Formate teilen sollten und vorher nicht
         * teilten. */
        char *hash = strchr(r->buf, '#');
        if (hash) *hash = '\0';

        char *p = r->buf;
        while (*p && r->count < LINES_MAX_WORDS) {
            while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
            if (!*p) break;

            r->word[r->count++] = p;
            while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') p++;
            if (*p) *p++ = '\0';
        }

        if (r->count > 0) return true;
    }
    return false;
}

static void build(char *err, size_t err_size, const char *path, int line,
                  const char *fmt, va_list ap)
{
    char msg[256];
    vsnprintf(msg, sizeof msg, fmt, ap);

    if (!err || !err_size) return;
    if (line > 0) snprintf(err, err_size, "%s:%d: %s", path, line, msg);
    else          snprintf(err, err_size, "%s: %s", path, msg);
}

bool lines_fail(const linereader *r, char *err, size_t err_size,
                const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    build(err, err_size, r->path, r->line, fmt, ap);
    va_end(ap);
    return false;
}

bool lines_fail_file(const char *path, char *err, size_t err_size,
                     const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    build(err, err_size, path, 0, fmt, ap);
    va_end(ap);
    return false;
}
