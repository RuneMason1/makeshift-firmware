#include <mkshft_assets.hpp>

namespace mkshft_assets {
namespace {
Asset assets[MAX_ASSETS] = {};
Asset pending = {};
uint16_t received = 0;
bool transferActive = false;
uint32_t lastTransferActivityMs = 0;
constexpr uint32_t TRANSFER_TIMEOUT_MS = 5000;
constexpr uint8_t MAX_VECTOR_COMMANDS = 24;
constexpr uint16_t MAX_VECTOR_DRAW_WORK = 2500;

bool transferExpired() {
  return transferActive &&
         static_cast<uint32_t>(millis() - lastTransferActivityMs) >=
             TRANSFER_TIMEOUT_MS;
}

int8_t findSlot(uint8_t id) {
  for (uint8_t index = 0; index < MAX_ASSETS; ++index) {
    if (assets[index].valid && assets[index].id == id) return index;
  }
  for (uint8_t index = 0; index < MAX_ASSETS; ++index) {
    if (!assets[index].valid) return index;
  }
  return -1;
}

bool validVector(const Asset &asset) {
  uint16_t cursor = 0;
  uint8_t commands = 0;
  uint32_t drawWork = 0;
  while (cursor < asset.length) {
    const uint8_t operation = asset.data[cursor++];
    if (operation == 0) return cursor == asset.length;
    if (++commands > MAX_VECTOR_COMMANDS) return false;

    const uint8_t *p = asset.data + cursor;
    if (operation == 1) { // Filled rectangle: x, y, width, height.
      if (cursor + 4 > asset.length || p[2] == 0 || p[3] == 0 ||
          static_cast<uint16_t>(p[0]) + p[2] > asset.width ||
          static_cast<uint16_t>(p[1]) + p[3] > asset.height)
        return false;
      drawWork += static_cast<uint32_t>(p[2]) * p[3];
      cursor += 4;
    } else if (operation == 2) { // Filled triangle: x1, y1, x2, y2, x3, y3.
      if (cursor + 6 > asset.length || p[0] >= asset.width ||
          p[1] >= asset.height || p[2] >= asset.width ||
          p[3] >= asset.height || p[4] >= asset.width ||
          p[5] >= asset.height)
        return false;
      const uint8_t minX = min(p[0], min(p[2], p[4]));
      const uint8_t maxX = max(p[0], max(p[2], p[4]));
      const uint8_t minY = min(p[1], min(p[3], p[5]));
      const uint8_t maxY = max(p[1], max(p[3], p[5]));
      drawWork += static_cast<uint32_t>(maxX - minX + 1) *
                  (maxY - minY + 1) / 2;
      cursor += 6;
    } else if (operation == 3) { // Thick line: x1, y1, x2, y2, width.
      if (cursor + 5 > asset.length || p[0] >= asset.width ||
          p[1] >= asset.height || p[2] >= asset.width ||
          p[3] >= asset.height || p[4] == 0 || p[4] > 6)
        return false;
      drawWork += (max(abs(static_cast<int>(p[2]) - p[0]),
                       abs(static_cast<int>(p[3]) - p[1])) + 1) * p[4] * 2;
      cursor += 5;
    } else if (operation == 4) { // Arc: center-x, center-y, radius, start/2, end/2, width.
      if (cursor + 6 > asset.length || p[2] == 0 || p[5] == 0 || p[5] > 6)
        return false;
      const int halfWidth = p[5] / 2;
      if (p[0] < p[2] + halfWidth || p[1] < p[2] + halfWidth ||
          static_cast<uint16_t>(p[0]) + p[2] + halfWidth >= asset.width ||
          static_cast<uint16_t>(p[1]) + p[2] + halfWidth >= asset.height)
        return false;
      int start = static_cast<int8_t>(p[3]) * 2;
      int end = static_cast<int8_t>(p[4]) * 2;
      if (end < start) end += 360;
      drawWork += static_cast<uint32_t>((end - start) / 2 + 1) * p[5] * 2;
      cursor += 6;
    } else {
      return false;
    }
    if (drawWork > MAX_VECTOR_DRAW_WORK) return false;
  }
  return false; // Every vector asset must terminate explicitly.
}
} // namespace

bool beginAsset(uint8_t id, Format format, uint8_t width, uint8_t height,
                uint16_t length) {
  cancelTransfer();
  const bool mono = format == Format::MONO_1BPP;
  const bool vector = format == Format::VECTOR_COMMANDS;
  const bool alpha = format == Format::ALPHA_4BPP;
  const uint16_t monoLength = static_cast<uint16_t>(
      (static_cast<uint16_t>(width) * height + 7) / 8);
  const uint16_t alphaLength = static_cast<uint16_t>(
      (static_cast<uint16_t>(width) * height + 1) / 2);
  if (id == 0 || (!mono && !vector && !alpha) || width == 0 || height == 0 ||
      width > (mono ? MAX_MONO_DIMENSION : 128) ||
      height > (mono ? MAX_MONO_DIMENSION : 128) ||
      (alpha && (width > MAX_ALPHA_DIMENSION || height > MAX_ALPHA_DIMENSION)) ||
      length == 0 || length > MAX_ASSET_BYTES ||
      (mono && length != monoLength) || (alpha && length != alphaLength))
    return false;
  pending = {id, format, width, height, length, {}, false};
  received = 0;
  transferActive = true;
  lastTransferActivityMs = millis();
  return true;
}

bool writeChunk(uint16_t offset, const uint8_t *data, size_t length) {
  if (transferExpired()) cancelTransfer();
  if (!transferActive || data == nullptr || offset != received || length == 0 ||
      offset + length > pending.length)
    return false;
  memcpy(pending.data + offset, data, length);
  received += length;
  lastTransferActivityMs = millis();
  return true;
}

bool commitAsset() {
  if (transferExpired()) cancelTransfer();
  if (!transferActive || received != pending.length) return false;
  if (pending.format == Format::VECTOR_COMMANDS && !validVector(pending))
    return false;
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
  lastTransferActivityMs = 0;
}

void updateTransferTimeout() {
  if (transferExpired()) cancelTransfer();
}

const Asset *find(uint8_t id) {
  for (uint8_t index = 0; index < MAX_ASSETS; ++index) {
    if (assets[index].valid && assets[index].id == id) return &assets[index];
  }
  return nullptr;
}

} // namespace mkshft_assets
