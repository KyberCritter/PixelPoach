// Receive and send scenes: drive the camera trade over the link.
#include <string.h>
#include <tonc.h>
#include "app.h"
#include "ui/ui.h"
#include "ui/screen.h"
#include "ui/photo_menu.h"
#include "cam/cam_store.h"
#include "cam/cam_trade.h"
#include "debug/dbg.h"
#include "link/cam_link.h"

// ---------- shared ----------

static EWRAM_BSS u8 rx_buf[TRADE_STREAM_BYTES];
static EWRAM_BSS u8 tx_buf[TRADE_STREAM_BYTES];
static EWRAM_BSS u8 img[CAM_IMG_BYTES];
static EWRAM_BSS u8 thumb[CAM_THUMB_BYTES];
static EWRAM_BSS u8 meta[CAM_META_BYTES];

// Write `v` right-aligned in `width` decimal digits at (x, y), zero padded.
// Inline and call-free: the progress callback runs from the link driver with
// IRQs masked, where an extra call can overrun the camera's next clock edge.
static inline void count_put(int x, int y, int v, int width)
{
    static const char dig[] = "0123456789";
    u16 *tm = text_map();
    if (v < 0) v = 0;
    for (int i = width - 1; i >= 0; i--) {
        map_put(tm, x + i, y, dig[v % 10], PB_UI);
        v /= 10;
    }
}

// Paint the static "/4102 bytes" suffix once, when the scene is drawn. Keeping
// it out of the per-byte callback matters: this code runs from the link driver
// with IRQs masked, right before the GBA clocks a byte.
static void xfer_label(void)
{
    map_put(text_map(), 11, 13, '/', PB_UI);
    count_put(12, 13, TRADE_DATA_BYTES, 4);
    ui_text(17, 13, " bytes", PB_UI);
}

// Live transfer progress, called from the link driver with IRQs masked: only
// the four count digits are redrawn, so the camera never waits on a full text
// redraw between clock edges. Only transfer bytes are counted: the first 2
// rounds are the handshake and mode/album-count header, so the display shows
// n - 2 (clamped at zero). Once both sides have clocked a round, the
// bottom-left status flips to LINK.
static void xfer_count(int n)
{
    int d = n - TRADE_HDR_BYTES;
    if (d < 0) d = 0;
    count_put(7, 13, d, 4);
    if (n == 2 || n == 3)               // send / receive master-turn parity
        ui_text(1, 19, "LINK   ", PB_UI);
}

// Info byte: photo count plus the album-full bit (0x5E = full).
static u8 reply_mode(void)
{
    int n = cam_store_count();
    return (n >= 30) ? TRADE_MODE_FULL : (u8)n;
}

// ---------- receive (GBA is Camera B, camera A sends) ----------

enum { RECV_READY, RECV_WAIT, RECV_PHOTO, RECV_FAIL };

static int rv_state;
static int rv_result;   // -1 = fail, >= 0 = slot saved
static int rv_nospace;  // parse OK but cam_store_add failed (album full / no storage)

static void recv_draw(void)
{
    ui_cls();
    ui_fill(3, 4, 24, 12, 3);
    ui_frame(3, 4, 24, 12);
    switch (rv_state) {
    case RECV_READY:
        ui_text(10, 5, "RECEIVE", PB_UI);
        ui_text(6, 7,  "On the camera:", PB_UI);
        ui_text(6, 8,  "1. Press SELECT.", PB_UI);
        ui_text(6, 9,  "2. UP to LINK,", PB_UI);
        ui_text(6, 10, "   press A.", PB_UI);
        ui_text(6, 11, "3. DOWN to TRANSFER,", PB_UI);
        ui_text(6, 12, "   press A.", PB_UI);
        ui_text(6, 13, "4. Pick SEND, A,", PB_UI);
        ui_text(6, 14, "   then the photo.", PB_UI);
        ui_text(6, 17, "A: Listen  B: Back", PB_UI);
        break;
    case RECV_WAIT:
        ui_text(9, 5, "RECEIVING...", PB_UI);
        ui_text(6, 9,  "Waiting for the", PB_UI);
        ui_text(6, 10, "camera to send.", PB_UI);
        ui_text(6, 12, "Hold B to cancel", PB_UI);
        ui_text(1, 19, "NO LINK", PB_UI);
        count_put(7, 13, 0, 4);
        xfer_label();
        break;
    case RECV_FAIL:
        ui_text(10, 5, "RECEIVE", PB_UI);
        if (rv_nospace) {
            if (cam_store_capacity() == 0) {
                ui_text(6, 9, "No storage found.", PB_UI);
            } else {
                ui_text(6, 9, "Album full.", PB_UI);
                ui_text(6, 10, "Erase one in View.", PB_UI);
            }
        } else {
            ui_text(6, 9, "No photo received.", PB_UI);
        }
        ui_text(6, 16, "B: Back", PB_UI);
        break;
    default:
        break;
    }
}

