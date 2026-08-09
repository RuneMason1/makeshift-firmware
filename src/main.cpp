
static char *serialNumber;

// std library
#include <queue>
#include <string>

// External libraries
#include <Arduino.h>
#include <TeensyID.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <functional>

// MakeShift libraries
#include <mkshft_lisp.hpp>
#include <mkshft_core.hpp>
#include <mkshft_ctrl.hpp>
#include <mkshft_display.hpp>
#include <mkshft_led.hpp>
#include <mkshft_ui.hpp>

#define LOGLVL_MKSHFT_MAIN LOGLVL_DEBUG
#define SLOWDOWN 1

// HID definitions
// #define VENDOR_ID               0x16BF
// #define PRODUCT_ID              0x047f
// #define RAWHID_USAGE_PAGE       0xFFAB  // recommended: 0xFF00 to 0xFFFF
// #define RAWHID_USAGE            0x0200  // recommended: 0x0100 to 0xFFFF

// #define RAWHID_RX_SIZE          64      // receive packet size
// #define RAWHID_RX_INTERVAL      8       // max # of ms between receive
// packets

// Hardware definitions

const long readInputPeriodUs =
    1000L; // microseconds between dial + button scanning cycle
const long ledRenderPeriodUs =
    26667L; // microseconds between updates to visual elements
 

/*
 * Packet counter to keep input and output on pace
 */
unsigned int packetCount = 0;

IntervalTimer readInputTimer;
IntervalTimer ledRenderTimer;

// State tracking
core::state_t stateCurr;
core::state_t statePrev;

// Loop-exclusive variables
uint8_t stateDelta = 0;
bool stateChanged = false;
uint8_t row, col;

// Layout *baseLayout;
// LoadingBar *testBar;

// volatile std::queue<dkEvent::Event> eventQueue;

// Helper functions
void onPacketReceived(const uint8_t *, size_t);
void handleSymExp(std::string);
void ledUpdate();
void testWidgets();

void setup()
{
  

#ifdef DEBUG
  delay(1000);
#endif

#ifdef MKSHFT_CTRL_H_
  mkshft_ctrl::init();
#endif

  mkshft_ctrl::packetSerial.setPacketHandler(&onPacketReceived);

  // TODO: organise define constants to MKSHFT
#ifdef DEBUG
  delay(500);
#endif

#ifdef CORE_H_
  core::init();
#endif

#ifdef DEBUG
  delay(500);
#endif

#ifdef LED_H_
  mkshft_ledMatrix::init();
#endif

#ifdef DEBUG
  delay(500);
#endif

#ifdef ILI9341_H_
  mkshft_display::init();
#endif

#ifdef DEBUG
  delay(500);
#endif

#ifdef MKSHFT_UI_H_
  mkshft_ui::init(&canvas);
#endif

#ifdef DEBUG
  delay(500);
#endif

#ifdef MKSHFT_LISP_H_
  mkshft_lisp::init(&mkshft_ctrl::sendLine);
#endif

  mkshft_ctrl::sendLine("MKSHFT:: Starting state scanning timers...");
  
#ifdef DEBUG
  delay(500);
#endif
  readInputTimer.begin(core::updateState, readInputPeriodUs);

  mkshft_ctrl::sendLine("MKSHFT:: Successfully started state scanning timer.");

  mkshft_ctrl::sendLine("MKSHFT:: Starting LED render timers...");

#ifdef DEBUG
  delay(500);
#endif
  ledRenderTimer.begin(ledUpdate, ledRenderPeriodUs);
  mkshft_ctrl::sendLine("MKSHFT:: Successfully started LED rendering timer.");

  // testWidgets();

  // TODO - initialize data sizes for each module in memory
  // baseLayout = &mkshft_ui::layouts.at("default");
  // testBar = baseLayout->addWidget("testBar", LoadingBar("testBar",
  // baseLayout));

  // // mkshft_ui::link(baseLayout, testBar);
  // baseLayout->setColors(RGB32(130, 20, 144), RGB32(20, 20, 20));
  // testBar->setBorderWidth(4);
  // testBar->setFillColor(tgx::RGB32_Red);
  // testBar->setBackgroundColor(tgx::RGB32_Black);
  // // testBar->setBorderColor(tgx::RGB32_Black);
  // baseLayout->render();
  //


  mkshft_ctrl::sendReady();
}

