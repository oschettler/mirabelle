/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Das SPARTAN-Protokoll, siehe net/spartan.h.
 *
 * Kein Test hier braucht ein Netz. Der Transport ist ein Rückruf, und der
 * liefert hier vorbereitete Bytes - ein Test, der einen Server bräuchte, prüfte
 * an guten Tagen das Protokoll und an schlechten die Leitung.
 */
#include "test.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "net/spartan.h"

/* --- Adressen zerlegen ------------------------------------------------------------ */

TEST(a_full_address_comes_apart)
{
    spartan_url u;
    REQUIRE(spartan_parse_url("spartan://mozz.us/gemtext.gmi", &u));

    CHECK_STR(u.host, "mozz.us");
    CHECK_EQ(u.port, SPARTAN_PORT);
    CHECK_STR(u.path, "/gemtext.gmi");
}

TEST(the_scheme_may_be_left_out)
{
    /* Ein Mensch tippt „mozz.us" und nicht „spartan://mozz.us/". */
    spartan_url u;
    REQUIRE(spartan_parse_url("mozz.us/seite", &u));
    CHECK_STR(u.host, "mozz.us");
    CHECK_STR(u.path, "/seite");

    REQUIRE(spartan_parse_url("mozz.us", &u));
    CHECK_STR(u.path, "/");           /* ohne Pfad ist es die Wurzel */
}

TEST(another_scheme_is_refused)
{
    /* Eine http-Adresse stillschweigend als SPARTAN zu behandeln führte zu
     * einer Anfrage, die niemand versteht - und der Nutzer sähe nur, dass
     * nichts kommt. */
    spartan_url u;
    CHECK(!spartan_parse_url("https://example.org/", &u));
    CHECK(!spartan_parse_url("gemini://example.org/", &u));

    /* Ein „://" IM PFAD ist dagegen kein Schema. Eine Seite darf eine Adresse
     * im Namen tragen, und wer hier zu grob sucht, verbietet gültige Pfade. */
    REQUIRE(spartan_parse_url("x.org/artikel/http://warum-nicht", &u));
    CHECK_STR(u.host, "x.org");
    CHECK_STR(u.path, "/artikel/http://warum-nicht");
}

TEST(a_port_is_taken_along)
{
    spartan_url u;
    REQUIRE(spartan_parse_url("spartan://example.org:3000/x", &u));
    CHECK_STR(u.host, "example.org");
    CHECK_EQ(u.port, 3000);
    CHECK_STR(u.path, "/x");

    /* Und was kein Port ist, wird abgelehnt statt geraten. */
    CHECK(!spartan_parse_url("spartan://example.org:abc/x", &u));
    CHECK(!spartan_parse_url("spartan://example.org:99999/x", &u));
    CHECK(!spartan_parse_url("spartan://example.org:0/x", &u));
}

TEST(nonsense_addresses_are_refused)
{
    spartan_url u;
    CHECK(!spartan_parse_url("", &u));
    CHECK(!spartan_parse_url("/nur/ein/pfad", &u));
    CHECK(!spartan_parse_url("spartan:///pfad", &u));
    CHECK(!spartan_parse_url("mit leerzeichen/x", &u));
    CHECK(!spartan_parse_url(NULL, &u));
}

TEST(an_address_goes_back_together)
{
    spartan_url u;
    char        out[SPARTAN_URL_MAX];

    REQUIRE(spartan_parse_url("spartan://mozz.us/a/b.gmi", &u));
    REQUIRE(spartan_format_url(&u, out, sizeof out));
    CHECK_STR(out, "spartan://mozz.us/a/b.gmi");

    /* Der Standardport steht nicht dabei, ein anderer schon. */
    REQUIRE(spartan_parse_url("spartan://x.org:3000/", &u));
    REQUIRE(spartan_format_url(&u, out, sizeof out));
    CHECK_STR(out, "spartan://x.org:3000/");
}

/* --- Verweise auflösen ------------------------------------------------------------- */

