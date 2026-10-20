// Drawing helpers for MODE4 bitmap screens.
//
// MODE4 has no text layer, so text, boxes, and buttons are painted straight
// into VRAM. Use these only while the display is in MODE4.
#ifndef UI_BMP_H
#define UI_BMP_H

#include <tonc.h>

void bmp_fill(int x, int y, int w, int h, u8 c);
void bmp_text(int x, int y, const char *s, u8 c);
void bmp_button(int x, int y, int letter);

#endif
