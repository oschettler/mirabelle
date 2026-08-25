/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Siehe spartan.h für den Vertrag.
 *
 * Nichts hier ruft ins Netz. Das Protokoll ist Rechnung auf Bytes; wer sie
 * überträgt, wird übergeben. Deshalb prüft die Testdatei das Protokoll und
 * nicht die Netzwerkkarte.
 */
#include "net/spartan.h"

#include <stdio.h>
#include <string.h>

/* Kopiert nach dst, aber nur ganz. Abschneiden hieße hier, eine andere Seite
 * abzurufen als die gemeinte. */
static bool copy_exact(char *dst, size_t cap, const char *src, size_t len)
{
    if (len >= cap) return false;
    memcpy(dst, src, len);
    dst[len] = '\0';
    return true;
}

/* --- Adressen ------------------------------------------------------------------- */

bool spartan_parse_url(const char *url, spartan_url *out)
{
    if (!url) return false;

    memset(out, 0, sizeof *out);
    out->port = SPARTAN_PORT;

    const char *p = url;
    while (*p == ' ') p++;

    /* Das Schema darf fehlen, ein anderes darf es nicht sein.
     *
     * Gesucht wird nur VOR dem ersten Schrägstrich. Ein „://" weiter hinten
     * gehört zum Pfad - eine Seite darf eine Adresse im Namen tragen, und
     * „x.org/artikel/http://..." ist ein gültiger Pfad und kein fremdes
     * Schema. */
    static const char SCHEME[] = "spartan://";
    if (strncmp(p, SCHEME, sizeof SCHEME - 1) == 0) {
        p += sizeof SCHEME - 1;
    } else {
        const char *first_slash = strchr(p, '/');
        const char *scheme_end  = strstr(p, "://");

        /* Diese Prüfung fängt nichts, was die Portprüfung weiter unten nicht
         * auch fangen würde: bei „https://x" steht hinter dem Doppelpunkt
         * „//x", und das sind keine Ziffern. Sie steht trotzdem hier, weil ein
         * fremdes Schema aus dem richtigen Grund abgelehnt gehört und nicht
         * aus Versehen. Wer die Portprüfung einmal lockert, hätte sonst
         * plötzlich einen http-Client. */
        if (scheme_end && (!first_slash || scheme_end < first_slash))
            return false;
    }

    /* Bis zum ersten "/" steht der Rechner, davor darf ein Port stehen. */
    const char *slash = strchr(p, '/');
    const char *hend  = slash ? slash : p + strlen(p);
    const char *colon = memchr(p, ':', (size_t)(hend - p));

    const char *host_end = colon ? colon : hend;
    if (host_end == p) return false;
    if (!copy_exact(out->host, sizeof out->host, p, (size_t)(host_end - p)))
        return false;

    /* Ein Rechnername mit Leerzeichen ist keiner. */
    if (strchr(out->host, ' ')) return false;

    if (colon) {
        int port = 0;
        for (const char *q = colon + 1; q < hend; q++) {
            if (*q < '0' || *q > '9') return false;
            port = port * 10 + (*q - '0');
            if (port > 65535) return false;
        }
        if (port == 0) return false;
        out->port = port;
    }

    /* Ohne Pfad ist der Pfad die Wurzel. */
    if (!slash) {
        out->path[0] = '/';
        out->path[1] = '\0';
        return true;
    }

    size_t plen = strlen(slash);
    while (plen > 0 && (slash[plen - 1] == ' ')) plen--;
    return copy_exact(out->path, sizeof out->path, slash, plen);
}

bool spartan_format_url(const spartan_url *u, char *out, size_t out_size)
{
    int n;
    if (u->port == SPARTAN_PORT)
        n = snprintf(out, out_size, "spartan://%s%s", u->host, u->path);
    else
        n = snprintf(out, out_size, "spartan://%s:%d%s", u->host, u->port, u->path);

    return n > 0 && (size_t)n < out_size;
}

/* Kürzt den Pfad auf sein Verzeichnis, also bis einschließlich des letzten
 * Schrägstrichs. */
static void dirname_of(char *path)
{
    char *slash = strrchr(path, '/');
    if (slash) slash[1] = '\0';
    else       { path[0] = '/'; path[1] = '\0'; }
}

/* Räumt "." und ".." aus einem Pfad heraus.
 *
 * Ohne das käme ein Verweis auf "../oben" als solcher beim Server an, und was
 * er damit macht, weiß niemand. Ein Pfad, der über die Wurzel hinaus nach oben
 * will, bleibt an der Wurzel stehen. */
static void normalize(char *path)
{
    char  out[SPARTAN_PATH_MAX];
    char *w = out;
    *w++ = '/';

    const char *p = path;
    while (*p == '/') p++;

    while (*p) {
        const char *seg = p;
        while (*p && *p != '/') p++;
        size_t len = (size_t)(p - seg);

        if (len == 1 && seg[0] == '.') {
            /* nichts */
        } else if (len == 2 && seg[0] == '.' && seg[1] == '.') {
            if (w > out + 1) {
                w--;                                  /* den Schrägstrich */
                while (w > out + 1 && w[-1] != '/') w--;
            }
        } else if (len > 0) {
            if ((size_t)(w - out) + len + 2 >= sizeof out) break;
            memcpy(w, seg, len);
            w += len;
            *w++ = '/';
        }

        while (*p == '/') p++;
    }

    /* Der abschließende Schrägstrich gehört nur dahin, wenn er im Original
     * stand - "/a/b" und "/a/b/" sind für einen Server zweierlei. */
    size_t olen = strlen(path);
    bool   trailing = olen > 0 && path[olen - 1] == '/';

    if (w > out + 1 && !trailing) w--;
    *w = '\0';

    snprintf(path, SPARTAN_PATH_MAX, "%s", out);
}

