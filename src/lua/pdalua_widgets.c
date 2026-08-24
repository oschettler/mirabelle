/* Bedienelemente für Lua, siehe pdalua.h.
 *
 * Bisher gab es für ein Skript nur die Zeichenfunktionen. Wer einen Rollbalken
 * wollte, musste ihn nachbauen — und ein nachgebautes Bedienelement sieht dem
 * echten nur so lange ähnlich, wie niemand hinsieht. Im SPARTAN-Browser fehlte
 * dem nachgebauten Balken der untere Pfeil.
 *
 * Deshalb reicht diese Datei das echte Widget durch. Es ist dasselbe, das der
 * Browser und das Notizfeld benutzen, mit denselben Tests und demselben
 * Aussehen. Ein Skript kann es nicht anders zeichnen als das Programm.
 *
 * ## Wie es sich anfühlt
 *
 *     local bar = scrollbar()
 *     bar:place(x, y, w, h)
 *     bar:set(#zeilen, sichtbar)
 *     bar:draw()
 *
 *     if bar:event(e) then oben = bar:value() + 1 end
 *
 * Das Widget hält seine Bildlaufposition selbst; `value()` fragt sie ab,
 * `scroll_to()` setzt sie. Für ein Skript ist der Balken damit die Wahrheit
 * über die Position — genauso, wie in C das scrollmodel es ist.
 *
 * ## Warum es ein Benutzerdatum ist und keine Zahl
 *
 * Ein Widget belegt Speicher und muss wieder freigegeben werden. Als
 * Benutzerdatum mit `__gc` erledigt Lua das selbst, wenn niemand mehr darauf
 * zeigt. Eine Kennung, die der Aufrufer zurückgeben müsste, wäre eine Zeile
 * Aufräumarbeit in jedem Skript — und eine Zeile, die irgendwann jemand
 * vergisst.
 */
#include <lauxlib.h>
#include <lua.h>

#include <stdio.h>
#include <string.h>

#include "lua/pdalua.h"
#include "ui/scroll.h"
#include "ui/theme.h"
#include "ui/widget.h"

#define BAR_MT "pda.scrollbar"

typedef struct {
    widget     *w;
    scrollmodel model;
} lua_bar;

/* --- Das Thema ------------------------------------------------------------------
 *
 * Widgets halten einen Zeiger auf ihr Thema, nie eine Kopie (widget.h) - und
 * das Thema muss sie überleben. Deshalb liegt hier eine Kopie im Zustand
 * selbst: sie lebt genau so lange wie Lua, und alles zeigt darauf.
 */
static const char KEY_THEME[] = "pda.theme";

static theme *state_theme(lua_State *L)
{
    lua_pushlightuserdata(L, (void *)KEY_THEME);
    lua_gettable(L, LUA_REGISTRYINDEX);

    theme *th = lua_touserdata(L, -1);
    lua_pop(L, 1);
    return th;
}

void pdalua_set_theme(lua_State *L, const theme *th)
{
    if (!th) return;

    theme *copy = state_theme(L);
    if (!copy) {
        copy = lua_newuserdatauv(L, sizeof *copy, 0);

        lua_pushlightuserdata(L, (void *)KEY_THEME);
        lua_pushvalue(L, -2);
        lua_settable(L, LUA_REGISTRYINDEX);
        lua_pop(L, 1);
    }
    *copy = *th;

    /* Und dieselben Zahlen als Tabelle, damit ein Skript nicht raten muss.
     * Ohne sie stünde die Breite eines Rollbalkens zweimal im System - einmal
     * im Thema und einmal als Zahl in einem Skript. */
    lua_newtable(L);

    struct { const char *name; int value; } n[] = {
        { "titlebar_h",   th->titlebar_h },
        { "border",       th->border },
        { "menubar_h",    th->menubar_h },
        { "menubar_left", th->menubar_left },
        { "menu_item_h",  th->menu_item_h },
        { "menu_pad",     th->menu_pad },
        { "button_h",     th->button_h },
        { "button_gap",   th->button_gap },
        { "dialog_pad",   th->dialog_pad },
        { "scrollbar_w",  th->scrollbar_w },
    };

    for (size_t i = 0; i < sizeof n / sizeof n[0]; i++) {
        lua_pushinteger(L, n[i].value);
        lua_setfield(L, -2, n[i].name);
    }
    lua_setglobal(L, "theme");
}

/* --- Der Rollbalken ---------------------------------------------------------- */

static lua_bar *check_bar(lua_State *L, int idx)
{
    return luaL_checkudata(L, idx, BAR_MT);
}