TEST(a_relative_link_stays_on_the_same_host)
{
    spartan_url base, out;
    REQUIRE(spartan_parse_url("spartan://mozz.us/ordner/seite.gmi", &base));

    REQUIRE(spartan_resolve(&base, "/ganz/anders", &out));
    CHECK_STR(out.host, "mozz.us");
    CHECK_STR(out.path, "/ganz/anders");

    /* Ohne führenden Schrägstrich relativ zum Verzeichnis, nicht zur Seite. */
    REQUIRE(spartan_resolve(&base, "nachbar.gmi", &out));
    CHECK_STR(out.path, "/ordner/nachbar.gmi");

    REQUIRE(spartan_resolve(&base, "unten/tiefer.gmi", &out));
    CHECK_STR(out.path, "/ordner/unten/tiefer.gmi");
}

TEST(dots_in_a_path_are_resolved_here_not_by_the_server)
{
    spartan_url base, out;
    REQUIRE(spartan_parse_url("spartan://x.org/a/b/seite.gmi", &base));

    REQUIRE(spartan_resolve(&base, "../oben.gmi", &out));
    CHECK_STR(out.path, "/a/oben.gmi");

    REQUIRE(spartan_resolve(&base, "./hier.gmi", &out));
    CHECK_STR(out.path, "/a/b/hier.gmi");

    /* Über die Wurzel hinaus geht es nicht. Ein Pfad wie "/../../etc" darf
     * nicht als solcher beim Server ankommen. */
    REQUIRE(spartan_resolve(&base, "../../../../../ganz/oben", &out));
    CHECK_STR(out.path, "/ganz/oben");
}

TEST(the_trailing_slash_survives)
{
    /* Für einen Server sind „/a/b" und „/a/b/" zweierlei - das eine ist eine
     * Seite, das andere ein Verzeichnis. */
    spartan_url base, out;
    REQUIRE(spartan_parse_url("spartan://x.org/a/seite", &base));

    REQUIRE(spartan_resolve(&base, "unter/", &out));
    CHECK_STR(out.path, "/a/unter/");

    REQUIRE(spartan_resolve(&base, "unter", &out));
    CHECK_STR(out.path, "/a/unter");
}

TEST(an_absolute_link_goes_somewhere_else_entirely)
{
    spartan_url base, out;
    REQUIRE(spartan_parse_url("spartan://x.org/a/", &base));

    REQUIRE(spartan_resolve(&base, "spartan://y.org:301/z", &out));
    CHECK_STR(out.host, "y.org");
    CHECK_EQ(out.port, 301);
    CHECK_STR(out.path, "/z");

    /* Und ein fremdes Schema bleibt abgelehnt, auch als Verweis. */
    CHECK(!spartan_resolve(&base, "https://y.org/", &out));
    CHECK(!spartan_resolve(&base, "", &out));
    CHECK(!spartan_resolve(&base, NULL, &out));
}

/* --- Anfrage bauen ------------------------------------------------------------------ */

TEST(the_request_line_is_three_words_and_a_crlf)
{
    spartan_url u;
    REQUIRE(spartan_parse_url("spartan://mozz.us/a.gmi", &u));

    char out[256];
    REQUIRE(spartan_build_request(&u, 0, out, sizeof out));
    CHECK_STR(out, "mozz.us /a.gmi 0\r\n");

    REQUIRE(spartan_build_request(&u, 42, out, sizeof out));
    CHECK_STR(out, "mozz.us /a.gmi 42\r\n");

    /* Passt sie nicht, wird nichts geschickt statt etwas Halbes. */
    char small[8];
    CHECK(!spartan_build_request(&u, 0, small, sizeof small));
}

/* --- Antwort lesen -------------------------------------------------------------------- */

static bool parse(const char *text, spartan_response *r)
{
    char err[256] = "";
    if (spartan_parse_response(text, strlen(text), r, err, sizeof err)) return true;

    printf("  %s\n", err);
    return false;
}

