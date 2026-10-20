// Title screen, main menu, and credits screen.
#include <stdio.h>
#include <string.h>
#include <tonc.h>
#include "app.h"
#include "ui/ui.h"
#include "ui/screen.h"
#include "cam/cam_store.h"
#include "storage/storage.h"
#include "core/charset.h"

// Bump this on release, matching GBScribe's title screen.
#define PIXELPOACH_VERSION "v0.1"

// ---------- title ----------

// Even-length strings land half a tile right of the odd-length ones with plain
// integer division, so nudge them one column left to keep every line on the
// same grid.
static int centered_x(const char *s)
{
    int len = (int)strlen(s);
    int x = (SCREEN_TW - len) / 2;
    if (!(len & 1))
        x--;
    return x;
}

static void text_centered(int y, const char *s)
{
    ui_text(centered_x(s), y, s, PB_UI);
}

void title_enter(void)
{
    ui_cls();

    // The credit is the widest title line, so size the box tightly around it
    // (one border column on each side). The credit sits one column right of
    // centre, and the box follows it so the border stays clear of the text.
    static const char *credit = "by Scott Ratchford";
    int credit_x = centered_x(credit) + 1;
    int box_w = (int)strlen(credit) + 2;
    int box_x = credit_x - 1;

    ui_fill(box_x, 4, box_w, 4, 3);
    ui_frame(box_x, 4, box_w, 4);

    text_centered(5, "PixelPoach");
    ui_text(credit_x, 6, credit, PB_UI);
    text_centered(11, "Game Boy Camera");
    text_centered(12, "photo transfer");
    text_centered(16, "PRESS START");

    char line[32];
    siprintf(line, "Save: %s", storage_name());
    text_centered(19, line);

    // Version sits in the bottom-right corner, like GBScribe.
    ui_text(SCREEN_TW - (int)strlen(PIXELPOACH_VERSION) - 1, 19,
            PIXELPOACH_VERSION, PB_UI);
}

void title_frame(int frame)
{
    (void)frame;
    if (key_hit(KEY_START | KEY_A))
        app_goto(ST_MENU);
}

// ---------- menu ----------

static const char *items[] = {
    "Receive photo",
    "Send photo",
    "View photos",
    "Debug",
    "Credits",
};
#define N_ITEMS 5

static int sel;

static void menu_draw(void)
{
    ui_cls();
    ui_fill(5, 4, 20, 9, 3);
    ui_frame(5, 4, 20, 9);
    for (int i = 0; i < N_ITEMS; i++) {
        ui_text(8, 6 + i, items[i], PB_UI);
        map_put(text_map(), 7, 6 + i, i == sel ? CH_ARR_RIGHT : CH_BLANK, PB_UI);
    }
    char line[32];
    siprintf(line, "Photos stored: %d", cam_store_count());
    ui_text(4, 16, line, PB_UI);
}

void menu_enter(void)
{
    sel = 0;
    menu_draw();
}

void menu_frame(int frame)
{
    (void)frame;
    if (key_hit(KEY_UP))   { sel = (sel + N_ITEMS - 1) % N_ITEMS; menu_draw(); }
    if (key_hit(KEY_DOWN)) { sel = (sel + 1) % N_ITEMS; menu_draw(); }
    if (key_hit(KEY_B))    { app_goto(ST_TITLE); return; }
    if (key_hit(KEY_A)) {
        switch (sel) {
        case 0: app_goto(ST_RECEIVE); break;
        case 1: app_goto(ST_SEND); break;
        case 2: app_goto(ST_VIEW); break;
        case 3: app_goto(ST_DEBUG); break;
        case 4: app_goto(ST_CREDITS); break;
        }
    }
}

// ---------- credits ----------

void credits_enter(void)
{
    // License notice, wrapped at word boundaries to fit the box.
    static const char *notice[] = {
        "Copyright (c) 2026",
        "Scott Ratchford.",
        "PixelPoach's original",
        "code and documentation",
        "are available under the",
        "MIT License.",
        "See the included LICENSE",
        "file for the full text.",
    };
#define N_NOTICE ((int)(sizeof(notice) / sizeof(notice[0])))

    ui_cls();
    ui_fill(1, 2, 28, 12, 3);
    ui_frame(1, 2, 28, 12);
    text_centered(3, "CREDITS");
    for (int i = 0; i < N_NOTICE; i++)
        ui_text(3, 5 + i, notice[i], PB_UI);
    ui_text(3, 15, "B: Back", PB_UI);
}

void credits_frame(int frame)
{
    (void)frame;
    if (key_hit(KEY_B))
        app_goto(ST_MENU);
}