static int l_new(lua_State *L)
{
    theme *th = state_theme(L);
    if (!th) return luaL_error(L, "scrollbar: kein Thema eingerichtet");

    lua_bar *b = lua_newuserdatauv(L, sizeof *b, 0);
    memset(b, 0, sizeof *b);

    luaL_getmetatable(L, BAR_MT);
    lua_setmetatable(L, -2);

    /* Der Balken zeigt auf das Modell IM Benutzerdatum. Beide leben und
     * sterben zusammen, also kann der Zeiger nicht baumeln. */
    b->w = scrollbar_create(th, NULL, SCROLLBAR_VERTICAL, &b->model);
    if (!b->w) return luaL_error(L, "scrollbar: kein Speicher");

    return 1;
}

static int l_gc(lua_State *L)
{
    lua_bar *b = check_bar(L, 1);

    widget_destroy(b->w);
    b->w = NULL;
    return 0;
}

static int l_place(lua_State *L)
{
    lua_bar *b = check_bar(L, 1);

    b->w->frame = rect_make((int)luaL_checkinteger(L, 2),
                            (int)luaL_checkinteger(L, 3),
                            (int)luaL_checkinteger(L, 4),
                            (int)luaL_checkinteger(L, 5));
    return 0;
}

static int l_set(lua_State *L)
{
    lua_bar *b = check_bar(L, 1);

    scroll_set(&b->model, (int)luaL_checkinteger(L, 2),
                          (int)luaL_checkinteger(L, 3));
    return 0;
}

static int l_value(lua_State *L)
{
    lua_pushinteger(L, check_bar(L, 1)->model.value);
    return 1;
}

static int l_scroll_to(lua_State *L)
{
    lua_bar *b = check_bar(L, 1);

    scroll_to(&b->model, (int)luaL_checkinteger(L, 2));
    return 0;
}

static int l_scroll_by(lua_State *L)
{
    lua_bar *b = check_bar(L, 1);

    scroll_by(&b->model, (int)luaL_checkinteger(L, 2));
    return 0;
}

static int l_max(lua_State *L)
{
    lua_pushinteger(L, scroll_max(&check_bar(L, 1)->model));
    return 1;
}

static int l_width(lua_State *L)
{
    lua_bar *b = check_bar(L, 1);

    int w = 0, h = 0;
    widget_measure(b->w, &w, &h);

    lua_pushinteger(L, w);
    return 1;
}

/* Lage und Größe, wie sie zuletzt gesetzt wurden. Vier Rückgabewerte statt
 * einer Tabelle: so lässt sich `local x, y, w, h = bar:frame()` schreiben, und
 * es entsteht kein Müll, den jemand einsammeln müsste. */
static int l_frame(lua_State *L)
{
    lua_bar *b = check_bar(L, 1);

    lua_pushinteger(L, b->w->frame.x);
    lua_pushinteger(L, b->w->frame.y);
    lua_pushinteger(L, b->w->frame.w);
    lua_pushinteger(L, b->w->frame.h);
    return 4;
}

static int l_draw(lua_State *L)
{
    lua_bar *b = check_bar(L, 1);
    gc      *g = pdalua_current_gc(L);

    /* Ohne Zeichenzustand geschieht nichts - wie bei allen Zeichenfunktionen.
     * Ein Skript, das ausserhalb des Zeichnens zeichnet, soll nicht abstürzen. */
    if (g) widget_draw(b->w, g);
    return 0;
}

static int l_event(lua_State *L)
{
    lua_bar *b = check_bar(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);

    event e;
    if (!pdalua_event_from_table(L, 2, &e)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    lua_pushboolean(L, widget_event(b->w, &e));
    return 1;
}

static const luaL_Reg METHODS[] = {
    { "place",     l_place },
    { "set",       l_set },
    { "value",     l_value },
    { "scroll_to", l_scroll_to },
    { "scroll_by", l_scroll_by },
    { "max",       l_max },
    { "width",     l_width },
    { "frame",     l_frame },
    { "draw",      l_draw },
    { "event",     l_event },
    { NULL, NULL }
};

void pdalua_open_widgets(lua_State *L)
{
    luaL_newmetatable(L, BAR_MT);

    lua_pushcfunction(L, l_gc);
    lua_setfield(L, -2, "__gc");

    lua_newtable(L);
    luaL_setfuncs(L, METHODS, 0);
    lua_setfield(L, -2, "__index");

    lua_pop(L, 1);

    lua_pushcfunction(L, l_new);
    lua_setglobal(L, "scrollbar");
}
