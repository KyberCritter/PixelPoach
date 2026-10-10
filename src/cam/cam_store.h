// Photo store: a camera-format .sav held in EWRAM and persisted to the cart's
// save area. GBA-only (uses the storage backend). The cart's .sav file IS the
// store, truncated to whatever the runtime-detected backend can hold.
#ifndef CAM_CAM_STORE_H
#define CAM_CAM_STORE_H

#include <tonc.h>
#include "cam/cam_save.h"

// Call once at startup: loads (or formats) the save and reports slot capacity.
void cam_store_init(void);

// Number of photo slots the current backend can hold (0 = no writable storage).
int cam_store_capacity(void);

// Number of photos currently stored.
int cam_store_count(void);

// Photo indices 0..cam_store_count-1 map to save slots 0..count-1 in order.
bool cam_store_occupied(int slot);

// Store a photo into the first free slot. Returns the slot index or -1 on
// failure (full / no storage).
int  cam_store_add(const uint8_t *img, const uint8_t *thumb, const uint8_t *meta);

// Read a stored photo's payloads. Returns false if the slot is empty.
bool cam_store_get(int slot, uint8_t *img, uint8_t *thumb, uint8_t *meta);

// Erase a stored photo (marks its vector entry empty).
bool cam_store_erase(int slot);

// The in-memory .sav base pointer (for the image viewer).
const uint8_t *cam_store_buf(void);

#endif
