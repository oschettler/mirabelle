/* Die Dateifunktionen der Plattformschicht für alles, was ein POSIX-artiges
 * Dateisystem hat - also für plat_sdl3 und plat_headless gleichermaßen.
 *
 * Der ESP32 bekommt eine eigene Umsetzung; deshalb liegen diese sechs
 * Funktionen getrennt und nicht in einem der beiden Backends.
 */

#include "plat.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

struct plat_file {
    FILE *fp;
};

plat_file *plat_open(const char *path, plat_mode mode)
{
    FILE *fp = fopen(path, mode == PLAT_READ ? "rb" : "wb");
    if (!fp) return NULL;

    plat_file *f = malloc(sizeof *f);
    if (!f) {
        fclose(fp);
        return NULL;
    }

    f->fp = fp;
    return f;
}

size_t plat_read(plat_file *f, void *buf, size_t n)
{
    return f ? fread(buf, 1, n, f->fp) : 0;
}

size_t plat_write(plat_file *f, const void *buf, size_t n)
{
    return f ? fwrite(buf, 1, n, f->fp) : 0;
}

void plat_close(plat_file *f)
{
    if (!f) return;
    fclose(f->fp);
    free(f);
}

bool plat_list(const char *dir, plat_dirent *out, int cap, int *count)
{
    DIR *d = opendir(dir);
    if (!d) return false;

    int n = 0;
    struct dirent *e;

    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
            continue;
        if (n >= cap) break;

        snprintf(out[n].name, sizeof out[n].name, "%s", e->d_name);

        /* d_type kennt nicht jedes Dateisystem. Wo es fehlt, zählt der
         * Eintrag als Datei; wer es genauer braucht, ruft stat selbst auf. */
        out[n].is_dir = (e->d_type == DT_DIR);
        n++;
    }

    closedir(d);
    if (count) *count = n;
    return true;
}

bool plat_mkdir(const char *path)
{
    if (mkdir(path, 0777) == 0) return true;
    return errno == EEXIST;
}
