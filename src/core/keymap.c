/* Siehe keymap.h für den Vertrag und data/keys/default.keys für das Format
 * und die echte Belegung.
 *
 * Der Parser folgt tools/fontc.c: jeder Fehler bekommt eine Meldung
 * "datei:zeile: meldung", und bei einem Fehler wird keine halbfertige
 * keymap zurückgegeben.
 */
#include "keymap.h"

#include "plat/plat.h"

#include <errno.h>
#include <stdarg.h>
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

/* --- Meldungen ------------------------------------------------------------- */

/* Wie fail() in tools/fontc.c: baut "datei:zeile: meldung" und liefert immer
 * true, damit Aufrufer direkt "return fail(...);" schreiben können. */
static bool fail(char *err, size_t err_size, const char *path, int line,
                  const char *fmt, ...)
{
    char msg[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);

    if (err && err_size > 0)
        snprintf(err, err_size, "%s:%d: %s", path, line, msg);

    return true;
}

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

/* --- Zeile lesen -------------------------------------------------------------
 *
 * Wie read_line() in tools/fontc.c: eine Zeile ohne Zeilenende in einen
 * wachsenden Puffer. false am echten Dateiende. */
static bool read_line(FILE *f, char **buf, size_t *cap, int *line_no)
{
    size_t len     = 0;
    bool   got_any = false;
    int    c;

    for (;;) {
        c = fgetc(f);
        if (c == EOF) {
            if (!got_any) return false;
            break;
        }
        got_any = true;
        if (c == '\n') break;

        if (len + 2 > *cap) {
            size_t newcap = (*cap == 0) ? 128 : (*cap * 2);
            char  *p      = realloc(*buf, newcap);
            if (!p) return false;
            *buf = p;
            *cap = newcap;
        }
        (*buf)[len++] = (char)c;
    }

    (*buf)[len] = '\0';
    if (len > 0 && (*buf)[len - 1] == '\r') (*buf)[len - 1] = '\0';
    (*line_no)++;
    return true;
}

/* Nächstes durch Leerraum getrenntes Wort ab *cursor, wie in tools/fontc.c. */
static const char *next_token(const char **cursor, size_t *len)
{
    const char *s = *cursor + strspn(*cursor, " \t");
    size_t      l = strcspn(s, " \t");
    if (l == 0) { *len = 0; return NULL; }

    *len    = l;
    *cursor = s + l;
    return s;
}

keymap *keymap_load(const char *path, char *err, size_t err_size)
{
    if (err && err_size > 0) err[0] = '\0';

    FILE *f = fopen(path, "rb");
    if (!f) {
        fail(err, err_size, path, 0, "Datei kann nicht geöffnet werden: %s",
             strerror(errno));
        return NULL;
    }

    keymap *km = calloc(1, sizeof *km);
    if (!km) {
        fclose(f);
        fail(err, err_size, path, 0, "Speicher reicht nicht");
        return NULL;
    }

    char  *line     = NULL;
    size_t line_cap = 0;
    int    line_no  = 0;

    while (read_line(f, &line, &line_cap, &line_no)) {
        char *p = line + strspn(line, " \t");

        char *hash = strchr(p, '#');
        if (hash) *hash = '\0';

        size_t tlen = strlen(p);
        while (tlen > 0 && (p[tlen - 1] == ' ' || p[tlen - 1] == '\t')) p[--tlen] = '\0';
        if (tlen == 0) continue;   /* Leerzeile oder reiner Kommentar */

        const char *cur = p;
        size_t      la, ls, lb, lext;
        const char *action_tok = next_token(&cur, &la);
        const char *shortcut_tok = next_token(&cur, &ls);
        const char *scope_tok = next_token(&cur, &lb);
        const char *extra_tok = next_token(&cur, &lext);

        if (!action_tok || !shortcut_tok || !scope_tok || extra_tok) {
            fail(err, err_size, path, line_no,
                 "erwartet genau drei Felder: Aktion, Kürzel, Bereich");
            free(line);
            fclose(f);
            keymap_free(km);
            return NULL;
        }

        if (la > KEYMAP_NAME_MAX) {
            fail(err, err_size, path, line_no,
                 "Aktionsname zu lang (höchstens %d Zeichen)", KEYMAP_NAME_MAX);
            free(line);
            fclose(f);
            keymap_free(km);
            return NULL;
        }
        if (lb > KEYMAP_NAME_MAX) {
            fail(err, err_size, path, line_no,
                 "Bereichsname zu lang (höchstens %d Zeichen)", KEYMAP_NAME_MAX);
            free(line);
            fclose(f);
            keymap_free(km);
            return NULL;
        }

        char shortcut_buf[128];
        if (ls >= sizeof shortcut_buf) {
            fail(err, err_size, path, line_no, "Kürzel zu lang: '%.*s'",
                 (int)ls, shortcut_tok);
            free(line);
            fclose(f);
            keymap_free(km);
            return NULL;
        }
        memcpy(shortcut_buf, shortcut_tok, ls);
        shortcut_buf[ls] = '\0';

        int     key;
        uint8_t mods;
        if (!keymap_parse_shortcut(shortcut_buf, &key, &mods)) {
            fail(err, err_size, path, line_no, "Kürzel nicht zerlegbar: '%s'",
                 shortcut_buf);
            free(line);
            fclose(f);
            keymap_free(km);
            return NULL;
        }

        keymap_entry e = {0};
        e.key  = key;
        e.mods = mods;
        e.line = line_no;
        memcpy(e.action, action_tok, la);
        e.action[la] = '\0';
        memcpy(e.scope, scope_tok, lb);
        e.scope[lb] = '\0';

        for (size_t i = 0; i < km->count; i++) {
            const keymap_entry *o = &km->entries[i];
            if (o->key == e.key && o->mods == e.mods && strcmp(o->scope, e.scope) == 0) {
                fail(err, err_size, path, line_no,
                     "Kürzel '%s' im Bereich '%s' ist bereits in Zeile %d an '%s' vergeben",
                     shortcut_buf, e.scope, o->line, o->action);
                free(line);
                fclose(f);
                keymap_free(km);
                return NULL;
            }
            if (strcmp(o->action, e.action) == 0) {
                fail(err, err_size, path, line_no,
                     "Aktion '%s' ist bereits in Zeile %d belegt", e.action, o->line);
                free(line);
                fclose(f);
                keymap_free(km);
                return NULL;
            }
        }

        if (!entries_push(km, e)) {
            fail(err, err_size, path, line_no, "Speicher reicht nicht");
            free(line);
            fclose(f);
            keymap_free(km);
            return NULL;
        }
    }

    bool had_error = ferror(f) != 0;
    fclose(f);
    free(line);

    if (had_error) {
        fail(err, err_size, path, 0, "Datei nicht lesbar: Lesefehler");
        keymap_free(km);
        return NULL;
    }

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
