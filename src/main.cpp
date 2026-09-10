
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
#include <mkshft_assets.hpp>
#include <mkshft_core.hpp>
#include <mkshft_ctrl.hpp>
#include <mkshft_display.hpp>
#include <mkshft_led.hpp>
#include <mkshft_media_cache.hpp>
#include <mkshft_runtime.hpp>
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
constexpr uint32_t usbIdleSleepDelayMs = 5UL * 60UL * 1000UL;
 

/*
 * Packet counter to keep input and output on pace
 */
unsigned int packetCount = 0;

IntervalTimer readInputTimer;

// State tracking
core::state_t stateCurr;
core::state_t statePrev;

// Loop-exclusive variables
uint8_t stateDelta = 0;
bool stateChanged = false;
uint8_t row, col;
uint32_t usbUnavailableSince = 0;
bool usbUnavailableTimerActive = false;
bool usbIdleSleeping = false;
uint8_t collectionDialIndex = 0;
uint8_t collectionButtonIndex = 0;

// Layout *baseLayout;
// LoadingBar *testBar;

// volatile std::queue<dkEvent::Event> eventQueue;

// Helper functions
void onPacketReceived(const uint8_t *, size_t);
void handleSymExp(std::string);
void ledUpdate();
void testWidgets();

void enterUsbIdleSleep() {
  if (usbIdleSleeping) return;

  readInputTimer.end();
  mkshft_ledMatrix::setEnabled(false);
  mkshft_display::setSleeping(true);
  usbIdleSleeping = true;
}

void exitUsbIdleSleep() {
  if (!usbIdleSleeping) return;

  mkshft_ledMatrix::setEnabled(true);
  mkshft_ui::showHomeScreen();
  mkshft_display::setSleeping(false);
  readInputTimer.begin(core::updateState, readInputPeriodUs);
  usbIdleSleeping = false;
}

void updateUsbIdleState(bool usbConnected) {
  if (usbConnected) {
    usbUnavailableTimerActive = false;
    exitUsbIdleSleep();
    return;
  }

  if (!usbUnavailableTimerActive) {
    usbUnavailableSince = millis();
    usbUnavailableTimerActive = true;
    return;
  }

  if (!usbIdleSleeping &&
      millis() - usbUnavailableSince >= usbIdleSleepDelayMs) {
    enterUsbIdleSleep();
  }
}

uint16_t buttonMask(const core::state_t &state) {
  uint16_t mask = 0;
  for (uint8_t i = 0; i < core::szButtonArray; ++i) {
    if (state.button[i]) mask |= static_cast<uint16_t>(1U << i);
  }
  return mask;
}

void traceButtonEdge(uint8_t buttonIndex, bool pressed, int8_t pixelIndex) {
  if (!mkshft_ctrl::connected) return;
  char trace[128] = {};
  snprintf(trace, sizeof(trace),
           "MKDBG EDGE t=%lu button=%u state=%u mask=%04X raw=%04X LED uart=%u pad=%04lX pixel=%d active=%04X",
           millis(), buttonIndex, pressed ? 1 : 0, buttonMask(stateCurr),
           stateCurr.buttonExtended[buttonIndex],
           mkshft_ledMatrix::isReady() ? 1 : 0,
           static_cast<unsigned long>(mkshft_ledMatrix::outputPadConfig()), pixelIndex,
           mkshft_ledMatrix::activePhysicalMask());
  mkshft_ctrl::sendString(trace);
}