void loop()
{
#if SLOWDOWN > 0
  delay(10);
#endif
  const bool usbConnected = Serial.dtr();
  if (usbConnected != mkshft_ctrl::connected) {
    mkshft_ctrl::connected = usbConnected;
    mkshft_ui::setUsbConnected(usbConnected);
  }

  statePrev = stateCurr;
  stateCurr = core::getState();

  // check button states
  for (int i = 0; i < core::szButtonArray; i++)
  {
    // Serial.print("Button ");
    // Serial.print(i);
    // Serial.print(" state check ");
    // Serial.print(mkshft_ledMatrix::ledMatrix[row][col].triggeredSeqIdx);
    // Serial.println();
    row = core::ButtonLookup[i][0];
    col = core::ButtonLookup[i][1];
    if (statePrev.button[i] != stateCurr.button[i])
    {
      Pixel::edge_t edge;
      if (stateCurr.button[i] == core::ON)
      {
        edge = Pixel::RISE;
      }
      else
      {
        edge = Pixel::FALL;
      }
      mkshft_ledMatrix::ledMatrix[row][col].triggeredSeqIdx = edge;
      stateChanged = true;
    }
    // if (stateCurr.button[15] == true) {
    //   Serial.println("bye bye!");
    //   Serial.end();
    // }
  }
  // check dial states
  for (int i = 0; i < core::szDialArray; i++)
  {
    if (stateCurr.dialRelative[i] != 0)
    {
      stateChanged = true;

      // if (i == 1)
      // { // update on just dial #2
      //   // multiply by 100 first to reduce scaling error
      //   // float progressPercent = (float)stateCurr.dial[i] * 100.0f;

      //   // // pull upper bound for now - negative dials become weirdness
      //   // progressPercent =
      //   //     progressPercent / (float)core::dialBounds[core::MAX][i];

      //   // int progress = round(progressPercent);

      //   // testBar->setProgress(progress);
      //   // baseLayout->render();
      // }
    }
  }
  // send updates
  if (stateChanged == true)
  {
    mkshft_ctrl::sendState(stateCurr);
    // core::printStateToSerial(core::getState());
  }
  stateChanged = false;
  mkshft_ui::renderUI();
  mkshft_display::update();
  mkshft_ctrl::update();
}

void ledUpdate()
{
#ifdef LED_H_
  mkshft_ledMatrix::updateState();
  mkshft_ledMatrix::showMatrix();
#endif
}