void receive_enter(void)
{
    rv_state = RECV_READY;
    recv_draw();
}

void receive_frame(int frame)
{
    (void)frame;
    if (rv_state == RECV_READY) {
        if (key_hit(KEY_B)) { app_goto(ST_MENU); return; }
        if (!key_hit(KEY_A)) return;

        rv_state = RECV_WAIT;
        recv_draw();
        VBlankIntrWait();               // commit the "receiving" screen

        // B's reply stream: handshake, mode, then filler.
        memset(tx_buf, 0, sizeof tx_buf);
        tx_buf[0] = TRADE_HANDSHAKE_B;
        tx_buf[1] = reply_mode();

        // Stay armed until a whole photo arrives or the user holds B. A gap in
        // the link stream (the camera has not started yet, or pauses between
        // bytes) must not drop us to the result screen while it is still
        // working; re-enter the exchange instead.
        int rc = CAM_XCHG_TIMEOUT;
        int rx_len = 0;
        rv_result = -1;
        rv_nospace = 0;
        for (;;) {
            rx_len = 0;
            rc = cam_link_exchange(false, tx_buf, sizeof tx_buf,
                                   rx_buf, TRADE_STREAM_BYTES, &rx_len, xfer_count);
            if (rc == CAM_XCHG_CANCELLED)
                break;
            // Do not gate on an exact wire length. A complete camera transfer
            // is not always the same number of rounds as our transmit buffer
            // (the camera stops after its payload, and the GBA clocks a
            // trailing byte), so requiring TRADE_SEND_BYTES here rejects valid
            // photos even though the whole image arrived. The parser validates
            // that the capture reaches a complete owner-metadata block.
            if (trade_parse_capture(rx_buf, rx_len, img, thumb, meta)) {
                rv_result = cam_store_add(img, thumb, meta);
                rv_nospace = (rv_result < 0);
                break;
            }
            // Stay armed. A gap in the stream (before the camera starts, or
            // between bytes) must not drop us to the failure screen while the
            // camera is still working. Hold B to cancel.
        }
        dbg_record(rc, rx_buf, rx_len, tx_buf, rx_len);

        if (rv_result >= 0) {
            // Show the complete photo immediately; B returns to the menu.
            photo_menu_show_image(img);
            rv_state = RECV_PHOTO;
        } else {
            rv_state = RECV_FAIL;
            recv_draw();
        }
    } else if (rv_state == RECV_PHOTO) {
        if (key_hit(KEY_A | KEY_B)) { pxl_init_video(); app_goto(ST_MENU); }
    } else if (rv_state == RECV_FAIL) {
        if (key_hit(KEY_A | KEY_B)) app_goto(ST_MENU);
    }
}

// ---------- send (GBA is Camera A, camera B receives) ----------

enum { SEND_SELECT, SEND_EMPTY, SEND_BUSY, SEND_DONE };

static int sd_state;
static int sd_cur;        // slot index
static int sd_start = -1; // slot to open on next enter (-1 = first occupied)
static bool sd_ok;
static bool sd_open;
static int sd_sel;

void send_set_start(int slot)
{
    sd_start = slot;
}

// Load the previewed photo into img/thumb/meta, then paint the shared photo
// menu. Those buffers hold the photo on screen so A can send it without
// re-reading the slot.
static void send_select_draw(void)
{
    cam_store_get(sd_cur, img, thumb, meta);
    photo_menu_draw(sd_cur);
}

// MODE0 message screens (empty album, busy, result).
static void send_draw(void)
{
    ui_cls();
    ui_fill(4, 4, 22, 12, 3);
    ui_frame(4, 4, 22, 12);
    switch (sd_state) {
    case SEND_EMPTY:
        ui_text(10, 5, "SEND BACK", PB_UI);
        ui_text(6, 9, "No photos stored.", PB_UI);
        ui_text(6, 16, "B: Back", PB_UI);
        break;
    case SEND_BUSY:
        ui_text(10, 5, "SENDING...", PB_UI);
        ui_text(6, 9,  "Camera must be in", PB_UI);
        ui_text(6, 10, "RECEIVE mode.", PB_UI);
        ui_text(6, 12, "Hold B to cancel", PB_UI);
        ui_text(1, 19, "NO LINK", PB_UI);
        count_put(7, 13, 0, 4);
        xfer_label();
        break;
    case SEND_DONE:
        ui_text(10, 5, "SEND BACK", PB_UI);
        ui_text(6, 8, sd_ok ? "Photo sent." : "Send failed.", PB_UI);
        ui_text(6, 16, "B: Back", PB_UI);
        break;
    }
}

