#include <string.h>
#include "cam_trade.h"

// The owner block always has the ASCII word "Magic" five bytes before its
// two checksum bytes (at offset 85 of 92). Use it to find where the image
// really starts: the GBA can miss the first two wire bytes when the camera
// starts clocking before the slave-side poll is running, which shifts every
// later block two bytes earlier in the captured stream.
static bool owner_block_ok(const uint8_t *meta)
{
    return memcmp(meta + (TRADE_META_BYTES - 2 - TRADE_MAGIC_LEN),
                  TRADE_MAGIC, TRADE_MAGIC_LEN) == 0;
}

static int capture_image_offset(const uint8_t *rx, int rx_len)
{
    // Prefer a layout whose metadata marker proves the alignment. Try the
    // full wire layout first, then the two-bytes-early capture that real
    // hardware produces.
    static const int candidates[] = {
        TRADE_OFF_IMAGE, TRADE_CAP_OFF_IMAGE
    };
    for (unsigned i = 0; i < sizeof candidates / sizeof candidates[0]; i++) {
        int off = candidates[i];
        int meta_off = off + TRADE_IMG_BYTES + TRADE_THUMB_BYTES;
        if (rx_len >= meta_off + TRADE_META_BYTES &&
            owner_block_ok(rx + meta_off))
            return off;
    }
    // No readable owner marker: fall back to the capture alignment observed
    // on real hardware rather than rejecting a photo that is otherwise there.
    return TRADE_CAP_OFF_IMAGE;
}

bool trade_parse_capture(const uint8_t *rx, int rx_len,
                         uint8_t *img, uint8_t *thumb, uint8_t *meta)
{
    int off = capture_image_offset(rx, rx_len);
    int meta_off = off + TRADE_IMG_BYTES + TRADE_THUMB_BYTES;
    if (rx_len < meta_off + TRADE_META_BYTES)
        return false;
    memcpy(img,   rx + off, TRADE_IMG_BYTES);
    memcpy(thumb, rx + off + TRADE_IMG_BYTES, TRADE_THUMB_BYTES);
    memcpy(meta,  rx + meta_off, TRADE_META_BYTES);
    return true;
}

int trade_build_send(int photo_number, const uint8_t *img,
                     const uint8_t *thumb, const uint8_t *meta, uint8_t *out)
{
    memset(out, 0, TRADE_SEND_BYTES);

    out[0] = TRADE_HANDSHAKE_A;
    out[1] = TRADE_MODE_EMPTY;       // A's album count (Mode 1); caller may adjust
    out[3] = (uint8_t)(TRADE_CMD_PHOTO | (photo_number & 0x3F));
    out[5] = 0x01;                   // ready

    memcpy(out + TRADE_OFF_IMAGE, img, TRADE_IMG_BYTES);
    memcpy(out + TRADE_OFF_THUMB, thumb, TRADE_THUMB_BYTES);
    memcpy(out + TRADE_OFF_META, meta, TRADE_META_BYTES);
    memcpy(out + TRADE_OFF_META + TRADE_META_BYTES, meta, TRADE_META_BYTES);
    out[TRADE_SEND_BYTES - 1] = meta[TRADE_META_OWNER_BYTE];
    return TRADE_SEND_BYTES;
}
