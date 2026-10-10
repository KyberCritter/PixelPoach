#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "cam_save.h"

static int fails = 0;
static int checks = 0;

#define CHECK(cond) do { checks++; if (!(cond)) { \
    fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

int main(void)
{
    // 1. Known-good checksum: state vector 0x00..0x1D must be (0xE2, 0x14).
    {
        uint8_t data[30];
        for (int i = 0; i < 30; i++) data[i] = (uint8_t)i;
        uint8_t ck[2];
        cam_checksum(data, 30, ck);
        CHECK(ck[0] == 0xE2 && ck[1] == 0x14);
    }

    // 2. cam_save_init: magic present, vector checksum self-consistent.
    {
        uint8_t *buf = (uint8_t *)malloc(CAM_SAVE_SIZE);
        cam_save_init(buf, CAM_SAVE_SIZE);
        CHECK(memcmp(buf + CAM_VECTOR_MAGIC, cam_magic, CAM_MAGIC_LEN) == 0);
        uint8_t ck[2];
        cam_checksum(buf + CAM_VECTOR_OFF, CAM_SLOT_COUNT, ck);
        CHECK(memcmp(buf + CAM_VECTOR_CHK, ck, 2) == 0);
        for (int s = 0; s < CAM_SLOT_COUNT; s++)
            CHECK(buf[CAM_VECTOR_OFF + s] == 0xFF);
        CHECK(cam_save_slot_count(CAM_SAVE_SIZE) == CAM_SLOT_COUNT);
        free(buf);
    }

    // 3. cam_save_put: image/thumb copied, owner block valid, vector updated.
    {
        uint8_t *buf = (uint8_t *)malloc(CAM_SAVE_SIZE);
        cam_save_init(buf, CAM_SAVE_SIZE);
        uint8_t img[CAM_IMG_BYTES], thumb[CAM_THUMB_BYTES];
        for (int i = 0; i < CAM_IMG_BYTES; i++)   img[i]   = (uint8_t)(i * 7 + 1);
        for (int i = 0; i < CAM_THUMB_BYTES; i++) thumb[i] = (uint8_t)(i * 3 + 2);
        CHECK(cam_save_put(buf, CAM_SAVE_SIZE, 3, img, thumb, NULL));

        uint32_t base = CAM_SLOT_BASE + 3 * CAM_SLOT_SIZE;
        CHECK(memcmp(buf + base + 0x0000, img, CAM_IMG_BYTES) == 0);
        CHECK(memcmp(buf + base + 0x0E00, thumb, CAM_THUMB_BYTES) == 0);
        // owner block: magic at its tail, checksum self-consistent
        CHECK(memcmp(buf + base + CAM_META_OWNER_OFF + (CAM_META_BYTES - 2 - CAM_MAGIC_LEN),
                     cam_magic, CAM_MAGIC_LEN) == 0);
        uint8_t ck[2];
        cam_checksum(buf + base + CAM_META_OWNER_OFF, CAM_META_BYTES - 2 - CAM_MAGIC_LEN, ck);
        CHECK(memcmp(buf + base + CAM_META_OWNER_OFF + CAM_META_BYTES - 2, ck, 2) == 0);
        CHECK(cam_save_occupied(buf, 3));
        CHECK(!cam_save_occupied(buf, 4));
        cam_checksum(buf + CAM_VECTOR_OFF, CAM_SLOT_COUNT, ck);
        CHECK(memcmp(buf + CAM_VECTOR_CHK, ck, 2) == 0);
        free(buf);
    }

    // 4. cam_image_decode: a tile of shade bits round-trips to pixel values.
    {
        uint8_t img[CAM_IMG_BYTES];
        memset(img, 0, sizeof img);
        // tile 0, row 0: two bytes. Make b0 = 0b11000000, b1 = 0b10100000.
        // Pixels (MSB first): p0 = b0[7]|(b1[7]<<1)=1|2=3, p1=1|0=1,
        // p2=0|(1<<1)=2, p3=0 ...
        img[0] = 0xC0;   // 1100 0000
        img[1] = 0xA0;   // 1010 0000
        uint8_t out[CAM_IMG_W * CAM_IMG_H];
        cam_image_decode(img, out);
        CHECK(out[0] == 3);
        CHECK(out[1] == 1);
        CHECK(out[2] == 2);
        CHECK(out[3] == 0);
        // pixel (8,0) is in tile 1 (col 1), whose bytes are zero
        CHECK(out[8] == 0);
    }

    printf("%d checks, %d failures\n", checks, fails);
    return fails ? 1 : 0;
}
