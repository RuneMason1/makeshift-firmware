#ifndef MKSHFT_MEDIA_CACHE_H_
#define MKSHFT_MEDIA_CACHE_H_

#include <Arduino.h>

#include <mkshft_ui.hpp>

namespace mkshft_media_cache {

struct SlotInfo {
  uint32_t key;
  uint8_t itemIndex;
  uint16_t width;
  uint16_t height;
  bool valid;
};

// Host-visible cache capacity. The backing allocation remains fixed and
// bounded; callers address assets by key rather than by a carousel slot.
constexpr size_t CACHE_CAPACITY_BYTES =
    mkshft_ui::MEDIA_CACHE_SLOTS * mkshft_ui::COLLECTION_ART_MAX_WIDTH *
    mkshft_ui::COLLECTION_ART_MAX_HEIGHT * sizeof(uint16_t);

bool beginKeyWrite(uint32_t key, uint16_t width, uint16_t height);
uint32_t takeEvictedKey();
bool writeByteChunk(uint32_t byteOffset, const uint8_t *data,
                    size_t dataLength);
bool commitKeyWrite();
int8_t findKey(uint32_t key);
bool bindKeyToItem(uint32_t key, uint8_t itemIndex);

bool beginWrite(uint8_t slot, uint8_t itemIndex, uint16_t width,
                uint16_t height);
bool writeChunk(uint32_t pixelOffset, const uint8_t *data, size_t dataLength);
bool commitWrite();
void cancelWrite();
void updateTransferTimeout();
// Clears carousel-item associations while retaining keyed assets for another
// collection or a later bind.
void invalidateBindings();
void invalidateAll();
bool isWriteActive();
int8_t findSlot(uint8_t itemIndex);
const SlotInfo *slotInfo(uint8_t slot);
const uint16_t *slotPixels(uint8_t slot);

} // namespace mkshft_media_cache

#endif
