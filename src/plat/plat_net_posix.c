/* Die Netzfunktionen aus plat.h für alles mit BSD-Sockets.
 *
 * Wie plat_files_posix.c eine eigene Datei: der ESP32 bekommt später seine
 * eigene über lwIP und bindet diese einfach nicht ein.
 *
 * Bewusst schlicht: aufbauen, schreiben, lesen, zumachen. Kein Zeitlimit
 * einzustellen wäre für ein Programm, das im Hintergrund läuft, zu wenig -
 * deshalb steht eines drin, und zwar für beide Richtungen. Ein Server, der
 * nicht antwortet, soll die Oberfläche nicht einfrieren.
 */
#include "plat/plat.h"

#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define TIMEOUT_SECONDS 10

struct plat_socket {
    int fd;
};

plat_socket *plat_connect(const char *host, int port, char *err, size_t err_size)
{
    char service[16];
    snprintf(service, sizeof service, "%d", port);

    struct addrinfo hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_UNSPEC;      /* IPv4 und IPv6 sind beide recht */
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *list = NULL;
    int rc = getaddrinfo(host, service, &hints, &list);
    if (rc != 0 || !list) {
        if (err && err_size)
            snprintf(err, err_size, "%s: %s", host, gai_strerror(rc));
        return NULL;
    }

    int fd = -1;
    for (struct addrinfo *a = list; a; a = a->ai_next) {
        fd = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (fd < 0) continue;

        struct timeval tv = { TIMEOUT_SECONDS, 0 };
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);

        if (connect(fd, a->ai_addr, a->ai_addrlen) == 0) break;

        close(fd);
        fd = -1;
    }
    freeaddrinfo(list);

    if (fd < 0) {
        if (err && err_size)
            snprintf(err, err_size, "%s:%d: %s", host, port, strerror(errno));
        return NULL;
    }

    plat_socket *s = calloc(1, sizeof *s);
    if (!s) {
        close(fd);
        if (err && err_size) snprintf(err, err_size, "kein Speicher");
        return NULL;
    }

    s->fd = fd;
    if (err && err_size) err[0] = '\0';
    return s;
}

bool plat_send(plat_socket *s, const char *data, size_t len)
{
    if (!s) return false;

    size_t sent = 0;
    while (sent < len) {
        /* MSG_NOSIGNAL gibt es nicht überall; stattdessen wird EPIPE als
         * gewöhnlicher Fehler behandelt. Ein Signal, das das ganze Programm
         * beendet, weil eine Seite nicht mehr da ist, wäre die falsche
         * Reaktion. */
        ssize_t n = send(s->fd, data + sent, len - sent, 0);
        if (n <= 0) return false;
        sent += (size_t)n;
    }
    return true;
}

long plat_recv(plat_socket *s, char *buf, size_t cap)
{
    if (!s || cap == 0) return -1;

    ssize_t n = recv(s->fd, buf, cap, 0);
    if (n < 0) return -1;
    return (long)n;
}

void plat_disconnect(plat_socket *s)
{
    if (!s) return;
    if (s->fd >= 0) close(s->fd);
    free(s);
}
