/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Ein Schema aus einer Lua-Tabelle, siehe pdalua.h.
 *
 * Das ist die Einlösung von D-15: der Vertrag ist die C-Struktur `schema` aus
 * app/schema.h, Lua nur die Schreibweise, in der sie in einer Datei steht. Der
 * Browser sieht einer Anwendung nicht an, woher sie kam.
 *
 * Diese Datei prüft die Gestalt der Tabelle - Zeichenketten, wo Namen stehen,
 * Listen, wo Listen stehen. Ob das Ergebnis eine Anwendung beschreibt, die es
 * geben kann, prüft schema_check() zum Schluss. Käme je ein zweiter Lader
 * dazu, prüfte er mit derselben Funktion.
 */
#include <lauxlib.h>
#include <lua.h>

#include <stdio.h>
#include <string.h>

#include "app/schema.h"
#include "lua/pdalua.h"

/* --- Lesen aus der Tabelle --------------------------------------------------------
 *
 * Alle Helfer lassen den Stapel, wie sie ihn vorfinden. Das ist bei Lua-Code
 * die Regel, an der sich Fehler entscheiden: ein Helfer, der etwas liegen
 * lässt, verschiebt jeden folgenden Index um eins, und der Fehler zeigt sich
 * weit von seiner Ursache entfernt.
 */

/* Holt tbl[key] als Zeichenkette nach dst. Fehlt der Schlüssel, bleibt dst
 * leer. false, wenn der Wert keine Zeichenkette ist oder nicht passt. */
static bool get_string(lua_State *L, int tbl, const char *key,
                       char *dst, size_t cap, char *err, size_t err_size)
{
    lua_getfield(L, tbl, key);

    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        dst[0] = '\0';
        return true;
    }

    /* Hier wird auf eine echte Zeichenkette bestanden. `type = 42` ist kein
     * kurz geschriebener Name, sondern ein Versehen, und es fällt sonst erst
     * auf, wenn irgendwo „42" als Sammlungsname auftaucht.
     *
     * Bei den Werten einer Auswahl ist es umgekehrt - siehe get_list(). */
    const char *s = lua_type(L, -1) == LUA_TSTRING ? lua_tostring(L, -1) : NULL;
    if (!s) {
        snprintf(err, err_size, "'%s' ist keine Zeichenkette", key);
        lua_pop(L, 1);
        return false;
    }
    if (strlen(s) >= cap) {
        snprintf(err, err_size, "'%s' ist zu lang (höchstens %zu Zeichen)",
                 key, cap - 1);
        lua_pop(L, 1);
        return false;
    }

    snprintf(dst, cap, "%s", s);
    lua_pop(L, 1);
    return true;
}

/* Holt tbl[key] als Feld von Zeichenketten. */
static bool get_list(lua_State *L, int tbl, const char *key,
                     char (*dst)[SCHEMA_NAME_MAX], int cap, int *count,
                     char *err, size_t err_size)
{
    *count = 0;

    lua_getfield(L, tbl, key);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return true;
    }
    if (!lua_istable(L, -1)) {
        snprintf(err, err_size, "'%s' ist keine Liste", key);
        lua_pop(L, 1);
        return false;
    }

    lua_Integer n = luaL_len(L, -1);
    for (lua_Integer i = 1; i <= n; i++) {
        if (*count >= cap) {
            snprintf(err, err_size, "'%s': höchstens %d Einträge", key, cap);
            lua_pop(L, 1);
            return false;
        }

        lua_rawgeti(L, -1, i);

        /* Hier wandelt lua_tostring Zahlen mit um, und das ist erwünscht: die
         * Werte einer Auswahl dürfen im Schema als Zahlen dastehen
         * (values = {1,2,3,4,5}), im Datensatz sind sie dann Text wie alles
         * andere. Eine Prioritätenliste als {"1","2","3"} zu schreiben wäre
         * eine Förmlichkeit ohne Gewinn. */
        const char *s = lua_tostring(L, -1);

        if (!s || strlen(s) >= SCHEMA_NAME_MAX) {
            snprintf(err, err_size, "'%s': Eintrag %d ist unbrauchbar",
                     key, (int)i);
            lua_pop(L, 2);
            return false;
        }

        snprintf(dst[*count], SCHEMA_NAME_MAX, "%s", s);
        (*count)++;
        lua_pop(L, 1);
    }

    lua_pop(L, 1);
    return true;
}

static bool get_bool(lua_State *L, int tbl, const char *key)
{
    lua_getfield(L, tbl, key);
    bool v = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return v;
}

/* --- Ein Feld ---------------------------------------------------------------------- */

static bool read_field(lua_State *L, int tbl, schema_field *f,
                       char *err, size_t err_size)
{
    memset(f, 0, sizeof *f);

    if (!get_string(L, tbl, "name", f->name, sizeof f->name, err, err_size))
        return false;
    if (!f->name[0]) {
        snprintf(err, err_size, "ein Feld ohne name");
        return false;
    }

    if (!get_string(L, tbl, "label", f->label, sizeof f->label, err, err_size))
        return false;

    char kind[SCHEMA_NAME_MAX];
    if (!get_string(L, tbl, "kind", kind, sizeof kind, err, err_size)) return false;
    if (!kind[0]) {
        snprintf(err, err_size, "Feld '%s': kind fehlt", f->name);
        return false;
    }

    /* Die Namen der Feldtypen sind dieselben wie in der Textfassung -
     * schema_kind_name() ist die eine Stelle, die sie kennt. */
    bool found = false;
    for (int k = 0; k <= FIELD_CHOICE && !found; k++)
        if (strcmp(schema_kind_name((field_kind)k), kind) == 0) {
            f->kind = (field_kind)k;
            found   = true;
        }

    if (!found) {
        snprintf(err, err_size, "Feld '%s': unbekannter Feldtyp '%s'", f->name, kind);
        return false;
    }

    f->required = get_bool(L, tbl, "required");

    return get_list(L, tbl, "values", f->values, SCHEMA_VALUES_MAX,
                    &f->value_count, err, err_size);
}

