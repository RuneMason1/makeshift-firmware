#include <mkshft_led.hpp>
#include <WS2812Serial.h>
#include <color.hpp>
#include <mkshft_core.hpp>
namespace mkshft_ledMatrix {
namespace {
struct LedChannel {
  uint8_t current = 0;
  uint8_t target = 0;
};

byte drawingMemory[PhysicalStripSz * 3] = {};
DMAMEM byte displayMemory[PhysicalStripSz * 12] = {};
WS2812Serial strip(PhysicalStripSz, displayMemory, drawingMemory, LED_PIN,
                   WS2812_GRB);
bool states[StripSz] = {};
LedChannel red[StripSz] = {};
LedChannel green[StripSz] = {};
LedChannel blue[StripSz] = {};
bool driverReady = false;
bool outputEnabled = true;
uint16_t physicalMask = 0;
uint32_t lastFadeFrameUs = 0;

uint8_t peakR = 216;
uint8_t peakG = 58;
uint8_t peakB = 4;
constexpr uint16_t bootSequenceStepDelayMs = 120;
constexpr uint8_t fadeStepUp = 18;
constexpr uint8_t fadeStepDown = 12;
// Match the established 10 ms main-loop behavior, but decouple fades from
// serial and display workload.
constexpr uint32_t fadeFramePeriodUs = 10000;

uint8_t stepToward(uint8_t current, uint8_t target, uint8_t step) {
  if (current == target) return current;
  if (current < target) {
    const uint16_t next = static_cast<uint16_t>(current) + step;
    return next >= target ? target : static_cast<uint8_t>(next);
  }

  const uint8_t delta = current - target;
  return current - (step < delta ? step : delta);
}

uint8_t linearLevelForOutput(uint8_t output) {
  for (uint16_t level = 0; level < 256; ++level) {
    if (gamma8[level] >= output) return static_cast<uint8_t>(level);
  }
  return 255;
}

bool advanceChannel(LedChannel &channel, uint8_t riseStep, uint8_t fallStep) {
  const uint8_t next =
      stepToward(channel.current, channel.target,
                 channel.current < channel.target ? riseStep : fallStep);
  const bool changed = next != channel.current;
  channel.current = next;
  return changed;
}

void clearPhysicalStrip() {
  for (uint8_t physicalIndex = 0; physicalIndex < PhysicalStripSz;
       ++physicalIndex) {
    strip.setPixel(physicalIndex, 0, 0, 0);
  }
}

void applyTargets() {
  physicalMask = 0;
  for (uint8_t logicalIndex = 0; logicalIndex < StripSz; logicalIndex++) {
    const uint8_t logicalRow = core::ButtonLookup[logicalIndex][0];
    const uint8_t logicalCol = core::ButtonLookup[logicalIndex][1];
    const uint8_t physicalIndex = MatrixLookup[logicalRow][logicalCol];
    const bool active = states[logicalIndex];
    if (active) physicalMask |= static_cast<uint16_t>(1U << physicalIndex);
    red[logicalIndex].target = active ? linearLevelForOutput(peakR) : 0;
    green[logicalIndex].target = active ? linearLevelForOutput(peakG) : 0;
    blue[logicalIndex].target = active ? linearLevelForOutput(peakB) : 0;
  }
}

void renderFrame() {
  for (uint8_t logicalIndex = 0; logicalIndex < StripSz; logicalIndex++) {
    const uint8_t logicalRow = core::ButtonLookup[logicalIndex][0];
    const uint8_t logicalCol = core::ButtonLookup[logicalIndex][1];
    const uint8_t physicalIndex = MatrixLookup[logicalRow][logicalCol];
    strip.setPixel(physicalIndex, gamma8[red[logicalIndex].current],
                   gamma8[green[logicalIndex].current],
                   gamma8[blue[logicalIndex].current]);
  }
}

void snapAllChannelsToTarget() {
  for (uint8_t logicalIndex = 0; logicalIndex < StripSz; logicalIndex++) {
    red[logicalIndex].current = red[logicalIndex].target;
    green[logicalIndex].current = green[logicalIndex].target;
    blue[logicalIndex].current = blue[logicalIndex].target;
  }
}

bool advanceFadeFrame() {
  bool changed = false;
  for (uint8_t logicalIndex = 0; logicalIndex < StripSz; logicalIndex++) {
    changed |= advanceChannel(red[logicalIndex], fadeStepUp, fadeStepDown);
    changed |= advanceChannel(green[logicalIndex], fadeStepUp, fadeStepDown);
    changed |= advanceChannel(blue[logicalIndex], fadeStepUp, fadeStepDown);
  }
  return changed;
}
}

void init() {
  driverReady = strip.begin();
#if defined(__IMXRT1062__)
  // Only touch drive strength; overwriting the entire pad register can change
  // edge timing and push WS2812 signaling out of tolerance.
  volatile uint32_t *const padRegister = portControlRegister(LED_PIN);
  *padRegister = (*padRegister & ~IOMUXC_PAD_DSE(7)) | IOMUXC_PAD_DSE(7);
#endif
  clearPhysicalStrip();
  if (driverReady) {
    strip.show();
    delay(1);
    renderFrame();
    strip.show();
  }
  lastFadeFrameUs = micros();
}

void playBootSequence(BootStepCallback callback) {
  if (!driverReady || !outputEnabled) return;

  for (uint8_t physicalIndex = 0; physicalIndex < PhysicalStripSz;
       ++physicalIndex) {
    clearPhysicalStrip();
    strip.setPixel(physicalIndex, peakR, peakG, peakB);
    strip.show();
    if (callback != nullptr) callback(physicalIndex, PhysicalStripSz);
    delay(bootSequenceStepDelayMs);
  }

  clearPhysicalStrip();
  strip.show();
  if (callback != nullptr) callback(PhysicalStripSz, PhysicalStripSz);
  delay(bootSequenceStepDelayMs);
}

int8_t setButtonState(uint8_t buttonIndex, bool pressed) {
  if (buttonIndex >= StripSz) {
    return -1;
  }
  states[buttonIndex] = pressed;
  const uint8_t row = core::ButtonLookup[buttonIndex][0];
  const uint8_t col = core::ButtonLookup[buttonIndex][1];
  const uint8_t changedPhysicalIndex = MatrixLookup[row][col];
  applyTargets();
  return changedPhysicalIndex;
}

bool isReady() { return driverReady; }

uint16_t activePhysicalMask() { return physicalMask; }

uint32_t outputPadConfig() {
#if defined(__IMXRT1062__)
  return *portControlRegister(LED_PIN);
#else
  return 0;
#endif
}

void setEnabled(bool enabled) {
  if (outputEnabled == enabled) return;

  outputEnabled = enabled;
  physicalMask = 0;
  for (bool &state : states) state = false;
  applyTargets();
  snapAllChannelsToTarget();
  clearPhysicalStrip();
  if (driverReady) strip.show();
  lastFadeFrameUs = micros();
}

bool isEnabled() { return outputEnabled; }

void setBaseColor(uint8_t r, uint8_t g, uint8_t b) {
  peakR = r;
  peakG = g;
  peakB = b;
  applyTargets();
}

void update() {
  if (!driverReady || !outputEnabled) return;
  const uint32_t now = micros();
  const uint32_t elapsed = now - lastFadeFrameUs;
  if (elapsed < fadeFramePeriodUs) return;
  // Do not burst through missed frames after a long display or serial task.
  lastFadeFrameUs = elapsed >= fadeFramePeriodUs * 4
                        ? now
                        : lastFadeFrameUs + fadeFramePeriodUs;
  if (!advanceFadeFrame()) return;
  renderFrame();
  strip.show();
}

void post() {
  // The production LED path uses fixed arrays. Heap-backed Pixel sequences
  // remain available only in explicitly enabled legacy builds.
}

void colorStripPixel(uint8_t row, uint8_t col, uint8_t r, uint8_t g,
                     uint8_t b) {
  if (row >= RowSz || col >= ColSz) return;
  if (!outputEnabled) return;
  strip.setPixel(MatrixLookup[row][col], r, g, b);
  if (driverReady) strip.show();
}

void colorStripPixel(uint8_t row, uint8_t col, Color color) {
  colorStripPixel(row, col, color.r, color.g, color.b);
}

} // namespace mkshft_ledMatrix
