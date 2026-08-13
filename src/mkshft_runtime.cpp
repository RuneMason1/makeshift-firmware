#include <mkshft_runtime.hpp>

namespace mkshft_runtime {
namespace {
Component activeComponents[MAX_COMPONENTS] = {};
Component pendingComponents[MAX_COMPONENTS] = {};
uint8_t activeCount = 0;
uint8_t pendingCount = 0;
uint8_t expectedCount = 0;
bool transactionActive = false;

bool validType(ComponentType type) {
  return type > ComponentType::NONE && type <= ComponentType::STATUS_CARD;
}

bool validZone(Zone zone) { return zone <= Zone::OVERLAY; }
} // namespace

void initCompatibilityDefaults() {
  activeComponents[0] = {1, ComponentType::CAROUSEL, Zone::FULL_SCREEN,
                         static_cast<uint8_t>(ENABLED | PRELOAD)};
  activeCount = 1;
  cancelManifest();
}

bool beginManifest(uint8_t version, uint8_t count) {
  if (version != PROTOCOL_VERSION || count > MAX_COMPONENTS) return false;
  pendingCount = 0;
  expectedCount = count;
  transactionActive = true;
  return true;
}

bool addComponent(const Component &component) {
  if (!transactionActive || pendingCount >= expectedCount ||
      !validType(component.type) || !validZone(component.zone))
    return false;

  for (uint8_t index = 0; index < pendingCount; ++index) {
    if (pendingComponents[index].id == component.id) return false;
  }
  pendingComponents[pendingCount++] = component;
  return true;
}

bool commitManifest() {
  if (!transactionActive || pendingCount != expectedCount) return false;
  for (uint8_t index = 0; index < pendingCount; ++index)
    activeComponents[index] = pendingComponents[index];
  activeCount = pendingCount;
  cancelManifest();
  return true;
}

void cancelManifest() {
  pendingCount = 0;
  expectedCount = 0;
  transactionActive = false;
}

bool isEnabled(ComponentType type) {
  for (uint8_t index = 0; index < activeCount; ++index) {
    if (activeComponents[index].type == type &&
        (activeComponents[index].flags & ENABLED) != 0)
      return true;
  }
  return false;
}

bool shouldPreload(ComponentType type) {
  for (uint8_t index = 0; index < activeCount; ++index) {
    if (activeComponents[index].type == type &&
        (activeComponents[index].flags & (ENABLED | PRELOAD)) ==
            (ENABLED | PRELOAD))
      return true;
  }
  return false;
}

uint8_t activeComponentCount() { return activeCount; }

} // namespace mkshft_runtime
