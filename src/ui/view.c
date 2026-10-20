// Photo viewer: browse stored camera images with the shared photo menu.
#include <tonc.h>
#include "app.h"
#include "ui/photo_menu.h"
#include "cam/cam_store.h"

static int v_cur;                     // slot index
static bool vm_open;
static int vm_sel;

void view_enter(void)
{
    // most recently stored photo (last occupied slot), else 0
    int cap = cam_store_capacity();
    v_cur = 0;
    for (int s = cap - 1; s >= 0; s--)
        if (cam_store_occupied(s)) { v_cur = s; break; }

    vm_open = false;
    photo_menu_draw(v_cur);
}

static void view_delete_current(void)
{
    int s = photo_menu_delete(v_cur);
    vm_open = false;
    if (s < 0) {
        pxl_init_video();
        app_goto(ST_MENU);
        return;
    }
    v_cur = s;
    photo_menu_draw(v_cur);
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
        if (key_hit(KEY_UP))         { vm_sel = PM_SEND;    photo_menu_draw_options(vm_sel); }
        else if (key_hit(KEY_LEFT))  { vm_sel = PM_DELETE;  photo_menu_draw_options(vm_sel); }
        else if (key_hit(KEY_RIGHT)) { vm_sel = PM_RECEIVE; photo_menu_draw_options(vm_sel); }

        if (key_hit(KEY_B)) {
            vm_open = false;
            photo_menu_draw(v_cur);
            return;
        }
        if (!key_hit(KEY_A))
            return;

        switch (vm_sel) {
        case PM_SEND:
            vm_open = false;
            send_set_start(v_cur);
            pxl_init_video();
            app_goto(ST_SEND);
            break;
        case PM_RECEIVE:
            vm_open = false;
            pxl_init_video();
            app_goto(ST_RECEIVE);
            break;
        case PM_DELETE:
            view_delete_current();
            break;
        }
        return;
    }

    if (key_hit(KEY_A)) {
        vm_open = true;
        vm_sel = PM_SEND;
        photo_menu_draw_options(vm_sel);
        return;
    }
    if (key_hit(KEY_LEFT))  { int s = photo_menu_next(v_cur, -1); if (s >= 0) { v_cur = s; photo_menu_draw(v_cur); } }
    if (key_hit(KEY_RIGHT)) { int s = photo_menu_next(v_cur, 1);  if (s >= 0) { v_cur = s; photo_menu_draw(v_cur); } }
    if (key_hit(KEY_SELECT)) {
        view_delete_current();
        return;
    }
    if (key_hit(KEY_B)) {
        pxl_init_video();
        app_goto(ST_MENU);
    }
}
