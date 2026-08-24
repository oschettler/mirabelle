/* Lua für dieses Projekt: ein Zustand, eine flache API, keine Zeiger nach
 * außen.
 *
 * ## Was hier die Schichtenregel bedeutet
 *
 * `src/lua/` darf alles benutzen - Grafik, Oberfläche, Speicher. Umgekehrt darf
 * nichts davon Lua kennen. Das ist keine Förmlichkeit: der Index (store/index.h)
 * ist schon einmal so gebaut, und aus demselben Grund. Auf dem Gerät kann Lua
 * fehlen, und die Anwendungen laufen trotzdem.
 *
 * Deshalb gibt es hier auch keine Funktion, die C nach Lua zieht. Diese Datei
 * ist eine Einbahnstraße: Lua ruft C, nie umgekehrt. Wer eine Rückrufliste
 * bräuchte, hätte den Ereignisbus.
 *
 * ## Ein Zustand, Anwendungen sind Tabellen (D-4)
 *
 * Picotron gibt jedem Prozess einen eigenen `lua_State`. Sauber, aber teuer,
 * und für einen Kurs zu viel Maschinerie. Wir behalten die Form - einen
 * Ereignisbus -, nicht die Isolation.
 *
 * ## Welche Standardbibliotheken offen sind
 *
 * base, table, string, math - und sonst keine. Kein `io`, kein `os`, kein
 * `package`. Eine Schemadatei ist eine Konfigurationsdatei, und die soll keine
 * Dateien löschen können, auch nicht versehentlich. Wer aus Lua an Daten
 * herankommen will, geht über `store`, und das kennt nur den Vault.
 */
#ifndef PDA_LUA_PDALUA_H
#define PDA_LUA_PDALUA_H

#include <stdbool.h>
#include <stddef.h>

#include "app/schema.h"
#include "app/shell.h"
#include "core/collate.h"
#include "core/i18n.h"
#include "gfx/draw.h"
#include "plat/plat.h"
#include "store/vault.h"

typedef struct lua_State lua_State;

/* Öffnet einen Zustand und richtet die API ein.
 *
 * cat darf NULL sein; dann liefert T() den Schlüssel zurück, wie überall.
 * Bei einem Fehler NULL und eine Meldung in err. */
lua_State *pdalua_open(const catalog *cat, char *err, size_t err_size);
void       pdalua_close(lua_State *L);

/* Setzt das Ziel der Zeichenfunktionen. Wird je Bild einmal gerufen, bevor das
 * Skript zeichnet.
 *
 * Der Zeichenzustand wird nicht kopiert - Lua zeichnet in genau das, worauf g
 * zeigt. Er muss also so lange leben, wie gezeichnet wird. NULL schaltet die
 * Zeichenfunktionen ab; sie tun dann nichts, statt abzustürzen. Das ist der
 * Normalzustand außerhalb des Zeichnens. */
void pdalua_set_gc(lua_State *L, gc *g);

/* Öffnet Lua den Vault unter dem Namen `store`.
 *
 * Ohne diesen Aufruf gibt es `store` nicht - ein Skript, das nur zeichnet,
 * bekommt keinen Zugang zu den Daten des Nutzers. Vault und Faltungstabellen
 * müssen den Zustand überleben; sie werden nicht kopiert.
 *
 * `store` ist die einzige Tür nach draußen. Es gibt kein io und kein os
 * (siehe oben), also ist alles, was ein Skript an Dateien anfassen kann, ein
 * Datensatz in einer Sammlung. */
void pdalua_set_vault(lua_State *L, vault *v,
                      const collate *sort, const collate *search);

/* Liest ein Schema aus einer Lua-Datei.
 *
 * Die Datei gibt eine Tabelle zurück, wie DESIGN.md Abschnitt 10 sie zeigt.
 * Heraus kommt dieselbe `schema`-Struktur, die schema_load() aus einer
 * Textdatei liest - das ist D-15: der Vertrag ist die Struktur, nicht die
 * Sprache. Ein Test lädt beide Fassungen desselben Schemas und vergleicht sie
 * Feld für Feld.
 *
 * Geprüft wird danach genauso streng: die Tabelle geht durch dieselbe
 * Schlussprüfung wie eine Textdatei. Eine Spalte, die kein Feld ist, fällt
 * also auch hier beim Laden auf. */
bool pdalua_schema(lua_State *L, const char *path, schema *out,
                   char *err, size_t err_size);

/* Dasselbe als Lader für die Schale: sie sucht Dateien mit der genannten
 * Endung und reicht jede hierher. Die Schale kennt Lua damit weiterhin nicht,
 * sie kennt nur einen Funktionszeiger. */
shell_schemas pdalua_schema_loader(lua_State *L);

/* Hinterlegt das Thema. Muss vor pdalua_open_widgets() gerufen werden.
 *
 * Der Zustand legt eine Kopie an - Widgets halten einen Zeiger auf ihr Thema
 * und müssen es überleben (widget.h). Dieselbe Kopie füllt die globale Tabelle
 * `theme`, damit ein Skript Maße nicht raten muss. */
void pdalua_set_theme(lua_State *L, const theme *th);

/* Richtet die Bedienelemente ein, die ein Skript benutzen darf: den
 * Rollbalken und die Gemtext-Anzeige. Es sind dieselben Widgets wie im
 * Programm, keine nachgebauten. */
void pdalua_open_widgets(lua_State *L);

/* Der Zeichenzustand, in den gerade gezeichnet wird, oder NULL. Für
 * Bedienelemente, die sich selbst zeichnen. */
gc *pdalua_current_gc(lua_State *L);

/* Der Textkatalog dieses Zustands, oder NULL. Bedienelemente holen daraus
 * ihre Beschriftungen - das Aufzählungszeichen etwa. */
const catalog *pdalua_current_catalog(lua_State *L);

/* Baut aus einer Lua-Ereignistabelle ein C-Ereignis. false, wenn die Tabelle
 * keines beschreibt. */
bool pdalua_event_from_table(lua_State *L, int idx, event *out);

/* Richtet `net` und `gemtext` ein - Seiten abrufen und zerlegen.
 *
 * Getrennt von pdalua_open(), weil ein Zustand, der nur ein Schema lesen soll,
 * nicht ins Netz greifen können muss. Wer es nicht aufruft, hat ein Lua ohne
 * Zugang nach draußen. */
void pdalua_open_net(lua_State *L);

/* Richtet app{}, on() und send() ein - die Anmeldung von Anwendungen und den
 * Ereignisbus (pdalua_apps.c).
 *
 * Getrennt von pdalua_open(), weil ein Zustand, der nur ein Schema lesen soll,
 * keine Anwendungen anmelden können muss. */
void pdalua_open_apps(lua_State *L);

/* Die Brücke zur Schale: eine Handvoll Funktionszeiger, mit denen shell.c die
 * angemeldeten Anwendungen aufruft, ohne Lua zu kennen. */
shell_scripting pdalua_scripting(lua_State *L);

/* Führt eine Datei aus. Bei einem Fehler false und die Lua-Meldung in err -
 * mit Dateiname und Zeilennummer, so wie Lua sie baut. */
bool pdalua_dofile(lua_State *L, const char *path, char *err, size_t err_size);

/* Führt eine Zeichenkette aus. name erscheint in Fehlermeldungen. */
bool pdalua_dostring(lua_State *L, const char *code, const char *name,
                     char *err, size_t err_size);

#endif /* PDA_LUA_PDALUA_H */