TEST(a_successful_response_comes_apart)
{
    spartan_response r;
    REQUIRE(parse("2 text/gemini\r\n# Titel\nZeile\n", &r));

    CHECK_EQ(r.status, SPARTAN_SUCCESS);
    CHECK_STR(r.meta, "text/gemini");
    CHECK_EQ(r.body_len, strlen("# Titel\nZeile\n"));
    CHECK(strncmp(r.body, "# Titel", 7) == 0);
}

TEST(a_lone_newline_is_tolerated)
{
    /* Die Spezifikation verlangt CRLF. Selbstgeschriebene Server liefern oft
     * nur LF, und nachsichtig zu sein kostet hier nichts. */
    spartan_response r;
    REQUIRE(parse("2 text/gemini\nHallo\n", &r));
    CHECK_STR(r.meta, "text/gemini");
    CHECK_STR(r.body, "Hallo\n");
}

TEST(the_other_three_statuses_come_through)
{
    spartan_response r;

    REQUIRE(parse("3 spartan://x.org/woanders\r\n", &r));
    CHECK_EQ(r.status, SPARTAN_REDIRECT);
    CHECK_STR(r.meta, "spartan://x.org/woanders");
    CHECK_EQ(r.body_len, 0u);

    REQUIRE(parse("4 gibt es nicht\r\n", &r));
    CHECK_EQ(r.status, SPARTAN_CLIENT_ERROR);
    CHECK_STR(r.meta, "gibt es nicht");

    REQUIRE(parse("5 kaputt\r\n", &r));
    CHECK_EQ(r.status, SPARTAN_SERVER_ERROR);
}

TEST(a_status_without_a_meta_is_allowed)
{
    spartan_response r;
    REQUIRE(parse("2\r\nnur der Rumpf\n", &r));
    CHECK_EQ(r.status, 2);
    CHECK_STR(r.meta, "");
    CHECK_STR(r.body, "nur der Rumpf\n");
}

TEST(a_broken_status_line_is_refused)
{
    spartan_response r;
    char             err[256] = "";

    CHECK(!spartan_parse_response("2 text/gemini", 13, &r, err, sizeof err));
    CHECK(strstr(err, "Statuszeile") != NULL);

    CHECK(!spartan_parse_response("9 was?\r\n", 8, &r, err, sizeof err));
    CHECK(strstr(err, "Status") != NULL);

    CHECK(!spartan_parse_response("x\r\n", 3, &r, err, sizeof err));
    CHECK(!spartan_parse_response("", 0, &r, err, sizeof err));
}

/* --- Abrufen über einen erfundenen Transport ------------------------------------------- */

typedef struct {
    const char *reply;      /* was die Gegenseite schickt */
    size_t      pos;

    char sent[512];         /* was wir geschickt haben */
    size_t sent_len;

    bool open_fails;
    bool send_fails;
    bool recv_fails;
    int  chunk;             /* wie viele Bytes je Aufruf höchstens */

    char host[128];
    int  port;
    bool closed;
    bool asked_for_nothing;
} fake;

static bool fake_open(void *user, const char *host, int port, char *err, size_t n)
{
    fake *f = user;
    snprintf(f->host, sizeof f->host, "%s", host);
    f->port = port;

    if (f->open_fails) {
        snprintf(err, n, "kein Anschluss unter dieser Nummer");
        return false;
    }
    return true;
}

static bool fake_send(void *user, const char *data, size_t len)
{
    fake *f = user;
    if (f->send_fails) return false;

    if (f->sent_len + len < sizeof f->sent) {
        memcpy(f->sent + f->sent_len, data, len);
        f->sent_len += len;
        f->sent[f->sent_len] = '\0';
    }
    return true;
}