static void send_begin(void)
{
    // img/thumb/meta were loaded when the select screen was drawn.
    int tx_len = trade_build_send(sd_cur, img, thumb, meta, tx_buf);
    tx_buf[1] = reply_mode();           // A's album count in Mode 1

    pxl_init_video();                   // leave the MODE4 photo layer
    sd_state = SEND_BUSY;
    send_draw();
    VBlankIntrWait();

    int rx_len = 0;
    // Stop clocking at the exact end of the stream: the camera ends the
    // transfer on its own and an extra clock leaves it waiting for more.
    int rc = cam_link_exchange(true, tx_buf, tx_len,
                               rx_buf, tx_len, &rx_len, xfer_count);
    dbg_record(rc, rx_buf, rx_len, tx_buf, tx_len);

    // A handshake alone only means the camera answered; the photo is in
    // its album only if every exchange of the stream completed.
    sd_ok = (rc == CAM_XCHG_OK) &&
            (rx_len == TRADE_SEND_BYTES) &&
            (rx_buf[0] == TRADE_HANDSHAKE_B);
    sd_state = SEND_DONE;
    send_draw();
}

static void send_delete_current(void)
{
    int s = photo_menu_delete(sd_cur);
    sd_open = false;
    if (s < 0) {
        sd_cur = 0;
        sd_state = SEND_EMPTY;
        pxl_init_video();               // leave the MODE4 photo layer
        send_draw();
        return;
    }
    sd_cur = s;
    send_select_draw();
}

void send_enter(void)
{
    sd_state = SEND_SELECT;
    sd_open = false;

    // Coming from the viewer, open on the photo that was on screen.
    int start = sd_start;
    sd_start = -1;
    sd_cur = (start >= 0 && cam_store_occupied(start))
           ? start : photo_menu_next(-1, 1);

    if (sd_cur < 0) {
        sd_cur = 0;
        sd_state = SEND_EMPTY;
        send_draw();
        return;
    }
    send_select_draw();
}

void send_frame(int frame)
{
    (void)frame;
    if (sd_state == SEND_EMPTY) {
        if (key_hit(KEY_A | KEY_B)) app_goto(ST_MENU);
        return;
    }
    if (sd_state == SEND_SELECT) {
        if (sd_open) {
            if (key_hit(KEY_UP))         { sd_sel = PM_SEND;    photo_menu_draw_options(sd_sel); }
            else if (key_hit(KEY_LEFT))  { sd_sel = PM_DELETE;  photo_menu_draw_options(sd_sel); }
            else if (key_hit(KEY_RIGHT)) { sd_sel = PM_RECEIVE; photo_menu_draw_options(sd_sel); }

            if (key_hit(KEY_B)) {
                sd_open = false;
                photo_menu_draw(sd_cur);
                return;
            }
            if (!key_hit(KEY_A))
                return;

            switch (sd_sel) {
            case PM_SEND:
                sd_open = false;
                send_begin();
                break;
            case PM_RECEIVE:
                sd_open = false;
                pxl_init_video();
                app_goto(ST_RECEIVE);
                break;
            case PM_DELETE:
                send_delete_current();
                break;
            }
            return;
        }

        if (key_hit(KEY_A)) {
            sd_open = true;
            sd_sel = PM_SEND;
            photo_menu_draw_options(sd_sel);
            return;
        }
        if (key_hit(KEY_LEFT))  { int s = photo_menu_next(sd_cur, -1); if (s >= 0) { sd_cur = s; send_select_draw(); } }
        if (key_hit(KEY_RIGHT)) { int s = photo_menu_next(sd_cur, 1);  if (s >= 0) { sd_cur = s; send_select_draw(); } }
        if (key_hit(KEY_B)) { pxl_init_video(); app_goto(ST_MENU); return; }
    } else if (sd_state == SEND_DONE) {
        if (key_hit(KEY_A | KEY_B)) app_goto(ST_MENU);
    }
}
