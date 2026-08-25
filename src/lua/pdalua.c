/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Siehe pdalua.h für den Vertrag.
 *
 * Der Zustand hält seine C-Seite in der Registry, nicht in Dateiglobalen: so
 * kann es zwei Zustände nebeneinander geben, und ein Test muss nicht aufräumen,
 * was ein anderer hinterlassen hat.
 */
#include "lua/pdalua.h"

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "gfx/font.h"
#include "gfx/pattern.h"
#include "gfx/text.h"
#include "ui/caret.h"

/* Die Schlüssel, unter denen die C-Seite in der Registry liegt. Ihre Adressen
 * sind die Schlüssel - so kann kein Lua-Code sie zufällig treffen. */
static const char KEY_GC[]  = "pda.gc";
static const char KEY_CAT[] = "pda.catalog";

static void set_ptr(lua_State *L, const char *key, void *p)
{
    lua_pushlightuserdata(L, (void *)key);
    if (p) lua_pushlightuserdata(L, p);
    else   lua_pushnil(L);
    lua_settable(L, LUA_REGISTRYINDEX);
}

static void *get_ptr(lua_State *L, const char *key)
{
    lua_pushlightuserdata(L, (void *)key);
    lua_gettable(L, LUA_REGISTRYINDEX);

    void *p = lua_touserdata(L, -1);
    lua_pop(L, 1);
    return p;
}

static gc *current_gc(lua_State *L)
{
    return get_ptr(L, KEY_GC);
}

gc *pdalua_current_gc(lua_State *L)
{
    return current_gc(L);
}

static const catalog *current_catalog(lua_State *L)
{
    return get_ptr(L, KEY_CAT);
}

const catalog *pdalua_current_catalog(lua_State *L)
{
    return current_catalog(L);
}

/* --- Zeichnen ------------------------------------------------------------------
 *
 * Ohne Zeichenzustand tun diese Funktionen nichts. Sie werfen keinen Fehler:
 * ein Skript, das außerhalb des Zeichnens etwas zeichnet, soll nicht abstürzen,
 * sondern schlicht nichts bewirken. Ein Absturz wäre hier die härtere Strafe
 * für den kleineren Fehler.
 */

#define GC_OR_RETURN(L)                        \
    gc *g = current_gc(L);                     \
    if (!g) return 0

static int l_cls(lua_State *L)
{
    GC_OR_RETURN(L);

    g->pat  = PAT_WHITE;
    g->mode = GFX_COPY;
    gfx_clear(g);
    g->pat = PAT_BLACK;
    return 0;
}

static int l_pset(lua_State *L)
{
    GC_OR_RETURN(L);
    gfx_pset(g, (int)luaL_checkinteger(L, 1), (int)luaL_checkinteger(L, 2));
    return 0;
}

static int l_line(lua_State *L)
{
    GC_OR_RETURN(L);
    gfx_line(g, (int)luaL_checkinteger(L, 1), (int)luaL_checkinteger(L, 2),
                (int)luaL_checkinteger(L, 3), (int)luaL_checkinteger(L, 4));
    return 0;
}

static rect rect_args(lua_State *L)
{
    return rect_make((int)luaL_checkinteger(L, 1), (int)luaL_checkinteger(L, 2),
                     (int)luaL_checkinteger(L, 3), (int)luaL_checkinteger(L, 4));
}

static int l_rect(lua_State *L)
{
    GC_OR_RETURN(L);
    gfx_frame_rect(g, rect_args(L));
    return 0;
}

static int l_rectfill(lua_State *L)
{
    GC_OR_RETURN(L);
    gfx_fill_rect(g, rect_args(L));
    return 0;
}

static int l_oval(lua_State *L)
{
    GC_OR_RETURN(L);
    gfx_frame_oval(g, rect_args(L));
    return 0;
}

static int l_ovalfill(lua_State *L)
{
    GC_OR_RETURN(L);
    gfx_fill_oval(g, rect_args(L));
    return 0;
}

static int l_rrect(lua_State *L)
{
    GC_OR_RETURN(L);
    gfx_frame_round_rect(g, rect_args(L), (int)luaL_optinteger(L, 5, 4));
    return 0;
}

static int l_rrectfill(lua_State *L)
{
    GC_OR_RETURN(L);
    gfx_fill_round_rect(g, rect_args(L), (int)luaL_optinteger(L, 5, 4));
    return 0;
}

/* clip(x, y, w, h) schränkt ein, clip() hebt die Einschränkung wieder auf.
 *
 * Ohne die zweite Form müsste sich ein Skript merken, wie groß sein Fenster
 * ist, nur um eine Einschränkung zurückzunehmen - und hätte die Zahl dann
 * zweimal. */
static int l_clip(lua_State *L)
{
    GC_OR_RETURN(L);

    if (lua_isnoneornil(L, 1)) {
        gc_clip(g, rect_make(0, 0, g->dst->w, g->dst->h));
        return 0;
    }

    gc_clip(g, rect_args(L));
    return 0;
}

