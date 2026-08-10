#ifndef PIXEL_H_
#define PIXEL_H_

#include <list>
#include <array>

#include <Arduino.h>

#include <color.hpp>

class Pixel {
public:
  enum edge_t { NONE = -1, RISE = 0, FALL = 1, EXTRA = 2 };
  enum phase_t { ATK = 0, SUS = 1 };
  volatile edge_t triggeredSeqIdx;

private:
  ColorSequenceItr activeEvent;
  ColorSequence activeSequence;
  ColorSequence savedSequences[3][2];
  int16_t row, col;
  uint32_t framesLeft;
  volatile edge_t currentSequenceIdx;
  volatile phase_t currentPhaseIdx;

public:
  Pixel();
  Pixel(uint8_t row, uint8_t col);

  // initialization/data update functions
  void setMatrixCoord(uint8_t r, uint8_t c);
  void resetToBlank();
  void setSequence(edge_t edge, ColorSequence seqRise, ColorSequence seqSus);
  void setSequence(edge_t edge, phase_t phase, ColorSequence seq);

  // frame logic functions
  void advanceSequence();
  void advanceEvent();
  void advanceFrame();

  // data access functions
  Color getFrame() { return getFrame(false); };
  Color getFrame(bool);
  uint8_t getRow() { return row; };
  uint8_t getCol() { return col; };

  void printToSerial();
};

#endif // PIXEL_H_
