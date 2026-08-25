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

#include "app/gemview.h"
#include "lua/pdalua.h"
#include "ui/scroll.h"
#include "ui/theme.h"
#include "ui/widget.h"

#define BAR_MT  "pda.scrollbar"
#define VIEW_MT "pda.gemview"

typedef struct {
    widget     *w;
    scrollmodel model;    /* nur benutzt, wenn der Balken zu nichts gehört */
} lua_bar;

/* Die Gemtext-Anzeige. Ihr Text gehört ihr nicht: gemview_set_text kopiert
 * nicht, sondern zeigt in den übergebenen Puffer (gemview.h). Für Lua heißt
 * das, dass die Zeichenkette am Leben bleiben muss, solange die Anzeige lebt -
 * deshalb liegt sie als nutzerwert 1 an diesem Benutzerdatum. Ein Skript kann
 * sie damit nicht versehentlich einsammeln lassen. */
typedef struct {
    widget *w;
} lua_view;

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
        { "grow_box",     th->grow_box },
        { "box_margin",   th->box_margin },
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

/* scrollbar()      - ein Balken mit eigener Position
 * scrollbar(ansicht) - ein Balken AN einer Anzeige
 *
 * Die zweite Form ist die wichtigere: Balken und Anzeige teilen sich dann ein
 * Bildlaufmodell, also gibt es keine zweite Wahrheit über die Position und
 * nichts, was der Aufrufer abgleichen müsste. Genauso hängt im Programm der
 * Balken am mehrzeiligen Textfeld. */
