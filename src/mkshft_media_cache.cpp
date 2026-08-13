#include <mkshft_media_cache.hpp>

namespace mkshft_media_cache {

DMAMEM uint16_t cachePixels[mkshft_ui::GAME_ART_CACHE_SLOTS]
                           [mkshft_ui::GAME_ART_MAX_WIDTH *
                            mkshft_ui::GAME_ART_MAX_HEIGHT];
SlotInfo slots[mkshft_ui::GAME_ART_CACHE_SLOTS] = {};
uint16_t transferWidth = 0;
uint16_t transferHeight = 0;
uint32_t pixelsReceived = 0;
uint8_t transferItemIndex = 0;
uint8_t transferSlot = 0;
bool transferActive = false;
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
      width > 0 && height > 0 && width <= mkshft_ui::GAME_ART_MAX_WIDTH &&
      height <= mkshft_ui::GAME_ART_MAX_HEIGHT;
  if (slot >= mkshft_ui::GAME_ART_CACHE_SLOTS ||
      (!textOnly && !validArtworkSize)) {
    transferActive = false;
    return false;
  }

  transferWidth = width;
  transferHeight = height;
  pixelsReceived = 0;
  transferItemIndex = itemIndex;
  transferSlot = slot;
  slots[slot].valid = false;
  transferActive = !textOnly;
  lastTransferActivityMs = transferActive ? millis() : 0;
  return true;
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
    cachePixels[transferSlot][pixelOffset + index] =
        (static_cast<uint16_t>(data[index * 2]) << 8) | data[index * 2 + 1];
  }
  pixelsReceived += pixelCount;
  lastTransferActivityMs = millis();
  return true;
}

bool commitWrite() {
  if (transferExpired()) cancelWrite();
  const uint32_t expectedPixels = transferWidth * transferHeight;
  if (!transferActive || pixelsReceived != expectedPixels) {
    return false;
  }

  transferActive = false;
  lastTransferActivityMs = 0;
  slots[transferSlot] = {transferItemIndex, transferWidth, transferHeight, true};
  return true;
}

void cancelWrite() {
  transferActive = false;
  transferWidth = 0;
  transferHeight = 0;
  pixelsReceived = 0;
  lastTransferActivityMs = 0;
}

void updateTransferTimeout() {
  if (transferExpired()) cancelWrite();
}

void invalidateAll() {
  cancelWrite();
  for (auto &slot : slots) slot.valid = false;
}

bool isWriteActive() { return transferActive; }

int8_t findSlot(uint8_t itemIndex) {
  for (uint8_t slot = 0; slot < mkshft_ui::GAME_ART_CACHE_SLOTS; ++slot) {
    if (slots[slot].valid && slots[slot].itemIndex == itemIndex) return slot;
  }
  return -1;
}

const SlotInfo *slotInfo(uint8_t slot) {
  if (slot >= mkshft_ui::GAME_ART_CACHE_SLOTS || !slots[slot].valid) return nullptr;
  return &slots[slot];
}

const uint16_t *slotPixels(uint8_t slot) {
  if (slot >= mkshft_ui::GAME_ART_CACHE_SLOTS || !slots[slot].valid) return nullptr;
  return cachePixels[slot];
}

} // namespace mkshft_media_cache
