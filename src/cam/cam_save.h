// Game Boy Camera .sav builder. PORTABLE C (no GBA headers) so it compiles for
// the host harness. Produces a camera-format SRAM image that existing desktop
// tools (GBCameraDump, gameboycameralib, the GBP web app) can read directly.
//
// The cart's save area IS the .sav, truncated to whatever the runtime-detected
// backend can hold (128K flash = all 30 slots; 64K/32K SRAM = fewer slots).
#ifndef CAM_CAM_SAVE_H
#define CAM_CAM_SAVE_H

#include <stdint.h>
#include <stdbool.h>

// Photo payload sizes (as exchanged over the link, not padded).
#define CAM_IMG_W        128
#define CAM_IMG_H        112
#define CAM_IMG_BYTES    3584   // 224 tiles * 16 bytes (2bpp)
#define CAM_THUMB_BYTES  256    // 32x32 thumbnail, 16 tiles
#define CAM_META_BYTES   92     // owner block: 85 data + "Magic" + checksum

// Camera SRAM layout (full save is 0x20000 = 128 KB).
#define CAM_SAVE_SIZE    0x20000
#define CAM_SLOT_SIZE    0x1000
#define CAM_SLOT_COUNT   30
#define CAM_SLOT_BASE    0x02000   // first image slot's data offset

// Header sub-regions.
#define CAM_VECTOR_OFF   0x011B2   // 30 bytes: slot -> image number (0..29) or 0xFF
#define CAM_VECTOR_MAGIC 0x011D0   // "Magic"
#define CAM_VECTOR_CHK   0x011D5   // sum/xor checksum over the 30 vector bytes

#define CAM_META_OWNER_OFF 0x0F00 // within a slot: owner block (92 bytes)
#define CAM_META_OWNER_CHK 0x0F5A // within a slot: owner checksum offset

// The "Magic" ASCII word that precedes every checksummed region.
#define CAM_MAGIC_LEN 5
extern const uint8_t cam_magic[CAM_MAGIC_LEN];

// 8-bit sum + 8-bit xor with seed (0x2F, 0x15), the verified Game Boy Camera
// checksum (Raphael-Boichot / funtography). The 5-byte "Magic" word is NOT
// included in `data`; its contribution is baked into the seed.
void cam_checksum(const uint8_t *data, int len, uint8_t out[2]);

// Number of image slots that fit in a save buffer of `size` bytes.
int cam_save_slot_count(uint32_t size);

// Initialise a save buffer as an empty camera save (zeroed, valid vector state
// + checksum). `size` may be less than CAM_SAVE_SIZE (truncated save).
void cam_save_init(uint8_t *buf, uint32_t size);

// Write one photo into `slot` (0..cam_save_slot_count-1), update the vector
// state, and fix its checksum. `meta` may be NULL (blank owner block written).
// Returns false if the slot does not fit in `size`.
bool cam_save_put(uint8_t *buf, uint32_t size, int slot,
                  const uint8_t *img, const uint8_t *thumb, const uint8_t *meta);

// True if `slot` is occupied in the vector state.
bool cam_save_occupied(const uint8_t *buf, int slot);

// Decode a 128x112 2bpp camera image into `out` (CAM_IMG_W*CAM_IMG_H bytes,
// each holding shade 0..3, row-major). The image is 224 GB 2bpp tiles in
// reading order (16 wide x 14 tall).
void cam_image_decode(const uint8_t *img, uint8_t *out);

#endif
