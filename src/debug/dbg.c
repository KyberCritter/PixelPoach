// Debug capture + hex viewer. GBA-only (uses the storage backend and tonc).
#include <stdio.h>
#include <string.h>
#include "debug/dbg.h"
#include "app.h"
#include "storage/storage.h"
#include "ui/screen.h"
#include "ui/ui.h"

EWRAM_BSS DbgCapture g_dbg;

static u32 dbg_offset(void)
{
    u32 s = storage_size();
    return s > DBG_RESERVE ? s - DBG_RESERVE : 0;
}

void dbg_record(int result, const u8 *rx, int rx_len, const u8 *tx, int tx_len)
{
    memset(&g_dbg, 0, sizeof g_dbg);
    g_dbg.magic = DBG_MAGIC;
    g_dbg.result = result;
    g_dbg.rx_len = rx_len;
    g_dbg.tx_len = tx_len;
    if (rx) memcpy(g_dbg.rx, rx, (rx_len < DBG_CAP ? rx_len : DBG_CAP));
    if (tx) memcpy(g_dbg.tx, tx, (tx_len < DBG_CAP ? tx_len : DBG_CAP));
    dbg_save();
}

void dbg_save(void)
{
    u32 off = dbg_offset();
    if (off)
        storage_write(off, &g_dbg, sizeof g_dbg);
}

void dbg_load(void)
{
    memset(&g_dbg, 0, sizeof g_dbg);
    u32 off = dbg_offset();
    if (off) {
        storage_read(off, &g_dbg, sizeof g_dbg);
        if (g_dbg.magic != DBG_MAGIC)
            memset(&g_dbg, 0, sizeof g_dbg);
    }
}

// ---- live progress (called from the link driver with IRQs masked) ----

static inline void put_hex4(int x, int y, int v)
{
    static const char hex[] = "0123456789ABCDEF";
    u16 *tm = text_map();
    for (int i = 3; i >= 0; i--) {
        map_put(tm, x + i, y, hex[(v >> (4 * i)) & 0xF], PB_UI);
    }
}

void dbg_live(int n)
{
    u16 *tm = text_map();
    map_put(tm, 1, 18, 'R', PB_UI);
    map_put(tm, 2, 18, 'X', PB_UI);
    map_put(tm, 3, 18, ' ', PB_UI);
    put_hex4(4, 18, n);
}

// ---- hex viewer scene ----

static int hv_show_tx;      // 0 = rx, 1 = tx
static int hv_off;          // byte offset into the shown buffer

static const char *result_text(int r)
{
    switch (r) {
    case CAM_XCHG_OK:        return "OK (remote done)";
    case CAM_XCHG_TIMEOUT:   return "TIMEOUT (no clock)";
    case CAM_XCHG_CANCELLED: return "CANCELLED (B)";
    default:                 return "UNKNOWN";
    }
}

static const u8 *hv_buf(void)
{
    return hv_show_tx ? g_dbg.tx : g_dbg.rx;
}

static int hv_len(void)
{
    int n = hv_show_tx ? g_dbg.tx_len : g_dbg.rx_len;
    if (n < 0) n = 0;
    if (n > DBG_CAP) n = DBG_CAP;
    return n;
}

static void dbg_draw(void)
{
    static const char hex[] = "0123456789ABCDEF";
    ui_cls();
    ui_text(11, 0, hv_show_tx ? "DBG: TX" : "DBG: RX", PB_UI);
    ui_text(1, 1, result_text(g_dbg.result), PB_UI);

    char line[30];
    siprintf(line, "rx=%d tx=%d", g_dbg.rx_len, g_dbg.tx_len);
    ui_text(1, 2, line, PB_UI);

    int len = hv_len();
    const u8 *buf = hv_buf();
    int rows = 15;                      // rows 3..17
    for (int r = 0; r < rows; r++) {
        int base = hv_off + r * 12;
        if (base >= len)
            break;
        int col = 1;
        // 4-hex offset
        for (int i = 3; i >= 0; i--)
            map_put(text_map(), col++, 3 + r, hex[(base >> (4 * i)) & 0xF], PB_UI);
        map_put(text_map(), col++, 3 + r, ' ', PB_UI);
        for (int b = 0; b < 12; b++) {
            if (base + b < len) {
                u8 v = buf[base + b];
                map_put(text_map(), col++, 3 + r, hex[v >> 4], PB_UI);
                map_put(text_map(), col++, 3 + r, hex[v & 0xF], PB_UI);
            } else {
                col += 2;
            }
        }
    }
    ui_text(1, 19, "Up/Dn scroll  L/R rx/tx", PB_UI);
}

void dbg_enter(void)
{
    hv_show_tx = 0;
    hv_off = 0;
    dbg_draw();
}

void dbg_frame(int frame)
{
    (void)frame;
    int step = 12;
    if (key_hit(KEY_UP))   { if (hv_off >= step) { hv_off -= step; dbg_draw(); } }
    if (key_hit(KEY_DOWN)) { if (hv_off + 15 * step < hv_len()) { hv_off += step; dbg_draw(); } }
    if (key_hit(KEY_L) || key_hit(KEY_R)) { hv_show_tx ^= 1; hv_off = 0; dbg_draw(); }
    if (key_hit(KEY_B))    { app_goto(ST_MENU); }
}
