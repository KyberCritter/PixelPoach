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

// A->B stream layout for a SEND (per funtography "Camera Transfer Protocol").
// Verified against a real transfer: the metadata checksum only validates when
// the image starts 5 bytes in (handshake 1 + mode 1 + 3 pad bytes), not 7.
#define TRADE_OFF_IMAGE    5            // handshake(1)+mode(1)+pad(3)
#define TRADE_IMG_BYTES    3584         // 128x112 2bpp, 224 tiles
#define TRADE_OFF_THUMB    (TRADE_OFF_IMAGE + TRADE_IMG_BYTES)     // 3589
#define TRADE_THUMB_BYTES  256
#define TRADE_OFF_META     (TRADE_OFF_THUMB + TRADE_THUMB_BYTES)   // 3845
#define TRADE_META_BYTES   92
#define TRADE_STREAM_BYTES 4160         // generous capture/playback buffer

// Total bytes the camera clocks for one photo, from the handshake through the
// trailing payload. Measured from a real transfer (4104 bytes); used only as
// the denominator of the on-screen "n/4104 bytes" progress counter, never as
// an acceptance gate.
#define TRADE_PHOTO_BYTES  4104

// Parse a captured A->B SEND stream into its photo payloads. The image,
// thumbnail and owner-metadata are copied out. Returns false if `rx_len` does
// not reach the metadata block.
bool trade_parse_capture(const uint8_t *rx, int rx_len,
                         uint8_t *img, uint8_t *thumb, uint8_t *meta);

// Build the A->B SEND stream for `img`/`thumb`/`meta`. Returns the number of
// bytes written to `out` (<= TRADE_STREAM_BYTES).
int trade_build_send(const uint8_t *img, const uint8_t *thumb,
                     const uint8_t *meta, uint8_t *out);

#endif
