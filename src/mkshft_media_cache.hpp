#ifndef MKSHFT_MEDIA_CACHE_H_
#define MKSHFT_MEDIA_CACHE_H_

#include <Arduino.h>

#include <mkshft_ui.hpp>

namespace mkshft_media_cache {

struct SlotInfo {
  uint8_t itemIndex;
  uint16_t width;
  uint16_t height;
  bool valid;
};

bool beginWrite(uint8_t slot, uint8_t itemIndex, uint16_t width,
                uint16_t height);
bool writeChunk(uint32_t pixelOffset, const uint8_t *data, size_t dataLength);
bool commitWrite();
void invalidateAll();
bool isWriteActive();
int8_t findSlot(uint8_t itemIndex);
const SlotInfo *slotInfo(uint8_t slot);
const uint16_t *slotPixels(uint8_t slot);

} // namespace mkshft_media_cache

#endif
