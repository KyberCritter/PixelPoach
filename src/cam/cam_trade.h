// Game Boy Camera LINK/TRANSFER ("trade") protocol. PORTABLE C — offsets,
// capture parsing and send-stream building, host-testable without hardware.
//
// The wire is full-duplex DMG serial at 8192 Hz with an alternating clock: the
// initiator (Camera A) clocks the first byte, the responder (Camera B) clocks
// the next, and so on. Both sides shift one byte every round regardless of who
// drives the clock.
#ifndef CAM_CAM_TRADE_H
#define CAM_CAM_TRADE_H

#include <stdint.h>
#include <stdbool.h>

// Handshake bytes (first round, full duplex: A and B exchange these at once).
#define TRADE_HANDSHAKE_A 0x29
#define TRADE_HANDSHAKE_B 0x12

// Transfer-mode byte: Mode 1 = picture count 0..29 (0x5E = album full,
// 0x00 = empty); Mode 2 = 0x80 + count (0xDE = full, 0x80 = empty). Camera B
// always answers Mode 1; Camera A uses Mode 1 to send, Mode 2 to receive.
#define TRADE_MODE_FULL   0x5E
#define TRADE_MODE_EMPTY  0x00

// A->B SEND wire layout. Verified against a real transfer and a two-camera
// link log: a 7-byte protocol prelude, then the 4096-byte photo buffer.
//
//   0  0x29                 handshake A
//   1  info byte            photo count; bit 6 = album full
//   2  0x00                 prelude
//   3  0x40 | photo number  command: transfer photo n (album position 0..29)
//   4  0x00                 sync (not ready)
//   5  0x01                 sync (ready)
//   6  0x00                 first bulk exchange (dummy, not stored)
//   7  photo buffer, 4096 bytes:
//        +0x000..0xDFF  image (3584)
//        +0xE00..0xEFF  thumbnail (256)
//        +0xF00..0xF5B  owner metadata (92)
//        +0xF5C..0xFB7  metadata echo (92)
//        +0xFB8..0xFFE  zero
//        +0xFFF         owner byte ($DA56: gender / blood type)
#define TRADE_OFF_IMAGE    7
#define TRADE_IMG_BYTES    3584         // 128x112 2bpp, 224 tiles
#define TRADE_OFF_THUMB    (TRADE_OFF_IMAGE + TRADE_IMG_BYTES)     // 3591
#define TRADE_THUMB_BYTES  256
#define TRADE_OFF_META     (TRADE_OFF_THUMB + TRADE_THUMB_BYTES)   // 3847
#define TRADE_META_BYTES   92
#define TRADE_OFF_TAIL     (TRADE_OFF_META + 2 * TRADE_META_BYTES) // 4031
#define TRADE_TAIL_BYTES   72
#define TRADE_SEND_BYTES   (TRADE_OFF_TAIL + TRADE_TAIL_BYTES)     // 4103

// Command byte: 0x40 | album position.
#define TRADE_CMD_PHOTO    0x40

// Owner byte ($DA56) lives at this offset in the owner metadata block, and is
// echoed as the last byte of the send buffer.
#define TRADE_META_OWNER_BYTE 13

#define TRADE_STREAM_BYTES 4160         // generous capture/playback buffer

// Bytes the GBA clocks as Camera B when receiving: the camera sends 4103,
// stops, and the GBA clocks one trailing byte before its wait times out.
// Used for the on-screen progress counter (which excludes the 2 header bytes).
#define TRADE_PHOTO_BYTES  4104

// Header rounds before transfer data: handshake + mode/album count. The UI
// counter shows transfer bytes only (4102/4102 on a complete photo).
#define TRADE_HDR_BYTES    2
#define TRADE_DATA_BYTES   (TRADE_PHOTO_BYTES - TRADE_HDR_BYTES)

// Parse a captured A->B SEND stream into its photo payloads. The image,
// thumbnail and owner-metadata are copied out. Returns false if `rx_len` does
// not reach the metadata block.
bool trade_parse_capture(const uint8_t *rx, int rx_len,
                         uint8_t *img, uint8_t *thumb, uint8_t *meta);

// Build the A->B SEND stream for album position `photo_number` (0..29) with
// `img`/`thumb`/`meta`. Returns the number of bytes written to `out`
// (TRADE_SEND_BYTES). The caller sets the info byte at out[1].
int trade_build_send(int photo_number, const uint8_t *img,
                     const uint8_t *thumb, const uint8_t *meta, uint8_t *out);

#endif