static long fake_recv(void *user, char *buf, size_t cap)
{
    fake *f = user;
    if (f->recv_fails) return -1;

    /* Ein Transport, der null Bytes erbitten soll, ist ein Fehler beim
     * Aufrufer - manche echten Umsetzungen warten dann ewig. Hier fällt es
     * auf, statt sich als hängender Test zu zeigen. */
    if (cap == 0) { f->asked_for_nothing = true; return 0; }

    size_t left = strlen(f->reply) - f->pos;
    if (left == 0) return 0;

    /* Stückweise liefern - so wie ein Netz es tut. Wer nur einen einzigen
     * großen Block prüft, prüft nicht, ob die Schleife stimmt. */
    size_t n = left;
    if (f->chunk > 0 && n > (size_t)f->chunk) n = (size_t)f->chunk;
    if (n > cap) n = cap;

    memcpy(buf, f->reply + f->pos, n);
    f->pos += n;
    return (long)n;
}

static void fake_close(void *user)
{
    ((fake *)user)->closed = true;
}

static spartan_transport fake_transport(fake *f)
{
    spartan_transport t = { f, fake_open, fake_send, fake_recv, fake_close };
    return t;
}

TEST(fetching_sends_a_request_and_reads_the_answer)
{
    fake f = { 0 };
    f.reply = "2 text/gemini\r\n# Hallo\n=> /weiter Mehr\n";
    f.chunk = 5;                      /* absichtlich kleinhäckseln */

    spartan_transport t = fake_transport(&f);

    spartan_url u;
    REQUIRE(spartan_parse_url("spartan://mozz.us/start.gmi", &u));

    char             buf[1024];
    spartan_response r;
    char             err[256] = "";

    /* Einmal aufrufen, nicht zweimal: der erfundene Transport gibt seine
     * Antwort nur ein Mal her, wie ein echter auch. */
    bool ok = spartan_fetch(&t, &u, buf, sizeof buf, &r, err, sizeof err);
    if (!ok) printf("  %s\n", err);
    REQUIRE(ok);

    CHECK_STR(f.host, "mozz.us");
    CHECK_EQ(f.port, SPARTAN_PORT);
    CHECK(strstr(f.sent, "mozz.us /start.gmi 0\r\n") != NULL);

    CHECK_EQ(r.status, 2);
    CHECK_STR(r.meta, "text/gemini");
    CHECK(strstr(r.body, "# Hallo") != NULL);
    CHECK(f.closed);
}

TEST(a_redirect_is_reported_and_not_followed)
{
    /* Einer Umleitung stillschweigend zu folgen hieße, dem Nutzer zu
     * verschweigen, wo er gelandet ist - und zwei Server, die aufeinander
     * zeigen, ergäben ein Aufhängen statt einer Meldung. */
    fake f = { 0 };
    f.reply = "3 spartan://x.org/woanders\r\n";

    spartan_transport t = fake_transport(&f);

    spartan_url u;
    REQUIRE(spartan_parse_url("spartan://mozz.us/start", &u));

    char             buf[512];
    spartan_response r;
    char             err[256] = "";
    REQUIRE(spartan_fetch(&t, &u, buf, sizeof buf, &r, err, sizeof err));

    CHECK_EQ(r.status, SPARTAN_REDIRECT);
    CHECK_STR(r.meta, "spartan://x.org/woanders");

    /* Und die neue Adresse lässt sich auflösen - das ist die Arbeit des
     * Aufrufers, nicht des Protokolls. */
    spartan_url next;
    REQUIRE(spartan_resolve(&u, r.meta, &next));
    CHECK_STR(next.host, "x.org");
}

TEST(a_transport_that_fails_says_so)
{
    spartan_url u;
    REQUIRE(spartan_parse_url("spartan://x.org/", &u));

    char             buf[256];
    spartan_response r;
    char             err[256] = "";

    fake f = { 0 };
    f.reply = "2 x\r\n";
    f.open_fails = true;
    spartan_transport t = fake_transport(&f);
    CHECK(!spartan_fetch(&t, &u, buf, sizeof buf, &r, err, sizeof err));
    CHECK(strstr(err, "Anschluss") != NULL);
    CHECK(!f.closed);                 /* was nicht offen war, wird nicht zu */

    fake g = { 0 };
    g.reply = "2 x\r\n";
    g.send_fails = true;
    t = fake_transport(&g);
    CHECK(!spartan_fetch(&t, &u, buf, sizeof buf, &r, err, sizeof err));
    CHECK(strstr(err, "Anfrage") != NULL);
    CHECK(g.closed);                  /* was offen war, schon */

    fake h = { 0 };
    h.reply = "2 x\r\n";
    h.recv_fails = true;
    t = fake_transport(&h);
    CHECK(!spartan_fetch(&t, &u, buf, sizeof buf, &r, err, sizeof err));
    CHECK(strstr(err, "Verbindung") != NULL);
    CHECK(h.closed);
}

