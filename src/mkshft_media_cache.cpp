#include <mkshft_media_cache.hpp>

namespace mkshft_media_cache {

DMAMEM uint16_t cachePixels[mkshft_ui::MEDIA_CACHE_SLOTS]
                           [mkshft_ui::COLLECTION_ART_MAX_WIDTH *
                            mkshft_ui::COLLECTION_ART_MAX_HEIGHT];
// Cache replacement is staged outside the DMA-backed display cache. This
// preserves the currently committed image through every chunk and timeout;
// only a complete commit replaces a slot.
uint16_t stagedPixels[mkshft_ui::COLLECTION_ART_MAX_WIDTH *
                      mkshft_ui::COLLECTION_ART_MAX_HEIGHT];
SlotInfo slots[mkshft_ui::MEDIA_CACHE_SLOTS] = {};
uint32_t slotLastUsed[mkshft_ui::MEDIA_CACHE_SLOTS] = {};
uint16_t transferWidth = 0;
uint16_t transferHeight = 0;
uint32_t pixelsReceived = 0;
uint8_t transferItemIndex = 0;
uint8_t transferSlot = 0;
uint32_t transferKey = 0;
bool transferActive = false;
uint32_t evictedKey = 0;
uint32_t lastTransferActivityMs = 0;
constexpr uint32_t TRANSFER_TIMEOUT_MS = 5000;

bool transferExpired() {
  return transferActive &&
         static_cast<uint32_t>(millis() - lastTransferActivityMs) >=
             TRANSFER_TIMEOUT_MS;
}

bool beginWrite(uint8_t slot, uint8_t itemIndex, uint16_t width,
                uint16_t height) {
  const bool textOnly = width == 0 && height == 0;
  const bool validArtworkSize =
      width > 0 && height > 0 &&
      width <= mkshft_ui::COLLECTION_ART_MAX_WIDTH &&
      height <= mkshft_ui::COLLECTION_ART_MAX_HEIGHT;
  if (slot >= mkshft_ui::MEDIA_CACHE_SLOTS ||
      (!textOnly && !validArtworkSize)) {
    transferActive = false;
    return false;
  }

  transferWidth = width;
  transferHeight = height;
  pixelsReceived = 0;
  transferItemIndex = itemIndex;
  transferKey = 0;
  transferSlot = slot;
  transferActive = !textOnly;
  lastTransferActivityMs = transferActive ? millis() : 0;
  return true;
}

int8_t findKey(uint32_t key) {
  if (key == 0) return -1;
  for (uint8_t slot = 0; slot < mkshft_ui::MEDIA_CACHE_SLOTS; ++slot) {
    if (slots[slot].valid && slots[slot].key == key) return slot;
  }
  return -1;
}

bool bindKeyToItem(uint32_t key, uint8_t itemIndex) {
  const int8_t slot = findKey(key);
  if (slot < 0) return false;
  for (auto &entry : slots) {
    if (entry.itemIndex == itemIndex) entry.itemIndex = UINT8_MAX;
  }
  slots[slot].itemIndex = itemIndex;
  slotLastUsed[slot] = millis();
  return true;
}

bool beginKeyWrite(uint32_t key, uint16_t width, uint16_t height) {
  if (key == 0 || width == 0 || height == 0 ||
      width > mkshft_ui::COLLECTION_ART_MAX_WIDTH ||
      height > mkshft_ui::COLLECTION_ART_MAX_HEIGHT) return false;

  evictedKey = 0;
  int8_t selected = findKey(key);
  if (selected < 0) {
    for (uint8_t slot = 0; slot < mkshft_ui::MEDIA_CACHE_SLOTS; ++slot) {
      if (!slots[slot].valid) {
        selected = slot;
        break;
      }
    }
  }
  if (selected < 0) {
    selected = 0;
    for (uint8_t slot = 1; slot < mkshft_ui::MEDIA_CACHE_SLOTS; ++slot) {
      if (slotLastUsed[slot] < slotLastUsed[selected]) selected = slot;
    }
  }

  // Uploading a file does not bind it to a collection item.
  if (!beginWrite(static_cast<uint8_t>(selected), UINT8_MAX, width, height)) return false;
  transferKey = key;
  return true;
}

uint32_t takeEvictedKey() {
  const uint32_t key = evictedKey;
  evictedKey = 0;
  return key;
}

bool writeChunk(uint32_t pixelOffset, const uint8_t *data, size_t dataLength) {
  if (transferExpired()) cancelWrite();
  if (!transferActive || data == nullptr || dataLength == 0 ||
      (dataLength % 2) != 0 || pixelOffset != pixelsReceived) {
    return false;
  }

  const uint32_t pixelCount = dataLength / 2;
  const uint32_t expectedPixels = transferWidth * transferHeight;
  if (pixelOffset + pixelCount > expectedPixels) {
    transferActive = false;
    return false;
  }

  for (uint32_t index = 0; index < pixelCount; ++index) {
    stagedPixels[pixelOffset + index] =
        (static_cast<uint16_t>(data[index * 2]) << 8) | data[index * 2 + 1];
  }
  pixelsReceived += pixelCount;
  lastTransferActivityMs = millis();
  return true;
}

bool writeByteChunk(uint32_t byteOffset, const uint8_t *data,
                    size_t dataLength) {
  if ((byteOffset % sizeof(uint16_t)) != 0 ||
      (dataLength % sizeof(uint16_t)) != 0) return false;
  return writeChunk(byteOffset / sizeof(uint16_t), data, dataLength);
}

bool commitWrite() {
  if (transferExpired()) cancelWrite();
  const uint32_t expectedPixels = transferWidth * transferHeight;
  if (!transferActive || pixelsReceived != expectedPixels) {
    return false;
  }

  transferActive = false;
  lastTransferActivityMs = 0;
  if (transferKey != 0 && slots[transferSlot].valid &&
      slots[transferSlot].key != transferKey) {
    evictedKey = slots[transferSlot].key;
  }
  memcpy(cachePixels[transferSlot], stagedPixels,
         expectedPixels * sizeof(uint16_t));
  slots[transferSlot] = {transferKey, transferItemIndex, transferWidth,
                         transferHeight, true};
  slotLastUsed[transferSlot] = millis();
  return true;
}

bool commitKeyWrite() {
  if (transferKey == 0) return false;
  return commitWrite();
}

void cancelWrite() {
  transferActive = false;
  transferWidth = 0;
  transferHeight = 0;
  pixelsReceived = 0;
  transferKey = 0;
  lastTransferActivityMs = 0;
}

void updateTransferTimeout() {
  if (transferExpired()) cancelWrite();
}

void invalidateBindings() {
  cancelWrite();
  for (auto &slot : slots) slot.itemIndex = UINT8_MAX;
}

void invalidateAll() {
  cancelWrite();
  for (uint8_t slot = 0; slot < mkshft_ui::MEDIA_CACHE_SLOTS; ++slot) {
    slots[slot].valid = false;
    slotLastUsed[slot] = 0;
  }
}

bool isWriteActive() { return transferActive; }

int8_t findSlot(uint8_t itemIndex) {
  for (uint8_t slot = 0; slot < mkshft_ui::MEDIA_CACHE_SLOTS; ++slot) {
    if (slots[slot].valid && slots[slot].itemIndex == itemIndex) return slot;
  }
  return -1;
}

const SlotInfo *slotInfo(uint8_t slot) {
  if (slot >= mkshft_ui::MEDIA_CACHE_SLOTS || !slots[slot].valid) return nullptr;
  return &slots[slot];
}

const uint16_t *slotPixels(uint8_t slot) {
  if (slot >= mkshft_ui::MEDIA_CACHE_SLOTS || !slots[slot].valid) return nullptr;
  slotLastUsed[slot] = millis();
  return cachePixels[slot];
}

} // namespace mkshft_media_cache
