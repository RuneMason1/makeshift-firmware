#ifndef MKSHFT_ASSETS_H_
#define MKSHFT_ASSETS_H_

#include <Arduino.h>

namespace mkshft_assets {

constexpr uint8_t MAX_ASSETS = 8;
constexpr uint16_t MAX_ASSET_BYTES = 128;

enum class Format : uint8_t {
  MONO_1BPP = 1,
};

struct Asset {
  uint8_t id;
  Format format;
  uint8_t width;
  uint8_t height;
  uint16_t length;
  uint8_t data[MAX_ASSET_BYTES];
  bool valid;
};

bool beginAsset(uint8_t id, Format format, uint8_t width, uint8_t height,
                uint16_t length);
bool writeChunk(uint16_t offset, const uint8_t *data, size_t length);
bool commitAsset();
void cancelTransfer();
const Asset *find(uint8_t id);

} // namespace mkshft_assets

#endif
