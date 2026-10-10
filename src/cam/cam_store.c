#include <string.h>
#include "cam/cam_store.h"
#include "debug/dbg.h"
#include "storage/storage.h"

// The full .sav is 128 KB; we only use/persist the first store_size() bytes.
// The last DBG_RESERVE bytes of storage belong to the debug capture.
EWRAM_BSS static u8 sav[CAM_SAVE_SIZE];

static u32 store_size(void)
{
    u32 s = storage_size();
    if (s > CAM_SAVE_SIZE)
        s = CAM_SAVE_SIZE;
    if (s > DBG_RESERVE)
        s -= DBG_RESERVE;
    return s;
}

// Vector state region: 30 slot bytes + "Magic" + checksum.
#define CAM_VECTOR_BYTES (CAM_SLOT_COUNT + CAM_MAGIC_LEN + 2)

static void persist_vector(void)
{
    storage_write(CAM_VECTOR_OFF, sav + CAM_VECTOR_OFF, CAM_VECTOR_BYTES);
}

void cam_store_init(void)
{
    storage_init();
    u32 size = store_size();
    if (size == 0) {
        cam_save_init(sav, 0);
        return;
    }

    // Load an existing save if it looks like one; otherwise format.
    storage_read(0, sav, size);
    if (memcmp(sav + CAM_VECTOR_MAGIC, cam_magic, CAM_MAGIC_LEN) != 0)
        cam_save_init(sav, size);
}

int cam_store_capacity(void)
{
    return cam_save_slot_count(store_size());
}

int cam_store_count(void)
{
    int n = 0;
    for (int s = 0; s < cam_store_capacity(); s++)
        if (cam_store_occupied(s))
            n++;
    return n;
}

bool cam_store_occupied(int slot)
{
    return cam_save_occupied(sav, slot);
}

int cam_store_add(const uint8_t *img, const uint8_t *thumb, const uint8_t *meta)
{
    int cap = cam_store_capacity();
    for (int s = 0; s < cap; s++) {
        if (cam_store_occupied(s))
            continue;
        if (!cam_save_put(sav, store_size(), s, img, thumb, meta))
            return -1;
        storage_write(CAM_SLOT_BASE + (u32)s * CAM_SLOT_SIZE,
                      sav + CAM_SLOT_BASE + (u32)s * CAM_SLOT_SIZE, CAM_SLOT_SIZE);
        persist_vector();
        return s;
    }
    return -1;
}

bool cam_store_get(int slot, uint8_t *img, uint8_t *thumb, uint8_t *meta)
{
    if (!cam_store_occupied(slot))
        return false;
    u32 base = CAM_SLOT_BASE + (u32)slot * CAM_SLOT_SIZE;
    memcpy(img,   sav + base + 0x0000, CAM_IMG_BYTES);
    memcpy(thumb, sav + base + 0x0E00, CAM_THUMB_BYTES);
    memcpy(meta,  sav + base + CAM_META_OWNER_OFF, CAM_META_BYTES);
    return true;
}

bool cam_store_erase(int slot)
{
    if (!cam_store_occupied(slot))
        return false;
    sav[CAM_VECTOR_OFF + slot] = 0xFF;
    cam_checksum(sav + CAM_VECTOR_OFF, CAM_SLOT_COUNT, sav + CAM_VECTOR_CHK);
    persist_vector();
    return true;
}

const uint8_t *cam_store_buf(void) { return sav; }
