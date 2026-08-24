/* Das SPARTAN://-Protokoll.
 *
 * Ein Protokoll, das auf eine Seite passt: eine Anfragezeile hin, eine
 * Statuszeile und ein Rumpf zurück, Verbindung zu. Kein Zustand, keine
 * Aushandlung, keine Kopfzeilen. Gerade deshalb ist es ein gutes erstes
 * Netzprotokoll - man kann es vollständig lesen und vollständig prüfen.
 *
 *     Anfrage:  <host> <pfad> <laenge>\r\n [danach <laenge> Bytes Rumpf]
 *     Antwort:  <ziffer> <angabe>\r\n [danach der Rumpf]
 *
 * Die Ziffer ist 2 (hier ist es), 3 (anderswo), 4 (dein Fehler) oder
 * 5 (mein Fehler). Bei 2 sagt die Angabe den Inhaltstyp, bei 3 die neue
 * Adresse, sonst ist sie eine Meldung für den Menschen.
 *
 * ## Warum hier keine Netzfunktion steht
 *
 * Alles in dieser Datei ist reine Rechnung auf Bytes: Adresse zerlegen,
 * Anfrage bauen, Antwort lesen. Kein Aufruf geht nach draußen. Der Transport
 * wird übergeben (siehe spartan_fetch), und ein Test schiebt dort
 * vorbereitete Bytes hinein.
 *
 * Das ist nicht Umständlichkeit, sondern der einzige Weg, ein Protokoll
 * verlässlich zu prüfen. Ein Test, der einen Server braucht, prüft an guten
 * Tagen das Protokoll und an schlechten das Netz.
 */
#ifndef PDA_NET_SPARTAN_H
#define PDA_NET_SPARTAN_H

#include <stdbool.h>
#include <stddef.h>

#define SPARTAN_HOST_MAX 128
#define SPARTAN_PATH_MAX 512
#define SPARTAN_URL_MAX  (SPARTAN_HOST_MAX + SPARTAN_PATH_MAX + 32)
#define SPARTAN_META_MAX 256

#define SPARTAN_PORT 300   /* die Voreinstellung des Protokolls */

/* --- Adressen ------------------------------------------------------------------- */

typedef struct {
    char host[SPARTAN_HOST_MAX];
    int  port;                        /* SPARTAN_PORT, wenn keiner dasteht */
    char path[SPARTAN_PATH_MAX];      /* beginnt immer mit "/" */
} spartan_url;

/* Zerlegt "spartan://host[:port]/pfad".
 *
 * Das Schema darf fehlen - "host/pfad" wird genauso verstanden, weil ein
 * Mensch es so eintippt. Ein anderes Schema wird abgelehnt: eine http-Adresse
 * hier stillschweigend als SPARTAN zu behandeln, führte zu einer Anfrage, die
 * niemand versteht.
 *
 * false, wenn kein Rechnername herauskommt oder etwas nicht passt. */
bool spartan_parse_url(const char *url, spartan_url *out);

/* Setzt eine Adresse wieder zusammen, mit Schema und - falls abweichend - Port. */
bool spartan_format_url(const spartan_url *u, char *out, size_t out_size);

/* Löst eine Adresse relativ zu einer Seite auf, so wie ein Verweis in einer
 * Gemtext-Zeile es verlangt:
 *
 *     "/anders"      -> derselbe Rechner, anderer Pfad
 *     "unten"        -> relativ zum Verzeichnis der Seite
 *     "spartan://x/" -> ganz woandershin
 *
 * false, wenn nichts Brauchbares herauskommt. */
bool spartan_resolve(const spartan_url *base, const char *href, spartan_url *out);

/* --- Anfrage und Antwort ---------------------------------------------------------- */

/* Baut die Anfragezeile einschließlich CRLF. body_len ist die Länge des
 * Rumpfes, den der Aufrufer danach sendet - null, wenn er nichts sendet.
 *
 * false, wenn out zu klein ist. */
bool spartan_build_request(const spartan_url *u, size_t body_len,
                           char *out, size_t out_size);

typedef enum {
    SPARTAN_SUCCESS  = 2,
    SPARTAN_REDIRECT = 3,
    SPARTAN_CLIENT_ERROR = 4,
    SPARTAN_SERVER_ERROR = 5
} spartan_status;

typedef struct {
    int  status;                    /* 2 bis 5 */
    char meta[SPARTAN_META_MAX];    /* Inhaltstyp, neue Adresse oder Meldung */

    /* Zeigt in den übergebenen Puffer, nicht in einen eigenen. */
    const char *body;
    size_t      body_len;
} spartan_response;

/* Zerlegt eine vollständige Antwort.
 *
 * Die Statuszeile endet laut Spezifikation mit CRLF; ein einzelnes LF wird
 * auch genommen, weil selbstgeschriebene Server es liefern und die
 * Nachsicht hier nichts kostet.
 *
 * false und eine Meldung in err, wenn die Statuszeile fehlt oder unsinnig
 * ist. Ein leerer Rumpf ist dagegen in Ordnung. */
bool spartan_parse_response(const char *data, size_t len, spartan_response *out,
                            char *err, size_t err_size);

/* --- Abrufen ---------------------------------------------------------------------
 *
 * Der Transport ist ein Rückruf, kein fester Aufruf. Für das Programm ist er
 * plat_net (plat.h), für einen Test ein Stück Speicher.
 *
 * send_all schickt len Bytes und liefert false, wenn das nicht ganz gelingt.
 * recv_some legt bis zu cap Bytes in buf und liefert die Anzahl; 0 heißt, die
 * Gegenseite hat geschlossen, negativ heißt Fehler.
 */
typedef struct {
    void *user;
    bool  (*open)(void *user, const char *host, int port, char *err, size_t err_size);
    bool  (*send_all)(void *user, const char *data, size_t len);
    long  (*recv_some)(void *user, char *buf, size_t cap);
    void  (*close)(void *user);
} spartan_transport;

/* Ruft eine Seite ab. Der Rumpf landet in buf; *out zeigt hinein.
 *
 * Folgt Weiterleitungen NICHT von selbst - der Aufrufer sieht Status 3 und die
 * neue Adresse in meta und entscheidet. Eine Umleitung stillschweigend zu
 * folgen hieße, dem Nutzer zu verschweigen, wo er gelandet ist, und eine
 * Schleife zwischen zwei Servern würde ein Aufhängen statt einer Meldung.
 *
 * false und eine Meldung in err bei einem Transportfehler oder einer
 * unsinnigen Antwort. */
bool spartan_fetch(const spartan_transport *t, const spartan_url *u,
                   char *buf, size_t buf_size, spartan_response *out,
                   char *err, size_t err_size);

#endif /* PDA_NET_SPARTAN_H */
