#include <mkshft_runtime.hpp>

namespace mkshft_runtime {
namespace {
Component activeComponents[MAX_COMPONENTS] = {};
Component pendingComponents[MAX_COMPONENTS] = {};
uint8_t activeCount = 0;
uint8_t pendingCount = 0;
uint8_t expectedCount = 0;
bool transactionActive = false;
uint32_t manifestLastActivityMs = 0;
constexpr uint32_t MANIFEST_TIMEOUT_MS = 5000;

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
  manifestLastActivityMs = millis();
  return true;
}

bool addComponent(const Component &component) {
  if (!transactionActive || pendingCount >= expectedCount ||
      component.id == 0 || !validType(component.type) ||
      !validZone(component.zone) ||
      (component.flags & ~(ENABLED | PRELOAD)) != 0 ||
      ((component.flags & PRELOAD) != 0 &&
       (component.flags & ENABLED) == 0))
    return false;

  for (uint8_t index = 0; index < pendingCount; ++index) {
    if (pendingComponents[index].id == component.id) return false;
    if ((pendingComponents[index].flags & ENABLED) != 0 &&
        (component.flags & ENABLED) != 0 &&
        pendingComponents[index].zone == component.zone)
      return false;
  }
  pendingComponents[pendingCount++] = component;
  manifestLastActivityMs = millis();
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
  manifestLastActivityMs = 0;
}

void updateManifestTimeout() {
  if (transactionActive && manifestLastActivityMs != 0 &&
      static_cast<uint32_t>(millis() - manifestLastActivityMs) >=
          MANIFEST_TIMEOUT_MS) {
    cancelManifest();
  }
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
