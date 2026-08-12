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

constexpr uint8_t peakR = 216;
constexpr uint8_t peakG = 58;
constexpr uint8_t peakB = 4;
constexpr uint16_t bootSequenceStepDelayMs = 120;
constexpr uint8_t fadeStepUp = 18;
constexpr uint8_t fadeStepDown = 12;

uint8_t stepToward(uint8_t current, uint8_t target, uint8_t step) {
  if (current == target) return current;
  if (current < target) {
    const uint16_t next = static_cast<uint16_t>(current) + step;
    return next >= target ? target : static_cast<uint8_t>(next);
  }

  const uint8_t delta = current - target;
  return current - (step < delta ? step : delta);
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

void runBootSequence() {
  if (!driverReady || !outputEnabled) return;

  // Sweep raw physical LED indices to separate strip propagation problems
  // from the current 16-button logical mapping.
  for (uint8_t physicalIndex = 0; physicalIndex < PhysicalStripSz;
       ++physicalIndex) {
    clearPhysicalStrip();
    strip.setPixel(physicalIndex, peakR, peakG, peakB);
    strip.show();
    delay(bootSequenceStepDelayMs);
  }

  clearPhysicalStrip();
  strip.show();
  delay(bootSequenceStepDelayMs);
}

void applyTargets() {
  physicalMask = 0;
  for (uint8_t logicalIndex = 0; logicalIndex < StripSz; logicalIndex++) {
    const uint8_t logicalRow = core::ButtonLookup[logicalIndex][0];
    const uint8_t logicalCol = core::ButtonLookup[logicalIndex][1];
    const uint8_t physicalIndex = MatrixLookup[logicalRow][logicalCol];
    const bool active = states[logicalIndex];
    if (active) physicalMask |= static_cast<uint16_t>(1U << physicalIndex);
    red[logicalIndex].target = active ? peakR : 0;
    green[logicalIndex].target = active ? peakG : 0;
    blue[logicalIndex].target = active ? peakB : 0;
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
    runBootSequence();
    renderFrame();
    strip.show();
  }
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
}

bool isEnabled() { return outputEnabled; }

void update() {
  if (!driverReady || !outputEnabled) return;
  if (!advanceFadeFrame()) return;
  renderFrame();
  strip.show();
}

void post() {
  if (!driverReady || !outputEnabled) return;

  // TODO: finish testing
  Serial.println("Testing color deltas");
  Color test1 = {0, 0, 0};
  Color test2 = {255, 255, 255};
  Serial.print("t1: ");
  printColor(test1);
  Serial.print(" | t2: ");
  printColor(test2);
  ColorDelta delta12 = test1 - test2;
  ColorDelta delta21 = test2 - test1;

  Serial.print(" | d12: ");
  printColor(delta12);
  Serial.print(" | d21: ");

  printColor(delta21);
  Serial.println();

  // color indicators
  for (uint8_t i = 0; i != 125; i++) {
    const Color color = {gamma8[i], 0, gamma8[i]};
    for (int n = 0; n < PhysicalStripSz; n++) {
      strip.setPixelColor(n, color.r, color.g, color.b);
    }
    strip.show();
    delay(1);
  }
  for (uint8_t i = 125; i != 255; i--) {
    const Color color = {gamma8[i], 0, gamma8[i]};
    for (int n = 0; n < PhysicalStripSz; n++) {
      strip.setPixelColor(n, color.r, color.g, color.b);
    }
    strip.show();
    delay(10);
  }
  Serial.println("end of POST");
  delay(1000);
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
