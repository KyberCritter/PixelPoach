// Shared MODE4 photo page and action menu used by View and Send.
#include <stdio.h>
#include <string.h>
#include <tonc.h>
#include "ui/photo_menu.h"
#include "ui/bmp.h"
#include "cam/cam_store.h"

// Palette 5 is the navy page behind the photo; 6/7 are the option plates.
enum { PM_PAGE_BG = 5, PM_OPT_BG = 6, PM_OPT_SEL = 7 };

static EWRAM_BSS u8 shade[CAM_IMG_W * CAM_IMG_H];

// Blit the decoded image into MODE4 page 0. VRAM ignores 8-bit writes on real
// hardware (the byte is mirrored into both halves of the halfword), so write
// whole halfwords: two consecutive pixels make up one 16-bit word.
static void photo_blit(void)
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

// Set up the MODE4 bitmap layer, install the palettes, and clear the page.
static void photo_page(u8 bg)
{
    REG_DISPCNT = DCNT_MODE4 | DCNT_BG2;
    pal_bg_mem[0] = RGB15(31, 31, 31);
    pal_bg_mem[1] = RGB15(21, 21, 21);
    pal_bg_mem[2] = RGB15(11, 11, 11);
    pal_bg_mem[3] = RGB15(2, 2, 2);
    pal_bg_mem[4] = RGB15(2, 2, 3);   // dark page behind a received photo
    pal_bg_mem[5] = RGB15(1, 4, 13);  // navy page behind the photo
    pal_bg_mem[6] = RGB15(2, 3, 9);   // unselected option plate (deep navy)
    pal_bg_mem[7] = RGB15(24, 28, 31); // selected option plate (ice blue)

    // Clear the whole bitmap page. The tiled UI's font/tilemap data still sits
    // in VRAM when we switch to MODE4; without this the area around the photo
    // shows that stale data as garbage. Halfword writes only (VRAM quirk).
    vu16 *vram = (vu16 *)MEM_VRAM;
    for (int i = 0; i < 240 * 160 / 2; i++)
        vram[i] = (u16)(bg | (bg << 8));
}

// 1-based position of `slot` among the stored photos (empty slots are skipped).
int photo_menu_position(int slot)
{
    int pos = 1;
    for (int s = 0; s < slot; s++)
        if (cam_store_occupied(s)) pos++;
    return pos;
}

// Solid plate behind each label keeps it legible over any photo. Unselected
// options get a deep navy plate; the selected one turns ice blue.
static void photo_menu_label(int x, int y, const char *s, bool selected)
{
    int w = (int)strlen(s) * 8;
    int bx = x - 2, by = y - 2, bw = w + 4, bh = 12;

    if (selected) {
        bmp_fill(bx, by, bw, bh, PM_OPT_SEL);
        bmp_text(x, y, s, 4);
    } else {
        bmp_fill(bx, by, bw, bh, PM_OPT_BG);
        bmp_text(x, y, s, 0);
    }
}

// Bottom bar: round A/B faces kept from the send scene, with the shared
// Options/Back prompts.
static void photo_menu_bar(void)
{
    bmp_button(53, 142, 'A');
    bmp_text(71, 146, "Options", 0);
    bmp_button(137, 142, 'B');
    bmp_text(155, 146, "Back", 0);
}

// Counter in the bottom-left, matching the send scene's X/Y placement.
static void photo_menu_counter(int slot)
{
    char counter[12];

    siprintf(counter, "%d/%d", photo_menu_position(slot), cam_store_count());
    bmp_text(6, 150, counter, 0);
}

void photo_menu_draw(int slot)
{
    photo_page(PM_PAGE_BG);

    // decode the slot's image tiles directly from the in-memory save
    const u8 *data = cam_store_buf() + CAM_SLOT_BASE + (u32)slot * CAM_SLOT_SIZE;
    cam_image_decode(data, shade);
    photo_blit();
    photo_menu_counter(slot);
    photo_menu_bar();
}

void photo_menu_show_image(const uint8_t *img)
{
    photo_page(4);                    // received photos use the dark page
    cam_image_decode(img, shade);
    photo_blit();
}

void photo_menu_draw_options(int sel)
{
    static const char *send = "Send Photo";
    static const char *recv = "Receive Photo";
    static const char *del = "Delete Photo";

    photo_menu_label((240 - (int)strlen(send) * 8) / 2, 3, send,
                     sel == PM_SEND);
    photo_menu_label(2, 76, del, sel == PM_DELETE);
    photo_menu_label(240 - (int)strlen(recv) * 8 - 2, 76, recv,
                     sel == PM_RECEIVE);
}

int photo_menu_next(int from, int dir)
{
    int cap = cam_store_capacity();
    if (cap == 0) return -1;
    for (int i = 0; i < cap; i++) {
        int s = (from + dir * (i + 1) + cap * 2) % cap;
        if (cam_store_occupied(s)) return s;
    }
    return -1;
}

int photo_menu_delete(int slot)
{
    cam_store_erase(slot);
    int s = photo_menu_next(slot, 1);
    if (s < 0) s = photo_menu_next(slot, -1);
    return s;
}
