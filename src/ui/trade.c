// Receive and send scenes: drive the camera trade over the link.
#include <stdio.h>
#include <string.h>
#include <tonc.h>
#include "app.h"
#include "ui/ui.h"
#include "ui/screen.h"
#include "core/charset.h"
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
static void count_put(int x, int y, int v, int width)
{
    static const char dig[] = "0123456789";
    u16 *tm = text_map();
    if (v < 0) v = 0;
    for (int i = width - 1; i >= 0; i--) {
        map_put(tm, x + i, y, dig[v % 10], PB_UI);
        v /= 10;
    }
}

// Live transfer progress, called from the link driver with IRQs masked: a
// fixed-width "NNNN/4102 bytes" counter so the digits never leave residue.
// Only transfer bytes are counted: the first 2 rounds are the handshake and
// mode/album-count header, so the display shows n - 2 (clamped at zero) and
// sits at 0000 until the camera actually starts. Call sites and write pattern
// match the original counter exactly; the only per-byte addition is the
// subtract. Once (when round 2 completes — both sides have clocked, so the
// camera is definitively talking) the bottom-left status flips to LINK.
static void xfer_count(int n)
{
    int d = n - TRADE_HDR_BYTES;
    if (d < 0) d = 0;
    count_put(7, 13, d, 4);
    map_put(text_map(), 11, 13, '/', PB_UI);
    count_put(12, 13, TRADE_DATA_BYTES, 4);
    ui_text(17, 13, " bytes", PB_UI);
    if (n == 2)
        ui_text(1, 19, "LINK   ", PB_UI);
}

// B reports its album count in Mode 1, never "full" (0x5E would make A abort).
static u8 reply_mode(void)
{
    int n = cam_store_count();
    return (u8)(n < 29 ? n : 29);
}

// ---------- receive (GBA is Camera B, camera A sends) ----------

enum { RECV_READY, RECV_WAIT, RECV_PHOTO, RECV_FAIL };

static int rv_state;
static int rv_result;   // -1 = fail, >= 0 = slot saved
static int rv_nospace;  // parse OK but cam_store_add failed (album full / no storage)

static void recv_draw(void)
{
    ui_cls();
    ui_fill(4, 4, 22, 12, 3);
    ui_frame(4, 4, 22, 12);
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
        ui_text(6, 16, "A: Listen  B: Back", PB_UI);
        break;
    case RECV_WAIT:
        ui_text(9, 5, "RECEIVING...", PB_UI);
        ui_text(6, 9,  "Waiting for the", PB_UI);
        ui_text(6, 10, "camera to send.", PB_UI);
        ui_text(6, 12, "Hold B to cancel", PB_UI);
        ui_text(1, 19, "NO LINK", PB_UI);
        xfer_count(0);
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
            if (trade_parse_capture(rx_buf, rx_len, img, thumb, meta)) {
                rv_result = cam_store_add(img, thumb, meta);
                rv_nospace = (rv_result < 0);
                break;
            }
        }
        dbg_record(rc, rx_buf, rx_len, tx_buf, rx_len);

        if (rv_result >= 0) {
            // Show the complete photo immediately; B returns to the menu.
            view_render_photo(img);
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

enum { SEND_SELECT, SEND_BUSY, SEND_DONE };

static int sd_state;
static int sd_cur;        // slot index
static bool sd_ok;

static int next_occupied(int from, int dir)
{
    int cap = cam_store_capacity();
    if (cap == 0) return -1;
    for (int i = 0; i < cap; i++) {
        int s = (from + dir * (i + 1) + cap * 2) % cap;
        if (cam_store_occupied(s)) return s;
    }
    return -1;
}

static void send_draw(void)
{
    ui_cls();
    ui_fill(4, 4, 22, 12, 3);
    ui_frame(4, 4, 22, 12);
    switch (sd_state) {
    case SEND_SELECT: {
        ui_text(10, 5, "SEND BACK", PB_UI);
        if (cam_store_count() == 0) {
            ui_text(6, 9, "No photos stored.", PB_UI);
            ui_text(6, 15, "B: Back", PB_UI);
        } else {
            char line[32];
            siprintf(line, "Photo %d / %d", sd_cur + 1, cam_store_capacity());
            ui_text(6, 8, line, PB_UI);
            ui_text(6, 10, "On the camera:", PB_UI);
            ui_text(6, 11, "1. Press SELECT.", PB_UI);
            ui_text(6, 12, "2. UP to LINK, A.", PB_UI);
            ui_text(6, 13, "3. DOWN to TRANSFER,", PB_UI);
            ui_text(6, 14, "   A. Pick RECEIVE.", PB_UI);
            ui_text(6, 16, "A: Send  B: Back", PB_UI);
        }
        break;
    }
    case SEND_BUSY:
        ui_text(10, 5, "SENDING...", PB_UI);
        ui_text(6, 9,  "Camera must be in", PB_UI);
        ui_text(6, 10, "RECEIVE mode.", PB_UI);
        ui_text(6, 12, "Hold B to cancel", PB_UI);
        ui_text(1, 19, "NO LINK", PB_UI);
        xfer_count(0);
        break;
    case SEND_DONE:
        ui_text(10, 5, "SEND BACK", PB_UI);
        ui_text(6, 8, sd_ok ? "Photo sent." : "Send failed.", PB_UI);
        ui_text(6, 16, "B: Back", PB_UI);
        break;
    }
}

void send_enter(void)
{
    sd_state = SEND_SELECT;
    sd_cur = next_occupied(-1, 1);
    if (sd_cur < 0) sd_cur = 0;
    send_draw();
}

void send_frame(int frame)
{
    (void)frame;
    if (sd_state == SEND_SELECT) {
        if (key_hit(KEY_B)) { app_goto(ST_MENU); return; }
        if (cam_store_count() == 0) return;
        if (key_hit(KEY_UP))   { int s = next_occupied(sd_cur, -1); if (s >= 0) { sd_cur = s; send_draw(); } }
        if (key_hit(KEY_DOWN)) { int s = next_occupied(sd_cur, 1);  if (s >= 0) { sd_cur = s; send_draw(); } }
        if (!key_hit(KEY_A)) return;

        cam_store_get(sd_cur, img, thumb, meta);
        int tx_len = trade_build_send(img, thumb, meta, tx_buf);
        tx_buf[1] = reply_mode();           // A's album count in Mode 1

        sd_state = SEND_BUSY;
        send_draw();
        VBlankIntrWait();

        int rx_len = 0;
        int rc = cam_link_exchange(true, tx_buf, tx_len,
                                   rx_buf, TRADE_STREAM_BYTES, &rx_len, xfer_count);
        dbg_record(rc, rx_buf, rx_len, tx_buf, tx_len);

        sd_ok = (rx_len >= 1) && (rx_buf[0] == TRADE_HANDSHAKE_B);
        sd_state = SEND_DONE;
        send_draw();
    } else if (sd_state == SEND_DONE) {
        if (key_hit(KEY_A | KEY_B)) app_goto(ST_MENU);
    }
}
