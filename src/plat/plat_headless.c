/* Die Headless-Umsetzung der Plattformschicht.
 *
 * Sie ist keine Notlösung, sondern der Grund, warum sich fast alles ohne
 * Bildschirm prüfen lässt: das Bild landet in einer Bitmap, Ereignisse kommen
 * aus einer Warteschlange, die der Test füllt, und die Uhr steht still, bis
 * jemand sie stellt. Nichts davon hängt an einer Systemuhr oder an einem
 * Fenster, also ist jeder Lauf gleich.
 *
 * Die Dateifunktionen liegen in plat_files_posix.c.
 */

#include "plat.h"

#include <string.h>

#define QUEUE_MAX 256

static bitmap   s_frame;
static bool     s_ready;
static int      s_w, s_h;
static int      s_present_count;
static uint32_t s_now;

static event s_queue[QUEUE_MAX];
static int   s_head;
static int   s_count;

/* --- Steuerung für Tests ------------------------------------------------- */

/* Läuft die Warteschlange über, wird das neue Ereignis verworfen. Ein Test,
 * der 256 Ereignisse am Stück einreiht, ohne sie abzuholen, tut ohnehin etwas
 * Ungewöhnliches. */
void plat_headless_push_event(const event *e)
{
    if (s_count >= QUEUE_MAX) return;
    s_queue[(s_head + s_count) % QUEUE_MAX] = *e;
    s_count++;
}

const bitmap *plat_headless_frame(void)
{
    return &s_frame;
}

int plat_headless_present_count(void)
{
    return s_present_count;
}

void plat_headless_set_time(uint32_t ms)
{
    s_now = ms;
}

/* --- Die dreizehn Funktionen --------------------------------------------- */

bool plat_init(const plat_config *cfg)
{
    s_w = (cfg && cfg->width  > 0) ? cfg->width  : 800;
    s_h = (cfg && cfg->height > 0) ? cfg->height : 480;

    if (!bitmap_init(&s_frame, s_w, s_h)) return false;

    s_ready         = true;
    s_present_count = 0;
    s_now           = 0;
    s_head          = 0;
    s_count         = 0;
    return true;
}

void plat_shutdown(void)
{
    if (!s_ready) return;
    bitmap_free(&s_frame);
    s_ready = false;
}

void plat_display_size(int *w, int *h)
{
    if (w) *w = s_w;
    if (h) *h = s_h;
}

void plat_present(const bitmap *fb)
{
    s_present_count++;
    if (fb->w == s_frame.w && fb->h == s_frame.h)
        memcpy(s_frame.bits, fb->bits, bitmap_bytes(fb));
}

bool plat_poll(event *out)
{
    if (s_count == 0) return false;

    *out   = s_queue[s_head];
    s_head = (s_head + 1) % QUEUE_MAX;
    s_count--;
    return true;
}

uint32_t plat_ticks_ms(void)
{
    return s_now;
}

/* Schläft nicht, sondern rückt die Uhr vor. Sonst wären Tests langsam und
 * trotzdem nicht deterministischer. */
void plat_sleep_ms(uint32_t ms)
{
    s_now += ms;
}
