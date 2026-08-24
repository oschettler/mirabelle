/* Anwendungen in Lua, siehe pdalua.h.
 *
 * Eine Lua-Anwendung ist eine Tabelle, die sich mit app{} anmeldet:
 *
 *     app{
 *       name  = "outline",
 *       title = "app.outline",          -- Schlüssel im Textkatalog
 *       update = function() end,
 *       draw   = function(w, h) end,
 *       event  = function(e) return false end,
 *     }
 *
 * Mehr Vereinbarung gibt es nicht. `draw` bekommt die Größe des Fensterinhalts
 * und zeichnet mit denselben Funktionen wie jedes andere Skript; `event`
 * liefert true, wenn es das Ereignis verbraucht hat.
 *
 * ## Der Ereignisbus
 *
 * `on(name, fn)` hört zu, `send(name, ...)` ruft alle Zuhörer. Damit reden
 * Anwendungen miteinander, ohne einander zu kennen.
 *
 * Das ist die Form, die Picotron hat, ohne dessen Isolation (D-4): dort hat
 * jeder Prozess einen eigenen Zustand und kann gar nicht anders als über
 * Nachrichten. Wir behalten den Bus als Naht - sollte die Isolation je
 * gebraucht werden, ist er die Stelle, an der man sie einzieht.
 */
#include <lauxlib.h>
#include <lua.h>

#include <stdio.h>
#include <string.h>

#include "app/shell.h"
#include "lua/pdalua.h"

/* Die angemeldeten Anwendungen und die Zuhörer liegen in der Registry, nicht
 * in Dateiglobalen: so kann es zwei Zustände nebeneinander geben. */
static const char KEY_APPS[]      = "pda.apps";
static const char KEY_LISTENERS[] = "pda.listeners";

/* Legt die Tabelle unter key an, falls es sie noch nicht gibt, und lässt sie
 * auf dem Stapel liegen. */
static void push_registry_table(lua_State *L, const char *key)
{
    lua_pushlightuserdata(L, (void *)key);
    lua_gettable(L, LUA_REGISTRYINDEX);

    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);

        lua_pushlightuserdata(L, (void *)key);
        lua_pushvalue(L, -2);
        lua_settable(L, LUA_REGISTRYINDEX);
    }
}

/* --- app{} ------------------------------------------------------------------------ */

static int l_app(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);

    lua_getfield(L, 1, "title");
    if (!lua_isstring(L, -1))
        return luaL_error(L, "app: title fehlt (ein Schlüssel aus dem Katalog)");
    lua_pop(L, 1);

    lua_getfield(L, 1, "draw");
    if (!lua_isfunction(L, -1))
        return luaL_error(L, "app: draw fehlt");
    lua_pop(L, 1);

    push_registry_table(L, KEY_APPS);
    lua_pushvalue(L, 1);
    lua_rawseti(L, -2, (lua_Integer)luaL_len(L, -2) + 1);
    lua_pop(L, 1);

    return 0;
}

/* --- on und send -------------------------------------------------------------------- */

static int l_on(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    push_registry_table(L, KEY_LISTENERS);

    lua_getfield(L, -1, name);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);

        lua_pushvalue(L, -1);
        lua_setfield(L, -3, name);
    }

    lua_pushvalue(L, 2);
    lua_rawseti(L, -2, (lua_Integer)luaL_len(L, -2) + 1);

    lua_pop(L, 2);
    return 0;
}

static int l_send(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);
    int         argc = lua_gettop(L) - 1;

    push_registry_table(L, KEY_LISTENERS);
    lua_getfield(L, -1, name);

    if (!lua_istable(L, -1)) {
        /* Niemand hört zu. Das ist kein Fehler - ein Sender soll nicht wissen
         * müssen, ob jemand da ist. */
        lua_pop(L, 2);
        return 0;
    }

    lua_Integer n = luaL_len(L, -1);
    for (lua_Integer i = 1; i <= n; i++) {
        lua_rawgeti(L, -1, i);

        for (int a = 0; a < argc; a++) lua_pushvalue(L, a + 2);

        /* Ein Zuhörer, der stolpert, bringt nicht den Sender zu Fall. Die
         * Meldung geht an log, damit sie nicht spurlos verschwindet. */
        if (lua_pcall(L, argc, 0, 0) != LUA_OK) {
            const char *msg = lua_tostring(L, -1);
            fprintf(stderr, "send('%s'): %s\n", name, msg ? msg : "?");
            lua_pop(L, 1);
        }
    }

    lua_pop(L, 2);
    return 0;
}

/* --- Die Brücke zur Schale ------------------------------------------------------------
 *
 * Ab hier ruft C nach Lua. Alles davor war umgekehrt, und der Unterschied ist
 * wichtig: hier muss jeder Aufruf über lua_pcall gehen. Ein Skript, das einen
 * Fehler macht, darf die Oberfläche nicht mitnehmen.
 */

/* Legt die Anwendung mit diesem Index auf den Stapel. false, wenn es sie nicht
 * gibt; dann liegt nichts obendrauf. */
static bool push_app(lua_State *L, int index)
{
    push_registry_table(L, KEY_APPS);
    lua_rawgeti(L, -1, index + 1);

    if (!lua_istable(L, -1)) {
        lua_pop(L, 2);
        return false;
    }
    lua_remove(L, -2);
    return true;
}

/* Holt ein Feld der Anwendung als Funktion und lässt Funktion und Anwendung
 * auf dem Stapel. false, wenn es das Feld nicht gibt. */
static bool push_method(lua_State *L, int index, const char *name)
{
    if (!push_app(L, index)) return false;

    lua_getfield(L, -1, name);
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return false;
    }
    return true;
}