static int l_new(lua_State *L)
{
    theme *th = state_theme(L);
    if (!th) return luaL_error(L, "scrollbar: kein Thema eingerichtet");

    lua_view *v = luaL_testudata(L, 1, VIEW_MT);

    lua_bar *b = lua_newuserdatauv(L, sizeof *b, 1);
    memset(b, 0, sizeof *b);

    luaL_getmetatable(L, BAR_MT);
    lua_setmetatable(L, -2);

    /* Gehört der Balken zu einer Anzeige, muss diese ihn überleben: ihr
     * Modell ist es, worauf er zeigt. Ein Verweis darauf am Balken hält sie
     * fest, solange es ihn gibt. */
    if (v) {
        lua_pushvalue(L, 1);
        lua_setiuservalue(L, -2, 1);
    }

    /* Sonst zeigt der Balken auf das Modell IM Benutzerdatum. Beide leben und
     * sterben zusammen, also kann der Zeiger nicht baumeln. */
    b->w = scrollbar_create(th, NULL, SCROLLBAR_VERTICAL,
                            v ? gemview_scroll(v->w) : &b->model);
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

/* Das Modell, auf das der Balken zeigt - das eigene oder das der Anzeige. Alle
 * Methoden gehen hierüber, damit keine von beiden Formen eine Sonderbehandlung
 * braucht. */
static scrollmodel *bar_model(lua_State *L, int idx)
{
    return scrollbar_model(check_bar(L, idx)->w);
}

static int l_set(lua_State *L)
{
    scroll_set(bar_model(L, 1), (int)luaL_checkinteger(L, 2),
                                (int)luaL_checkinteger(L, 3));
    return 0;
}

static int l_value(lua_State *L)
{
    lua_pushinteger(L, bar_model(L, 1)->value);
    return 1;
}

static int l_scroll_to(lua_State *L)
{
    scroll_to(bar_model(L, 1), (int)luaL_checkinteger(L, 2));
    return 0;
}

static int l_scroll_by(lua_State *L)
{
    scroll_by(bar_model(L, 1), (int)luaL_checkinteger(L, 2));
    return 0;
}

static int l_max(lua_State *L)
{
    lua_pushinteger(L, scroll_max(bar_model(L, 1)));
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

/* --- Die Gemtext-Anzeige ------------------------------------------------------
 *
 * Alles, was ein Skript hiermit tut, täte es sonst selbst: umbrechen, Verweise
 * durchzählen, sie mit Ziffern auswählen, blättern. Das ist keine Zeile, die
 * ein Skript besser kann als das Programm - es ist eine Zeile, die es anders
 * macht.
 */

static lua_view *check_view(lua_State *L, int idx)
{
    return luaL_checkudata(L, idx, VIEW_MT);
}

static int v_new(lua_State *L)
{
    theme *th = state_theme(L);
    if (!th) return luaL_error(L, "gemview: kein Thema eingerichtet");

    lua_view *v = lua_newuserdatauv(L, sizeof *v, 1);
    v->w = NULL;

    luaL_getmetatable(L, VIEW_MT);
    lua_setmetatable(L, -2);

    v->w = gemview_create(th, pdalua_current_catalog(L));
    if (!v->w) return luaL_error(L, "gemview: kein Speicher");

    /* Tasten und Ziffern kommen nur bei einem Widget an, das den Fokus hat.
     * Ein Skript hat nur dieses eine, also bekommt es ihn gleich - sonst wäre
     * die erste Frage jedes Skriptschreibers, warum nichts geschieht. */
    v->w->focused = true;
    return 1;
}

static int v_gc(lua_State *L)
{
    lua_view *v = check_view(L, 1);

    widget_destroy(v->w);
    v->w = NULL;
    return 0;
}

static int v_place(lua_State *L)
{
    lua_view *v = check_view(L, 1);

    v->w->frame = rect_make((int)luaL_checkinteger(L, 2),
                            (int)luaL_checkinteger(L, 3),
                            (int)luaL_checkinteger(L, 4),
                            (int)luaL_checkinteger(L, 5));
    return 0;
}

/* Der Text wird nicht kopiert (gemview.h). Damit er die Anzeige überlebt,
 * bleibt die Lua-Zeichenkette als Nutzerwert am Benutzerdatum hängen: Lua
 * sammelt sie dann nicht ein, solange es die Anzeige gibt. */
static int v_set_text(lua_State *L)
{
    lua_view *v = check_view(L, 1);

    if (lua_isnoneornil(L, 2)) {
        gemview_set_text(v->w, NULL, 0);

        lua_pushnil(L);
        lua_setiuservalue(L, 1, 1);
        return 0;
    }

    size_t      len  = 0;
    const char *text = luaL_checklstring(L, 2, &len);

    lua_pushvalue(L, 2);
    lua_setiuservalue(L, 1, 1);

    gemview_set_text(v->w, text, len);
    return 0;
}

static int v_draw(lua_State *L)
{
    lua_view *v = check_view(L, 1);
    gc       *g = pdalua_current_gc(L);

    if (g) widget_draw(v->w, g);
    return 0;
}

static int v_event(lua_State *L)
{
    lua_view *v = check_view(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);

    event e;
    if (!pdalua_event_from_table(L, 2, &e)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    lua_pushboolean(L, widget_event(v->w, &e));
    return 1;
}

static int v_link_count(lua_State *L)
{
    lua_pushinteger(L, gemview_link_count(check_view(L, 1)->w));
    return 1;
}

static int v_link_url(lua_State *L)
{
    lua_view *v = check_view(L, 1);

    size_t      len = 0;
    const char *url = gemview_link_url(v->w, (int)luaL_checkinteger(L, 2), &len);

    if (!url) lua_pushnil(L);
    else      lua_pushlstring(L, url, len);
    return 1;
}

static int v_selected(lua_State *L)
{
    lua_pushinteger(L, gemview_selected_link(check_view(L, 1)->w));
    return 1;
}

static int v_select(lua_State *L)
{
    lua_view *v = check_view(L, 1);

    gemview_select_link(v->w, (int)luaL_checkinteger(L, 2));
    return 0;
}

/* true, WENN seit dem letzten Aufruf ein Verweis geöffnet wurde - Doppelklick
 * oder Return. Der Merker wird dabei zurückgesetzt, wie bei list_was_opened:
 * so kann ein Skript ihn nach jedem Ereignis abfragen, ohne mitzuzählen. */
static int v_was_opened(lua_State *L)
{
    lua_pushboolean(L, gemview_was_opened(check_view(L, 1)->w));
    return 1;
}

static int v_frame(lua_State *L)
{
    lua_view *v = check_view(L, 1);

    lua_pushinteger(L, v->w->frame.x);
    lua_pushinteger(L, v->w->frame.y);
    lua_pushinteger(L, v->w->frame.w);
    lua_pushinteger(L, v->w->frame.h);
    return 4;
}

static const luaL_Reg VIEW_METHODS[] = {
    { "place",      v_place },
    { "set_text",   v_set_text },
    { "draw",       v_draw },
    { "event",      v_event },
    { "link_count", v_link_count },
    { "link_url",   v_link_url },
    { "selected",   v_selected },
    { "select",     v_select },
    { "was_opened", v_was_opened },
    { "frame",      v_frame },
    { NULL, NULL }
};

/* Eine Klasse: Metatabelle mit __gc und __index, dazu der Erzeuger als
 * globale Funktion. Zwei Bedienelemente reichen, um das einmal aufzuschreiben
 * statt zweimal. */
static void open_class(lua_State *L, const char *mt, lua_CFunction gcf,
                       const luaL_Reg *methods, lua_CFunction ctor,
                       const char *name)
{
    luaL_newmetatable(L, mt);

    lua_pushcfunction(L, gcf);
    lua_setfield(L, -2, "__gc");

    lua_newtable(L);
    luaL_setfuncs(L, methods, 0);
    lua_setfield(L, -2, "__index");

    lua_pop(L, 1);

    lua_pushcfunction(L, ctor);
    lua_setglobal(L, name);
}

void pdalua_open_widgets(lua_State *L)
{
    open_class(L, VIEW_MT, v_gc, VIEW_METHODS, v_new, "gemview");
    open_class(L, BAR_MT,  l_gc, METHODS,      l_new, "scrollbar");
}
