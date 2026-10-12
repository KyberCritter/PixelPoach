// Top-level scene state machine.
#ifndef APP_H
#define APP_H

#include <stdint.h>

typedef enum {
    ST_TITLE,
    ST_MENU,
    ST_RECEIVE,   // capture a photo the camera sends
    ST_SEND,      // send a stored photo back to the camera
    ST_VIEW,      // browse stored photos
    ST_DEBUG,     // hex viewer of the last link exchange
    ST_ABOUT,
} AppState;

void app_init(void);
void app_frame(int frame);
void app_goto(AppState s);   // runs the scene's enter() (full redraw)

// scene hooks
void title_enter(void);     void title_frame(int frame);
void menu_enter(void);      void menu_frame(int frame);
void receive_enter(void);   void receive_frame(int frame);
void send_enter(void);      void send_frame(int frame);
void view_enter(void);      void view_frame(int frame);
// Paint a raw camera image (CAM_IMG_BYTES) full-screen in MODE4. Used by the
// receive scene to show the photo as soon as it has been captured.
void view_render_photo(const uint8_t *img);
void about_enter(void);     void about_frame(int frame);
void dbg_enter(void);       void dbg_frame(int frame);

// MODE0 text-mode video setup (main.c). The view scene calls this on exit to
// restore the tiled display after switching to MODE4 bitmap.
void pxl_init_video(void);

#endif
