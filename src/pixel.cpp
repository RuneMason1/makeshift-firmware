#include <pixel.hpp>
// #define DEBUG 1

Pixel::Pixel() {
  row = -1;
  col = -1;
  resetToBlank();
}

Pixel::Pixel(uint8_t r, uint8_t c) {
  row = r;
  col = c;
  resetToBlank();
}

void Pixel::setMatrixCoord(uint8_t r, uint8_t c) {
    row = r;
    col = c;
}

void Pixel::resetToBlank() {
  savedSequences[FALL][SUS].events.clear();
  savedSequences[FALL][SUS].events.push_front(EventOFF);
  savedSequences[FALL][SUS].loop = true;
  triggeredSeqIdx = NONE;
  currentSequenceIdx = FALL;
  currentPhaseIdx = SUS;

  activeSequence = savedSequences[currentSequenceIdx][currentPhaseIdx];

  activeEvent = activeSequence.events.begin();
  framesLeft = activeEvent->lengthFrames;
}

void Pixel::setSequence(edge_t edge, ColorSequence seqAtk,
                          ColorSequence seqSus) {

#ifdef DEBUG
  Serial.println("Setting sequences");
  Serial.println("ATK Seq: ");
  printSequence(seqAtk);
  Serial.println("SUS Seq: ");
  printSequence(seqSus);
#endif
  savedSequences[edge][ATK] = seqAtk;
  // savedSequences[edge][ATK].loop = seqAtk.loop;
  savedSequences[edge][SUS] = seqSus;
  // savedSequences[edge][SUS].loop = seqSus.loop;
}

void Pixel::setSequence(edge_t edge, phase_t phase,
                                 ColorSequence seq) {
  savedSequences[edge][phase] = seq;
  // savedSequences[edge][phase].loop = seq.loop;
}

void Pixel::advanceFrame() {
  if (triggeredSeqIdx != NONE) {// sequence update on edge trigger detected
    advanceSequence();
  } else {
    if (framesLeft == 0) {  // event update on last frame
      advanceEvent();
    } else { // finally do regular frame advance
      framesLeft--;
    }
  }
}

void Pixel::advanceEvent() {
  if (activeSequence.events.empty()) {
    resetToBlank();
    return;
  }
  // advance to the next event
  activeEvent++;

  // check if the end of sequence has been reached
  if (activeEvent == activeSequence.events.end()) {
    if (activeSequence.loop == true) {
      // restart sequence if it loops
      activeEvent = activeSequence.events.begin();
      // set frame count from new event
      framesLeft = activeEvent->lengthFrames - 1;
    } else { // pass to advanceSequence if loop ends
      advanceSequence();
    }
  }
}

void Pixel::advanceSequence() {
  // default sequence index
  edge_t targetSequenceIdx = currentSequenceIdx;
  phase_t targetPhase = ATK;

#ifdef DEBUG
  Serial.print("Advancing Sequence for: ");
  printSequence(activeSequence);
  printToSerial();
#endif

  // reset update flags

  if (triggeredSeqIdx != NONE) {
    targetSequenceIdx = triggeredSeqIdx;
    currentSequenceIdx = triggeredSeqIdx;
  } else {
    if (activeSequence.loop == false) {
      // become sus if the attack sequences ends
      targetPhase = SUS;
    }
  }
  // reset trigger
  triggeredSeqIdx = NONE;


  const ColorSequence &target = savedSequences[targetSequenceIdx][targetPhase];
  if (target.events.empty()) {
    resetToBlank();
    return;
  }
  activeSequence.events = target.events;
  activeSequence.loop = target.loop;
  activeEvent = activeSequence.events.begin();
  framesLeft = activeEvent->lengthFrames - 1;
}

Color Pixel::getFrame(bool adjusted) {
  Color c;
  c = activeEvent->color;

#ifdef DEBUG
    // Serial.print("rgb calc: ");
    // Serial.print(c.r);
    // Serial.print(",");
    // Serial.print(c.g);
    // Serial.print(",");
    // Serial.print(c.b);
#endif
    // sets rgb with bitwise operations to go fast
    c.r = (adjusted * gamma8[c.r]) | (!adjusted * c.r);
    c.g = (adjusted * gamma8[c.g]) | (!adjusted * c.g);
    c.b = (adjusted * gamma8[c.b]) | (!adjusted * c.b);
#ifdef DEBUG
    // Serial.print(" | adjustment: ");
    // Serial.print(adjusted);
    // Serial.print(" | ");
    // Serial.print(c.r);
    // Serial.print(",");
    // Serial.print(c.g);
    // Serial.print(",");
    // Serial.print(c.b);
    // Serial.print(" | ");
#endif

    return c;
}

void Pixel::printToSerial() {
  Serial.print("pixel: ");
  Serial.print(row);
  Serial.print(",");
  Serial.print(col);
  Serial.print(" | ");
  Serial.print("framesLeft: ");
  Serial.print(framesLeft);
  Serial.print(" | ");
  Serial.print("lengthFrames: ");
  Serial.print(activeEvent->lengthFrames);
  Serial.print(" | ");
  Serial.print("rgb: ");
  printColor(activeEvent->color);
  Serial.print(" | ");
  Serial.print("loop: ");
  Serial.print(activeSequence.loop);
  Serial.println();
}
