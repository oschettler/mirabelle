/* Siehe caret.h. */
#include "ui/caret.h"

static bool     on      = true;
static bool     started = false;
static bool     woke    = false;
static uint32_t since   = 0;

void caret_tick(uint32_t now_ms)
{
    if (!started || woke) {
        started = true;
        woke    = false;
        on      = true;
        since   = now_ms;
        return;
    }

    /* Vorzeichenlos gerechnet, damit der Überlauf der Uhr nach 49 Tagen
     * nichts anderes bedeutet als der Schritt davor. */
    uint32_t elapsed = now_ms - since;
    if (elapsed < CARET_BLINK_MS) return;

    /* Aus einem Sprung über viele Halbperioden zählt nur, ob es ungerade
     * viele waren. Eine Schleife über Millionen Schritte wäre die Alternative. */
    if ((elapsed / CARET_BLINK_MS) % 2 != 0) on = !on;
    since = now_ms - elapsed % CARET_BLINK_MS;
}

bool caret_on(void)
{
    return on;
}

void caret_wake(void)
{
    on   = true;
    woke = true;
}

void caret_reset(void)
{
    on      = true;
    started = false;
    woke    = false;
    since   = 0;
}
