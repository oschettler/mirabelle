/* Der Zugang zu den Daten aus Lua, siehe pdalua.h.
 *
 * Ein Datensatz ist in Lua eine Tabelle: die Felder unter ihren Namen, der
 * Gemtext-Körper unter `body`, die Kennung unter `id`. Ein Listenfeld wird ein
 * Lua-Feld, ein Skalar eine Zeichenkette. Mehr Abbildung braucht es nicht - und
 * weniger wäre eine, bei der ein Skript wissen müsste, wie Front Matter
 * aussieht.
 *
 * Geschrieben wird über denselben Weg wie im Browser: Text zusammensetzen,
 * einlesen, speichern. frontmatter.h kennt keinen Setter, und das soll so
 * bleiben - ein Datensatz entsteht aus seiner Datei.
 */
#include <lauxlib.h>
#include <lua.h>

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/collate.h"
#include "lua/pdalua.h"
#include "store/query.h"
#include "store/record.h"
#include "store/vault.h"

#define IDS_MAX   512
#define TEXT_MAX  (64 * 1024)

static const char KEY_VAULT[]  = "pda.vault";
static const char KEY_SORT[]   = "pda.collate.sort";
static const char KEY_SEARCH[] = "pda.collate.search";

static void reg_set(lua_State *L, const char *key, void *p)
{
    lua_pushlightuserdata(L, (void *)key);
    if (p) lua_pushlightuserdata(L, p);
    else   lua_pushnil(L);
    lua_settable(L, LUA_REGISTRYINDEX);
}

static void *reg_get(lua_State *L, const char *key)
{
    lua_pushlightuserdata(L, (void *)key);
    lua_gettable(L, LUA_REGISTRYINDEX);

    void *p = lua_touserdata(L, -1);
    lua_pop(L, 1);
    return p;
}

static vault *the_vault(lua_State *L)
{
    vault *v = reg_get(L, KEY_VAULT);
    if (!v) luaL_error(L, "store: kein Vault geöffnet");
    return v;
}

/* --- Datensatz nach Lua ----------------------------------------------------------- */

static void push_record(lua_State *L, const char *id, record *rec)
{
    lua_newtable(L);

    /* Die Kennung wird nicht eigens gesetzt: vault_save schreibt sie in die
     * Datei, also steht sie im Front Matter und kommt mit der Schleife
     * darunter. Sie hier zusätzlich zu setzen sähe sorgfältig aus, würde aber
     * eine Zeile später überschrieben. */
    (void)id;

    lua_pushstring(L, record_body(rec));
    lua_setfield(L, -2, "body");

    frontmatter *fm = record_fields(rec);
    int          n  = frontmatter_count(fm);

    for (int i = 0; i < n; i++) {
        const char *key = frontmatter_key_at(fm, i);
        if (!key) continue;

        int count = frontmatter_list_count(fm, key);

        /* Ein Skalar bleibt eine Zeichenkette. Ihn als einelementiges Feld zu
         * liefern wäre gleichförmiger, aber jedes Skript müsste dann
         * auspacken, was gar nicht eingepackt war. */
        if (count == 1) {
            lua_pushstring(L, frontmatter_list_at(fm, key, 0));
        } else {
            lua_newtable(L);
            for (int j = 0; j < count; j++) {
                lua_pushstring(L, frontmatter_list_at(fm, key, j));
                lua_rawseti(L, -2, j + 1);
            }
        }
        lua_setfield(L, -2, key);
    }
}

/* --- Lua nach Datensatz ------------------------------------------------------------
 *
 * Die Schlüssel werden sortiert geschrieben. Lua-Tabellen haben keine
 * Reihenfolge, und ohne eine feste käme bei jedem Speichern eine andere Datei
 * heraus - jedes Sicherungswerkzeug meldete dann Änderungen, die keine sind.
 */

typedef struct { char key[64]; } keyname;

static int keyname_cmp(const void *a, const void *b)
{
    return strcmp(((const keyname *)a)->key, ((const keyname *)b)->key);
}

static int append(char *out, size_t cap, size_t *n, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int wrote = vsnprintf(out + *n, cap - *n, fmt, ap);
    va_end(ap);

    if (wrote < 0 || (size_t)wrote >= cap - *n) return 0;
    *n += (size_t)wrote;
    return 1;
}