static void report(lua_State *L, const char *what)
{
    const char *msg = lua_tostring(L, -1);
    fprintf(stderr, "%s: %s\n", what, msg ? msg : "unbekannter Fehler");
    lua_pop(L, 1);
}

static int sc_count(void *user)
{
    lua_State *L = user;

    push_registry_table(L, KEY_APPS);
    int n = (int)luaL_len(L, -1);
    lua_pop(L, 1);
    return n;
}

static const char *sc_title(void *user, int index)
{
    lua_State *L = user;

    /* Der Text bleibt gültig, weil er in der Anwendungstabelle liegt und die
     * so lange lebt wie der Zustand. Lua gibt ihn nicht frei, solange die
     * Tabelle ihn hält. */
    if (!push_app(L, index)) return NULL;

    lua_getfield(L, -1, "title");
    const char *title = lua_tostring(L, -1);
    lua_pop(L, 2);
    return title;
}

static void sc_update(void *user, int index)
{
    lua_State *L = user;
    if (!push_method(L, index, "update")) return;

    lua_remove(L, -2);          /* die Anwendung selbst wird nicht gebraucht */
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) report(L, "update");
}

static void sc_draw(void *user, int index, gc *g, int w, int h)
{
    lua_State *L = user;
    if (!push_method(L, index, "draw")) return;

    lua_remove(L, -2);

    pdalua_set_gc(L, g);
    lua_pushinteger(L, w);
    lua_pushinteger(L, h);

    if (lua_pcall(L, 2, 0, 0) != LUA_OK) report(L, "draw");
    pdalua_set_gc(L, NULL);
}

/* Baut aus einem Ereignis eine Lua-Tabelle.
 *
 * Die Art wird als Wort übergeben und nicht als Zahl: ein Skript soll
 * `if e.kind == "mouse_down"` schreiben können und nicht eine Zahl
 * nachschlagen müssen. */
static void push_event(lua_State *L, const event *e)
{
    static const char *KINDS[] = {
        "none", "mouse_down", "mouse_up", "mouse_move",
        "wheel", "key_down", "key_up", "text", "quit"
    };

    lua_newtable(L);

    const char *kind = ((size_t)e->kind < sizeof KINDS / sizeof KINDS[0])
                     ? KINDS[e->kind] : "none";

    lua_pushstring(L, kind);   lua_setfield(L, -2, "kind");
    lua_pushinteger(L, e->x);  lua_setfield(L, -2, "x");
    lua_pushinteger(L, e->y);  lua_setfield(L, -2, "y");
    lua_pushinteger(L, e->key);      lua_setfield(L, -2, "key");
    lua_pushinteger(L, e->clicks);   lua_setfield(L, -2, "clicks");
    lua_pushinteger(L, e->wheel);    lua_setfield(L, -2, "wheel");
    lua_pushinteger(L, e->mods);     lua_setfield(L, -2, "mods");
    lua_pushstring(L, e->text);      lua_setfield(L, -2, "text");
}

/* Der Rückweg: aus einer Lua-Tabelle wieder ein Ereignis.
 *
 * Gebraucht, sobald ein Skript ein Ereignis an ein Bedienelement weiterreicht
 * (pdalua_widgets.c). Die Namen sind dieselben wie in push_event - sie stehen
 * absichtlich nebeneinander, damit auffällt, wenn einer fehlt. */
bool pdalua_event_from_table(lua_State *L, int idx, event *out)
{
    if (!lua_istable(L, idx)) return false;

    memset(out, 0, sizeof *out);

    lua_getfield(L, idx, "kind");
    const char *kind = lua_tostring(L, -1);
    lua_pop(L, 1);
    if (!kind) return false;

    static const char *KINDS[] = {
        "none", "mouse_down", "mouse_up", "mouse_move",
        "wheel", "key_down", "key_up", "text", "quit"
    };

    out->kind = EV_NONE;
    for (size_t i = 0; i < sizeof KINDS / sizeof KINDS[0]; i++)
        if (strcmp(KINDS[i], kind) == 0) { out->kind = (event_kind)i; break; }

    struct { const char *name; int *slot; } ints[] = {
        { "x", &out->x }, { "y", &out->y }, { "key", &out->key },
        { "clicks", &out->clicks }, { "wheel", &out->wheel },
    };

    for (size_t i = 0; i < sizeof ints / sizeof ints[0]; i++) {
        lua_getfield(L, idx, ints[i].name);
        *ints[i].slot = (int)lua_tointeger(L, -1);
        lua_pop(L, 1);
    }

    lua_getfield(L, idx, "mods");
    out->mods = (uint8_t)lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, idx, "text");
    const char *text = lua_tostring(L, -1);
    if (text) snprintf(out->text, sizeof out->text, "%s", text);
    lua_pop(L, 1);

    return true;
}

static bool sc_event(void *user, int index, const event *e)
{
    lua_State *L = user;
    if (!push_method(L, index, "event")) return false;

    lua_remove(L, -2);
    push_event(L, e);

    if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
        report(L, "event");
        return false;
    }

    bool used = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return used;
}

shell_scripting pdalua_scripting(lua_State *L)
{
    shell_scripting sc = { L, sc_count, sc_title, sc_update, sc_draw, sc_event };
    return sc;
}

/* --- Einrichten -------------------------------------------------------------------- */

void pdalua_open_apps(lua_State *L)
{
    lua_pushcfunction(L, l_app);  lua_setglobal(L, "app");
    lua_pushcfunction(L, l_on);   lua_setglobal(L, "on");
    lua_pushcfunction(L, l_send); lua_setglobal(L, "send");
}