void setup()
{
  

#ifdef DEBUG
  delay(1000);
#endif

#ifdef MKSHFT_CTRL_H_
  mkshft_ctrl::init();
#endif

  mkshft_ctrl::packetSerial.setPacketHandler(&onPacketReceived);
  mkshft_runtime::initCompatibilityDefaults();

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
  mkshft_ui::playBootSequence();
#endif

#ifdef DEBUG
  delay(500);
#endif

#ifdef MKSHFT_LISP_H_
  mkshft_lisp::init(&mkshft_ctrl::sendLine);
#endif

  mkshft_ctrl::sendLine("MKSHFT:: Starting state scanning timers...");

  // Prime debounce and LED state synchronously. Starting the timer from an
  // unobserved matrix state can create a burst of false button/LED edges.
  for (uint8_t sample = 0; sample < 24; ++sample) {
    core::updateState();
    delay(1);
  }
  stateCurr = core::getState();
  statePrev = stateCurr;
  for (uint8_t button = 0; button < core::szButtonArray; ++button) {
    mkshft_ledMatrix::setButtonState(button, stateCurr.button[button]);
  }

#ifdef DEBUG
  delay(500);
#endif
  readInputTimer.begin(core::updateState, readInputPeriodUs);

  mkshft_ctrl::sendLine("MKSHFT:: Successfully started state scanning timer.");



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
    if (usbConnected) mkshft_ctrl::sendString("MKDBG LINK connected");
  }
  updateUsbIdleState(usbConnected);
  if (usbIdleSleeping) {
    mkshft_ctrl::update();
    return;
  }

  statePrev = stateCurr;
  stateCurr = core::getState();

  const bool collectionInputActive =
      mkshft_ctrl::connected &&
      mkshft_runtime::isEnabled(mkshft_runtime::ComponentType::CAROUSEL) &&
      mkshft_ui::isLocalCollectionActive() &&
      mkshft_ui::isGameCardVisible();
  if (collectionInputActive) {
    if (stateCurr.dialRelative[collectionDialIndex] != 0) {
      mkshft_ui::moveLocalCollectionSelection(
          stateCurr.dialRelative[collectionDialIndex]);
      mkshft_ctrl::sendString(std::string("GAME_SELECT:") +
                              mkshft_ui::selectedCollectionItemId());
    }
    if (statePrev.button[collectionButtonIndex] !=
            stateCurr.button[collectionButtonIndex] &&
        stateCurr.button[collectionButtonIndex] == core::ON &&
        mkshft_ui::isGameCardVisible()) {
      mkshft_ui::showCollectionLaunchFeedback();
      mkshft_ctrl::sendString(std::string("GAME_LAUNCH:") +
                              mkshft_ui::selectedCollectionItemId());
    }
  }
  if (mkshft_ui::updateLocalCollectionTimeout()) {
    mkshft_ctrl::sendString("GAME_PRELOAD_FIRST");
  }
  mkshft_ui::updateOverlayTimeout();
  mkshft_ui::updateNowPlayingTicker();

  // check button states
  for (int i = 0; i < core::szButtonArray; i++)
  {
    // Serial.print("Button ");
    // Serial.print(i);
    // Serial.print(" state check ");
    // Serial.print(mkshft_ledMatrix::ledMatrix[row][col].triggeredSeqIdx);
    // Serial.println();
    if (statePrev.button[i] != stateCurr.button[i])
    {
      const bool pressed = stateCurr.button[i] == core::ON;
      const int8_t pixelIndex = mkshft_ledMatrix::setButtonState(i, pressed);
      traceButtonEdge(i, pressed, pixelIndex);
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
    core::state_t stateToSend = stateCurr;
    if (collectionInputActive) {
      stateToSend.dialRelative[collectionDialIndex] = 0;
      stateToSend.button[collectionButtonIndex] = false;
    }
    mkshft_ctrl::sendState(stateToSend);
    // core::printStateToSerial(core::getState());
  }
  stateChanged = false;
  mkshft_ui::renderUI();
  mkshft_display::update();
  mkshft_ledMatrix::update();
  mkshft_assets::updateTransferTimeout();
  mkshft_media_cache::updateTransferTimeout();
  mkshft_runtime::updateManifestTimeout();
  mkshft_ctrl::update();
}

