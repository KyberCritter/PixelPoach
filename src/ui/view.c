// Photo viewer: MODE4 bitmap render of a stored camera image.
#include <stdio.h>
#include <tonc.h>
#include "app.h"
#include "ui/ui.h"
#include "ui/screen.h"
#include "cam/cam_store.h"
#include "cam/cam_save.h"

static int v_cur;                     // slot index
static EWRAM_BSS u8 shade[CAM_IMG_W * CAM_IMG_H];

static void view_draw(void)
{
    // decode the slot's image tiles directly from the in-memory save
    const u8 *slot = cam_store_buf() + CAM_SLOT_BASE + (u32)v_cur * CAM_SLOT_SIZE;
    cam_image_decode(slot, shade);

    vu8 *dst = (vu8 *)MEM_VRAM;      // MODE4 page 0
    int ox = (240 - CAM_IMG_W) / 2;
    int oy = (160 - CAM_IMG_H) / 2;
    for (int y = 0; y < CAM_IMG_H; y++) {
        for (int x = 0; x < CAM_IMG_W; x++) {
            int sx = ox + x, sy = oy + y;
            if (sx >= 0 && sx < 240 && sy >= 0 && sy < 160)
                dst[sy * 240 + sx] = shade[y * CAM_IMG_W + x];
        }
    }
}

void view_enter(void)
{
    // switch to MODE4 bitmap
    REG_DISPCNT = DCNT_MODE4 | DCNT_BG2;
    pal_bg_mem[0] = RGB15(31, 31, 31);
    pal_bg_mem[1] = RGB15(21, 21, 21);
    pal_bg_mem[2] = RGB15(11, 11, 11);
    pal_bg_mem[3] = RGB15(2, 2, 2);

    // most recently stored photo (last occupied slot), else 0
    int cap = cam_store_capacity();
    v_cur = 0;
    for (int s = cap - 1; s >= 0; s--)
        if (cam_store_occupied(s)) { v_cur = s; break; }

    view_draw();
}

static int next_occ(int from, int dir)
{
    int cap = cam_store_capacity();
    if (cap == 0) return -1;
    for (int i = 0; i < cap; i++) {
        int s = (from + dir * (i + 1) + cap * 2) % cap;
        if (cam_store_occupied(s)) return s;
    }
    return -1;
}

void view_frame(int frame)
{
    (void)frame;
    if (cam_store_count() == 0) {
        if (key_hit(KEY_B)) {
            pxl_init_video();
            app_goto(ST_MENU);
        }
        return;
    }
    if (key_hit(KEY_UP))   { int s = next_occ(v_cur, -1); if (s >= 0) { v_cur = s; view_draw(); } }
    if (key_hit(KEY_DOWN)) { int s = next_occ(v_cur, 1);  if (s >= 0) { v_cur = s; view_draw(); } }
    if (key_hit(KEY_SELECT)) {
        cam_store_erase(v_cur);
        int s = next_occ(v_cur, 1);
        if (s < 0) s = next_occ(v_cur, -1);
        if (s < 0) {
            pxl_init_video();
            app_goto(ST_MENU);
            return;
        }
        v_cur = s;
        view_draw();
    }
    if (key_hit(KEY_B)) {
        pxl_init_video();
        app_goto(ST_MENU);
    }
}