static int l_camera(lua_State *L)
{
    GC_OR_RETURN(L);
    g->origin.x = (int)luaL_optinteger(L, 1, 0);
    g->origin.y = (int)luaL_optinteger(L, 2, 0);
    return 0;
}

/* --- Muster und Modus -------------------------------------------------------------
 *
 * Beides sind Namen und keine Zahlen. Eine Zahl müsste jemand nachschlagen; ein
 * Name steht im Skript und sagt, was er meint. Ein unbekannter Name ist ein
 * Fehler - stillschweigend auf schwarz zurückzufallen hieße, einen Tippfehler
 * als Gestaltung auszugeben.
 */

static const struct { const char *name; const pattern *pat; } PATTERNS[] = {
    { "black", &PAT_BLACK }, { "white", &PAT_WHITE },
    { "gray",  &PAT_GRAY50 }, { "gray50", &PAT_GRAY50 },
    { "gray25", &PAT_GRAY25 }, { "gray75", &PAT_GRAY75 },
    { "hatch", &PAT_HATCH },
};

static int l_pattern(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);
    gc         *g    = current_gc(L);

    for (size_t i = 0; i < sizeof PATTERNS / sizeof PATTERNS[0]; i++)
        if (strcmp(PATTERNS[i].name, name) == 0) {
            if (g) g->pat = *PATTERNS[i].pat;
            return 0;
        }

    return luaL_error(L, "unbekanntes Muster '%s'", name);
}

static const struct { const char *name; gfx_mode mode; } MODES[] = {
    { "copy", GFX_COPY }, { "or", GFX_OR }, { "xor", GFX_XOR },
    { "clear", GFX_CLEAR }, { "notcopy", GFX_NOTCOPY },
};

static int l_mode(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);
    gc         *g    = current_gc(L);

    for (size_t i = 0; i < sizeof MODES / sizeof MODES[0]; i++)
        if (strcmp(MODES[i].name, name) == 0) {
            if (g) g->mode = MODES[i].mode;
            return 0;
        }

    return luaL_error(L, "unbekannter Modus '%s'", name);
}

/* --- Text ------------------------------------------------------------------------ */

extern const font system12;

static int l_print(lua_State *L)
{
    const char *s = luaL_checkstring(L, 1);
    int         x = (int)luaL_checkinteger(L, 2);
    int         y = (int)luaL_checkinteger(L, 3);

    gc *g = current_gc(L);
    if (!g) return 0;

    /* y ist die Oberkante, nicht die Grundlinie. Ein Skript soll nicht wissen
     * müssen, wo eine Schrift ihre Grundlinie hat. */
    gfx_text(g, &system12, x, y + system12.ascent, s);
    return 0;
}

static int l_textwidth(lua_State *L)
{
    lua_pushinteger(L, text_width(&system12, luaL_checkstring(L, 1)));
    return 1;
}

static int l_textheight(lua_State *L)
{
    lua_pushinteger(L, system12.size);
    return 1;
}

/* Blinkt die Schreibmarke gerade sichtbar? Ein Skript, das ein Eingabefeld
 * selbst zeichnet, soll denselben Takt haben wie die Felder des Programms -
 * zwei Schreibmarken, die gegeneinander blinken, sehen aus wie ein Fehler.
 *
 * Der Takt kommt aus ui/caret.h und wird nur im Hauptprogramm gestellt. Ein
 * Skript kann ihn abfragen, aber nicht verstellen.
 */
static int l_caret(lua_State *L)
{
    lua_pushboolean(L, caret_on());
    return 1;
}

/* --- Texte aus dem Katalog --------------------------------------------------------- */

static int l_T(lua_State *L)
{
    const char *key = luaL_checkstring(L, 1);
    const catalog *c = current_catalog(L);

    /* Mit weiteren Argumenten werden {0}, {1}, ... eingesetzt. */
    int argc = lua_gettop(L) - 1;
    if (argc <= 0) {
        lua_pushstring(L, T(c, key));
        return 1;
    }

    const char *args[8];
    if (argc > 8) argc = 8;
    for (int i = 0; i < argc; i++) args[i] = luaL_checkstring(L, i + 2);

    char out[512];
    if (!Tf(c, key, out, sizeof out, args, argc))
        return luaL_error(L, "Text zu '%s' passt nicht in den Puffer", key);

    lua_pushstring(L, out);
    return 1;
}

static int l_Tn(lua_State *L)
{
    const char *key = luaL_checkstring(L, 1);
    int         n   = (int)luaL_checkinteger(L, 2);

    char out[512];
    if (!Tn(current_catalog(L), key, n, out, sizeof out))
        return luaL_error(L, "Text zu '%s' passt nicht in den Puffer", key);

    lua_pushstring(L, out);
    return 1;
}

/* --- Einrichten -------------------------------------------------------------------- */

