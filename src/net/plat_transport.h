/* Der Transport, der über plat.h geht - die Brücke zwischen dem Protokoll und
 * der Plattformschicht.
 *
 * Eine eigene, winzige Datei, damit net/spartan.c weiterhin nichts von plat.h
 * weiß. Das Protokoll bleibt reine Rechnung auf Bytes und lässt sich ohne
 * Netz prüfen; wer es wirklich benutzen will, holt sich hier den Transport.
 */
#ifndef PDA_NET_PLAT_TRANSPORT_H
#define PDA_NET_PLAT_TRANSPORT_H

#include "net/spartan.h"

/* Liefert einen Transport auf Basis von plat_connect() und Verwandten.
 *
 * Der Zustand steckt im Transport selbst; er darf nur für einen Abruf zugleich
 * benutzt werden. Zwei Seiten gleichzeitig zu holen hieße, zwei davon zu
 * nehmen. */
spartan_transport plat_spartan_transport(void);

#endif /* PDA_NET_PLAT_TRANSPORT_H */
