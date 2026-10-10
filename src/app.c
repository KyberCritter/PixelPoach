#include <tonc.h>
#include "app.h"
#include "cam/cam_store.h"
#include "debug/dbg.h"
#include "link/cam_link.h"

static AppState state;

void app_goto(AppState s)
{
    state = s;
    switch (s) {
    case ST_TITLE:    title_enter();    break;
    case ST_MENU:     menu_enter();     break;
    case ST_RECEIVE:  receive_enter();  break;
    case ST_SEND:     send_enter();     break;
    case ST_VIEW:     view_enter();     break;
    case ST_DEBUG:    dbg_enter();      break;
    case ST_ABOUT:    about_enter();    break;
    }
}

void app_init(void)
{
    cam_store_init();
    cam_link_init();
    dbg_load();
    app_goto(ST_TITLE);
}

void app_frame(int frame)
{
    switch (state) {
    case ST_TITLE:    title_frame(frame);    break;
    case ST_MENU:     menu_frame(frame);     break;
    case ST_RECEIVE:  receive_frame(frame);  break;
    case ST_SEND:     send_frame(frame);     break;
    case ST_VIEW:     view_frame(frame);     break;
    case ST_DEBUG:    dbg_frame(frame);      break;
    case ST_ABOUT:    about_frame(frame);    break;
    }
}