void onPacketReceived(const uint8_t *buffer, size_t bufSz) {
  using namespace mkshft_ctrl;
  if (bufSz == 0) {
    return;
  }

  MessageType header = (MessageType)buffer[0];

  if (header != MessageType::DISCONNECT) {
    connected = true;
    mkshft_ui::setUsbConnected(true);
  }

#if LOGLVL_MKSHFT_MAIN >= LOGLVL_TRACE
  // start debug message
  std::string msg = "Got packet: '";
  send(STRING, (uint8_t *)msg.data(), msg.length());
  send(STRING, buffer, bufSz);
  msg = "' of size ";
  char szStr[16] = {};
  snprintf(szStr, sizeof(szStr), "%u", bufSz);
  send(STRING, (uint8_t *)msg.data(), msg.length());
  send(STRING, (uint8_t *)szStr, strlen(szStr));
  sendLine("");
#endif

  switch (header) {
  case MessageType::PING: {
    sendByte(ACK);
    break;
  }
  case MessageType::STRING: {
#if LOGLVL_MKSHFT_MAIN >= LOGLVL_DEBUG
    // start debug message
    std::string msg = "Got packet: '";
    send(STRING, (uint8_t *)msg.data(), msg.length());
    send(STRING, buffer, bufSz);
    msg = "' of size ";
    char szStr[16] = {};
    snprintf(szStr, sizeof(szStr), "%u", bufSz);
    send(STRING, (uint8_t *)msg.data(), msg.length());
    send(STRING, (uint8_t *)szStr, strlen(szStr));
    sendLine("");
#endif
    // convert buffer to string
    // push the string back from header
    if (bufSz > 1){
      std::string exp;
      exp.assign((char *)buffer, bufSz);
      exp = exp.substr(1);
      handleSymExp(exp);
    }
    break;
  }
  case MessageType::ACK:
    break;
  case MessageType::ERROR:
    break;
  case MessageType::READY:
    connected = true;
    sendReady();
    break;
  case MessageType::DISCONNECT:
    connected = false;
    mkshft_ui::setUsbConnected(false);
    break;
  case MessageType::GAME_CARD_BEGIN: {
    // width:u16, height:u16, titleLength:u8, title:utf8
    if (bufSz < 6) {
      sendByte(MessageType::ERROR);
      break;
    }
    const uint16_t width = (buffer[1] << 8) | buffer[2];
    const uint16_t height = (buffer[3] << 8) | buffer[4];
    const uint8_t titleLength = buffer[5];
    const bool validLength =
        titleLength > 0 && bufSz == static_cast<size_t>(6 + titleLength);
    if (!validLength ||
        !mkshft_ui::beginGameCard(reinterpret_cast<const char *>(buffer + 6),
                                  titleLength, width, height)) {
      sendByte(MessageType::ERROR);
      break;
    }
    sendByte(MessageType::ACK);
    break;
  }
  case MessageType::GAME_ART_CHUNK: {
    // pixelOffset:u32, pixels:RGB565 big-endian
    if (bufSz < 7) {
      sendByte(MessageType::ERROR);
      break;
    }
    const uint32_t pixelOffset =
        (static_cast<uint32_t>(buffer[1]) << 24) |
        (static_cast<uint32_t>(buffer[2]) << 16) |
        (static_cast<uint32_t>(buffer[3]) << 8) | buffer[4];
    if (!mkshft_ui::writeGameArtChunk(pixelOffset, buffer + 5, bufSz - 5)) {
      sendByte(MessageType::ERROR);
      break;
    }
    sendByte(MessageType::ACK);
    break;
  }
  case MessageType::GAME_CARD_COMMIT:
    sendByte(mkshft_ui::commitGameCard() ? MessageType::ACK
                                         : MessageType::ERROR);
    break;
  case MessageType::SCREEN_HOME:
    mkshft_ui::showHomeScreen();
    sendByte(MessageType::ACK);
    break;
  default:
    break;
  }
}

void handleSymExp(std::string expStr) {

#if LOGLVL_MKSHFT_MAIN >= LOGLVL_DEBUG
    // // start debug message
    // std::string msg;

    // int sz = exp.size();
    // int len = 2;
    // while (sz > 10) {
    //   sz = sz / 10;
    //   ++len;
    // }

    // // do some size shenanigans because std::to_string doesn't exist
    // char buf[len];
    // snprintf(buf, len, "%u", exp.size());
    // msg += buf;
#endif

    auto tokens = mkshft_lisp::tokenize(expStr);

    SymExp res = mkshft_lisp::parseTokens(tokens);
    if (res.type != SexpType::ERROR)
    {
      mkshft_lisp::log("Parsing successful");
      auto symRes = mkshft_lisp::toSym(res);
      mkshft_lisp::log(symRes);
      mkshft_lisp::logln("");
    }

#if LOGLVL_MKSHFT_MAIN >= LOGLVL_DEBUG
    std::string msg = "tokenized exp";
    mkshft_ctrl::sendLine(msg);

    std::string tkn;
    for (auto t : tokens) {
      tkn = "TokenType: ";
      switch (t.type) {
      case PAR:
        tkn += "PAR";
        break;
      case SPC:
        tkn += "SPC";
        break;
      case SYM:
        tkn += "SYM";
        break;
      case NUM:
        tkn += "NUM";
        break;
      default:
        tkn += "UNDEF";
        break;
      }
      tkn += " | data: ";
      tkn.append(t.value);
      mkshft_ctrl::sendLine(tkn);
    }
    // mkshft_ctrl::sendLine("");
#endif

    // Serial.write(buffer, size);
    // Serial.println();
}
