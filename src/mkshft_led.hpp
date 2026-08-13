#ifndef LED_H_
#define LED_H_

#include <color.hpp>

namespace mkshft_ledMatrix {

using BootStepCallback = void (*)(uint8_t stepIndex, uint8_t stepCount);

const uint8_t LED_PIN = 8;
const uint8_t RowSz = 4;
const uint8_t ColSz = 4;
const uint8_t StripSz = RowSz * ColSz;
const uint8_t PhysicalStripSz = 24;

// Lookup table for strip number given row + col
const uint8_t MatrixLookup[RowSz][ColSz] = {
    {3, 2, 1, 0},
    {4, 5, 6, 7},
    {11, 10, 8, 9},
    {12, 13, 14, 15},
};

// Lookup table for row + col given strip number
const uint8_t StripLookup[RowSz * ColSz][2] = {
    {0, 3}, {0, 2}, {0, 1}, {0, 0}, {1, 0}, {1, 1}, {1, 2}, {1, 3},
    {2, 3}, {2, 2}, {2, 1}, {2, 0}, {3, 0}, {3, 1}, {3, 2}, {3, 3}};

void init();
void playBootSequence(BootStepCallback callback = nullptr);
void post();
void update();
void setEnabled(bool enabled);
bool isEnabled();
bool isReady();
int8_t setButtonState(uint8_t buttonIndex, bool pressed);
uint16_t activePhysicalMask();
uint32_t outputPadConfig();
void setBaseColor(uint8_t r, uint8_t g, uint8_t b);

void colorStripPixel(uint8_t row, uint8_t col, uint8_t r, uint8_t g, uint8_t b);
void colorStripPixel(uint8_t row, uint8_t col, Color color);

} // namespace mkshft_ledMatrix

#endif // LED_H_
