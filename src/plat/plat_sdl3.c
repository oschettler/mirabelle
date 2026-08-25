/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Die Plattformschicht auf SDL3.
 *
 * Zwei Dinge überlässt diese Umsetzung bewusst SDL, statt sie selbst zu
 * rechnen: die ganzzahlige Vergrößerung und die Umrechnung der
 * Mauskoordinaten. SDL_LOGICAL_PRESENTATION_INTEGER_SCALE hält die logische
 * Auflösung fest und vergrößert nur die Anzeige - genau, was Entscheidung D-9
 * verlangt -, und SDL_ConvertEventToRenderCoordinates rechnet Ereignisse in
 * dieselbe logische Ebene zurück. Damit entfällt die gesamte Fehlerklasse
 * "stimmt bei einfacher Vergrößerung, verschiebt sich bei doppelter".
 *
 * Die Dateifunktionen liegen in plat_files_posix.c.
 */

#include "plat.h"

#include <SDL3/SDL.h>
#include <string.h>

#include "expand.h"

#define TEXT_QUEUE_MAX 16

static SDL_Window   *s_window;
static SDL_Renderer *s_renderer;
static SDL_Texture  *s_texture;
static int           s_w, s_h;
static expand_table  s_expand;

/* Ein Textereignis von SDL kann mehrere Codepunkte auf einmal liefern - etwa
 * beim Einfügen oder bei manchen Eingabemethoden. Unser event fasst genau
 * einen, also werden die übrigen hier zwischengelagert. */
static event s_text_queue[TEXT_QUEUE_MAX];
static int   s_text_head, s_text_count;

/* Die größte ganzzahlige Vergrößerung, mit der das Fenster noch bequem auf den
 * Bildschirm passt. "Bequem" heißt: nicht mehr als 85 Prozent der nutzbaren
 * Fläche, damit Menüleiste, Dock und Fensterrahmen Platz behalten.
 *
 * Ohne das stand die Vergrößerung fest auf 2, also 1600 x 960 - mehr, als auf
 * ein Notebookdisplay passt. */
static int auto_scale(int w, int h)
{
    SDL_Rect usable;
    if (!SDL_GetDisplayUsableBounds(SDL_GetPrimaryDisplay(), &usable))
        return 1;

    int max_w = usable.w * 85 / 100;
    int max_h = usable.h * 85 / 100;

    int scale = 1;
    while (scale < 3 && w * (scale + 1) <= max_w && h * (scale + 1) <= max_h)
        scale++;

    return scale;
}

