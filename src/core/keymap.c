/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Siehe keymap.h für den Vertrag und data/keys/default.keys für das Format
 * und die echte Belegung.
 *
 * Gelesen wird mit dem gemeinsamen Zeilenleser (core/lines.h): drei Wörter je
 * Zeile, `#` leitet einen Kommentar ein. Jeder Fehler bekommt eine Meldung
 * "datei:zeile: meldung", und bei einem Fehler wird keine halbfertige keymap
 * zurückgegeben - eine Belegung, die zur Hälfte gilt, wäre schlimmer als
 * keine.
 */
#include "keymap.h"

#include "core/lines.h"
#include "plat/plat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KEYMAP_NAME_MAX 64   /* längste Aktion bzw. längster Bereich */

typedef struct {
    int     key;
    uint8_t mods;
    char    action[KEYMAP_NAME_MAX + 1];
    char    scope[KEYMAP_NAME_MAX + 1];
    int     line;            /* für Fehlermeldungen bei Doppelbelegung */
} keymap_entry;

struct keymap {
    keymap_entry *entries;
    size_t        count;
    size_t        cap;
};

/* --- Kürzel zerlegen --------------------------------------------------------
 *
 * Ein Kürzel ist eine Folge von durch '+' getrennten Feldern: null oder
 * mehr Modifikatornamen, gefolgt von genau einem Tastennamen. */

typedef struct {
    const char *name;
    uint8_t     bit;
} mod_name;

static const mod_name MOD_NAMES[] = {
    { "cmd",   MOD_CMD   },
    { "ctrl",  MOD_CTRL  },
    { "shift", MOD_SHIFT },
    { "alt",   MOD_ALT   },
};

typedef struct {
    const char *name;
    int         key;
} key_name;

static const key_name KEY_NAMES[] = {
    { "tab",      KEY_TAB       },
    { "return",   KEY_RETURN    },
    { "esc",      KEY_ESCAPE    },
    { "space",    KEY_SPACE     },
    { "backspace",KEY_BACKSPACE },
    { "delete",   KEY_DELETE    },
    { "up",       KEY_UP        },
    { "down",     KEY_DOWN      },
    { "left",     KEY_LEFT      },
    { "right",    KEY_RIGHT     },
    { "home",     KEY_HOME      },
    { "end",      KEY_END       },
    { "pageup",   KEY_PAGE_UP   },
    { "pagedown", KEY_PAGE_DOWN },
    { "f1",  KEY_F1  }, { "f2",  KEY_F2  }, { "f3",  KEY_F3  },
    { "f4",  KEY_F4  }, { "f5",  KEY_F5  }, { "f6",  KEY_F6  },
    { "f7",  KEY_F7  }, { "f8",  KEY_F8  }, { "f9",  KEY_F9  },
    { "f10", KEY_F10 }, { "f11", KEY_F11 }, { "f12", KEY_F12 },
};

/* Vergleicht das Feld [s, s+len) ASCII-unabhängig von Groß-/Kleinschreibung
 * mit dem nullterminierten name. */