/* Baut den Dateitext aus der Tabelle auf dem Stapel bei idx. */
static bool compose(lua_State *L, int idx, char *out, size_t cap, char *err, size_t err_size)
{
    keyname keys[64];
    int     count = 0;

    lua_pushnil(L);
    while (lua_next(L, idx) != 0) {
        if (lua_type(L, -2) == LUA_TSTRING && count < 64) {
            const char *k = lua_tostring(L, -2);
            if (strcmp(k, "body") != 0 && strlen(k) < sizeof keys[0].key)
                snprintf(keys[count++].key, sizeof keys[0].key, "%s", k);
        }
        lua_pop(L, 1);
    }
    qsort(keys, (size_t)count, sizeof keys[0], keyname_cmp);

    size_t n = 0;
    if (!append(out, cap, &n, "---\n")) goto too_big;

    for (int i = 0; i < count; i++) {
        lua_getfield(L, idx, keys[i].key);

        if (lua_istable(L, -1)) {
            if (!append(out, cap, &n, "%s: [", keys[i].key)) goto too_big;

            lua_Integer len = luaL_len(L, -1);
            for (lua_Integer j = 1; j <= len; j++) {
                lua_rawgeti(L, -1, j);
                const char *v = lua_tostring(L, -1);
                if (!append(out, cap, &n, "%s%s", j > 1 ? ", " : "", v ? v : "")) {
                    lua_pop(L, 2);
                    goto too_big;
                }
                lua_pop(L, 1);
            }
            if (!append(out, cap, &n, "]\n")) { lua_pop(L, 1); goto too_big; }
        } else {
            const char *v = lua_tostring(L, -1);
            if (v && *v) {
                if (strchr(v, '\n')) {
                    lua_pop(L, 1);
                    snprintf(err, err_size,
                             "store.put: '%s' enthält einen Zeilenumbruch",
                             keys[i].key);
                    return false;
                }
                if (!append(out, cap, &n, "%s: %s\n", keys[i].key, v)) {
                    lua_pop(L, 1);
                    goto too_big;
                }
            }
        }
        lua_pop(L, 1);
    }

    lua_getfield(L, idx, "body");
    const char *body = lua_tostring(L, -1);
    int ok = append(out, cap, &n, "---\n%s", body ? body : "");
    lua_pop(L, 1);
    if (!ok) goto too_big;

    return true;

too_big:
    snprintf(err, err_size, "store.put: der Datensatz ist zu groß");
    return false;
}

/* --- Die Funktionen ---------------------------------------------------------------- */

static int l_get(lua_State *L)
{
    const char *coll = luaL_checkstring(L, 1);
    const char *id   = luaL_checkstring(L, 2);

    char    err[256] = "";
    record *rec = vault_load(the_vault(L), coll, id, err, sizeof err);
    if (!rec) {
        /* Nicht da ist kein Fehler, sondern nil. Ein Skript, das einen
         * Datensatz sucht, den es nicht mehr gibt, soll das prüfen können,
         * ohne pcall darum zu legen. */
        lua_pushnil(L);
        return 1;
    }

    push_record(L, id, rec);
    record_free(rec);
    return 1;
}

static int l_put(lua_State *L)
{
    const char *coll = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);

    static char text[TEXT_MAX];
    char        err[256] = "";
    if (!compose(L, 2, text, sizeof text, err, sizeof err))
        return luaL_error(L, "%s", err);

    record *rec = record_parse(text, strlen(text), coll, err, sizeof err);
    if (!rec) return luaL_error(L, "store.put: %s", err);

    char id[RECORD_ID_LEN + 1];
    bool ok = vault_save(the_vault(L), coll, rec, id, sizeof id, err, sizeof err);
    record_free(rec);

    if (!ok) return luaL_error(L, "store.put: %s", err);

    lua_pushstring(L, id);
    return 1;
}

static int l_delete(lua_State *L)
{
    const char *coll = luaL_checkstring(L, 1);
    const char *id   = luaL_checkstring(L, 2);

    char err[256] = "";
    if (!vault_delete(the_vault(L), coll, id, err, sizeof err))
        return luaL_error(L, "store.delete: %s", err);

    return 0;
}

/* Baut eine Abfrage aus einer Lua-Tabelle:
 *
 *     store.find("Aufgaben", { status = "offen", text = "Köln",
 *                              sort = "due", desc = false })
 *
 * Jeder andere Schlüssel ist eine Gleichheitsbedingung auf einem Feld. Die
 * Namen `text`, `sort` und `desc` sind dafür reserviert - eine Sammlung mit
 * einem Feld namens `sort` müsste anders heißen, und das ist der Preis dafür,
 * dass die häufige Abfrage kurz bleibt. */
