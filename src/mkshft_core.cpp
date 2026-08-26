#include <mkshft_core.hpp>

inline namespace core {
bool dialRollover[szDialArray] = {1, 1, 1, 1};

int dialBounds[2][szDialArray] = {{-127, -127, -127, -127},
                                  {128, 128, 128, 128}};

volatile bool buttonState[szButtonArray] = {0, 0, 0, 0, 0, 0, 0, 0,
                                            0, 0, 0, 0, 0, 0, 0, 0};
volatile uint8_t buttonEdgeEventQueue[szButtonArray] = {0, 0, 0, 0, 0, 0, 0, 0,
                                                        0, 0, 0, 0, 0, 0, 0, 0};
volatile uint16_t buttonExtendedState[szButtonArray] = {0, 0, 0, 0, 0, 0, 0, 0,
                                                        0, 0, 0, 0, 0, 0, 0, 0};

volatile int dialState[szDialArray] = {0, 0, 0, 0};
volatile int dialStateRelative[szDialArray] = {0, 0, 0, 0};
int dialSubstepRemainder[szDialArray] = {0, 0, 0, 0};

constexpr int ENCODER_COUNTS_PER_DETENT = 4;

Encoder *dials;

#ifdef DEBUG
void corePrint(char *msg) {
  std::string msgString = msg;
  mkshft_ctrl::sendString("CORE:: " + msgString);
}

void corePrintln(char *msg) {
  std::string msgString = msg;
  mkshft_ctrl::sendLine("CORE:: " + msgString);
}

void test() { Serial.println("TEST TICKLE"); }

#endif

void readDials() {
  int dialTemp;
  bool maxState, minState, boundIndex;
  for (int i = 0; i < szDialArray; i++) {
    dialTemp = dials[i].read();
    dialSubstepRemainder[i] += dialTemp - dialState[i];
    while (dialSubstepRemainder[i] >= ENCODER_COUNTS_PER_DETENT) {
      dialStateRelative[i]++;
      dialSubstepRemainder[i] -= ENCODER_COUNTS_PER_DETENT;
    }
    while (dialSubstepRemainder[i] <= -ENCODER_COUNTS_PER_DETENT) {
      dialStateRelative[i]--;
      dialSubstepRemainder[i] += ENCODER_COUNTS_PER_DETENT;
    }

    // These bools come from checking if the state we just read has passed the
    // upper or lower bound of the specific dial we're checking
    maxState = (dialTemp > dialBounds[1][i]);
    minState = (dialTemp < dialBounds[0][i]);
    if (maxState || minState) {
      /* if the check above passes, maxState == !minState is true
       *
       * We can then use the truth table below to determine which bound (upper
       * or lower) should be applied to the dial state. This table is needed
       * to incorporate dynamically controlled rollover states, so the
       * software can apply software rollover on demand
       *
       *         | Max/min |    Rollover state | desired |
       *         |   (1/0) | (1 = on, 0 = off) |   index |
       *         |---------+-------------------+---------|
       *         |       0 |                 0 |       0 |
       *         |       0 |                 1 |       1 |
       *         |       1 |                 0 |       1 |
       *         |       1 |                 1 |       0 |
       *
       * This table is the same as a truth table for a logical XOR function
       * with two inputs, meaning that (maxState XOR dialRollover) will return
       * the index of the relevant bound (0 for upper, 1 for lower).
       *
       * In C/C++, there is only logical AND (a && b), logical NOT (!a), and
       * logical OR (a || b).
       *
       * However, bitwise XOR (a ^ b) on booleans works out to be the same as
       * logical XOR, which is what we use here.
       * */

      boundIndex = maxState ^ dialRollover[i];

      // Serial.print("dial: ");
      // Serial.print(i);
      // Serial.print(" | ");
      // Serial.print("temp: ");
      // Serial.print(dialTemp);
      // Serial.print(" | ");
      // Serial.print("rel: ");
      // Serial.print(dialStateRelative[i]);
      // Serial.print("max: ");
      // Serial.print(maxState);
      // Serial.print(" | ");
      // Serial.print("min: ");
      // Serial.print(minState);
      // Serial.print(" | ");
      // Serial.print("boundIdx: ");
      // Serial.print(boundIndex);
      // Serial.println();

      dialState[i] = dialBounds[!boundIndex][i];
      dials[i].write(dialState[i]);
    } else {
      dialState[i] = dialTemp;
    }
  }
  return;
}

void scanButtons() {
  int currButton = 0;
  bool edgeRise = false;
  bool edgeFall = false;
  bool prevState = false;
  bool currState = false;
  for (uint8_t p = 0; p < szMatrixPollArray; p++) {
    digitalWrite(pollPins[p], HIGH);
    delayMicroseconds(10);
    for (uint8_t s = 0; s < szMatrixScanArray; s++) {
      prevState = buttonState[currButton];
      // Serial.print(" | s: ");
      // Serial.print(s);
      // Serial.print(" | currButton: ");
      /**
       * The following lines operate on buttonExtendedState as a 16bit
       * register which tracks sampled button history.
       *
       * Two examples to illustrate how the debouncing code works for a
       * momentary switch are provided below.
       *
       * For reference, a momentary switch is like a mouse button or keyboard
       * button - it only reacts when the button is held, and returns to its
       * original position when the button is let go.
       *
       * In the examples, when the switch is pressed or held down, it will be
       * described as CLOSED (because the circuit closes and contact is made),
       * when the switch is let go it will be described as OPEN.
       *
       *
       *
       * Example i)
       * ---------------
       *  Switch has been held CLOSED for a while and is about to trigger
       *  a button on event, buttonExtendedState is reading 0xE000,
       *   in binary: 1110 0000 0000 0000
       *
       *
       * -> Switch continues to be held CLOSED and the following happens:
       *
       *  1 => the current state (buttonExtendedState) is left shifted once:
       *              (buttonExtendedStat << 1)
       *        from: 1110000000000000 --┐
       *          to: 1100000000000000 <-┘
       *
       *  2 => Matrix scans the switch pin, reads true (0x0001), then applies
       *       the logical not (!) operator to flip the Least Significal Bit
       *       (LSB):
       *              (!digitalReadFast())
       *     as read: 0000000000000001 --┐
       *     flipped: 0000000000000000 <-┘
       *
       *
       * -> Once the two steps above finish, the bitwise OR operator is applied
       *    across across the two values along with 0xE000 (* see below for why
       *    this constant):
       *
       *       1100000000000000  <- result from step 1 bitshift
       *   OR  0000000000000000  <- result from step 2 read
       *   OR  1110000000000000  <- constant (0xE000)
       *     = 1110000000000000  (still 0xE000)
       *
       *
       * -> The result just above is assigned back to buttonExtendedState as
       *    the new state.
       *
       *  buttonExtendedState = 1110000000000000
       *
       *
       * -> The new state is then compared with 0xE000 to qualify it as either
       *    fully CLOSED or still OPEN :
       *
       *      buttonExtendedState: 1110000000000000  <-┐
       *                   0xE000: 1110000000000000  <-┘
       *
       *     (buttonExtendedState == 0xE000) ? --> returns true
       *
       *
       *  => The comparison passed since they're equal, buttonState is
       * assigned the result of the comparison which in this cas is TRUE
       *     buttonState: TRUE
       *
       *
       *  (*) -   0xE000 is a tuning constant, it's more intuitive to consider
       *          it as 1110000000000000, representing a snapshot of what a true
       *          ON event that passes debounce looks like as a binary string.
       *          Example ii) will hopefully give more insight.
       *
       *
       *
       *
       * Example ii)
       * ---------------
       *  Switch has been CLOSED for a few ticks, and a bounce event happens
       *  i.e., the switch is momentarily pulled OPEN from imperfect contact
       *
       *  In this example, the switch starts at 0xF000, or in binary:
       *              1111 0000 0000 0000
       * -> Switch bounces after being CLOSED for a small time
       *
       * ==> the current state is left shifted:
       *              (buttonExtendedStat << 1)
       *        from: 1111000000000000 (0xFFC0)
       *          to: 1110000000000000 (0xFF80)
       *
       * ==> Matrix scanes the switch pin, returns false (0x0000), then
       *     the logical not (!) operator is applies to flip the LSB :
       *              (!digitalReadFast())
       *     as read: 0000000000000000
       *     flipped: 0000000000000001
       *
       *
       * -> The values obtained above get OR'd with 0xE000:
       *      Step 1 result  OR  Step 2 result  OR     0xE000
       *   ( 1110000000000000 | 0000000000000001 | 1110000000000000 )
       *            = 1110000000000001  (still 0xE000)
       *
       *
       * -> The result above is then stored in buttonExtendedState as the new
       *    buttonExtendedState = 1110000000000000
       *
       *
       * -> The new state is then compared with 0xE000 (*) to qualify it as
       *    either fully CLOSED or still OPEN :
       *              (buttonExtendedState == 0xE000) -> FALSE
       *      result: 1110000000000001  <-┐ these not the same
       *      0xE000: 1110000000000000  <-┘
       *
       *  => The comparison failed here because the digitalRead passed in a 1
       *     to the LSB of the buttonState is assigned the result of the
       *     comparison which in this case is FALSE, thuse setting buttonState
       *     to FALSE as well
       */
      buttonExtendedState[currButton] =
          (buttonExtendedState[currButton] << 1) |
          !(uint16_t)digitalReadFast(scanPins[s]) | 0xC000;
      edgeRise = (buttonExtendedState[currButton] == 0xE000);
      edgeFall = (buttonExtendedState[currButton] == 0xFFFF);

      buttonState[currButton] = (edgeRise || buttonState[currButton]);
      buttonState[currButton] = (!edgeFall && buttonState[currButton]);

      currState = buttonState[currButton];

      /**
       * The four lines below does something spicy, it pushes the two-bit
       * combination of edge state into the buttonEdgeEventQueue only if there
       * is an edge event.
       *
       * prevState and currState is used as the two-bit edge, i.e. if
       * prevState = 0, currState = 1, the code below interprets it as an edge
       * event, and sends it into the queue through bitshift and addition.
       *
       * If the two bit combination is 00 or 11 (open or held), the shift
       * doesn't happen, and the boolean logic code adds 0 to the start,
       * essentially not moving the queue at all.
       *
       * An if conditional adds about 10ns to the operation, which nets
       * positive, but since the code already runs on ns scale, I'm keeping it
       * strictly bit logic to keep the timing more consistent.
       */

      buttonEdgeEventQueue[currButton] <<= (1 && (prevState ^ currState));
      buttonEdgeEventQueue[currButton] += ((!prevState) && currState);
      buttonEdgeEventQueue[currButton] <<= (1 && (prevState ^ currState));
      buttonEdgeEventQueue[currButton] += (prevState && (!currState));
      // Serial.print("currButton: ");
      // Serial.print(buttonState[0]);
      // Serial.print(" ext: ");
      // Serial.println(buttonExtendedState[0], BIN);
      currButton++;
    }
    digitalWrite(pollPins[p], LOW);
  }
  return;
}

void updateState() {
  scanButtons();
  readDials();
  return;
}

state_t getState() {
  struct state_t s;

  noInterrupts();
  s.snapShotTime = millis();

  for (int i = 0; i < szButtonArray; i++) {
    s.button[i] = buttonState[i];
    s.buttonEdgeEventQueue[i] = buttonEdgeEventQueue[i];
    s.buttonExtended[i] = buttonExtendedState[i];
  }

  for (int i = 0; i < szDialArray; i++) {
    s.dial[i] = dialState[i];
    s.dialRelative[i] = dialStateRelative[i];
    dialStateRelative[i] = 0;
  }
  interrupts();

  return s;
}

/*
   bool diffStates(state_t first, state_t second, state_delta_t *dif)
   {
     uint8_t i = 0;
     bool isDifferent = 0;
     for (; i < szButtonArray; i++)
     {
       dif->button[i] = (first.button[i] - second.button[i]);
       // corePrint(first.button[i]);
       // Serial.print(' ');
       // Serial.print(second.button[i]);
       // Serial.print(' ');
       // Serial.print(dif->button[i]);
       isDifferent = isDifferent | dif->button[i];
     }
     for (i = 0; i < szDialArray; i++)
     {
       dif->dial[i] = (first.dial[i] - second.dial[i]);
       isDifferent = isDifferent | (dif->dial[i] != 0);
     }

     dif->snapShotTime = millis();

     return isDifferent;
   }*/

void init() {
  Serial.println("CORE:: Initiating Core variables");

  dials = new Encoder[szDialArray]{{pinEncoder_0_A, pinEncoder_0_B},
                                   {pinEncoder_1_A, pinEncoder_1_B},
                                   {pinEncoder_2_A, pinEncoder_2_B},
                                   {pinEncoder_3_A, pinEncoder_3_B}};

  for (int i = 0; i < szDialArray; i++) {
    dials[i].write(0);
  }

  for (int i = 0; i < szMatrixPollArray; i++) {
    pinMode(pollPins[i], OUTPUT);
    digitalWrite(pollPins[i], LOW);
  }

  for (int i = 0; i < szMatrixScanArray; i++) {
    pinMode(scanPins[i], INPUT_PULLDOWN);
  }

  // Start from a stable released history instead of interpreting power-up
  // zeroes as an in-progress debounce sequence.
  for (int i = 0; i < szButtonArray; i++) {
    buttonState[i] = false;
    buttonEdgeEventQueue[i] = 0;
    buttonExtendedState[i] = 0xFFFF;
  }
  Serial.println("Successfully initiated scanning pins and Encoder objects");
}

int getBound(bound_t bound, int inputAddress, int *boundValue) {
  if (inputAddress >= 20) {
    return -1;
  } else if (inputAddress > 16) {
    *boundValue = dialBounds[bound][inputAddress - 16];
  } else {
    *boundValue = bound;
  }
  return 0;
}

void printStateToSerial(state_t states, bool ext) {
  Serial.print("states: ");
  for (int i = 0; i < 16; i++) {
    Serial.print(states.button[i]);
    Serial.print(' ');
  }
  Serial.print("| ");

  if (ext == true) {
    Serial.print("ext: ");
    for (int i = 0; i < 16; i++) {
      Serial.print(states.buttonExtended[i], BIN);
      Serial.print(' ');
    }
    Serial.print("| ");
  }

  Serial.print("dials: ");
  for (int i = 0; i < szDialArray; i++) {
    Serial.print(states.dial[i]);
    Serial.print(' ');
  }
  Serial.println();
}

void printStateToSerial(state_t states) { printStateToSerial(states, false); }

} // namespace core
