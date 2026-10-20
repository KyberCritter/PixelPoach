#include "ui/bmp.h"
#include "gfx/font.h"

// VRAM ignores 8-bit writes (the byte mirrors into both halves of the
// halfword), so each pixel is a read-modify-write of the whole halfword.
static void bmp_px(int x, int y, u8 c)
{
    if (x < 0 || x >= 240 || y < 0 || y >= 160) return;
    vu16 *p = (vu16 *)MEM_VRAM + (y * 240 + x) / 2;
    u16 v = *p;
    if (x & 1) v = (v & 0x00FF) | ((u16)c << 8);
    else       v = (v & 0xFF00) | c;
    *p = v;
}

// One ROM-font glyph (tile `code`) in MODE4 palette index `c`. Pixels are
// stored low-nibble-first in memory order, matching the tiled BG upload.
static void bmp_glyph(int x, int y, int code, u8 c)
{
    const u8 *ft = (const u8 *)font_tiles;
    for (int r = 0; r < 8; r++)
        for (int i = 0; i < 8; i++) {
            u8 b = ft[code * 32 + r * 4 + (i >> 1)];
            u8 px = (i & 1) ? (b >> 4) : (b & 0xF);
            if (px) bmp_px(x + i, y + r, c);
        }
}

void bmp_text(int x, int y, const char *s, u8 c)
{
    for (; *s; s++, x += 8) bmp_glyph(x, y, (u8)*s, c);
}

void bmp_fill(int x, int y, int w, int h, u8 c)
{
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            bmp_px(x + i, y + j, c);
}

// A round GBA-style face button (white disc, dark letter) 16x16.
void bmp_button(int x, int y, int letter)
{
    for (int j = 0; j < 16; j++)
        for (int i = 0; i < 16; i++) {
            int dx = 2 * i - 15, dy = 2 * j - 15;
            if (dx * dx + dy * dy <= 225)
                bmp_px(x + i, y + j, 0);
        }
    bmp_glyph(x + 4, y + 4, letter, 4);
}
