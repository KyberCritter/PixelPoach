// Title screen, main menu, and about screen.
#include <stdio.h>
#include <tonc.h>
#include "app.h"
#include "ui/ui.h"
#include "ui/screen.h"
#include "cam/cam_store.h"
#include "storage/storage.h"
#include "core/charset.h"

// ---------- title ----------

void title_enter(void)
{
    ui_cls();
    ui_fill(6, 4, 20, 4, 3);
    ui_frame(6, 4, 20, 4);
    ui_text(9, 5, "PixelPoach", PB_UI);
    ui_text(7, 6, "by Scott Ratchford", PB_UI);
    ui_text(8, 12, "Game Boy Camera", PB_UI);
    ui_text(8, 13, "photo transfer", PB_UI);
    ui_text(8, 16, "PRESS START", PB_UI);

    char line[32];
    siprintf(line, "Save: %s", storage_name());
    ui_text(4, 19, line, PB_UI);
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
    "About",
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
        case 4: app_goto(ST_ABOUT); break;
        }
    }
}

// ---------- about ----------

void about_enter(void)
{
    ui_cls();
    ui_fill(4, 4, 22, 11, 3);
    ui_frame(4, 4, 22, 11);
    ui_text(11, 5, "ABOUT", PB_UI);
    ui_text(6, 8,  "Dumps Game Boy Camera", PB_UI);
    ui_text(6, 9,  "photos to this cart's", PB_UI);
    ui_text(6, 10, ".sav over the link", PB_UI);
    ui_text(6, 11, "cable, and can send", PB_UI);
    ui_text(6, 12, "them back.", PB_UI);
    ui_text(6, 16, "B: Back", PB_UI);
}

void about_frame(int frame)
{
    (void)frame;
    if (key_hit(KEY_B))
        app_goto(ST_MENU);
}
