/* Die Hauptschleife.
 *
 * Mehr ist es nicht: Ereignisse abholen, Zustand fortschreiben, zeichnen,
 * ausgeben. Alles Inhaltliche steht in demo.c und ist dadurch ohne Bildschirm
 * prüfbar; hier bleibt nur, was ohne echtes Fenster keinen Sinn ergibt.
 */

#include <stdio.h>
#include <string.h>

#include "core/i18n.h"
#include "core/keymap.h"
#include "demo.h"
#include "gfx/bitmap.h"
#include "gfx/draw.h"
#include "gfx/pbm.h"
#include "plat/plat.h"
#include "ui/theme.h"

/* "--shot datei.pbm" zeichnet ein Bild, schreibt den Bildspeicher als PBM und
 * beendet sich. Das ist genauer als ein Bildschirmabzug: es zeigt exakt die
 * Pixel, die das Programm erzeugt, ohne Fensterrahmen und ohne Skalierung.
 * Aus demselben Gedanken wird in M14 der Aufnahmeapparat. */
int main(int argc, char **argv)
{
    const char *shot = NULL;
    for (int i = 1; i + 1 < argc; i++)
        if (strcmp(argv[i], "--shot") == 0) shot = argv[i + 1];

    plat_config cfg = { .width = 800, .height = 480, .scale = 0,
                        .title = "PDA" };
    if (!plat_init(&cfg)) return 1;

    int w, h;
    plat_display_size(&w, &h);

    bitmap fb;
    if (!bitmap_init(&fb, w, h)) {
        plat_shutdown();
        return 1;
    }

    gc g;
    gc_init(&g, &fb);

    char    err[512] = "";
    keymap *km = keymap_load(PDA_KEYMAP_PATH, err, sizeof err);
    if (!km) fprintf(stderr, "Tastenbelegung: %s\n", err);

    theme th;
    theme_defaults(&th);
    if (!theme_load(&th, PDA_THEME_PATH, err, sizeof err))
        fprintf(stderr, "Thema: %s\n", err);

    catalog *cat = i18n_load(PDA_LANG_PATH, err, sizeof err);
    if (!cat) fprintf(stderr, "Textkatalog: %s\n", err);

    demo_state st;
    if (!demo_init(&st, km, cat, &th, w, h)) {
        bitmap_free(&fb);
        i18n_free(cat);
        keymap_free(km);
        plat_shutdown();
        return 1;
    }

    if (shot) {
        demo_draw(&st, &g);
        plat_present(&fb);
        bool ok = pbm_write_p4(shot, &fb);
        demo_free(&st);
        bitmap_free(&fb);
        i18n_free(cat);
        keymap_free(km);
        plat_shutdown();
        return ok ? 0 : 1;
    }

    while (st.running) {
        event e;
        while (plat_poll(&e)) demo_event(&st, &e);

        demo_draw(&st, &g);
        plat_present(&fb);
        plat_sleep_ms(16);
    }

    demo_free(&st);
    bitmap_free(&fb);
    i18n_free(cat);
    keymap_free(km);
    plat_shutdown();
    return 0;
}
