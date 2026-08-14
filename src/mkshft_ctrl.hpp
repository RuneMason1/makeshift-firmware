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

extern SLIPPacketSerial packetSerial;

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
void sendError(MessageType request, ProtocolError error);

// wraps PacketSerial.send with a connection check
void send(MessageType, const uint8_t *, size_t);
void sendRaw(const uint8_t *, size_t);

} // namespace mkshft_ctrl

#endif // MKSHFT_CTRL_H_
