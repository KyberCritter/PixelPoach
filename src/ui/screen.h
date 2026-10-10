// Shared screen layout constants and BG map helpers.
#ifndef UI_SCREEN_H
#define UI_SCREEN_H

#include <tonc.h>
#include "gfx/font.h"

// BG0 = glyph layer (text), BG1 = chrome solids behind it.
#define SBB_TEXT    30
#define SBB_CHROME  31

// Palette banks
#define PB_INK      0
#define PB_UI       1
#define PB_SOLID    2
#define PB_SEL      3

#define SCREEN_TW   30
#define SCREEN_TH   20

INLINE u16 *text_map(void)   { return se_mem[SBB_TEXT]; }
INLINE u16 *chrome_map(void) { return se_mem[SBB_CHROME]; }

INLINE void map_put(u16 *map, int x, int y, int tile, int pb)
{ map[y * 32 + x] = SE_PALBANK(pb) | tile; }

#define SOLID(v) FONT_SOLID_TILE(v)

#endif
