#ifndef LED_H_
#define LED_H_

#include <array>
#include <math.h>

#include <Adafruit_NeoPixel.h>

#include <color.hpp>
#include <pixel.hpp>

namespace mkshft_ledMatrix {

const uint8_t LED_PIN = 8;
// const uint8_t LED_ON = 0;

const uint8_t RowSz = 4;
const uint8_t ColSz = 4;
const uint8_t StripSz = RowSz * ColSz;

const uint32_t refreshDelta = 1;
const uint32_t frames = 4;

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
void post();
void updateState();
void showMatrix();
void setEnabled(bool enabled);

void colorStripPixel(uint8_t row, uint8_t col, uint8_t r, uint8_t g, uint8_t b);
void colorStripPixel(uint8_t row, uint8_t col, Color color);

void applyToMatrix(void (*apply)(uint8_t, uint8_t));

extern Pixel ledMatrix[RowSz][ColSz];

} // namespace mkshft_ledMatrix

#endif // LED_H_