static uint16_t rgb565(int r, int g, int b)
{
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

bool plat_init(const plat_config *cfg)
{
    s_w = (cfg && cfg->width  > 0) ? cfg->width  : 800;
    s_h = (cfg && cfg->height > 0) ? cfg->height : 480;

    if (!SDL_Init(SDL_INIT_VIDEO)) return false;

    int scale = (cfg && cfg->scale > 0) ? cfg->scale : auto_scale(s_w, s_h);
    if (scale < 1) scale = 1;
    if (scale > 3) scale = 3;

    const char *title = (cfg && cfg->title) ? cfg->title : "mirabelle";
    if (!SDL_CreateWindowAndRenderer(title, s_w * scale, s_h * scale, 0,
                                     &s_window, &s_renderer)) {
        SDL_Quit();
        return false;
    }

    SDL_SetRenderLogicalPresentation(s_renderer, s_w, s_h,
                                     SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);

    s_texture = SDL_CreateTexture(s_renderer, SDL_PIXELFORMAT_RGB565,
                                  SDL_TEXTUREACCESS_STREAMING, s_w, s_h);
    if (!s_texture) {
        SDL_DestroyRenderer(s_renderer);
        SDL_DestroyWindow(s_window);
        SDL_Quit();
        s_renderer = NULL;
        s_window   = NULL;
        return false;
    }

    /* Ein Bild aus einem Bit je Pixel verträgt keine Glättung. */
    SDL_SetTextureScaleMode(s_texture, SDL_SCALEMODE_NEAREST);

    expand_table_init(&s_expand, rgb565(0x10, 0x14, 0x18), rgb565(0xF2, 0xF2, 0xEC));

    s_text_head  = 0;
    s_text_count = 0;

    SDL_StartTextInput(s_window);
    return true;
}

void plat_shutdown(void)
{
    if (s_texture)  SDL_DestroyTexture(s_texture);
    if (s_renderer) SDL_DestroyRenderer(s_renderer);
    if (s_window)   SDL_DestroyWindow(s_window);

    s_texture  = NULL;
    s_renderer = NULL;
    s_window   = NULL;
    SDL_Quit();
}

void plat_display_size(int *w, int *h)
{
    if (w) *w = s_w;
    if (h) *h = s_h;
}

void plat_present(const bitmap *fb)
{
    void *pixels = NULL;
    int   pitch  = 0;

    if (!SDL_LockTexture(s_texture, NULL, &pixels, &pitch)) return;
    expand_rows(&s_expand, fb, 0, fb->h - 1, (uint16_t *)pixels,
                pitch / (int)sizeof(uint16_t));
    SDL_UnlockTexture(s_texture);

    SDL_RenderClear(s_renderer);
    SDL_RenderTexture(s_renderer, s_texture, NULL, NULL);
    SDL_RenderPresent(s_renderer);
}

/* --- Eingabe ------------------------------------------------------------- */

static uint8_t mods_from_sdl(SDL_Keymod m)
{
    uint8_t r = 0;
    if (m & SDL_KMOD_SHIFT) r |= MOD_SHIFT;
    if (m & SDL_KMOD_CTRL)  r |= MOD_CTRL;
    if (m & SDL_KMOD_ALT)   r |= MOD_ALT;
    if (m & SDL_KMOD_GUI)   r |= MOD_CMD;

#ifndef SDL_PLATFORM_MACOS
    /* Außerhalb des Mac gibt es keine Befehlstaste. Damit ein Kürzel überall
     * als Cmd+N geschrieben werden kann, gilt dort Strg zusätzlich als Cmd. */
    if (m & SDL_KMOD_CTRL) r |= MOD_CMD;
#endif
    return r;
}

static int key_from_sdl(SDL_Keycode k)
{
    switch (k) {
    case SDLK_UP:       return KEY_UP;
    case SDLK_DOWN:     return KEY_DOWN;
    case SDLK_LEFT:     return KEY_LEFT;
    case SDLK_RIGHT:    return KEY_RIGHT;
    case SDLK_HOME:     return KEY_HOME;
    case SDLK_END:      return KEY_END;
    case SDLK_PAGEUP:   return KEY_PAGE_UP;
    case SDLK_PAGEDOWN: return KEY_PAGE_DOWN;
    case SDLK_DELETE:   return KEY_DELETE;
    case SDLK_F1:  return KEY_F1;   case SDLK_F2:  return KEY_F2;
    case SDLK_F3:  return KEY_F3;   case SDLK_F4:  return KEY_F4;
    case SDLK_F5:  return KEY_F5;   case SDLK_F6:  return KEY_F6;
    case SDLK_F7:  return KEY_F7;   case SDLK_F8:  return KEY_F8;
    case SDLK_F9:  return KEY_F9;   case SDLK_F10: return KEY_F10;
    case SDLK_F11: return KEY_F11;  case SDLK_F12: return KEY_F12;
    default: break;
    }

    /* Druckbare Tasten tragen ihren layoutabhängigen Codepunkt. SDL_Keycode
     * ist bereits layoutabhängig, also übernehmen wir ihn unverändert - nur
     * Buchstaben werden kleingeschrieben, damit ein Kürzel nicht davon
     * abhängt, ob die Umschalttaste gedrückt war. */
    if (k >= 'A' && k <= 'Z') return k - 'A' + 'a';
    if (k < KEY_SPECIAL)      return (int)k;
    return 0;
}

/* Zerlegt die UTF-8-Kette eines Textereignisses in einzelne Codepunkte und
 * reiht sie ein. Die Kette wird byteweise an den Folgebytes getrennt; eine
 * Prüfung auf Gültigkeit braucht es nicht, SDL liefert gültiges UTF-8. */
static void queue_text(const char *s)
{
    while (*s && s_text_count < TEXT_QUEUE_MAX) {
        int n = 1;
        while (s[n] && ((unsigned char)s[n] & 0xC0u) == 0x80u) n++;
        if (n > 7) return;

        event e = { .kind = EV_TEXT };
        memcpy(e.text, s, (size_t)n);
        e.text[n] = '\0';

        s_text_queue[(s_text_head + s_text_count) % TEXT_QUEUE_MAX] = e;
        s_text_count++;
        s += n;
    }
}

bool plat_poll(event *out)
{
    if (s_text_count > 0) {
        *out        = s_text_queue[s_text_head];
        s_text_head = (s_text_head + 1) % TEXT_QUEUE_MAX;
        s_text_count--;
        return true;
    }

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        SDL_ConvertEventToRenderCoordinates(s_renderer, &ev);
        memset(out, 0, sizeof *out);

        switch (ev.type) {
        case SDL_EVENT_QUIT:
            out->kind = EV_QUIT;
            return true;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            out->kind   = (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
                        ? EV_MOUSE_DOWN : EV_MOUSE_UP;
            out->x      = (int)ev.button.x;
            out->y      = (int)ev.button.y;
            out->button = ev.button.button;
            out->clicks = ev.button.clicks;
            out->mods   = mods_from_sdl(SDL_GetModState());
            return true;

        case SDL_EVENT_MOUSE_MOTION:
            out->kind = EV_MOUSE_MOVE;
            out->x    = (int)ev.motion.x;
            out->y    = (int)ev.motion.y;
            out->mods = mods_from_sdl(SDL_GetModState());
            return true;

        case SDL_EVENT_MOUSE_WHEEL:
            out->kind  = EV_WHEEL;
            out->x     = (int)ev.wheel.mouse_x;
            out->y     = (int)ev.wheel.mouse_y;
            out->wheel = (int)ev.wheel.y;
            return true;

        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            out->kind = (ev.type == SDL_EVENT_KEY_DOWN) ? EV_KEY_DOWN : EV_KEY_UP;
            out->key  = key_from_sdl(ev.key.key);
            out->mods = mods_from_sdl(ev.key.mod);
            if (out->key == 0) continue;    /* Taste ohne Entsprechung */
            return true;

        case SDL_EVENT_TEXT_INPUT:
            /* Entscheidung D-2: getippte Zeichen kommen ausschließlich von
             * hier, niemals aus einem Tastencode. Nur so funktionieren
             * QWERTZ, tote Tasten und AltGr. */
            queue_text(ev.text.text);
            if (s_text_count > 0) return plat_poll(out);
            continue;

        default:
            continue;
        }
    }

    return false;
}

uint32_t plat_ticks_ms(void)
{
    return (uint32_t)SDL_GetTicks();
}

void plat_sleep_ms(uint32_t ms)
{
    SDL_Delay(ms);
}
