/* Siehe plat_transport.h. */
#include "net/plat_transport.h"

#include <stddef.h>

#include "plat/plat.h"

/* Der offene Anschluss. Er liegt in einer Dateiglobalen und nicht im
 * Transport, weil der als Wert weitergereicht wird - eine Kopie mit einem
 * eigenen Zeiger darauf wäre eine zweite Verbindung, die es nicht gibt.
 *
 * Damit kann nur ein Abruf zugleich laufen, und genau das steht im Vertrag.
 * Für ein Programm mit einem Fenster und einer Adresszeile ist das keine
 * Einschränkung; wer je zwei zugleich braucht, gibt dem Transport ein
 * user-Feld und dieser Datei ihren letzten Zustand ab. */
static plat_socket *s_sock;

static bool t_open(void *user, const char *host, int port, char *err, size_t err_size)
{
    (void)user;

    if (s_sock) plat_disconnect(s_sock);
    s_sock = plat_connect(host, port, err, err_size);
    return s_sock != NULL;
}

static bool t_send(void *user, const char *data, size_t len)
{
    (void)user;
    return plat_send(s_sock, data, len);
}

static long t_recv(void *user, char *buf, size_t cap)
{
    (void)user;
    return plat_recv(s_sock, buf, cap);
}

static void t_close(void *user)
{
    (void)user;
    plat_disconnect(s_sock);
    s_sock = NULL;
}

spartan_transport plat_spartan_transport(void)
{
    spartan_transport t = { NULL, t_open, t_send, t_recv, t_close };
    return t;
}