TEST(a_silent_server_is_an_error_not_an_empty_page)
{
    fake f = { 0 };
    f.reply = "";

    spartan_transport t = fake_transport(&f);

    spartan_url u;
    REQUIRE(spartan_parse_url("spartan://x.org/", &u));

    char             buf[256];
    spartan_response r;
    char             err[256] = "";
    CHECK(!spartan_fetch(&t, &u, buf, sizeof buf, &r, err, sizeof err));
    CHECK(strstr(err, "nichts") != NULL);
}

TEST(a_page_larger_than_the_buffer_is_cut_off_not_refused)
{
    /* Eine halbe Seite ist mehr wert als eine Fehlermeldung, und der Nutzer
     * sieht am abgeschnittenen Ende, dass etwas fehlt. */
    fake f = { 0 };
    f.reply = "2 text/gemini\r\n"
              "0123456789012345678901234567890123456789"
              "0123456789012345678901234567890123456789";
    f.chunk = 7;

    spartan_transport t = fake_transport(&f);

    spartan_url u;
    REQUIRE(spartan_parse_url("spartan://x.org/", &u));

    char             buf[40];
    spartan_response r;
    char             err[256] = "";
    REQUIRE(spartan_fetch(&t, &u, buf, sizeof buf, &r, err, sizeof err));

    CHECK_EQ(r.status, 2);
    CHECK(r.body_len > 0);
    CHECK(r.body_len < 40u);
    CHECK(f.closed);

    /* Und dabei wurde nie um null Bytes gebeten. */
    CHECK(!f.asked_for_nothing);
}

TEST(without_a_transport_nothing_happens)
{
    spartan_url u;
    REQUIRE(spartan_parse_url("spartan://x.org/", &u));

    char             buf[64];
    spartan_response r;
    char             err[256] = "";

    CHECK(!spartan_fetch(NULL, &u, buf, sizeof buf, &r, err, sizeof err));
    CHECK(strstr(err, "Transport") != NULL);

    spartan_transport empty = { 0 };
    CHECK(!spartan_fetch(&empty, &u, buf, sizeof buf, &r, err, sizeof err));
}

int main(void)
{
    RUN(a_full_address_comes_apart);
    RUN(the_scheme_may_be_left_out);
    RUN(another_scheme_is_refused);
    RUN(a_port_is_taken_along);
    RUN(nonsense_addresses_are_refused);
    RUN(an_address_goes_back_together);

    RUN(a_relative_link_stays_on_the_same_host);
    RUN(dots_in_a_path_are_resolved_here_not_by_the_server);
    RUN(the_trailing_slash_survives);
    RUN(an_absolute_link_goes_somewhere_else_entirely);

    RUN(the_request_line_is_three_words_and_a_crlf);

    RUN(a_successful_response_comes_apart);
    RUN(a_lone_newline_is_tolerated);
    RUN(the_other_three_statuses_come_through);
    RUN(a_status_without_a_meta_is_allowed);
    RUN(a_broken_status_line_is_refused);

    RUN(fetching_sends_a_request_and_reads_the_answer);
    RUN(a_redirect_is_reported_and_not_followed);
    RUN(a_transport_that_fails_says_so);
    RUN(a_silent_server_is_an_error_not_an_empty_page);
    RUN(a_page_larger_than_the_buffer_is_cut_off_not_refused);
    RUN(without_a_transport_nothing_happens);

    return test_summary();
}
