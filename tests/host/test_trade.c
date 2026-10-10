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

    uint8_t stream[TRADE_STREAM_BYTES];
    int len = trade_build_send(img, thumb, meta, stream);
    CHECK(len <= TRADE_STREAM_BYTES);
    CHECK(stream[0] == TRADE_HANDSHAKE_A);
    // image must sit exactly at TRADE_OFF_IMAGE (handshake+mode+pad)
    CHECK(memcmp(stream + TRADE_OFF_IMAGE, img, TRADE_IMG_BYTES) == 0);

    uint8_t oimg[TRADE_IMG_BYTES], othumb[TRADE_THUMB_BYTES], ometa[TRADE_META_BYTES];
    CHECK(trade_parse_capture(stream, len, oimg, othumb, ometa));
    CHECK(memcmp(oimg, img, TRADE_IMG_BYTES) == 0);
    CHECK(memcmp(othumb, thumb, TRADE_THUMB_BYTES) == 0);
    CHECK(memcmp(ometa, meta, TRADE_META_BYTES) == 0);

    // a too-short stream must not parse
    CHECK(!trade_parse_capture(stream, TRADE_OFF_META - 1, oimg, othumb, ometa));

    printf("%d checks, %d failures\n", checks, fails);
    return fails ? 1 : 0;
}