static const luaL_Reg API[] = {
    { "cls",        l_cls },
    { "pset",       l_pset },
    { "line",       l_line },
    { "rect",       l_rect },
    { "rectfill",   l_rectfill },
    { "oval",       l_oval },
    { "ovalfill",   l_ovalfill },
    { "rrect",      l_rrect },
    { "rrectfill",  l_rrectfill },
    { "clip",       l_clip },
    { "camera",     l_camera },
    { "pattern",    l_pattern },
    { "mode",       l_mode },
    { "print",      l_print },
    { "textwidth",  l_textwidth },
    { "textheight", l_textheight },
    { "caret",      l_caret },
    { "T",          l_T },
    { "Tn",         l_Tn },
    { NULL, NULL }
};

lua_State *pdalua_open(const catalog *cat, char *err, size_t err_size)
{
    lua_State *L = luaL_newstate();
    if (!L) {
        if (err && err_size) snprintf(err, err_size, "kein Speicher für Lua");
        return NULL;
    }

    /* Nur diese vier. Kein io, kein os, kein package - siehe pdalua.h. */
    static const luaL_Reg LIBS[] = {
        { LUA_GNAME,      luaopen_base   },
        { LUA_TABLIBNAME, luaopen_table  },
        { LUA_STRLIBNAME, luaopen_string },
        { LUA_MATHLIBNAME, luaopen_math  },
        { NULL, NULL }
    };

    for (const luaL_Reg *lib = LIBS; lib->func; lib++) {
        luaL_requiref(L, lib->name, lib->func, 1);
        lua_pop(L, 1);
    }

    /* Luas eigenes print heißt ab jetzt log. print() zeichnet - so steht es in
     * DESIGN.md Abschnitt 11, und so kennt man es aus PICO-8. Weggenommen wird
     * es nicht: wer etwas ausgeben will, soll das können. */
    lua_getglobal(L, "print");
    lua_setglobal(L, "log");

    for (const luaL_Reg *fn = API; fn->func; fn++) {
        lua_pushcfunction(L, fn->func);
        lua_setglobal(L, fn->name);
    }

    /* Die Tastennummern als Namen. Ohne sie stünde in jedem Skript eine Zahl
     * wie 257, und niemand wüsste, welche Taste das ist - am wenigsten der,
     * der sie in einem halben Jahr wiederliest. */
    static const struct { const char *name; int key; } KEYS[] = {
        { "up", KEY_UP }, { "down", KEY_DOWN },
        { "left", KEY_LEFT }, { "right", KEY_RIGHT },
        { "home", KEY_HOME }, { "last", KEY_END },
        { "pageup", KEY_PAGE_UP }, { "pagedown", KEY_PAGE_DOWN },
        { "enter", KEY_RETURN }, { "escape", KEY_ESCAPE },
        { "tab", KEY_TAB }, { "space", KEY_SPACE },
        { "backspace", KEY_BACKSPACE }, { "del", KEY_DELETE },
    };

    lua_newtable(L);
    for (size_t i = 0; i < sizeof KEYS / sizeof KEYS[0]; i++) {
        lua_pushinteger(L, KEYS[i].key);
        lua_setfield(L, -2, KEYS[i].name);
    }
    lua_setglobal(L, "key");

    set_ptr(L, KEY_CAT, (void *)(uintptr_t)cat);
    set_ptr(L, KEY_GC, NULL);

    if (err && err_size) err[0] = '\0';
    return L;
}

void pdalua_close(lua_State *L)
{
    if (L) lua_close(L);
}

void pdalua_set_gc(lua_State *L, gc *g)
{
    set_ptr(L, KEY_GC, g);
}

/* Holt die Fehlermeldung vom Stapel und räumt ihn auf. */
static bool take_error(lua_State *L, char *err, size_t err_size)
{
    const char *msg = lua_tostring(L, -1);
    if (err && err_size) snprintf(err, err_size, "%s", msg ? msg : "unbekannter Fehler");
    lua_pop(L, 1);
    return false;
}

bool pdalua_dofile(lua_State *L, const char *path, char *err, size_t err_size)
{
    if (luaL_loadfile(L, path) != LUA_OK) return take_error(L, err, err_size);
    if (lua_pcall(L, 0, 0, 0) != LUA_OK)  return take_error(L, err, err_size);

    if (err && err_size) err[0] = '\0';
    return true;
}

bool pdalua_dostring(lua_State *L, const char *code, const char *name,
                     char *err, size_t err_size)
{
    char chunk[128];
    snprintf(chunk, sizeof chunk, "=%s", name ? name : "?");

    if (luaL_loadbuffer(L, code, strlen(code), chunk) != LUA_OK)
        return take_error(L, err, err_size);
    if (lua_pcall(L, 0, 0, 0) != LUA_OK)
        return take_error(L, err, err_size);

    if (err && err_size) err[0] = '\0';
    return true;
}
