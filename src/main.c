/* Die Hauptschleife.
 *
 * Mehr ist es nicht: Ereignisse abholen, zeichnen, ausgeben. Alles
 * Inhaltliche steht in app/shell.c und ist dadurch ohne Bildschirm prüfbar;
 * hier bleibt nur, was ohne echtes Fenster keinen Sinn ergibt.
 *
 * Diese Datei ist auch die einzige, die entscheidet, WAS es gibt: sie lädt die
 * Tabellen, öffnet den Vault, richtet - falls vorhanden - Lua ein und übergibt
 * das alles an die Schale. Die Schale selbst kennt weder Lua noch SQLite; sie
 * bekommt, was da ist.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/shell.h"
#include "core/collate.h"
#include "core/i18n.h"
#include "core/keymap.h"
#include "gfx/bitmap.h"
#include "gfx/draw.h"
#include "gfx/pbm.h"
#include "plat/plat.h"
#include "store/vault.h"
#include "ui/theme.h"

#ifdef PDA_WITH_LUA
#include "lua/pdalua.h"
#endif

#ifndef PDA_DATA_DIR
#define PDA_DATA_DIR "data"
#endif

/* Alles, was das Programm besitzt. In einer Struktur, damit das Aufräumen an
 * einer Stelle steht und nicht an jedem Rücksprung wiederholt wird. */
typedef struct {
    keymap  *km;
    catalog *cat;
    collate *sort;
    collate *search;
    vault   *vault;
    shell   *sh;
    bitmap   fb;

#ifdef PDA_WITH_LUA
    lua_State *lua;
#endif
} program;

static void program_free(program *p)
{
    if (p->sh) shell_destroy(p->sh);
#ifdef PDA_WITH_LUA
    if (p->lua) pdalua_close(p->lua);
#endif
    if (p->vault) vault_close(p->vault);

    collate_free(p->search);
    collate_free(p->sort);
    i18n_free(p->cat);
    keymap_free(p->km);
    bitmap_free(&p->fb);
}

/* Setzt eine führende Tilde in den Heimatpfad um.
 *
 * Die Tilde ist eine Erfindung der Kommandozeile: die Schale ersetzt sie,
 * bevor ein Programm sie zu sehen bekommt. Wer den Pfad dagegen in eine
 * Konfigurationsdatei schreibt oder als Voreinstellung im Programm hat, muss
 * es selbst tun - sonst sucht das Programm ein Verzeichnis, das wörtlich „~"
 * heißt, und legt es im schlimmsten Fall an.
 *
 * Das steht hier und nicht in plat.h: der Vault (store/vault.h) bekommt einen
 * fertigen Pfad, und wo der herkommt, ist die Entscheidung des Programms. */
static const char *expand_home(const char *path)
{
    static char out[1024];

    if (!path || path[0] != '~' || (path[1] && path[1] != '/')) return path;

    const char *home = getenv("HOME");
    if (!home || !*home) return path;

    snprintf(out, sizeof out, "%s%s", home, path + 1);
    return out;
}

/* Der Vault liegt unter ~/PDA, falls nichts anderes gesagt wird. Ein eigener
 * Ort ist praktisch, um mit einem leeren Stand zu arbeiten, ohne die eigenen
 * Daten anzufassen. */
static const char *vault_path(int argc, char **argv)
{
    for (int i = 1; i + 1 < argc; i++)
        if (strcmp(argv[i], "--vault") == 0) return expand_home(argv[i + 1]);

    const char *env = getenv("PDA_VAULT");
    return expand_home(env && *env ? env : "~/PDA");
}

static bool has_flag(int argc, char **argv, const char *name)
{
    for (int i = 1; i < argc; i++)
        if (strcmp(argv[i], name) == 0) return true;
    return false;
}

static const char *arg_after(int argc, char **argv, const char *name)
{
    for (int i = 1; i + 1 < argc; i++)
        if (strcmp(argv[i], name) == 0) return argv[i + 1];
    return NULL;
}

/* Lädt eine Datei unterhalb von data/ und meldet, wenn es nicht geht. Der
 * Rückgabewert ist der Pfad, damit sich der Aufruf in eine Zeile schreiben
 * lässt. */
static const char *data_path(const char *rel)
{
    static char path[512];
    snprintf(path, sizeof path, "%s/%s", PDA_DATA_DIR, rel);
    return path;
}

