#include <mkshft_led.hpp>
#include <WS2812Serial.h>
#include <color.hpp>
#include <mkshft_core.hpp>
namespace mkshft_ledMatrix {
namespace {
byte drawingMemory[StripSz * 3] = {};
DMAMEM byte displayMemory[StripSz * 12] = {};
WS2812Serial strip(StripSz, displayMemory, drawingMemory, LED_PIN, WS2812_GRB);
bool states[StripSz] = {};
bool driverReady = false;
bool outputEnabled = true;
uint16_t physicalMask = 0;

constexpr uint8_t peakR = 216;
constexpr uint8_t peakG = 58;
constexpr uint8_t peakB = 4;

void renderStates() {
  physicalMask = 0;
  for (uint8_t logicalIndex = 0; logicalIndex < StripSz; logicalIndex++) {
    const uint8_t logicalRow = core::ButtonLookup[logicalIndex][0];
    const uint8_t logicalCol = core::ButtonLookup[logicalIndex][1];
    const uint8_t physicalIndex = MatrixLookup[logicalRow][logicalCol];
    const bool active = states[logicalIndex];
    if (active) physicalMask |= static_cast<uint16_t>(1U << physicalIndex);
    strip.setPixel(physicalIndex, active ? peakR : 0, active ? peakG : 0,
                   active ? peakB : 0);
  }
  if (driverReady && outputEnabled) strip.show();
}
}

void init() {
  driverReady = strip.begin();
#if defined(__IMXRT1062__)
  // Preserve the UART mux while matching the original GPIO drive strength.
  *portControlRegister(LED_PIN) = IOMUXC_PAD_DSE(7);
#endif
  strip.clear();
  if (driverReady) {
    strip.show();
    delay(1);
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
  renderStates();
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
  strip.clear();
  if (driverReady) strip.show();
}

bool isEnabled() { return outputEnabled; }

void update() {}

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
    for (int n = 0; n < StripSz; n++) {
      strip.setPixelColor(n, color.r, color.g, color.b);
    }
    strip.show();
    delay(1);
  }
  for (uint8_t i = 125; i != 255; i--) {
    const Color color = {gamma8[i], 0, gamma8[i]};
    for (int n = 0; n < StripSz; n++) {
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

} // namespace mkshft_ledMatrix
