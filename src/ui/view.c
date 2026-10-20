// Photo viewer: MODE4 bitmap render of a stored camera image.
#include <stdio.h>
#include <string.h>
#include <tonc.h>
#include "app.h"
#include "ui/ui.h"
#include "ui/screen.h"
#include "ui/bmp.h"
#include "cam/cam_store.h"
#include "cam/cam_save.h"

static int v_cur;                     // slot index
static EWRAM_BSS u8 shade[CAM_IMG_W * CAM_IMG_H];

static void view_setup_mode4(u8 bg);
static void view_menu_label(int x, int y, const char *s, bool selected);

// A-button menu, with the labels on the screen edges like the Game Boy
// Camera's menu (but kept horizontal).
enum { VM_SEND, VM_RECV, VM_DELETE };
static bool vm_open;
static int vm_sel;

// Blit the decoded image into MODE4 page 0. VRAM ignores 8-bit writes on real
// hardware (the byte is mirrored into both halves of the halfword), so write
// whole halfwords: two consecutive pixels make up one 16-bit word.
static void view_blit(void)
{
    vu16 *dst = (vu16 *)MEM_VRAM;
    int ox = (240 - CAM_IMG_W) / 2;
    int oy = (160 - CAM_IMG_H) / 2;
    for (int y = 0; y < CAM_IMG_H; y++) {
        vu16 *row = dst + ((oy + y) * 240 + ox) / 2;
        const u8 *src = &shade[y * CAM_IMG_W];
        for (int x = 0; x < CAM_IMG_W; x += 2)
            row[x >> 1] = (u16)(src[x] | (src[x + 1] << 8));
    }
}

// Position of the current photo among the stored ones (1-based), for the
// "X/Y" counter. Slots can be empty after deletes, so count occupied slots.
static int view_position(void)
{
    int pos = 1;
    for (int s = 0; s < v_cur; s++)
        if (cam_store_occupied(s)) pos++;
    return pos;
}

static void view_draw_counter(void)
{
    char counter[12];

    siprintf(counter, "%d/%d", view_position(), cam_store_count());
    view_menu_label(2, 149, counter, false);
}

static void view_draw(void)
{
    // Clear the whole bitmap first: overlays (the A menu) live outside the
    // photo rectangle and would otherwise survive a photo change.
    view_setup_mode4(5);

    // decode the slot's image tiles directly from the in-memory save
    const u8 *slot = cam_store_buf() + CAM_SLOT_BASE + (u32)v_cur * CAM_SLOT_SIZE;
    cam_image_decode(slot, shade);
    view_blit();
    view_draw_counter();
}

// Set up the MODE4 bitmap layer and paint one image buffer. Used by the viewer
// and by the receive scene, which shows the photo as soon as it arrives.
static void view_setup_mode4(u8 bg)
{
    REG_DISPCNT = DCNT_MODE4 | DCNT_BG2;
    pal_bg_mem[0] = RGB15(31, 31, 31);
    pal_bg_mem[1] = RGB15(21, 21, 21);
    pal_bg_mem[2] = RGB15(11, 11, 11);
    pal_bg_mem[3] = RGB15(2, 2, 2);
    pal_bg_mem[4] = RGB15(2, 2, 3);   // dark UI plates, distinct from the 4 shades
    pal_bg_mem[5] = RGB15(31, 21, 21); // viewer page background (light red)
    pal_bg_mem[6] = RGB15(16, 2, 2);  // unselected menu outline (dark red)
    pal_bg_mem[7] = RGB15(12, 26, 12); // selected menu plate (medium green)

    // Clear the whole bitmap page. The tiled UI's font/tilemap data still sits
    // in VRAM when we switch to MODE4; without this the area around the photo
    // shows that stale data as garbage. Halfword writes only (VRAM quirk).
    vu16 *vram = (vu16 *)MEM_VRAM;
    for (int i = 0; i < 240 * 160 / 2; i++)
        vram[i] = (u16)(bg | (bg << 8));
}

// Paint a raw 2bpp camera image (CAM_IMG_BYTES) centred on a MODE4 screen.
void view_render_photo(const uint8_t *img)
{
    view_setup_mode4(4);              // receive/send scenes keep the dark page
    cam_image_decode(img, shade);
    view_blit();
}

void view_enter(void)
{
    // most recently stored photo (last occupied slot), else 0
    int cap = cam_store_capacity();
    v_cur = 0;
    for (int s = cap - 1; s >= 0; s--)
        if (cam_store_occupied(s)) { v_cur = s; break; }

    vm_open = false;
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

static void view_delete_current(void)
{
    cam_store_erase(v_cur);
    int s = next_occ(v_cur, 1);
    if (s < 0) s = next_occ(v_cur, -1);
    vm_open = false;
    if (s < 0) {
        pxl_init_video();
        app_goto(ST_MENU);
        return;
    }
    v_cur = s;
    view_draw();
}

// Solid plate behind each label keeps it legible over any photo. Unselected
// options get a dark red outline; the selected one turns medium green.
static void view_menu_label(int x, int y, const char *s, bool selected)
{
    int w = (int)strlen(s) * 8;
    int bx = x - 2, by = y - 2, bw = w + 4, bh = 12;

    if (selected) {
        bmp_fill(bx, by, bw, bh, 7);
        bmp_text(x, y, s, 4);
    } else {
        bmp_fill(bx, by, bw, bh, 6);
        bmp_text(x, y, s, 0);
    }
}

static void view_menu_draw(void)
{
    static const char *send = "Send Photo";
    static const char *recv = "Receive Photo";
    static const char *del = "Delete Photo";

    view_menu_label((240 - (int)strlen(send) * 8) / 2, 3, send,
                    vm_sel == VM_SEND);
    view_menu_label((240 - (int)strlen(recv) * 8) / 2, 149, recv,
                    vm_sel == VM_RECV);
    view_menu_label(2, 76, del, vm_sel == VM_DELETE);
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

    if (vm_open) {
        if (key_hit(KEY_UP))        { vm_sel = VM_SEND;   view_menu_draw(); }
        else if (key_hit(KEY_DOWN)) { vm_sel = VM_RECV;   view_menu_draw(); }
        else if (key_hit(KEY_LEFT)) { vm_sel = VM_DELETE; view_menu_draw(); }

        if (key_hit(KEY_B)) {
            vm_open = false;
            view_draw();
            return;
        }
        if (!key_hit(KEY_A))
            return;

        switch (vm_sel) {
        case VM_SEND:
            vm_open = false;
            send_set_start(v_cur);
            pxl_init_video();
            app_goto(ST_SEND);
            break;
        case VM_RECV:
            vm_open = false;
            pxl_init_video();
            app_goto(ST_RECEIVE);
            break;
        case VM_DELETE:
            view_delete_current();
            break;
        }
        return;
    }

    if (key_hit(KEY_A)) {
        vm_open = true;
        vm_sel = VM_SEND;
        view_menu_draw();
        return;
    }
    if (key_hit(KEY_LEFT))  { int s = next_occ(v_cur, -1); if (s >= 0) { v_cur = s; view_draw(); } }
    if (key_hit(KEY_RIGHT)) { int s = next_occ(v_cur, 1);  if (s >= 0) { v_cur = s; view_draw(); } }
    if (key_hit(KEY_SELECT)) {
        view_delete_current();
        return;
    }
    if (key_hit(KEY_B)) {
        pxl_init_video();
        app_goto(ST_MENU);
    }
}
