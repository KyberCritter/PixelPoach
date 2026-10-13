// GBA link-port driver for the Game Boy Camera trade protocol. GBA-ONLY.
//
// The camera trade is full-duplex DMG serial at 8192 Hz with an ALTERNATING
// clock: the initiator (Camera A) clocks one byte, then the responder (Camera
// B) clocks the next, and so on. This file bit-bangs that over the GBA link
// port in general-purpose GPIO mode, the same technique GBScribe/AdvancePrint
// use for the printer (only here we alternate master and slave roles).
//
// Wiring (GBC/Pocket link cable, crosses SO<->SI):
//   GBA SC <-> camera SC (direction flips per byte)
//   GBA SO  -> camera SI
//   GBA SI <-  camera SO
#ifndef LINK_CAM_LINK_H
#define LINK_CAM_LINK_H

#include <tonc.h>

// RCNT bits in GPIO mode (bit 15 = general-purpose mode).
#define R_GPIO   (1 << 15)
#define R_SC     (1 << 0)
#define R_SD     (1 << 1)
#define R_SI     (1 << 2)
#define R_SO     (1 << 3)
#define R_SC_OUT (1 << 4)
#define R_SD_OUT (1 << 5)
#define R_SI_OUT (1 << 6)
#define R_SO_OUT (1 << 7)

// Half-period tick count for master mode (8192 Hz, TM2 prescaler 1).
#define CAM_HALF_NORMAL 1024

// Exchange result codes.
enum {
    CAM_XCHG_OK = 0,        // remote finished cleanly (stopped clocking)
    CAM_XCHG_TIMEOUT,       // no clock seen at all (camera absent / not started)
    CAM_XCHG_CANCELLED,     // B pressed mid-transfer
};

void cam_link_init(void);

// Clock one byte out (and one back) with the GBA as clock master.
u8 cam_link_master_byte(u8 send);

// Receive one byte clocked by the camera (GBA as slave). `send` is the byte
// the GBA shifts out on the same clock edges. Returns false on timeout (the
// camera never clocked).
bool cam_link_slave_byte(u8 send, u8 *recv);

// Blocking alternating-clock exchange of up to `max_rounds` bytes. `is_a`
// selects the initiator role (drives the first clock). `tx`/`tx_len` supply
// the bytes the GBA shifts out; bytes past tx_len are sent as 0x00. Received
// bytes land in `rx`. Returns a CAM_XCHG_* code; *rx_len is set to the number
// of bytes exchanged. IRQs are masked for the duration (link timing).
// `progress`, if non-NULL, is called with the byte index while the GBA is clock
// master (inside the settle window, where the camera is already waiting), plus
// once when the exchange ends. It must never run between a byte and the next
// slave read, or the camera's next clock edge can be missed and the tail of the
// transfer dropped.
int cam_link_exchange(bool is_a, const u8 *tx, int tx_len,
                      u8 *rx, int max_rounds, int *rx_len,
                      void (*progress)(int n));

#endif
