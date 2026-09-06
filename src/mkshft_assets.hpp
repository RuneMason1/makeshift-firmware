#ifndef MKSHFT_ASSETS_H_
#define MKSHFT_ASSETS_H_

#include <Arduino.h>

namespace mkshft_assets {

constexpr uint8_t MAX_ASSETS = 8;
// Ctrl supplies visual assets for the active cue set at connect time. Keep
// storage bounded while allowing smooth alpha glyphs without firmware art.
constexpr uint8_t MAX_MONO_DIMENSION = 96;
constexpr uint8_t MAX_ALPHA_DIMENSION = 64;
constexpr uint16_t MAX_ASSET_BYTES =
    (MAX_ALPHA_DIMENSION * MAX_ALPHA_DIMENSION) / 2;

enum class Format : uint8_t {
  MONO_1BPP = 1,
  // Bounded command stream; validated before it can reach the renderer.
  VECTOR_COMMANDS = 2,
  // Four-bit foreground opacity, packed high-nibble first.
  ALPHA_4BPP = 3,
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
void updateTransferTimeout();
const Asset *find(uint8_t id);

} // namespace mkshft_assets

#endif
