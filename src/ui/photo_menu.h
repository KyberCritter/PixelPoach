// Shared photo page and action menu for the View and Send scenes.
//
// Both scenes show one stored photo at a time on a navy MODE4 page, with the
// X/Y photo counter and the A/B button bar along the bottom. Pressing A opens
// the same Send/Delete/Receive option overlay in either scene.
#ifndef UI_PHOTO_MENU_H
#define UI_PHOTO_MENU_H

#include <stdbool.h>
#include <stdint.h>

// Options in the shared menu (D-pad: Up = Send, Left = Delete, Right = Receive).
enum { PM_SEND, PM_DELETE, PM_RECEIVE };

// Repaint a stored slot: navy background, centred photo, X/Y counter, and the
// A Options / B Back button bar.
void photo_menu_draw(int slot);

// Paint a raw camera image on the dark result page (no counter or buttons).
// Used by the receive scene to show a photo as soon as it arrives.
void photo_menu_show_image(const uint8_t *img);

// Overlay the option labels on the current page, highlighting `sel`.
void photo_menu_draw_options(int sel);

// 1-based position of `slot` among the occupied photos.
int photo_menu_position(int slot);

// Next occupied slot in direction `dir` (-1/+1), or -1 when there is none.
int photo_menu_next(int from, int dir);

// Erase `slot` and return the next occupied slot, or -1 when none remain.
int photo_menu_delete(int slot);

#endif
