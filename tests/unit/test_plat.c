/* Die Plattformschicht in ihrer Headless-Umsetzung. */

#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "plat/plat.h"

/* Steuerschnittstelle von plat_headless.c, nur für Tests. Sie steht bewusst
 * nicht in plat.h: kein Backend außer diesem hat sie, und kein Programm
 * außerhalb der Tests darf sich darauf verlassen. */
extern void          plat_headless_push_event(const event *e);
extern const bitmap *plat_headless_frame(void);
extern int           plat_headless_present_count(void);
extern void          plat_headless_set_time(uint32_t ms);

static void temp_path(char *buf, size_t n, const char *name)
{
    const char *dir = getenv("TMPDIR");
    if (!dir || !*dir) dir = "/tmp";

    size_t len = strlen(dir);
    if (len > 0 && dir[len - 1] == '/') snprintf(buf, n, "%s%s", dir, name);
    else                                snprintf(buf, n, "%s/%s", dir, name);
}

TEST(plat_init_uses_default_size)
{
    plat_config cfg = { 0 };
    REQUIRE(plat_init(&cfg));

    int w = 0, h = 0;
    plat_display_size(&w, &h);
    CHECK_EQ(w, 800);
    CHECK_EQ(h, 480);

    plat_shutdown();
}

TEST(plat_init_honours_requested_size)
{
    plat_config cfg = { .width = 320, .height = 240 };
    REQUIRE(plat_init(&cfg));

    int w = 0, h = 0;
    plat_display_size(&w, &h);
    CHECK_EQ(w, 320);
    CHECK_EQ(h, 240);

    plat_shutdown();
}

TEST(plat_present_keeps_the_frame)
{
    plat_config cfg = { .width = 32, .height = 16 };
    REQUIRE(plat_init(&cfg));

    bitmap fb;
    REQUIRE(bitmap_init(&fb, 32, 16));
    for (int i = 0; i < 16; i++) bitmap_set(&fb, i, i, 1);

    CHECK_EQ(plat_headless_present_count(), 0);
    plat_present(&fb);
    CHECK_EQ(plat_headless_present_count(), 1);
    CHECK(bitmap_equal(plat_headless_frame(), &fb));

    plat_present(&fb);
    CHECK_EQ(plat_headless_present_count(), 2);

    bitmap_free(&fb);
    plat_shutdown();
}

TEST(plat_poll_returns_events_in_order)
{
    plat_config cfg = { .width = 32, .height = 16 };
    REQUIRE(plat_init(&cfg));

    event a = { .kind = EV_MOUSE_DOWN, .x = 3, .y = 4, .button = 1 };
    event b = { .kind = EV_TEXT };
    memcpy(b.text, "ä", 3);
    event c = { .kind = EV_KEY_DOWN, .key = KEY_ESCAPE };

    plat_headless_push_event(&a);
    plat_headless_push_event(&b);
    plat_headless_push_event(&c);

    event got;
    REQUIRE(plat_poll(&got));
    CHECK_EQ(got.kind, EV_MOUSE_DOWN);
    CHECK_EQ(got.x, 3);
    CHECK_EQ(got.y, 4);

    REQUIRE(plat_poll(&got));
    CHECK_EQ(got.kind, EV_TEXT);
    CHECK_STR(got.text, "ä");

    REQUIRE(plat_poll(&got));
    CHECK_EQ(got.kind, EV_KEY_DOWN);
    CHECK_EQ(got.key, KEY_ESCAPE);

    CHECK(!plat_poll(&got));

    plat_shutdown();
}

/* Die Uhr steht still, bis jemand sie stellt. Ohne das wären Tests, die auf
 * Zeit reagieren, nicht wiederholbar. */
TEST(plat_time_is_deterministic)
{
    plat_config cfg = { .width = 32, .height = 16 };
    REQUIRE(plat_init(&cfg));

    CHECK_EQ(plat_ticks_ms(), 0u);

    plat_headless_set_time(1000);
    CHECK_EQ(plat_ticks_ms(), 1000u);

    plat_sleep_ms(16);
    CHECK_EQ(plat_ticks_ms(), 1016u);

    plat_sleep_ms(0);
    CHECK_EQ(plat_ticks_ms(), 1016u);

    plat_shutdown();
}

