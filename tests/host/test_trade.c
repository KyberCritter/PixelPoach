#include <stdio.h>
#include <string.h>
#include "cam_trade.h"

static int fails = 0;
static int checks = 0;

#define CHECK(cond) do { checks++; if (!(cond)) { \
    fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

int main(void)
{
    uint8_t img[TRADE_IMG_BYTES], thumb[TRADE_THUMB_BYTES], meta[TRADE_META_BYTES];
    for (int i = 0; i < TRADE_IMG_BYTES; i++)   img[i]   = (uint8_t)(i * 11 + 3);
    for (int i = 0; i < TRADE_THUMB_BYTES; i++) thumb[i] = (uint8_t)(i * 5 + 7);
    for (int i = 0; i < TRADE_META_BYTES; i++)  meta[i]  = (uint8_t)(i + 1);
    // A real owner block ends with "Magic" + checksum; the parser uses the
    // marker to recover the image alignment.
    memcpy(meta + (TRADE_META_BYTES - 2 - TRADE_MAGIC_LEN),
           TRADE_MAGIC, TRADE_MAGIC_LEN);

    // The wire offsets are protocol constants: prelude = 7 bytes, payload =
    // 4096 bytes (image 3584 + thumb 256 + tag 92 + echo 92 + tail 72).
    CHECK(TRADE_OFF_IMAGE == 7);
    CHECK(TRADE_OFF_THUMB == 3591);
    CHECK(TRADE_OFF_META == 3847);
    CHECK(TRADE_SEND_BYTES == 4103);

    uint8_t stream[TRADE_STREAM_BYTES];
    int len = trade_build_send(7, img, thumb, meta, stream);
    CHECK(len == TRADE_SEND_BYTES);
    CHECK(stream[0] == TRADE_HANDSHAKE_A);
    CHECK(stream[3] == (TRADE_CMD_PHOTO | 7));
    CHECK(stream[5] == 0x01);
    // image must sit exactly at TRADE_OFF_IMAGE (7-byte prelude)
    CHECK(memcmp(stream + TRADE_OFF_IMAGE, img, TRADE_IMG_BYTES) == 0);
    CHECK(memcmp(stream + TRADE_OFF_THUMB, thumb, TRADE_THUMB_BYTES) == 0);
    CHECK(memcmp(stream + TRADE_OFF_META + TRADE_META_BYTES,
                 meta, TRADE_META_BYTES) == 0);
    CHECK(stream[TRADE_SEND_BYTES - 1] == meta[TRADE_META_OWNER_BYTE]);

    uint8_t oimg[TRADE_IMG_BYTES], othumb[TRADE_THUMB_BYTES], ometa[TRADE_META_BYTES];
    CHECK(trade_parse_capture(stream, len, oimg, othumb, ometa));
    CHECK(memcmp(oimg, img, TRADE_IMG_BYTES) == 0);
    CHECK(memcmp(othumb, thumb, TRADE_THUMB_BYTES) == 0);
    CHECK(memcmp(ometa, meta, TRADE_META_BYTES) == 0);

    // Real GBA captures can omit the first two wire bytes, moving every later
    // block two bytes earlier. The parser must still recover the same photo.
    uint8_t capture[TRADE_STREAM_BYTES];
    int cap_len = len - 2;
    memcpy(capture, stream + 2, (size_t)cap_len);
    CHECK(trade_parse_capture(capture, cap_len, oimg, othumb, ometa));
    CHECK(memcmp(oimg, img, TRADE_IMG_BYTES) == 0);
    CHECK(memcmp(othumb, thumb, TRADE_THUMB_BYTES) == 0);
    CHECK(memcmp(ometa, meta, TRADE_META_BYTES) == 0);

    // a too-short stream must not parse
    CHECK(!trade_parse_capture(stream, TRADE_OFF_META - 1, oimg, othumb, ometa));

    printf("%d checks, %d failures\n", checks, fails);
    return fails ? 1 : 0;
}