static bool field_ieq(const char *s, size_t len, const char *name)
{
    if (strlen(name) != len) return false;
    for (size_t i = 0; i < len; i++) {
        char a = s[i], b = name[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if (a != b) return false;
    }
    return true;
}

bool keymap_parse_shortcut(const char *text, int *key, uint8_t *mods)
{
    if (!text || !*text) return false;

    uint8_t     m = 0;
    const char *p = text;

    for (;;) {
        const char *plus   = strchr(p, '+');
        size_t      len    = plus ? (size_t)(plus - p) : strlen(p);
        if (len == 0) return false;   /* leeres Feld, z.B. "Cmd+" oder "+A" */

        if (plus) {
            bool found = false;
            for (size_t i = 0; i < sizeof MOD_NAMES / sizeof MOD_NAMES[0]; i++) {
                if (field_ieq(p, len, MOD_NAMES[i].name)) {
                    m |= MOD_NAMES[i].bit;
                    found = true;
                    break;
                }
            }
            if (!found) return false;
            p = plus + 1;
            continue;
        }

        /* letztes Feld: die Taste selbst */
        if (len == 1) {
            unsigned char c = (unsigned char)p[0];
            if (c >= 'A' && c <= 'Z') c = (unsigned char)(c - 'A' + 'a');
            *key  = c;
            *mods = m;
            return true;
        }

        for (size_t i = 0; i < sizeof KEY_NAMES / sizeof KEY_NAMES[0]; i++) {
            if (field_ieq(p, len, KEY_NAMES[i].name)) {
                *key  = KEY_NAMES[i].key;
                *mods = m;
                return true;
            }
        }
        return false;
    }
}

/* --- Beschreiben (Rückrichtung) --------------------------------------------- */

/* Feste Reihenfolge Cmd, Ctrl, Alt, Shift - siehe keymap.h. */
static const mod_name DESCRIBE_MOD_ORDER[] = {
    { "Cmd",   MOD_CMD   },
    { "Ctrl",  MOD_CTRL  },
    { "Alt",   MOD_ALT   },
    { "Shift", MOD_SHIFT },
};

static const char *describe_key_name(int key)
{
    for (size_t i = 0; i < sizeof KEY_NAMES / sizeof KEY_NAMES[0]; i++)
        if (KEY_NAMES[i].key == key) return KEY_NAMES[i].name;
    return NULL;
}

/* Erster Buchstabe groß, Rest wie in KEY_NAMES (klein) - genügt für die
 * vorhandenen Namen, die alle nur den ersten Buchstaben groß zeigen
 * ("Tab", "Esc", "PageUp" ist eine Ausnahme und wird unten behandelt). */
static void write_capitalized(char *out, const char *name)
{
    size_t i = 0;
    for (; name[i]; i++) out[i] = name[i];
    out[i] = '\0';
    if (out[0] >= 'a' && out[0] <= 'z') out[0] = (char)(out[0] - 'a' + 'A');
}

static bool format_shortcut(int key, uint8_t mods, char *out, size_t out_size)
{
    char buf[64];
    size_t n = 0;

    for (size_t i = 0; i < sizeof DESCRIBE_MOD_ORDER / sizeof DESCRIBE_MOD_ORDER[0]; i++) {
        if (!(mods & DESCRIBE_MOD_ORDER[i].bit)) continue;
        size_t len = strlen(DESCRIBE_MOD_ORDER[i].name);
        if (n + len + 1 >= sizeof buf) return false;
        memcpy(buf + n, DESCRIBE_MOD_ORDER[i].name, len);
        n += len;
        buf[n++] = '+';
    }

    /* Benannte Tasten zuerst prüfen: Backspace, Tab, Return, Esc und Space
     * tragen selbst kleine Codepunktwerte (Steuerzeichen) und dürfen nicht
     * in den Zweig für druckbare Zeichen rutschen. */
    const char *name = describe_key_name(key);
    if (name) {
        char cap[16];
        if (strlen(name) >= sizeof cap) return false;

        /* PageUp/PageDown bestehen aus zwei groß beginnenden Wörtern -
         * KEY_NAMES trägt sie klein ("pageup"), hier wird "Page" erkannt
         * und das zweite Wort ebenfalls groß geschrieben. */
        write_capitalized(cap, name);
        if (strncmp(cap, "Page", 4) == 0 && strlen(cap) > 4)
            cap[4] = (char)(cap[4] - 'a' + 'A');

        size_t len = strlen(cap);
        if (n + len + 1 > sizeof buf) return false;
        memcpy(buf + n, cap, len + 1);
    } else if (key >= 0x21 && key <= 0x7E) {
        /* druckbares ASCII-Zeichen: als Großbuchstabe */
        unsigned char c = (unsigned char)key;
        if (c >= 'a' && c <= 'z') c = (unsigned char)(c - 'a' + 'A');
        if (n + 2 > sizeof buf) return false;
        buf[n++] = (char)c;
        buf[n]   = '\0';
    } else {
        return false;   /* unbekannte oder nicht darstellbare Taste */
    }

    if (strlen(buf) + 1 > out_size) return false;
    strcpy(out, buf);
    return true;
}

/* --- Wachsende Liste --------------------------------------------------------- */

static bool entries_push(keymap *km, keymap_entry e)
{
    if (km->count == km->cap) {
        size_t        newcap = km->cap ? km->cap * 2 : 16;
        keymap_entry *p       = realloc(km->entries, newcap * sizeof *p);
        if (!p) return false;
        km->entries = p;
        km->cap     = newcap;
    }
    km->entries[km->count++] = e;
    return true;
}

/* Bricht ab und gibt nichts zurück: der Aufrufer bekommt NULL und eine
 * Meldung, nie eine Belegung, in der die Hälfte der Zeilen fehlt. */
static keymap *give_up(keymap *km, linereader *r)
{
    lines_close(r);
    keymap_free(km);
    return NULL;
}

keymap *keymap_load(const char *path, char *err, size_t err_size)
{
    if (err && err_size > 0) err[0] = '\0';

    linereader r;
    if (!lines_open(&r, path, err, err_size)) return NULL;

    keymap *km = calloc(1, sizeof *km);
    if (!km) {
        lines_close(&r);
        lines_fail_file(path, err, err_size, "Speicher reicht nicht");
        return NULL;
    }

    while (lines_next(&r)) {
        if (r.count != 3) {
            lines_fail(&r, err, err_size,
                       "erwartet genau drei Felder: Aktion, Kürzel, Bereich");
            return give_up(km, &r);
        }

        const char *action   = r.word[0];
        const char *shortcut = r.word[1];
        const char *scope    = r.word[2];

        if (strlen(action) > KEYMAP_NAME_MAX) {
            lines_fail(&r, err, err_size,
                       "Aktionsname zu lang (höchstens %d Zeichen)", KEYMAP_NAME_MAX);
            return give_up(km, &r);
        }
        if (strlen(scope) > KEYMAP_NAME_MAX) {
            lines_fail(&r, err, err_size,
                       "Bereichsname zu lang (höchstens %d Zeichen)", KEYMAP_NAME_MAX);
            return give_up(km, &r);
        }

        keymap_entry e = {0};
        if (!keymap_parse_shortcut(shortcut, &e.key, &e.mods)) {
            lines_fail(&r, err, err_size, "Kürzel nicht zerlegbar: '%s'", shortcut);
            return give_up(km, &r);
        }

        e.line = r.line;
        snprintf(e.action, sizeof e.action, "%s", action);
        snprintf(e.scope,  sizeof e.scope,  "%s", scope);

        for (size_t i = 0; i < km->count; i++) {
            const keymap_entry *o = &km->entries[i];

            if (o->key == e.key && o->mods == e.mods &&
                strcmp(o->scope, e.scope) == 0) {
                lines_fail(&r, err, err_size,
                           "Kürzel '%s' im Bereich '%s' ist bereits in Zeile %d "
                           "an '%s' vergeben",
                           shortcut, e.scope, o->line, o->action);
                return give_up(km, &r);
            }
            if (strcmp(o->action, e.action) == 0) {
                lines_fail(&r, err, err_size,
                           "Aktion '%s' ist bereits in Zeile %d belegt",
                           e.action, o->line);
                return give_up(km, &r);
            }
        }

        if (!entries_push(km, e)) {
            lines_fail(&r, err, err_size, "Speicher reicht nicht");
            return give_up(km, &r);
        }
    }

    lines_close(&r);
    return km;
}

void keymap_free(keymap *km)
{
    if (!km) return;
    free(km->entries);
    free(km);
}

int keymap_count(const keymap *km)
{
    return (int)km->count;
}

const char *keymap_lookup(const keymap *km, int key, uint8_t mods, const char *scope)
{
    if (scope) {
        for (size_t i = 0; i < km->count; i++) {
            const keymap_entry *e = &km->entries[i];
            if (e->key == key && e->mods == mods && strcmp(e->scope, scope) == 0)
                return e->action;
        }
    }
    for (size_t i = 0; i < km->count; i++) {
        const keymap_entry *e = &km->entries[i];
        if (e->key == key && e->mods == mods && strcmp(e->scope, "global") == 0)
            return e->action;
    }
    return NULL;
}

bool keymap_describe(const keymap *km, const char *action, char *out, size_t out_size)
{
    for (size_t i = 0; i < km->count; i++) {
        const keymap_entry *e = &km->entries[i];
        if (strcmp(e->action, action) == 0) {
            if (format_shortcut(e->key, e->mods, out, out_size)) return true;
            break;
        }
    }
    if (out && out_size > 0) out[0] = '\0';
    return false;
}