void ledUpdate()
{
#ifdef LED_H_
  mkshft_ledMatrix::update();
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
    mkshft_assets::cancelTransfer();
    mkshft_media_cache::cancelWrite();
    mkshft_media_cache::invalidateBindings();
    mkshft_runtime::cancelManifest();
    mkshft_ui::cancelCollectionList();
    mkshft_ui::setUsbConnected(false);
    break;
  case MessageType::COLLECTION_CARD_BEGIN: {
    // slot:u8, gameIndex:u8, width:u16, height:u16, titleLength:u8, title:utf8
    if (bufSz < 8) {
      sendError(header, ProtocolError::MALFORMED_PACKET);
      break;
    }
    const uint8_t slot = buffer[1];
    const uint8_t gameIndex = buffer[2];
    const uint16_t width = (buffer[3] << 8) | buffer[4];
    const uint16_t height = (buffer[5] << 8) | buffer[6];
    const uint8_t titleLength = buffer[7];
    const bool validLength =
        titleLength > 0 && bufSz == static_cast<size_t>(8 + titleLength);
    if (!validLength ||
        !mkshft_ui::beginCollectionCard(slot, gameIndex,
                                  reinterpret_cast<const char *>(buffer + 8),
                                  titleLength, width, height)) {
      sendError(header, ProtocolError::REJECTED_VALUE);
      break;
    }
    sendByte(MessageType::ACK);
    break;
  }
  case MessageType::COLLECTION_ART_CHUNK: {
    // pixelOffset:u32, pixels:RGB565 big-endian
    if (bufSz < 7) {
      sendError(header, ProtocolError::MALFORMED_PACKET);
      break;
    }
    const uint32_t pixelOffset =
        (static_cast<uint32_t>(buffer[1]) << 24) |
        (static_cast<uint32_t>(buffer[2]) << 16) |
        (static_cast<uint32_t>(buffer[3]) << 8) | buffer[4];
    if (!mkshft_ui::writeCollectionArtChunk(pixelOffset, buffer + 5, bufSz - 5)) {
      sendError(header, ProtocolError::INVALID_STATE);
      break;
    }
    break;
  }
  case MessageType::COLLECTION_CARD_COMMIT:
    if (mkshft_ui::commitCollectionCard()) sendByte(MessageType::ACK);
    else sendError(header, ProtocolError::INVALID_STATE);
    break;
  case MessageType::SCREEN_HOME:
    mkshft_ui::showHomeScreen();
    sendByte(MessageType::ACK);
    break;
  case MessageType::COLLECTION_LIST_BEGIN:
    if (bufSz != 2) sendError(header, ProtocolError::MALFORMED_PACKET);
    else if (!mkshft_ui::beginCollectionList(buffer[1]))
      sendError(header, ProtocolError::REJECTED_VALUE);
    else sendByte(MessageType::ACK);
    break;
  case MessageType::COLLECTION_LIST_ITEM: {
    if (bufSz < 4) {
      sendError(header, ProtocolError::MALFORMED_PACKET);
      break;
    }
    const uint8_t appIdLength = buffer[1];
    const uint8_t titleLength = buffer[2];
    const bool valid = appIdLength > 0 && titleLength > 0 &&
                       bufSz == static_cast<size_t>(3 + appIdLength + titleLength);
    if (!valid) {
      mkshft_ui::cancelCollectionList();
      sendError(header, ProtocolError::MALFORMED_PACKET);
    } else if (!mkshft_ui::addCollectionListItem(
                   reinterpret_cast<const char *>(buffer + 3), appIdLength,
                   reinterpret_cast<const char *>(buffer + 3 + appIdLength),
                   titleLength)) {
      mkshft_ui::cancelCollectionList();
      sendError(header, ProtocolError::REJECTED_VALUE);
    } else sendByte(MessageType::ACK);
    break;
  }
  case MessageType::COLLECTION_LIST_COMMIT:
    if (mkshft_ui::commitCollectionList()) sendByte(MessageType::ACK);
    else {
      mkshft_ui::cancelCollectionList();
      sendError(header, ProtocolError::INVALID_STATE);
    }
    break;
  case MessageType::COLLECTION_PRESENTATION: {
    // idleLength:u8, activeLength:u8, idle:utf8, active:utf8. Labels belong
    // to the host cue; firmware only owns the reusable collection renderer.
    if (bufSz < 3) {
      sendError(header, ProtocolError::MALFORMED_PACKET);
      break;
    }
    const uint8_t idleLength = buffer[1];
    const uint8_t activeLength = buffer[2];
    if (bufSz != static_cast<size_t>(3 + idleLength + activeLength) ||
        !mkshft_ui::setCollectionPresentation(
            reinterpret_cast<const char *>(buffer + 3), idleLength,
            reinterpret_cast<const char *>(buffer + 3 + idleLength),
            activeLength)) {
      sendError(header, ProtocolError::REJECTED_VALUE);
      break;
    }
    sendByte(MessageType::ACK);
    break;
  }
  case MessageType::GOXLR_STATUS: {
    // flags:u8 (bit 0 adjusted, bit 1 muted), percent:u8, name:utf8
    if (bufSz < 4) {
      sendError(header, ProtocolError::MALFORMED_PACKET);
      break;
    }
    mkshft_ui::showGoXlrStatus((buffer[1] & 0x01) != 0,
                               (buffer[1] & 0x02) != 0,
                               reinterpret_cast<const char *>(buffer + 3),
                               bufSz - 3, min<uint8_t>(buffer[2], 100));
    sendByte(MessageType::ACK);
    break;
  }
  case MessageType::NOW_PLAYING:
    if (bufSz <= 2 || bufSz > 161 || buffer[1] > 1) {
      sendError(header, ProtocolError::REJECTED_VALUE);
      break;
    }
    mkshft_ui::setNowPlaying(buffer[1] != 0,
                             reinterpret_cast<const char *>(buffer + 2),
                             bufSz - 2);
    sendByte(MessageType::ACK);
    break;
  case MessageType::OVERLAY_GLYPH:
    if (bufSz != 2 || !mkshft_ui::showActionGlyph(buffer[1])) {
      sendError(header, ProtocolError::REJECTED_VALUE);
      break;
    }
    sendByte(MessageType::ACK);
    break;
  case MessageType::RUNTIME_MANIFEST_BEGIN:
    if (bufSz != 3) sendError(header, ProtocolError::MALFORMED_PACKET);
    else if (!mkshft_runtime::beginManifest(buffer[1], buffer[2]))
      sendError(header, ProtocolError::REJECTED_VALUE);
    else sendByte(MessageType::ACK);
    break;
  case MessageType::RUNTIME_COMPONENT: {
    if (bufSz != 5) {
      mkshft_runtime::cancelManifest();
      sendError(header, ProtocolError::MALFORMED_PACKET);
      break;
    }
    const mkshft_runtime::Component component = {
        buffer[1],
        static_cast<mkshft_runtime::ComponentType>(buffer[2]),
        static_cast<mkshft_runtime::Zone>(buffer[3]), buffer[4]};
    if (!mkshft_runtime::addComponent(component)) {
      mkshft_runtime::cancelManifest();
      sendError(header, ProtocolError::REJECTED_VALUE);
      break;
    }
    sendByte(MessageType::ACK);
    break;
  }
  case MessageType::RUNTIME_MANIFEST_COMMIT:
    if (mkshft_runtime::commitManifest()) sendByte(MessageType::ACK);
    else {
      mkshft_runtime::cancelManifest();
      sendError(header, ProtocolError::INVALID_STATE);
    }
    break;
  case MessageType::RUNTIME_CAPABILITIES: {
    // Base fields stay first for v1 readers; later fields are additive. The
    // trailing capability fields are the machine-readable source of truth.
    const uint32_t cacheBytes = mkshft_media_cache::CACHE_CAPACITY_BYTES;
    const uint8_t capabilities[] = {
        mkshft_runtime::PROTOCOL_VERSION, mkshft_runtime::MAX_COMPONENTS,
        0xFE, 0x1F, mkshft_assets::MAX_ASSETS,
        mkshft_ui::MEDIA_CACHE_SLOTS, 0x00, 0xF0,
        4, 0x01, 0x00, 0xF0,
        static_cast<uint8_t>(cacheBytes >> 24),
        static_cast<uint8_t>(cacheBytes >> 16),
        static_cast<uint8_t>(cacheBytes >> 8),
        static_cast<uint8_t>(cacheBytes)};
    send(MessageType::RUNTIME_CAPABILITIES, capabilities,
         sizeof(capabilities));
    char cacheStatus[64] = {};
    snprintf(cacheStatus, sizeof(cacheStatus),
             "MKSHFT_CACHE protocol=3 bytes=%lu packet=240",
             static_cast<unsigned long>(mkshft_media_cache::CACHE_CAPACITY_BYTES));
    sendLine(cacheStatus);
    sendLine("MKSHFT_LED indicator=1");
    break;
  }
  case MessageType::CACHE_FILE_BEGIN: {
    // key:u32, width:u16, height:u16. Logical names stay on Ctrl; firmware
    // only provides bounded storage keyed by the opaque value.
    if (bufSz != 11) {
      sendError(header, ProtocolError::MALFORMED_PACKET,
                bufSz >= 3 ? (static_cast<uint16_t>(buffer[bufSz - 2]) << 8) | buffer[bufSz - 1] : 0);
      break;
    }
    const uint16_t transactionId = (static_cast<uint16_t>(buffer[9]) << 8) | buffer[10];
    const uint32_t key = (static_cast<uint32_t>(buffer[1]) << 24) |
                         (static_cast<uint32_t>(buffer[2]) << 16) |
                         (static_cast<uint32_t>(buffer[3]) << 8) | buffer[4];
    const uint16_t width = (static_cast<uint16_t>(buffer[5]) << 8) | buffer[6];
    const uint16_t height = (static_cast<uint16_t>(buffer[7]) << 8) | buffer[8];
    if (!mkshft_media_cache::beginKeyWrite(key, width, height))
      sendError(header, ProtocolError::REJECTED_VALUE, transactionId);
    else sendAck(header, transactionId);
    break;
  }
  case MessageType::CACHE_FILE_CHUNK: {
    // byteOffset:u32, RGB565 bytes. Byte offsets make the wire format
    // independent of image-specific pixel terminology.
    constexpr size_t CACHE_CHUNK_MAX_BYTES = 1024;
    if (bufSz < 7 || bufSz - 5 > CACHE_CHUNK_MAX_BYTES) {
      sendError(header, ProtocolError::MALFORMED_PACKET);
      break;
    }
    const uint32_t byteOffset = (static_cast<uint32_t>(buffer[1]) << 24) |
                                (static_cast<uint32_t>(buffer[2]) << 16) |
                                (static_cast<uint32_t>(buffer[3]) << 8) | buffer[4];
    if (!mkshft_media_cache::writeByteChunk(byteOffset, buffer + 5, bufSz - 5)) {
      mkshft_media_cache::cancelWrite();
      sendError(header, ProtocolError::INVALID_STATE);
    }
    break;
  }
  case MessageType::CACHE_FILE_COMMIT: {
    const uint16_t transactionId = bufSz == 3 ? (static_cast<uint16_t>(buffer[1]) << 8) | buffer[2] : 0;
    if (bufSz != 3) sendError(header, ProtocolError::MALFORMED_PACKET, transactionId);
    else if (mkshft_media_cache::commitKeyWrite()) {
      const uint32_t evictedKey = mkshft_media_cache::takeEvictedKey();
      if (evictedKey != 0) {
        char eviction[64] = {};
        snprintf(eviction, sizeof(eviction), "MKSHFT_CACHE_EVICT key=%lu",
                 static_cast<unsigned long>(evictedKey));
        sendLine(eviction);
      }
      sendAck(header, transactionId);
    } else {
      mkshft_media_cache::cancelWrite();
      sendError(header, ProtocolError::INVALID_STATE, transactionId);
    }
    break;
  }
  case MessageType::CACHE_FILE_BIND: {
    // key:u32, collectionItemIndex:u8. The host may bind an already uploaded
    // asset to any active collection without copying its pixel buffer again.
    if (bufSz != 8) {
      sendError(header, ProtocolError::MALFORMED_PACKET,
                bufSz >= 3 ? (static_cast<uint16_t>(buffer[bufSz - 2]) << 8) | buffer[bufSz - 1] : 0);
      break;
    }
    const uint16_t transactionId = (static_cast<uint16_t>(buffer[6]) << 8) | buffer[7];
    const uint32_t key = (static_cast<uint32_t>(buffer[1]) << 24) |
                         (static_cast<uint32_t>(buffer[2]) << 16) |
                         (static_cast<uint32_t>(buffer[3]) << 8) | buffer[4];
    if (!mkshft_ui::bindCollectionAsset(key, buffer[5]))
      sendError(header, ProtocolError::INVALID_STATE, transactionId);
    else
      sendAck(header, transactionId);
    break;
  }
  case MessageType::LED_INDICATOR:
    if (bufSz != 6 || buffer[2] > 1 ||
        !mkshft_ledMatrix::setIndicator(buffer[1], buffer[2] != 0,
                                       buffer[3], buffer[4], buffer[5]))
      sendError(header, ProtocolError::MALFORMED_PACKET);
    else sendAck(header);
    break;
  case MessageType::ASSET_BEGIN: {
    if (bufSz != 7 && bufSz != 9) {
      sendError(header, ProtocolError::MALFORMED_PACKET);
      break;
    }
    const uint16_t transactionId = bufSz == 9
        ? (static_cast<uint16_t>(buffer[7]) << 8) | buffer[8] : 0;
    const uint16_t length =
        (static_cast<uint16_t>(buffer[5]) << 8) | buffer[6];
    if (!mkshft_assets::beginAsset(
            buffer[1], static_cast<mkshft_assets::Format>(buffer[2]),
            buffer[3], buffer[4], length))
      sendError(header, ProtocolError::REJECTED_VALUE, transactionId);
    else sendAck(header, transactionId);
    break;
  }
  case MessageType::ASSET_CHUNK: {
    if (bufSz < 4) {
      mkshft_assets::cancelTransfer();
      sendError(header, ProtocolError::MALFORMED_PACKET);
      break;
    }
    const uint16_t offset =
        (static_cast<uint16_t>(buffer[1]) << 8) | buffer[2];
    if (!mkshft_assets::writeChunk(offset, buffer + 3, bufSz - 3)) {
      mkshft_assets::cancelTransfer();
      sendError(header, ProtocolError::INVALID_STATE);
    }
    break;
  }
  case MessageType::ASSET_COMMIT: {
    if (bufSz != 1 && bufSz != 3) {
      sendError(header, ProtocolError::MALFORMED_PACKET);
      break;
    }
    const uint16_t transactionId = bufSz == 3
        ? (static_cast<uint16_t>(buffer[1]) << 8) | buffer[2] : 0;
    if (mkshft_assets::commitAsset()) sendAck(header, transactionId);
    else {
      mkshft_assets::cancelTransfer();
      sendError(header, ProtocolError::INVALID_STATE, transactionId);
    }
    break;
  }
  case MessageType::DEVICE_VISUALS:
    if (bufSz != 12 || buffer[1] != 1 ||
        !mkshft_ui::applyVisualPreferences(
            buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7],
            buffer[8], buffer[9], buffer[10], buffer[11])) {
      sendError(header, ProtocolError::REJECTED_VALUE);
      break;
    }
    sendByte(MessageType::ACK);
    break;
  case MessageType::COLLECTION_INPUT_BINDING:
    if (bufSz != 3 || buffer[1] >= core::szDialArray ||
        buffer[2] >= core::szButtonArray) {
      sendError(header, ProtocolError::REJECTED_VALUE);
      break;
    }
    collectionDialIndex = buffer[1];
    collectionButtonIndex = buffer[2];
    sendByte(MessageType::ACK);
    break;
  case MessageType::STATUS_BADGE:
    // zone:u8 (1 lower-left, 2 lower-right), flags:u8, percent:u8, label:utf8
    if (bufSz < 5 ||
        !mkshft_ui::showStatusBadge(
            buffer[1], (buffer[2] & 0x01) != 0,
            (buffer[2] & 0x02) != 0,
            reinterpret_cast<const char *>(buffer + 4), bufSz - 4,
            min<uint8_t>(buffer[3], 100))) {
      sendError(header, ProtocolError::REJECTED_VALUE);
      break;
    }
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
