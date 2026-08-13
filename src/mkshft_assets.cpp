#include <mkshft_assets.hpp>

namespace mkshft_assets {
namespace {
Asset assets[MAX_ASSETS] = {};
Asset pending = {};
uint16_t received = 0;
bool transferActive = false;

int8_t findSlot(uint8_t id) {
  for (uint8_t index = 0; index < MAX_ASSETS; ++index) {
    if (assets[index].valid && assets[index].id == id) return index;
  }
  for (uint8_t index = 0; index < MAX_ASSETS; ++index) {
    if (!assets[index].valid) return index;
  }
  return -1;
}
} // namespace

bool beginAsset(uint8_t id, Format format, uint8_t width, uint8_t height,
                uint16_t length) {
  cancelTransfer();
  const uint16_t expectedLength =
      static_cast<uint16_t>((static_cast<uint16_t>(width) * height + 7) / 8);
  if (id == 0 || format != Format::MONO_1BPP || width == 0 || height == 0 ||
      width > 32 || height > 32 || length == 0 || length > MAX_ASSET_BYTES ||
      length != expectedLength)
    return false;
  pending = {id, format, width, height, length, {}, false};
  received = 0;
  transferActive = true;
  return true;
}

bool writeChunk(uint16_t offset, const uint8_t *data, size_t length) {
  if (!transferActive || data == nullptr || offset != received || length == 0 ||
      offset + length > pending.length)
    return false;
  memcpy(pending.data + offset, data, length);
  received += length;
  return true;
}

bool commitAsset() {
  if (!transferActive || received != pending.length) return false;
  const int8_t slot = findSlot(pending.id);
  if (slot < 0) return false;
  pending.valid = true;
  assets[slot] = pending;
  cancelTransfer();
  return true;
}

void cancelTransfer() {
  pending = {};
  received = 0;
  transferActive = false;
}

const Asset *find(uint8_t id) {
  for (uint8_t index = 0; index < MAX_ASSETS; ++index) {
    if (assets[index].valid && assets[index].id == id) return &assets[index];
  }
  return nullptr;
}

} // namespace mkshft_assets
