#ifndef MKSHFT_CTRL_H_
#define MKSHFT_CTRL_H_

#include <Arduino.h>
#include <PacketSerial.h>
#include <TeensyID.h>

#include <map>
#include <string>
#include <vector>
#include <functional>

#include <mkshft_core.hpp>

#define LOGLVL_MKSHFT_CTRL LOGLVL_DEBUG

inline namespace mkshft_ctrl {

// Cache transfers use the established sub-240-byte host frame limit. Retain
// the LED-good SLIP framing and 256-byte decoder until a larger-frame path
// has separately passed physical cold-boot validation.
using MakeShiftPacketSerial = SLIPPacketSerial;
extern MakeShiftPacketSerial packetSerial;

enum MessageType {
  PING,
  ACK,
  READY,
  STATE_UPDATE,
  ERROR,
  STRING,
  DISCONNECT,
  GAME_CARD_BEGIN,
  GAME_ART_CHUNK,
  GAME_CARD_COMMIT,
  SCREEN_HOME,
  GAME_LIST_BEGIN,
  GAME_LIST_ITEM,
  GAME_LIST_COMMIT,
  GOXLR_STATUS,
  NOW_PLAYING,
  ACTION_GLYPH,
  RUNTIME_MANIFEST_BEGIN,
  RUNTIME_COMPONENT,
  RUNTIME_MANIFEST_COMMIT,
  RUNTIME_CAPABILITIES,
  ASSET_BEGIN,
  ASSET_CHUNK,
  ASSET_COMMIT,
  DEVICE_VISUALS,
  COLLECTION_INPUT_BINDING,
  STATUS_BADGE,
  // Generic host-managed RGB565 cache. Ctrl Core already reserves these
  // values, so keep them stable across firmware revisions.
  CACHE_FILE_BEGIN,
  CACHE_FILE_CHUNK,
  CACHE_FILE_COMMIT,
  CACHE_FILE_BIND,
  LED_INDICATOR,
  // Collection presentation is host-owned. The device only renders the
  // current cue's labels; it never assigns provider-specific behavior.
  COLLECTION_PRESENTATION,

  // Generic names for the stable legacy collection packet values.
  COLLECTION_CARD_BEGIN = GAME_CARD_BEGIN,
  COLLECTION_ART_CHUNK = GAME_ART_CHUNK,
  COLLECTION_CARD_COMMIT = GAME_CARD_COMMIT,
  COLLECTION_LIST_BEGIN = GAME_LIST_BEGIN,
  COLLECTION_LIST_ITEM = GAME_LIST_ITEM,
  COLLECTION_LIST_COMMIT = GAME_LIST_COMMIT,
  OVERLAY_GLYPH = ACTION_GLYPH,
};

enum class ProtocolError : uint8_t {
  MALFORMED_PACKET = 1,
  REJECTED_VALUE = 2,
  INVALID_STATE = 3,
  UNSUPPORTED = 4,
};

extern bool connected;

bool getWidgets();

void sendState(core::state_t);
void sendLayouts();

void init();

// Alias to keep the implementation detail out of the way
inline void update() { packetSerial.update(); };

void sendReady();

void sendString(std::string);
void sendLine(std::string);
void sendByte(MessageType t);
void sendAck(MessageType request, uint16_t transactionId = 0);
void sendError(MessageType request, ProtocolError error, uint16_t transactionId = 0);

// wraps PacketSerial.send with a connection check
void send(MessageType, const uint8_t *, size_t);
void sendRaw(const uint8_t *, size_t);

} // namespace mkshft_ctrl

#endif // MKSHFT_CTRL_H_
