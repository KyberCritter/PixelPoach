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