TEST(plat_file_roundtrip)
{
    char path[512];
    temp_path(path, sizeof path, "pda_plat_file.bin");

    const char payload[] = "Grüße aus Köln";

    plat_file *f = plat_open(path, PLAT_WRITE);
    REQUIRE(f != NULL);
    CHECK_EQ(plat_write(f, payload, sizeof payload), sizeof payload);
    plat_close(f);

    char back[64] = { 0 };
    f = plat_open(path, PLAT_READ);
    REQUIRE(f != NULL);
    CHECK_EQ(plat_read(f, back, sizeof back), sizeof payload);
    plat_close(f);

    CHECK_STR(back, payload);
    remove(path);
}

TEST(plat_open_missing_file_returns_null)
{
    char path[512];
    temp_path(path, sizeof path, "pda_plat_gibt_es_nicht.bin");
    remove(path);

    CHECK(plat_open(path, PLAT_READ) == NULL);
}

TEST(plat_list_finds_file_and_skips_dot_entries)
{
    char dir[512], path[512];
    temp_path(dir, sizeof dir, "pda_plat_dir");
    remove(dir);
    REQUIRE(plat_mkdir(dir));

    snprintf(path, sizeof path, "%s/marke.txt", dir);
    plat_file *f = plat_open(path, PLAT_WRITE);
    REQUIRE(f != NULL);
    plat_write(f, "x", 1);
    plat_close(f);

    plat_dirent entries[16];
    int count = -1;
    REQUIRE(plat_list(dir, entries, 16, &count));
    CHECK_EQ(count, 1);
    CHECK_STR(entries[0].name, "marke.txt");

    for (int i = 0; i < count; i++) {
        CHECK(strcmp(entries[i].name, ".")  != 0);
        CHECK(strcmp(entries[i].name, "..") != 0);
    }

    remove(path);
    rmdir(dir);
}

/* Zweimal anlegen ist kein Fehler: der Aufrufer will, dass das Verzeichnis
 * da ist, nicht dass er es angelegt hat. */
TEST(plat_mkdir_is_idempotent)
{
    char dir[512];
    temp_path(dir, sizeof dir, "pda_plat_mkdir_zweimal");
    rmdir(dir);

    CHECK(plat_mkdir(dir));
    CHECK(plat_mkdir(dir));

    rmdir(dir);
}

TEST(plat_list_on_missing_dir_fails)
{
    char dir[512];
    temp_path(dir, sizeof dir, "pda_plat_kein_verzeichnis");

    plat_dirent entries[4];
    int count = -1;
    CHECK(!plat_list(dir, entries, 4, &count));
}

TEST(plat_can_be_restarted)
{
    plat_config a = { .width = 64, .height = 32 };
    REQUIRE(plat_init(&a));
    plat_shutdown();

    plat_config b = { .width = 128, .height = 64 };
    REQUIRE(plat_init(&b));

    int w = 0, h = 0;
    plat_display_size(&w, &h);
    CHECK_EQ(w, 128);
    CHECK_EQ(h, 64);
    CHECK_EQ(plat_headless_present_count(), 0);   /* Zähler beginnt neu */

    plat_shutdown();
}

int main(void)
{
    RUN(plat_init_uses_default_size);
    RUN(plat_init_honours_requested_size);
    RUN(plat_present_keeps_the_frame);
    RUN(plat_poll_returns_events_in_order);
    RUN(plat_time_is_deterministic);
    RUN(plat_file_roundtrip);
    RUN(plat_open_missing_file_returns_null);
    RUN(plat_list_finds_file_and_skips_dot_entries);
    RUN(plat_mkdir_is_idempotent);
    RUN(plat_list_on_missing_dir_fails);
    RUN(plat_can_be_restarted);
    return test_summary();
}
