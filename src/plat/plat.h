/* Die Plattformschicht: die gesamte Schnittstelle zur Außenwelt.
 *
 * Neunzehn Funktionen. Wer auf ein neues Gerät portiert, implementiert genau
 * das und sonst nichts. Die Zahl steht auch in DESIGN.md und im Handbuch;
 * tests/plat_count.sh zählt nach, damit sie nicht auseinanderlaufen. Alles darüber ist portables C und weiß weder von SDL noch
 * von einem Betriebssystem.
 *
 * Es gibt drei Umsetzungen: plat_sdl3 für den Arbeitsplatz, plat_esp32 für das
 * Gerät und plat_headless für die Tests. Die Headless-Variante ist keine
 * Notlösung, sondern der Grund, warum sich fast alles ohne Bildschirm prüfen
 * lässt.
 */
#ifndef PDA_PLAT_H
#define PDA_PLAT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "gfx/bitmap.h"

/* --- Ereignisse ---------------------------------------------------------- */

typedef enum {
    EV_NONE = 0,
    EV_MOUSE_DOWN,
    EV_MOUSE_UP,
    EV_MOUSE_MOVE,
    EV_WHEEL,
    EV_KEY_DOWN,
    EV_KEY_UP,
    EV_TEXT,
    EV_QUIT
} event_kind;

enum {
    MOD_SHIFT = 1u << 0,
    MOD_CTRL  = 1u << 1,
    MOD_ALT   = 1u << 2,
    MOD_CMD   = 1u << 3    /* Befehlstaste auf dem Mac, sonst gleich MOD_CTRL */
};

/* Tastencodes. Druckbare Tasten tragen den Codepunkt des Zeichens, das die
 * Taste bei unveränderter Belegung erzeugt - also den layoutabhängigen Wert,
 * damit Cmd+Z dort liegt, wo auf einer deutschen Tastatur das Z ist.
 * Nicht druckbare Tasten liegen oberhalb von U+10FFFF und können deshalb nie
 * mit einem Zeichen verwechselt werden. */
enum {
    KEY_BACKSPACE = 0x08,
    KEY_TAB       = 0x09,
    KEY_RETURN    = 0x0D,
    KEY_ESCAPE    = 0x1B,
    KEY_SPACE     = 0x20,

    KEY_SPECIAL   = 0x40000000,
    KEY_UP = KEY_SPECIAL, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
    KEY_HOME, KEY_END, KEY_PAGE_UP, KEY_PAGE_DOWN, KEY_DELETE,
    KEY_F1, KEY_F2, KEY_F3,  KEY_F4,  KEY_F5,  KEY_F6,
    KEY_F7, KEY_F8, KEY_F9,  KEY_F10, KEY_F11, KEY_F12
};

typedef struct {
    event_kind kind;
    int        x, y;        /* Zeiger, in Bildschirmkoordinaten */
    int        button;      /* 1 links, 2 mitte, 3 rechts */
    int        clicks;      /* 2 bei Doppelklick */
    int        wheel;       /* Rasten, positiv nach oben */
    int        key;
    uint8_t    mods;
    bool       is_touch;    /* dann gibt es keinen Hover-Zustand */
    char       text[8];     /* UTF-8, genau ein Codepunkt, nullterminiert */
} event;

/* --- Leben --------------------------------------------------------------- */

typedef struct {
    int         width, height;  /* logische Größe, 0 heißt Voreinstellung */
    int         scale;          /* ganzzahlige Vergrößerung, 0 heißt automatisch */
    const char *title;
} plat_config;

bool plat_init(const plat_config *cfg);
void plat_shutdown(void);

/* --- Anzeige ------------------------------------------------------------- */

void plat_display_size(int *w, int *h);

/* Gibt ein 1-Bit-Vollbild aus. Die Größe muss zu plat_display_size() passen. */
void plat_present(const bitmap *fb);

/* --- Eingabe ------------------------------------------------------------- */

/* Liefert false, wenn keine Ereignisse mehr anstehen. */
bool plat_poll(event *out);

/* --- Zeit ---------------------------------------------------------------- */

uint32_t plat_ticks_ms(void);
void     plat_sleep_ms(uint32_t ms);

/* --- Dateien ------------------------------------------------------------- */

typedef enum { PLAT_READ, PLAT_WRITE } plat_mode;
typedef struct plat_file plat_file;

typedef struct {
    char name[256];
    bool is_dir;
} plat_dirent;

plat_file *plat_open(const char *path, plat_mode mode);
size_t     plat_read(plat_file *f, void *buf, size_t n);
size_t     plat_write(plat_file *f, const void *buf, size_t n);
void       plat_close(plat_file *f);

/* Schreibt bis zu cap Einträge nach out und die tatsächliche Anzahl nach
 * count. "." und ".." kommen nicht vor. false, wenn dir nicht lesbar ist. */
bool plat_list(const char *dir, plat_dirent *out, int cap, int *count);

/* Legt ein Verzeichnis an. true auch dann, wenn es schon existiert - der
 * Aufrufer will, dass es da ist, nicht dass er es angelegt hat. */
bool plat_mkdir(const char *path);

/* Benennt um und ersetzt dabei ein etwaiges Ziel.
 *
 * Gebraucht für sicheres Speichern: erst vollständig in eine Nebendatei
 * schreiben, dann darüberlegen. Bricht das Programm mittendrin ab, steht die
 * alte Fassung noch da statt einer halben neuen. Bei Nutzerdaten ist das kein
 * Luxus. */
bool plat_rename(const char *from, const char *to);

/* Löscht eine Datei. true auch, wenn sie schon nicht mehr da war. */
bool plat_remove(const char *path);

/* --- Netz -----------------------------------------------------------------
 *
 * Vier Funktionen, und keine davon weiß etwas über ein Protokoll. Sie sind das
 * Gegenstück zu den Dateifunktionen: eine Verbindung ist ein Ding, in das man
 * schreibt und aus dem man liest.
 *
 * Auf dem Arbeitsplatz sind das gewöhnliche Sockets, auf dem Gerät wird es
 * lwIP - und beides sieht von hier aus gleich aus. Das ist der Grund, warum
 * das Protokoll (net/spartan.h) den Transport übergeben bekommt statt ihn zu
 * kennen: so hängt es an keiner der beiden Umsetzungen.
 *
 * Wo es kein Netz gibt, liefert plat_connect() NULL mit einer Meldung. Das ist
 * kein Sonderfall, sondern der Normalzustand in Tests und auf einem Gerät ohne
 * Verbindung.
 */

typedef struct plat_socket plat_socket;

/* Baut eine Verbindung auf. NULL und eine Meldung in err, wenn nicht. */
plat_socket *plat_connect(const char *host, int port, char *err, size_t err_size);

/* Schickt alles oder gar nichts: false, wenn nicht alle len Bytes durchgingen.
 * Teilweise gesendete Anfragen wären schlimmer als gar keine. */
bool plat_send(plat_socket *s, const char *data, size_t len);

/* Liest bis zu cap Bytes. 0 heißt: die Gegenseite hat geschlossen. Negativ
 * heißt Fehler. cap muss größer als null sein. */
long plat_recv(plat_socket *s, char *buf, size_t cap);

void plat_disconnect(plat_socket *s);

#endif /* PDA_PLAT_H */
