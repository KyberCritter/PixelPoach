// PixelPoach — dump Game Boy Camera photos to a GBA flash cart via the link
// cable, and send them back so the camera keeps them.
#include <tonc.h>
#include "gfx/font.h"
#include "ui/screen.h"
#include "app.h"

void pxl_init_video(void)
{
    REG_DISPCNT = DCNT_MODE0 | DCNT_BG0 | DCNT_BG1;
    REG_BG0CNT = BG_CBB(0) | BG_SBB(SBB_TEXT)   | BG_4BPP | BG_REG_32x32 | BG_PRIO(0);
    REG_BG1CNT = BG_CBB(0) | BG_SBB(SBB_CHROME) | BG_4BPP | BG_REG_32x32 | BG_PRIO(1);

    memcpy32(&tile_mem[0][0], font_tiles, FONT_TILE_COUNT * 8);

    pal_bg_mem[16 * PB_INK   + 1] = RGB15(2,  2,  3);
    pal_bg_mem[16 * PB_UI    + 1] = RGB15(26, 27, 29);
    pal_bg_mem[16 * PB_SEL   + 1] = RGB15(1,  1,  2);
    pal_bg_mem[16 * PB_SOLID + 1] = RGB15(31, 31, 30);
    pal_bg_mem[16 * PB_SOLID + 2] = RGB15(4,  5,  7);
    pal_bg_mem[16 * PB_SOLID + 3] = RGB15(6,  7, 10);
    pal_bg_mem[16 * PB_SOLID + 4] = RGB15(31, 24,  6);
    pal_bg_mem[16 * PB_SOLID + 5] = RGB15(10, 12, 16);
    pal_bg_mem[0] = RGB15(2, 2, 3);
}

int main(void)
{
    irq_init(NULL);
    irq_enable(II_VBLANK);

    pxl_init_video();
    app_init();

    int frame = 0;
    while (1) {
        VBlankIntrWait();
        key_poll();
        frame++;
        app_frame(frame);
    }
    return 0;
}