static void build_query(lua_State *L, int idx, const char *coll, query *q)
{
    query_init(q, coll);
    if (lua_isnoneornil(L, idx)) return;

    luaL_checktype(L, idx, LUA_TTABLE);

    lua_getfield(L, idx, "text");
    if (lua_isstring(L, -1)) query_text(q, lua_tostring(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, idx, "desc");
    bool desc = lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, idx, "sort");
    if (lua_isstring(L, -1)) query_order(q, lua_tostring(L, -1), desc);
    lua_pop(L, 1);

    lua_pushnil(L);
    while (lua_next(L, idx) != 0) {
        if (lua_type(L, -2) == LUA_TSTRING) {
            const char *k = lua_tostring(L, -2);
            const char *v = lua_tostring(L, -1);

            if (v && strcmp(k, "text") != 0 && strcmp(k, "sort") != 0 &&
                strcmp(k, "desc") != 0)
                query_where(q, k, QF_EQUALS, v);
        }
        lua_pop(L, 1);
    }
}

/* Läuft über die Sammlung und liefert die passenden Datensätze als Feld.
 *
 * Über query_matches und nicht über den Index: das ist der Weg, den es immer
 * gibt (D-3), und ein Skript soll nicht davon abhängen, ob eine Datenbank
 * dabei ist. */
static int find_into(lua_State *L, const char *coll, query *q, bool ids_only)
{
    vault *v = the_vault(L);

    static char ids[IDS_MAX][RECORD_ID_LEN + 1];
    int         n = 0;
    char        err[256] = "";

    if (!vault_list(v, coll, ids, IDS_MAX, &n, err, sizeof err))
        return luaL_error(L, "store.find: %s", err);

    const collate *search = reg_get(L, KEY_SEARCH);
    const collate *sort   = reg_get(L, KEY_SORT);

    /* Erst sammeln, dann sortieren, dann nach Lua. Sortiert wird über
     * query_compare, damit dieselbe Reihenfolge herauskommt wie im Browser. */
    typedef struct { char id[RECORD_ID_LEN + 1]; record *rec; } hit;
    static hit hits[IDS_MAX];
    int        kept = 0;

    for (int i = 0; i < n; i++) {
        record *rec = vault_load(v, coll, ids[i], err, sizeof err);
        if (!rec) continue;

        if (!query_matches(q, rec, search)) { record_free(rec); continue; }

        snprintf(hits[kept].id, sizeof hits[kept].id, "%s", ids[i]);
        hits[kept].rec = rec;
        kept++;
    }

    /* Ein Einfügesortieren statt qsort: qsort reicht keinen Kontext durch, und
     * die Abfrage samt Tabelle müsste sonst in einer Dateiglobalen liegen. Bei
     * ein paar hundert Datensätzen ist der Unterschied nicht zu messen. */
    for (int i = 1; i < kept; i++) {
        hit h = hits[i];
        int j = i - 1;
        while (j >= 0 && query_compare(q, hits[j].rec, h.rec, sort) > 0) {
            hits[j + 1] = hits[j];
            j--;
        }
        hits[j + 1] = h;
    }

    lua_newtable(L);
    for (int i = 0; i < kept; i++) {
        if (ids_only) lua_pushstring(L, hits[i].id);
        else          push_record(L, hits[i].id, hits[i].rec);

        lua_rawseti(L, -2, i + 1);
        record_free(hits[i].rec);
    }
    return 1;
}

static int l_find(lua_State *L)
{
    const char *coll = luaL_checkstring(L, 1);

    query q;
    build_query(L, 2, coll, &q);
    return find_into(L, coll, &q, false);
}

static int l_ids(lua_State *L)
{
    const char *coll = luaL_checkstring(L, 1);

    query q;
    build_query(L, 2, coll, &q);
    return find_into(L, coll, &q, true);
}

static int l_search(lua_State *L)
{
    const char *coll = luaL_checkstring(L, 1);
    const char *text = luaL_checkstring(L, 2);

    query q;
    query_init(&q, coll);
    query_text(&q, text);
    return find_into(L, coll, &q, false);
}

static const luaL_Reg STORE[] = {
    { "get",    l_get },
    { "put",    l_put },
    { "delete", l_delete },
    { "find",   l_find },
    { "ids",    l_ids },
    { "search", l_search },
    { NULL, NULL }
};

void pdalua_set_vault(lua_State *L, vault *v,
                      const collate *sort, const collate *search)
{
    reg_set(L, KEY_VAULT,  v);
    reg_set(L, KEY_SORT,   (void *)(uintptr_t)sort);
    reg_set(L, KEY_SEARCH, (void *)(uintptr_t)search);

    if (!v) {
        lua_pushnil(L);
        lua_setglobal(L, "store");
        return;
    }

    lua_newtable(L);
    for (const luaL_Reg *fn = STORE; fn->func; fn++) {
        lua_pushcfunction(L, fn->func);
        lua_setfield(L, -2, fn->name);
    }
    lua_setglobal(L, "store");
}
