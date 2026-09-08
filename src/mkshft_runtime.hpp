#ifndef MKSHFT_RUNTIME_H_
#define MKSHFT_RUNTIME_H_

#include <Arduino.h>

namespace mkshft_runtime {

constexpr uint8_t PROTOCOL_VERSION = 1;
constexpr uint8_t MAX_COMPONENTS = 8;

enum class ComponentType : uint8_t {
  NONE = 0,
  CAROUSEL = 1,
  METER_BANK = 2,
  TRANSPORT_BAR = 3,
  LOGO_PANEL = 4,
  BUTTON_GRID = 5,
  LIST_PICKER = 6,
  STATUS_CARD = 7,
};

enum class Zone : uint8_t {
  SPECIAL = 0,
  LOWER_LEFT = 1,
  LOWER_RIGHT = 2,
  UPPER = 3,
  CENTER = 4,
  FULL_SCREEN = SPECIAL,
  LEFT = LOWER_LEFT,
  TOP_BAR = UPPER,
  OVERLAY = CENTER,
};

enum ComponentFlags : uint8_t {
  ENABLED = 1U << 0,
  PRELOAD = 1U << 1,
};

struct Component {
  uint8_t id;
  ComponentType type;
  Zone zone;
  uint8_t flags;
};

void initCompatibilityDefaults();
bool beginManifest(uint8_t version, uint8_t expectedCount);
bool addComponent(const Component &component);
bool commitManifest();
void cancelManifest();
void updateManifestTimeout();

bool isEnabled(ComponentType type);
bool shouldPreload(ComponentType type);
uint8_t activeComponentCount();

} // namespace mkshft_runtime

#endif
