// Receive and send scenes: drive the camera trade over the link.
#include <stdio.h>
#include <string.h>
#include <tonc.h>
#include "app.h"
#include "ui/ui.h"
#include "ui/screen.h"
#include "core/charset.h"
#include "gfx/font.h"
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

// ---------- MODE4 overlay (draw text and buttons onto a bitmap photo) ----------
//
// MODE4 shows the centred photo as a BG2 bitmap with no text layer, so the
// send scene's counter and button hints are painted straight into VRAM. VRAM
// ignores 8-bit writes (the byte mirrors into both halves of the halfword), so
// each pixel is a read-modify-write of the whole halfword.

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

static void bmp_text(int x, int y, const char *s, u8 c)
{
    for (; *s; s++, x += 8) bmp_glyph(x, y, (u8)*s, c);
}

// A round GBA-style face button (white disc, dark letter) 16x16.
static void bmp_button(int x, int y, int letter)
{
    for (int j = 0; j < 16; j++)
        for (int i = 0; i < 16; i++) {
            int dx = 2 * i - 15, dy = 2 * j - 15;
            if (dx * dx + dy * dy <= 225)
                bmp_px(x + i, y + j, 0);
        }
    bmp_glyph(x + 4, y + 4, letter, 4);
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
            // A short capture means we lost a byte (the stream shifted), not
            // just the optional trailing clock. Saving it would display a
            // photo with horizontal lines, so reject it.
            if (rx_len >= TRADE_SEND_BYTES &&
                trade_parse_capture(rx_buf, rx_len, img, thumb, meta)) {
                rv_result = cam_store_add(img, thumb, meta);
                rv_nospace = (rv_result < 0);
                break;
            }
            // Something was clocked, so the camera did start a transfer. If it
            // did not complete, report the failure instead of re-arming and
            // waiting forever on a camera that has already finished.
            if (rx_len > 0)
                break;
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

enum { SEND_SELECT, SEND_EMPTY, SEND_BUSY, SEND_DONE };

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

// The select screen is a MODE4 photo (one at a time) with the photo number in
// the bottom-left and A/B button hints along the bottom. img/thumb/meta hold
// the photo currently on screen so A can send it without re-reading the slot.
static void send_select_draw(void)
{
    cam_store_get(sd_cur, img, thumb, meta);
    view_render_photo(img);

    int ord = 0;
    for (int s = 0; s <= sd_cur; s++)
        if (cam_store_occupied(s)) ord++;

    char line[16];
    siprintf(line, "%d/%d", ord, cam_store_count());
    bmp_text(6, 150, line, 0);

    bmp_button(56, 142, 'A');
    bmp_text(74, 146, "Send", 0);
    bmp_button(116, 142, 'B');
    bmp_text(134, 146, "Cancel", 0);
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

void send_enter(void)
{
    sd_state = SEND_SELECT;
    sd_cur = next_occupied(-1, 1);
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
        if (key_hit(KEY_UP))   { int s = next_occupied(sd_cur, -1); if (s >= 0) { sd_cur = s; send_select_draw(); } }
        if (key_hit(KEY_DOWN)) { int s = next_occupied(sd_cur, 1);  if (s >= 0) { sd_cur = s; send_select_draw(); } }
        if (key_hit(KEY_B)) { pxl_init_video(); app_goto(ST_MENU); return; }
        if (!key_hit(KEY_A)) return;

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
    } else if (sd_state == SEND_DONE) {
        if (key_hit(KEY_A | KEY_B)) app_goto(ST_MENU);
    }
}
