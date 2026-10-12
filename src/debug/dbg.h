// Debug capture + logging. Records the last link exchange (result code and the
// raw bytes sent/received), shows a scrollable hex viewer on screen, and
// persists the capture to a reserved region at the end of the save area so it
// survives a power cycle and can be read back from the .sav file.
#ifndef DEBUG_DBG_H
#define DEBUG_DBG_H

#include <tonc.h>
#include "link/cam_link.h"

// Bytes reserved at the END of the save area for the debug capture. Sector
// aligned (4 KB) so the FLASH1M backend's read-modify-write never overlaps the
// camera .sav. The camera .sav shrinks by this much.
#define DBG_RESERVE 0x1000

#define DBG_MAGIC 0x50444247u          // "GBDP"

// First this many bytes of each direction are captured (enough to see the
// handshake, mode byte, and the start of the image stream).
#define DBG_CAP 2000

typedef struct {
    u32 magic;
    int result;                        // CAM_XCHG_*
    int rx_len;                        // total bytes received (may exceed DBG_CAP)
    int tx_len;
    u8  rx[DBG_CAP];                   // first DBG_CAP received bytes
    u8  tx[DBG_CAP];                   // first DBG_CAP sent bytes
} DbgCapture;

extern EWRAM_BSS DbgCapture g_dbg;

// Record an exchange result into g_dbg (and persist it).
void dbg_record(int result, const u8 *rx, int rx_len, const u8 *tx, int tx_len);

// Persist / reload g_dbg to/from the reserved save region.
void dbg_save(void);
void dbg_load(void);

// Scene: scrollable hex viewer of the last capture.
void dbg_enter(void);
void dbg_frame(int frame);

#endif
