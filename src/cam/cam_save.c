#include <string.h>
#include "cam_save.h"

const uint8_t cam_magic[CAM_MAGIC_LEN] = { 'M', 'a', 'g', 'i', 'c' };

void cam_checksum(const uint8_t *data, int len, uint8_t out[2])
{
    uint8_t sum = 0x2F;
    uint8_t xr  = 0x15;
    for (int i = 0; i < len; i++) {
        sum = (uint8_t)(sum + data[i]);
        xr  = (uint8_t)(xr ^ data[i]);
    }
    out[0] = sum;
    out[1] = xr;
}

int cam_save_slot_count(uint32_t size)
{
    if (size < CAM_SLOT_BASE)
        return 0;
    int n = (int)((size - CAM_SLOT_BASE) / CAM_SLOT_SIZE);
    return n < CAM_SLOT_COUNT ? n : CAM_SLOT_COUNT;
}

void cam_save_init(uint8_t *buf, uint32_t size)
{
    memset(buf, 0, size);
    // vector state: all slots empty (0xFF)
    for (int i = 0; i < CAM_SLOT_COUNT; i++)
        buf[CAM_VECTOR_OFF + i] = 0xFF;
    // "Magic" + checksum for the (empty) vector
    memcpy(buf + CAM_VECTOR_MAGIC, cam_magic, CAM_MAGIC_LEN);
    cam_checksum(buf + CAM_VECTOR_OFF, CAM_SLOT_COUNT, buf + CAM_VECTOR_CHK);
}

bool cam_save_put(uint8_t *buf, uint32_t size, int slot,
                  const uint8_t *img, const uint8_t *thumb, const uint8_t *meta)
{
    if (slot < 0 || slot >= cam_save_slot_count(size))
        return false;
    if (slot >= CAM_SLOT_COUNT)
        return false;

    uint32_t base = CAM_SLOT_BASE + (uint32_t)slot * CAM_SLOT_SIZE;

    // image tiles: 0x000..0xDFF, thumbnail: 0xE00..0xEFF
    memcpy(buf + base + 0x0000, img, CAM_IMG_BYTES);
    memcpy(buf + base + 0x0E00, thumb, CAM_THUMB_BYTES);

    // owner metadata: 92 bytes at 0xF00..0xF5B, then its echo at 0xF5C..0xFB7
    uint8_t owner[CAM_META_BYTES];
    if (meta) {
        memcpy(owner, meta, CAM_META_BYTES);
    } else {
        memset(owner, 0, CAM_META_BYTES);
        memcpy(owner + (CAM_META_BYTES - CAM_MAGIC_LEN - 2), cam_magic, CAM_MAGIC_LEN);
        cam_checksum(owner, CAM_META_BYTES - CAM_MAGIC_LEN - 2,
                     owner + CAM_META_BYTES - 2);
    }
    memcpy(buf + base + CAM_META_OWNER_OFF, owner, CAM_META_BYTES);
    memcpy(buf + base + 0x0F5C, owner, CAM_META_BYTES);
    // camera-owner region + tail: 0xAA (factory/unused marker)
    memset(buf + base + 0x0FB8, 0xAA, 0x0FFF - 0x0FB8 + 1);

    // mark occupied: image number = slot (numbered in address order)
    buf[CAM_VECTOR_OFF + slot] = (uint8_t)slot;

    cam_checksum(buf + CAM_VECTOR_OFF, CAM_SLOT_COUNT, buf + CAM_VECTOR_CHK);
    return true;
}

bool cam_save_occupied(const uint8_t *buf, int slot)
{
    return buf[CAM_VECTOR_OFF + slot] != 0xFF;
}

void cam_image_decode(const uint8_t *img, uint8_t *out)
{
    for (int py = 0; py < CAM_IMG_H; py++) {
        int ty = py >> 3;
        int y  = py & 7;
        for (int px = 0; px < CAM_IMG_W; px++) {
            int tx = px >> 3;
            int x  = px & 7;
            const uint8_t *tile = img + (ty * 16 + tx) * 16 + y * 2;
            int b0 = (tile[0] >> (7 - x)) & 1;
            int b1 = (tile[1] >> (7 - x)) & 1;
            out[py * CAM_IMG_W + px] = (uint8_t)(b0 | (b1 << 1));
        }
    }
}
