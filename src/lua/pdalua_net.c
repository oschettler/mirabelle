/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Netz und Gemtext für Lua, siehe pdalua.h.
 *
 * Zwei kleine Tische, die zusammen einen Browser möglich machen:
 *
 *     net.fetch(adresse)          holt eine Seite
 *     net.resolve(basis, verweis) löst einen Verweis auf
 *     gemtext.parse(text)         zerlegt eine Seite in Zeilen
 *
 * Alles drei ist eine Hülle um etwas, das in C schon steht und geprüft ist
 * (net/spartan.h, store/gemtext.h). Das ist der Sinn der Sache: eine Anwendung
 * in Lua soll keine Protokolle nachbauen, sondern die benutzen, die es gibt.
 *
 * ## Warum das Abrufen blockiert
 *
 * net.fetch wartet, bis die Antwort da ist oder das Zeitlimit zuschlägt
 * (plat_net_posix.c: zehn Sekunden). Solange steht die Oberfläche.
 *
 * Das ist eine bewusste Entscheidung und keine Schlamperei. Nebenläufigkeit in
 * ein Skriptsystem einzuziehen, hieße Rückrufe, einen Zustandsautomaten je
 * Anwendung und die Frage, was passiert, wenn zwei Abrufe gleichzeitig laufen.
 * Für einen Taschencomputer, der eine Seite holt und sie anzeigt, ist das
 * mehr Maschinerie als Nutzen. Wenn es je stört, ist der Ereignisbus die
 * Stelle, an der man es nachrüstet.
 */
#include <lauxlib.h>
#include <lua.h>

#include <stdio.h>
#include <string.h>

#include "lua/pdalua.h"
#include "net/plat_transport.h"
#include "net/spartan.h"
#include "store/gemtext.h"

#define PAGE_MAX (256 * 1024)

/* --- net --------------------------------------------------------------------- */

static int l_fetch(lua_State *L)
{
    const char *url = luaL_checkstring(L, 1);

    spartan_url u;
    if (!spartan_parse_url(url, &u)) {
        lua_pushnil(L);
        lua_pushstring(L, "keine brauchbare Adresse");
        return 2;
    }

    /* Der Puffer liegt statisch: eine Seite von einem Viertelmegabyte auf dem
     * Stapel wäre auf dem Gerät das Ende. Damit kann nur ein Abruf zugleich
     * laufen - genau wie beim Transport, und aus demselben Grund. */
    static char page[PAGE_MAX];

    spartan_transport t = plat_spartan_transport();
    spartan_response  r;
    char              err[256] = "";

    if (!spartan_fetch(&t, &u, page, sizeof page, &r, err, sizeof err)) {
        lua_pushnil(L);
        lua_pushstring(L, err);
        return 2;
    }

    lua_newtable(L);
    lua_pushinteger(L, r.status);         lua_setfield(L, -2, "status");
    lua_pushstring(L, r.meta);            lua_setfield(L, -2, "meta");
    lua_pushlstring(L, r.body, r.body_len); lua_setfield(L, -2, "body");

    /* Die aufgelöste Adresse mitgeben: das Skript hat vielleicht „mozz.us"
     * getippt, und der nächste Verweis muss relativ dazu aufgelöst werden. */
    char full[SPARTAN_URL_MAX];
    if (spartan_format_url(&u, full, sizeof full)) {
        lua_pushstring(L, full);
        lua_setfield(L, -2, "url");
    }
    return 1;
}

static int l_resolve(lua_State *L)
{
    const char *base = luaL_checkstring(L, 1);
    const char *href = luaL_checkstring(L, 2);

    spartan_url b, out;
    if (!spartan_parse_url(base, &b) || !spartan_resolve(&b, href, &out)) {
        lua_pushnil(L);
        return 1;
    }

    char full[SPARTAN_URL_MAX];
    if (!spartan_format_url(&out, full, sizeof full)) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushstring(L, full);
    return 1;
}

/* --- gemtext ------------------------------------------------------------------- */

typedef struct { lua_State *L; int n; } gem_ctx;

static const char *kind_name(gem_kind k)
{
    switch (k) {
    case GEM_TEXT:    return "text";
    case GEM_LINK:    return "link";
    case GEM_PRE:     return "pre";
    case GEM_HEADING: return "heading";
    case GEM_ITEM:    return "item";
    case GEM_QUOTE:   return "quote";
    }
    return "text";
}

static void on_line(const gem_line *line, void *user)
{
    gem_ctx   *c = user;
    lua_State *L = c->L;

    lua_newtable(L);

    lua_pushstring(L, kind_name(line->kind));
    lua_setfield(L, -2, "kind");

    lua_pushlstring(L, line->text, line->text_len);
    lua_setfield(L, -2, "text");

    if (line->kind == GEM_LINK) {
        lua_pushlstring(L, line->url, line->url_len);
        lua_setfield(L, -2, "url");
    }
    if (line->kind == GEM_HEADING) {
        lua_pushinteger(L, line->level);
        lua_setfield(L, -2, "level");
    }

    lua_rawseti(L, -2, ++c->n);
}

static int l_gemtext_parse(lua_State *L)
{
    size_t      len  = 0;
    const char *text = luaL_checklstring(L, 1, &len);

    lua_newtable(L);

    gem_ctx c = { L, 0 };
    gemtext_parse(text, len, on_line, &c);
    return 1;
}

/* --- Einrichten ------------------------------------------------------------------ */

static void set_table(lua_State *L, const char *name, const luaL_Reg *fns)
{
    lua_newtable(L);
    for (const luaL_Reg *f = fns; f->func; f++) {
        lua_pushcfunction(L, f->func);
        lua_setfield(L, -2, f->name);
    }
    lua_setglobal(L, name);
}

void pdalua_open_net(lua_State *L)
{
    static const luaL_Reg NET[] = {
        { "fetch",   l_fetch },
        { "resolve", l_resolve },
        { NULL, NULL }
    };
    static const luaL_Reg GEM[] = {
        { "parse", l_gemtext_parse },
        { NULL, NULL }
    };

    set_table(L, "net", NET);
    set_table(L, "gemtext", GEM);
}