/* --- Das Ganze ---------------------------------------------------------------------- */

static bool read_schema(lua_State *L, int tbl, schema *s, char *err, size_t err_size)
{
    memset(s, 0, sizeof *s);

    if (!get_string(L, tbl, "type",   s->type,   sizeof s->type,   err, err_size) ||
        !get_string(L, tbl, "folder", s->folder, sizeof s->folder, err, err_size) ||
        !get_string(L, tbl, "label",  s->label,  sizeof s->label,  err, err_size))
        return false;

    if (!get_string(L, tbl, "sort", s->sort, sizeof s->sort, err, err_size))
        return false;
    s->sort_desc = get_bool(L, tbl, "sort_desc");

    if (!get_list(L, tbl, "columns", s->columns, SCHEMA_COLUMNS_MAX,
                  &s->column_count, err, err_size))
        return false;
    if (!get_list(L, tbl, "form", s->form, SCHEMA_FIELDS_MAX,
                  &s->form_count, err, err_size))
        return false;

    char view[SCHEMA_NAME_MAX];
    if (!get_string(L, tbl, "view", view, sizeof view, err, err_size)) return false;

    if (!view[0] || strcmp(view, "list") == 0) {
        s->view = VIEW_LIST;
    } else if (strcmp(view, "month") == 0) {
        s->view = VIEW_MONTH;
        if (!get_string(L, tbl, "view_field", s->view_field, sizeof s->view_field,
                        err, err_size))
            return false;
        if (!s->view_field[0]) {
            snprintf(err, err_size, "view = 'month' ohne view_field");
            return false;
        }
    } else {
        snprintf(err, err_size, "unbekannte Ansicht '%s' (list oder month)", view);
        return false;
    }

    lua_getfield(L, tbl, "fields");
    if (!lua_istable(L, -1)) {
        snprintf(err, err_size, "fields fehlt oder ist keine Liste");
        lua_pop(L, 1);
        return false;
    }

    lua_Integer n = luaL_len(L, -1);
    for (lua_Integer i = 1; i <= n; i++) {
        if (s->field_count >= SCHEMA_FIELDS_MAX) {
            snprintf(err, err_size, "höchstens %d Felder", SCHEMA_FIELDS_MAX);
            lua_pop(L, 1);
            return false;
        }

        lua_rawgeti(L, -1, i);
        if (!lua_istable(L, -1)) {
            snprintf(err, err_size, "fields[%d] ist keine Tabelle", (int)i);
            lua_pop(L, 2);
            return false;
        }

        bool ok = read_field(L, lua_gettop(L), &s->fields[s->field_count],
                             err, err_size);
        lua_pop(L, 1);
        if (!ok) { lua_pop(L, 1); return false; }

        /* Gesucht wird VOR dem Hochzählen: dann reicht die Suche nur über die
         * schon eingetragenen Felder, und ein Treffer ist wirklich eine
         * Dopplung und nicht das Feld selbst. */
        if (schema_field_by_name(s, s->fields[s->field_count].name)) {
            snprintf(err, err_size, "das Feld '%s' gibt es schon",
                     s->fields[s->field_count].name);
            lua_pop(L, 1);
            return false;
        }
        s->field_count++;
    }
    lua_pop(L, 1);
    return true;
}

bool pdalua_schema(lua_State *L, const char *path, schema *out,
                   char *err, size_t err_size)
{
    int base = lua_gettop(L);

    if (luaL_loadfile(L, path) != LUA_OK || lua_pcall(L, 0, 1, 0) != LUA_OK) {
        const char *msg = lua_tostring(L, -1);
        snprintf(err, err_size, "%s", msg ? msg : "unbekannter Fehler");
        lua_settop(L, base);
        return false;
    }

    if (!lua_istable(L, -1)) {
        snprintf(err, err_size, "%s: die Datei gibt keine Tabelle zurück", path);
        lua_settop(L, base);
        return false;
    }

    schema tmp;
    char   msg[256] = "";
    bool   ok = read_schema(L, lua_gettop(L), &tmp, msg, sizeof msg);

    /* Den Stapel auf den Stand vor dem Aufruf zurücksetzen. Ein einzelner
     * liegengebliebener Wert fällt nicht auf - Lua wächst mit -, aber ein
     * Lader, der bei jedem Aufruf einen hinterlässt, füllt ihn irgendwann.
     * Die Regel ist einfacher als jede Buchführung darüber, wie viel gerade
     * obendrauf liegt. */
    lua_settop(L, base);

    if (!ok) {
        snprintf(err, err_size, "%s: %s", path, msg);
        return false;
    }

    /* Dieselbe Schlussprüfung wie bei der Textfassung. Sie steht in schema.c
     * und ist von dort öffentlich gemacht - zwei Lader, aber nur eine
     * Vorstellung davon, was ein gültiges Schema ist. */
    if (!schema_check(&tmp, path, err, err_size)) return false;

    *out = tmp;
    return true;
}

/* --- Als Lader für die Schale ----------------------------------------------- */

static bool loader(void *user, const char *path, schema *out,
                   char *err, size_t err_size)
{
    return pdalua_schema((lua_State *)user, path, out, err, err_size);
}

shell_schemas pdalua_schema_loader(lua_State *L)
{
    shell_schemas ld = { L, ".lua", loader };
    return ld;
}
