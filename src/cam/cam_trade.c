#include <string.h>
#include "cam_trade.h"

bool trade_parse_capture(const uint8_t *rx, int rx_len,
                         uint8_t *img, uint8_t *thumb, uint8_t *meta)
{
    if (rx_len < TRADE_OFF_META + TRADE_META_BYTES)
        return false;
    memcpy(img,   rx + TRADE_OFF_IMAGE, TRADE_IMG_BYTES);
    memcpy(thumb, rx + TRADE_OFF_THUMB, TRADE_THUMB_BYTES);
    memcpy(meta,  rx + TRADE_OFF_META,  TRADE_META_BYTES);
    return true;
}

int trade_build_send(const uint8_t *img, const uint8_t *thumb,
                     const uint8_t *meta, uint8_t *out)
{
    int n = 0;
    out[n++] = TRADE_HANDSHAKE_A;
    out[n++] = TRADE_MODE_EMPTY;             // A's album count (Mode 1); caller may adjust
    while (n < TRADE_OFF_IMAGE) out[n++] = 0x00;   // pad to the image offset
    memcpy(out + n, img, TRADE_IMG_BYTES);   n += TRADE_IMG_BYTES;
    memcpy(out + n, thumb, TRADE_THUMB_BYTES); n += TRADE_THUMB_BYTES;
    memcpy(out + n, meta, TRADE_META_BYTES);   n += TRADE_META_BYTES;
    memcpy(out + n, meta, TRADE_META_BYTES);   n += TRADE_META_BYTES; // "Metadata again"
    while (n < TRADE_STREAM_BYTES) out[n++] = 0x00;
    return n;
}
