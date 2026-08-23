/* Ein Fenster.
 *
 * Der Inhalt ist eine eigene Bitmap mit eigenem Ursprung. Die
 * Fensterverwaltung kopiert sie unter den Rahmen; eine Anwendung kann deshalb
 * gar nicht über ihr Fenster hinausmalen.
 */
#ifndef PDA_UI_WINDOW_H
#define PDA_UI_WINDOW_H

#include <stdbool.h>

#include "gfx/bitmap.h"
#include "gfx/draw.h"

enum {
    WIN_CLOSABLE  = 1u << 0,
    WIN_MOVABLE   = 1u << 1,
    WIN_RESIZABLE = 1u << 2,
    WIN_MODAL     = 1u << 3,

    WIN_PLAIN     = WIN_CLOSABLE | WIN_MOVABLE,
    WIN_NORMAL    = WIN_CLOSABLE | WIN_MOVABLE | WIN_RESIZABLE
};

/* Welcher Teil eines Fensters getroffen wurde. */
typedef enum {
    HIT_NONE = 0,
    HIT_CONTENT,
    HIT_TITLEBAR,
    HIT_CLOSE_BOX,
    HIT_GROW_BOX
} hit_part;

typedef struct window window;

/* Rahmen des Fensters auf dem Schirm, einschließlich Titelleiste. */
rect        window_frame(const window *w);
const char *window_title(const window *w);
unsigned    window_flags(const window *w);
bool        window_is_active(const window *w);

/* Der Inhaltsbereich in Bildschirmkoordinaten. */
rect window_content_rect(const window *w);

/* Zeichenziel für den Inhalt. Der Ursprung liegt bei (0,0) des Inhalts. */
void window_gc(window *w, gc *g);

void  window_set_title(window *w, const char *title);
void *window_user(const window *w);
void  window_set_user(window *w, void *user);

#endif /* PDA_UI_WINDOW_H */