bool spartan_resolve(const spartan_url *base, const char *href, spartan_url *out)
{
    if (!href || !*href) return false;

    /* Eine vollständige Adresse steht für sich. */
    if (strstr(href, "://")) return spartan_parse_url(href, out);

    *out = *base;

    if (href[0] == '/') {
        if (!copy_exact(out->path, sizeof out->path, href, strlen(href)))
            return false;
    } else {
        char dir[SPARTAN_PATH_MAX];
        snprintf(dir, sizeof dir, "%s", base->path);
        dirname_of(dir);

        int n = snprintf(out->path, sizeof out->path, "%s%s", dir, href);
        if (n < 0 || (size_t)n >= sizeof out->path) return false;
    }

    normalize(out->path);
    return true;
}

/* --- Anfrage ---------------------------------------------------------------------- */

bool spartan_build_request(const spartan_url *u, size_t body_len,
                           char *out, size_t out_size)
{
    int n = snprintf(out, out_size, "%s %s %zu\r\n", u->host, u->path, body_len);
    return n > 0 && (size_t)n < out_size;
}

/* --- Antwort ----------------------------------------------------------------------- */

bool spartan_parse_response(const char *data, size_t len, spartan_response *out,
                            char *err, size_t err_size)
{
    memset(out, 0, sizeof *out);

    const char *nl = memchr(data, '\n', len);
    if (!nl) {
        snprintf(err, err_size, "die Statuszeile hört nicht auf");
        return false;
    }

    size_t line_len = (size_t)(nl - data);
    if (line_len > 0 && data[line_len - 1] == '\r') line_len--;

    if (line_len < 1 || data[0] < '2' || data[0] > '5') {
        snprintf(err, err_size, "unbekannter Status");
        return false;
    }
    out->status = data[0] - '0';

    /* Zwischen Ziffer und Angabe steht genau ein Leerzeichen; fehlt die
     * Angabe ganz, ist das erlaubt. */
    size_t meta_at = 1;
    if (meta_at < line_len && data[meta_at] == ' ') meta_at++;

    if (!copy_exact(out->meta, sizeof out->meta, data + meta_at, line_len - meta_at)) {
        snprintf(err, err_size, "die Angabe in der Statuszeile ist zu lang");
        return false;
    }

    out->body     = nl + 1;
    out->body_len = len - (size_t)(nl + 1 - data);

    if (err && err_size) err[0] = '\0';
    return true;
}

/* --- Abrufen ------------------------------------------------------------------------ */

bool spartan_fetch(const spartan_transport *t, const spartan_url *u,
                   char *buf, size_t buf_size, spartan_response *out,
                   char *err, size_t err_size)
{
    if (!t || !t->open || !t->send_all || !t->recv_some) {
        snprintf(err, err_size, "kein Transport");
        return false;
    }

    if (!t->open(t->user, u->host, u->port, err, err_size)) return false;

    char request[SPARTAN_URL_MAX];
    if (!spartan_build_request(u, 0, request, sizeof request)) {
        if (t->close) t->close(t->user);
        snprintf(err, err_size, "die Adresse ist zu lang");
        return false;
    }

    if (!t->send_all(t->user, request, strlen(request))) {
        if (t->close) t->close(t->user);
        snprintf(err, err_size, "die Anfrage ging nicht durch");
        return false;
    }

    /* Lesen, bis die Gegenseite schließt - das ist beim SPARTAN das Ende der
     * Antwort. Es gibt keine Längenangabe, und deshalb auch keinen Zustand,
     * in dem man auf etwas wartet, das nicht mehr kommt. */
    size_t n = 0;
    for (;;) {
        if (n + 1 >= buf_size) {
            /* Voll. Was da ist, wird ausgewertet - eine halbe Seite ist mehr
             * wert als eine Fehlermeldung, und der Nutzer sieht am
             * abgeschnittenen Ende, dass etwas fehlt.
             *
             * Hier abzubrechen ist nicht nur Zierde: ohne diese Zeile käme
             * recv_some mit einer Kapazität von null dran. Manche Umsetzungen
             * liefern dann null und alles geht gut, andere warten auf etwas,
             * das nie kommt. Ein Aufruf, der null Bytes erbittet, ergibt
             * keinen Sinn und wird deshalb gar nicht erst gemacht. */
            break;
        }

        long got = t->recv_some(t->user, buf + n, buf_size - 1 - n);
        if (got < 0) {
            if (t->close) t->close(t->user);
            snprintf(err, err_size, "die Verbindung brach ab");
            return false;
        }
        if (got == 0) break;
        n += (size_t)got;
    }

    if (t->close) t->close(t->user);
    buf[n] = '\0';

    if (n == 0) {
        snprintf(err, err_size, "die Gegenseite hat nichts geschickt");
        return false;
    }

    return spartan_parse_response(buf, n, out, err, err_size);
}