int main(int argc, char **argv)
{
    const char *shot = arg_after(argc, argv, "--shot");
    const char *lang = arg_after(argc, argv, "--lang");
    if (!lang) lang = "de";

    plat_config cfg = { .width = 800, .height = 480, .scale = 0, .title = "PDA" };
    if (!plat_init(&cfg)) {
        fprintf(stderr, "Der Bildschirm ließ sich nicht öffnen.\n");
        return 1;
    }

    int w, h;
    plat_display_size(&w, &h);

    program p;
    memset(&p, 0, sizeof p);

    char err[512] = "";
    if (!bitmap_init(&p.fb, w, h)) {
        fprintf(stderr, "Kein Speicher für den Bildspeicher.\n");
        plat_shutdown();
        return 1;
    }

    theme th;
    theme_defaults(&th);
    if (!theme_load(&th, data_path("themes/desktop.theme"), err, sizeof err))
        fprintf(stderr, "Thema: %s\n", err);

    char langfile[64];
    snprintf(langfile, sizeof langfile, "lang/%s.strings", lang);

    p.km     = keymap_load(data_path("keys/default.keys"), err, sizeof err);
    p.cat    = i18n_load(data_path(langfile), err, sizeof err);
    p.sort   = collate_load(data_path("lang/de.sort"), err, sizeof err);
    p.search = collate_load(data_path("collate/search.fold"), err, sizeof err);

    if (!p.km || !p.cat) {
        fprintf(stderr, "Grundlagen fehlen: %s\n", err);
        program_free(&p);
        plat_shutdown();
        return 1;
    }

    const char *vpath = vault_path(argc, argv);

    p.vault = vault_open(vpath, err, sizeof err);
    if (!p.vault) {
        fprintf(stderr, "Der Vault unter %s ließ sich nicht öffnen: %s\n", vpath, err);
        fprintf(stderr, "Ein anderer Ort geht mit --vault <verzeichnis>"
                        " oder über PDA_VAULT.\n");
        program_free(&p);
        plat_shutdown();
        return 1;
    }

    shell_config sc = {
        .data_dir = PDA_DATA_DIR,
        .vault    = p.vault,
        .theme    = &th,
        .catalog  = p.cat,
        .keymap   = p.km,
        .sort     = p.sort,
        .search   = p.search,
        .screen_w = w,
        .screen_h = h,
    };

#ifdef PDA_WITH_LUA
    /* Skriptanwendungen, falls Lua dabei ist. Fehlt es, fehlen sie - die
     * Anwendungen aus den Schemadateien laufen weiter. */
    static shell_scripting scripting;

    p.lua = pdalua_open(p.cat, err, sizeof err);
    if (p.lua) {
        pdalua_open_apps(p.lua);
        pdalua_open_net(p.lua);
        pdalua_set_theme(p.lua, &th);
        pdalua_open_widgets(p.lua);
        pdalua_set_vault(p.lua, p.vault, p.sort, p.search);

        plat_dirent apps[32];
        int         n = 0;
        char        dir[512];
        snprintf(dir, sizeof dir, "%s/apps", PDA_DATA_DIR);

        if (plat_list(dir, apps, 32, &n)) {
            for (int i = 0; i < n; i++) {
                size_t len = strlen(apps[i].name);
                if (apps[i].is_dir || len < 5) continue;
                if (strcmp(apps[i].name + len - 4, ".lua") != 0) continue;

                char path[700];
                snprintf(path, sizeof path, "%s/%s", dir, apps[i].name);

                if (!pdalua_dofile(p.lua, path, err, sizeof err))
                    fprintf(stderr, "%s\n", err);
            }
        }

        scripting  = pdalua_scripting(p.lua);
        sc.scripts = &scripting;
    } else {
        fprintf(stderr, "Lua: %s\n", err);
    }
#endif

    p.sh = shell_create(&sc, err, sizeof err);
    if (!p.sh) {
        fprintf(stderr, "Schale: %s\n", err);
        program_free(&p);
        plat_shutdown();
        return 1;
    }

    /* "--apps" zählt auf, was gefunden wurde, und beendet sich. Praktisch, um
     * zu sehen, ob eine eigene Schemadatei oder ein eigenes Skript angekommen
     * ist - und ob nicht, warum. */
    if (arg_after(argc, argv, "--apps") || has_flag(argc, argv, "--apps")) {
        for (int i = 0; i < shell_app_count(p.sh); i++) {
            const char *label = shell_app_label(p.sh, i);
            printf("%d  %-16s %s\n", i, label, T(p.cat, label));
        }
        if (shell_last_error(p.sh)[0])
            fprintf(stderr, "%s\n", shell_last_error(p.sh));

        program_free(&p);
        plat_shutdown();
        return 0;
    }

    /* Mit der ersten Anwendung anfangen. Ein leerer Schreibtisch wäre
     * korrekt, aber niemand möchte ein Programm starten, das nichts zeigt. */
    if (shell_app_count(p.sh) > 0 && !shell_open_app(p.sh, 0, err, sizeof err))
        fprintf(stderr, "%s\n", err);

    gc g;
    gc_init(&g, &p.fb);

    /* "--shot datei.pbm" zeichnet ein Bild, schreibt den Bildspeicher als PBM
     * und beendet sich. Das ist genauer als ein Bildschirmabzug: es zeigt
     * exakt die Pixel, die das Programm erzeugt, ohne Fensterrahmen und ohne
     * Skalierung. Aus demselben Gedanken wird in M14 der Aufnahmeapparat. */
    if (shot) {
        shell_draw(p.sh, &g);
        plat_present(&p.fb);

        bool ok = pbm_write_p4(shot, &p.fb);
        program_free(&p);
        plat_shutdown();
        return ok ? 0 : 1;
    }

    while (shell_running(p.sh)) {
        event e;
        while (plat_poll(&e)) shell_event(p.sh, &e);

        shell_draw(p.sh, &g);
        plat_present(&p.fb);
        plat_sleep_ms(16);
    }

    program_free(&p);
    plat_shutdown();
    return 0;
}
