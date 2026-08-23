/* Maße und Trefferflächen der Fensterrahmen, aus einer Datei.
 *
 * Nichts hiervon steht als Zahl im Zeichencode. Der Grund ist M15: dort
 * bekommt das Gerät ein zweites Thema mit größeren Bedienelementen und
 * großzügigeren Trefferflächen, weil ein Finger kein Mauszeiger ist. Stünden
 * die Maße verstreut im Code, würde daraus eine Verzweigungsorgie.
 *
 * Format wie bei data/keys/default.keys: eine Zeile "name wert", # leitet
 * einen Kommentar ein.
 */
#ifndef PDA_UI_THEME_H
#define PDA_UI_THEME_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    int titlebar_h;    /* Höhe der Titelleiste einschließlich Trennlinie */
    int border;        /* Strichstärke des Rahmens */
    int close_box;     /* Kantenlänge des Schließfelds */
    int grow_box;      /* Kantenlänge des Größenfelds unten rechts */
    int box_margin;    /* Abstand der Felder vom Rand */
    int stripe_gap;    /* Abstand der Streifen in der aktiven Titelleiste */
    int title_pad;     /* Luft links und rechts vom Titel */
    int hit_slop;      /* Trefferfläche wächst um so viel über das Gezeichnete */
    int min_w, min_h;  /* kleinste Fenstergröße */

    int menubar_h;     /* Höhe der Menüleiste am oberen Rand */
    int menu_item_h;   /* Zeilenhöhe in einem Aufklappmenü */
    int menu_pad;      /* Luft links und rechts in Menüs */
    int menu_gap;      /* Abstand zwischen Titel und Kürzel */

    int dialog_pad;    /* Rand im Dialog */
    int button_h;      /* Höhe eines Knopfs */
    int button_min_w;  /* kleinste Knopfbreite */
    int button_gap;    /* Abstand zwischen Knöpfen */

    char font[32];
} theme;

/* Bei einem Fehler false und eine Meldung "datei:zeile: text" in err. */
bool theme_load(theme *th, const char *path, char *err, size_t err_size);

/* Die eingebauten Voreinstellungen, falls keine Datei vorhanden ist. */
void theme_defaults(theme *th);

#endif /* PDA_UI_THEME_H */
