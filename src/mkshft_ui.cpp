#include <mkshft_ui.hpp>

namespace mkshft_ui {

Layout *currentLayout;
std::map<std::string, Layout> layouts;

DMAMEM uint16_t gameArtwork[GAME_ART_CACHE_SLOTS]
                              [GAME_ART_MAX_WIDTH * GAME_ART_MAX_HEIGHT];
struct ArtworkSlot {
  uint8_t gameIndex;
  uint16_t width;
  uint16_t height;
  bool valid;
};
ArtworkSlot artworkSlots[GAME_ART_CACHE_SLOTS] = {};
char gameTitle[GAME_TITLE_MAX_LENGTH + 1] = {};
uint16_t gameArtWidth = 0;
uint16_t gameArtHeight = 0;
uint32_t gamePixelsReceived = 0;
uint8_t gameTransferIndex = 0;
uint8_t gameTransferSlot = 0;
bool gameTransferActive = false;
bool gameCardVisible = false;
bool usbConnected = false;

struct CachedGame {
  char appId[GAME_APP_ID_MAX_LENGTH + 1];
  char title[GAME_TITLE_MAX_LENGTH + 1];
};

CachedGame cachedGames[GAME_LIST_MAX_ITEMS] = {};
uint8_t cachedGameCount = 0;
uint8_t expectedGameCount = 0;
uint8_t selectedGameIndex = 0;
bool localGameCarouselActive = false;
uint32_t gameCardLastInteractionMs = 0;
bool actionGlyphVisible = false;
uint32_t actionGlyphShownMs = 0;
bool goXlrVisible = false;
uint32_t goXlrShownMs = 0;
char goXlrChannelLabel[33] = "Chromecast";
uint8_t goXlrChannelPercent = 0;
bool goXlrPercentKnown = false;
constexpr size_t NOW_PLAYING_MAX_LENGTH = 160;
char nowPlaying[NOW_PLAYING_MAX_LENGTH + 1] = {};
bool nowPlayingActive = false;
int nowPlayingOffset = 0;
uint32_t nowPlayingLastFrameMs = 0;
uint32_t nowPlayingHoldUntilMs = 0;
bool nowPlayingHoldingAtEnd = false;

constexpr uint32_t GAME_CARD_TIMEOUT_MS = 5000;
constexpr uint32_t ACTION_GLYPH_TIMEOUT_MS = 1500;
constexpr uint32_t GOXLR_TIMEOUT_MS = 3000;

namespace {
int textWidth(const char *text) {
  int width = 0;
  for (const char *cursor = text; *cursor != '\0'; ++cursor) {
    int advance = 0;
    defaultCanvas->measureChar(*cursor, iVec2(0, 0), *baseFont,
                               DEFAULT_TEXT_ANCHOR, &advance);
    width += advance;
  }
  return width;
}

void drawNowPlayingTicker() {
  if (!usbConnected || !nowPlayingActive || nowPlaying[0] == '\0' ||
      gameCardVisible)
    return;
  constexpr int top = 11;
  constexpr int bottom = 31;
  constexpr int availableWidth = 304;
  const int width = textWidth(nowPlaying);
  const int x = width <= availableWidth ? (320 - width) / 2
                                        : 8 - nowPlayingOffset;
  defaultCanvas->fillRect(iBox2(0, 319, top, bottom), RGB32(8, 13, 18));
  defaultCanvas->drawText(nowPlaying, iVec2(x, 27),
                          *baseFont, RGB32(245, 240, 220));
}

void drawPersistentGoXlrLabel() {
  if (!usbConnected || !goXlrPercentKnown) return;
  constexpr int right = 311;
  constexpr int labelBaseline = 230;
  char percent[8] = {};
  snprintf(percent, sizeof(percent), "%u%%", goXlrChannelPercent);
  const int labelWidth = textWidth(goXlrChannelLabel);
  const int percentWidth = textWidth(percent);
  // Always clear the full badge width so a shorter channel name cannot leave
  // pixels behind from a previous label such as "Chromecast".
  defaultCanvas->fillRect(iBox2(196, 319, 190, 239), RGB32(8, 13, 18));
  defaultCanvas->drawText(percent, iVec2(right - percentWidth, 207),
                          *baseFont, RGB32(245, 240, 220));
  defaultCanvas->drawText(goXlrChannelLabel,
                          iVec2(right - labelWidth, labelBaseline),
                          *baseFont, RGB32(216, 58, 4));
}

void drawGameTitle(const char *title) {
  constexpr size_t maxChars = 32;
  char lines[2][maxChars + 1] = {};
  const char *cursor = title;
  uint8_t lineCount = 0;

  while (*cursor != '\0' && lineCount < 2) {
    while (*cursor == ' ') ++cursor;
    const size_t remaining = strlen(cursor);
    size_t count = min(remaining, maxChars);
    if (remaining > maxChars) {
      size_t breakAt = count;
      while (breakAt > 0 && cursor[breakAt] != ' ') --breakAt;
      if (breakAt > 0) count = breakAt;
    }
    memcpy(lines[lineCount], cursor, count);
    lines[lineCount][count] = '\0';
    cursor += count;
    ++lineCount;
  }

  while (*cursor == ' ') ++cursor;
  if (*cursor != '\0' && lineCount > 0) {
    char *last = lines[lineCount - 1];
    const size_t length = strlen(last);
    if (length >= 3) memcpy(last + length - 3, "...", 3);
  }

  const int firstY = lineCount == 1 ? 34 : 22;
  for (uint8_t line = 0; line < lineCount; ++line) {
    const int x = max(8, (320 - textWidth(lines[line])) / 2);
    defaultCanvas->drawText(lines[line], iVec2(x, firstY + line * 18), *baseFont,
                            RGB32(245, 240, 220));
  }
}

int8_t findArtworkSlot(uint8_t gameIndex) {
  for (uint8_t slot = 0; slot < GAME_ART_CACHE_SLOTS; ++slot) {
    if (artworkSlots[slot].valid && artworkSlots[slot].gameIndex == gameIndex)
      return slot;
  }
  return -1;
}

void renderGameCard(int8_t slot) {
  defaultCanvas->fillRect(iBox2(0, 319, 0, 239), RGB32(8, 13, 18));
  defaultCanvas->fillRect(iBox2(0, 319, 0, 9), RGB32(255, 184, 77));
  drawGameTitle(gameTitle);

  constexpr int artSize = 150;
  constexpr int artX = (320 - artSize) / 2;
  constexpr int artY = 50;
  defaultCanvas->fillRect(iBox2(artX - 3, artX + artSize + 2,
                                artY - 3, artY + artSize + 2),
                          RGB32(18, 29, 38));
  if (slot >= 0 && artworkSlots[slot].valid) {
    const uint16_t width = artworkSlots[slot].width;
    const uint16_t height = artworkSlots[slot].height;
    Image<RGB565> artwork(gameArtwork[slot], width, height);
    for (int y = 0; y < artSize; ++y) {
      const int sourceY = (y * height) / artSize;
      for (int x = 0; x < artSize; ++x) {
        const int sourceX = (x * width) / artSize;
        defaultCanvas->drawPixel<false>(iVec2(artX + x, artY + y),
                                        artwork(sourceX, sourceY));
      }
    }
  } else {
    defaultCanvas->drawText("LOADING ART", iVec2(112, 130), *baseFont,
                            RGB32(151, 166, 175));
  }

  const char *action = "CLICK TO PLAY";
  defaultCanvas->drawText(action, iVec2((320 - textWidth(action)) / 2, 226), *baseFont,
                          RGB32(42, 214, 168));
}

} // namespace

template <class T> bool Layout::addWidget(T w) {
  WidgetType wType = w.getType();
  T* widg = _emplaceWidget(w);
  if (widg != NULL) {
#ifdef DEBUG
    Serial.print("Created ");
    // Serial.print();
    Serial.print(" with id \"");
    Serial.print(id.data());
    Serial.print("\" in layout \"");
    Serial.print(currentLayout->getId().data());
    Serial.print("\"");
    Serial.println();
#endif
    renderingOrder.push_back(id);
    renderedWidgets.insert(std::make_pair(id, widg));
  }
  return 0;
}

bool Layout::addWidget(WidgetType t, std::string id) {
  auto idItr = currentLayout->renderingOrder.begin();
  auto idEnd = currentLayout->renderingOrder.end();
  while (idItr != idEnd){
    if (*idItr == id) {
      return true;
    }
    ++idItr;
  }

  bool emplaced = false;
  Widget *widg;

  std::string target;

#ifdef DEBUG
#endif
  switch (t) {
  case W_BOX: {
    auto e = currentLayout->boxes.emplace(id, WBox(id));
    widg = &e.first->second;
    emplaced = e.second;

    target = "box";

    break;
  }
  case W_CIRCLE: {
    auto e = currentLayout->circles.emplace(id, WCircle(id));
    widg = &e.first->second;
    emplaced = e.second;

    target = "circle";

    break;
  }

  case W_TRIANGLE: {
    auto e = currentLayout->triangles.emplace(id, WTriangle(id));
    widg = &e.first->second;
    emplaced = e.second;

    target = "triangle";

    break;
  }
  case W_TEXT_BOX: {
    auto e = currentLayout->textBoxes.emplace(id, WTextBox(id));
    widg = &e.first->second;
    emplaced = e.second;

    target = "textbox";

    break;
  }
  case W_PROGRESS_BAR: {
    auto e = currentLayout->progressBars.emplace(id, WProgressBar(id));
    widg = &e.first->second;
    emplaced = e.second;

    target = "progress bar";

    break;
  }
  default: {
  }
  }

  if (emplaced == true) {
#ifdef DEBUG
    Serial.print("Created ");
    Serial.print(target.data());
    Serial.print(" with id \"");
    Serial.print(id.data());
    Serial.print("\" in layout \"");
    Serial.print(currentLayout->getId().data());
    Serial.print("\"");
    Serial.println();
#endif
    renderingOrder.push_back(id);
    renderedWidgets.insert(std::make_pair(id, widg));
  }
  return 0;
}

void Layout::removeWidget(std::string id) {
  uint8_t removedSz = 0;
  removedSz += renderedWidgets.erase(id);
  removedSz += (progressBars.erase(id) * 2);
  removedSz += (textBoxes.erase(id) * 4);
  removedSz += (circles.erase(id) * 8);
  removedSz += (boxes.erase(id) * 16);
  // expect removedSz to be only 3, 5, 9, or 17 - otherwise it would imply
  // that was a duplicate id somewhere
}
WBox* Layout::_emplaceWidget(WBox w) {
  auto emplaceRes = currentLayout->boxes.emplace(w.getID(), w);
  return &emplaceRes.first->second;
}
WCircle* Layout::_emplaceWidget(WCircle w) {
  auto emplaceRes = currentLayout->circles.emplace(w.getID(), w);
  return &emplaceRes.first->second;
}
WTriangle* Layout::_emplaceWidget(WTriangle w) {
  auto emplaceRes = currentLayout->triangles.emplace(w.getID(), w);
  return &emplaceRes.first->second;
}
WProgressBar* Layout::_emplaceWidget(WProgressBar w) {
  auto emplaceRes = currentLayout->progressBars.emplace(w.getID(), w);
  return &emplaceRes.first->second;
}
WTextBox* Layout::_emplaceWidget(WTextBox w) {
  auto emplaceRes = currentLayout->textBoxes.emplace(w.getID(), w);
  return &emplaceRes.first->second;
}

void init(Image<RGB565> *cnv) {
  Serial.println("Initializing UI...");

  Serial.println("Setting default canvas...");
  setDefaultCanvas(cnv);

  Serial.println("creating default layout...");
  layouts.emplace("default", Layout("default"));
  currentLayout = &layouts.at("default");

  Serial.println("Testing UI library...");
  splashScreen();
}


void renderUI() {
  // auto __pair = currentLayout->renderedWidgets.begin();
  // auto __end = currentLayout->renderedWidgets.end();
  for (uint n = 0; n < currentLayout->renderingOrder.size(); n++){
    // Serial.print("rendering ");
    // Serial.println(id.data());
    currentLayout->renderedWidgets.at(
      currentLayout->renderingOrder[n]
        )->render();
  }
}

void splashScreen() {
  defaultCanvas->blit(splash565, iVec2(0, 0));
  setUsbConnected(usbConnected);
  drawNowPlayingTicker();
  drawPersistentGoXlrLabel();

  // currentLayout->addWidget(WTriangle("testTriangle", iVec2(0,0), iVec2(50,0), iVec2(0,50)));
  // currentLayout->triangles.at("testTriangle")
  //     .setColors(tgx::RGB32_Purple, tgx::RGB32_Blue);
  // Serial.println("a");

  // defaultCanvas->blit(splash565, iVec2(0,0));
  // defaultCanvas->fillRect(iVec2(0,0),iVec2(320, 240), RGB565(0,0,0));
  // currentLayout->addWidget(W_BOX, "testBox2");
  // currentLayout->boxes.at("testBox2").setSize(12, 12);
  // currentLayout->boxes.at("testBox2")
  //     .setColors(tgx::RGB32_Green, tgx::RGB32_Blue);
  // Serial.println("b");

  // std::string textName = "testText";
  // currentLayout->addWidget(W_TEXT_BOX, textName);
  // currentLayout->textBoxes.at(textName).setSize(320, 240);
  // currentLayout->textBoxes.at(textName)
  //     .setColors(tgx::RGB32_White, tgx::RGB32_Blue);
  // Serial.println("c");


  // tgx::iBox2 boxxx = tgx::iBox2(0,0,0,0);
  // int xadv = 0;
  // std::string testString = "beegin..";
  // for (char c = 20; c <= 127; ++c) {
  //   boxxx = defaultCanvas->measureChar(c, iVec2(25, 25), *baseFont, &xadv);
  //   Serial.print("Size of baseFont char \'");
  //   Serial.print(c);
  //   Serial.print("\': ");
  //   Serial.print(boxxx.lx());
  //   Serial.print(" x ");
  //   Serial.print(boxxx.ly());
  //   Serial.print(" | xadv: ");
  //   Serial.print(xadv);
  //   Serial.println();
  //   testString += c;
  // }

  // Serial.print("testString:");
  // Serial.println(testString.data());
  // currentLayout->textBoxes.at(textName).setText(testString);

  // layouts.at("default").
}

void setUsbConnected(bool connected) {
  usbConnected = connected;
  if (!connected) {
    goXlrPercentKnown = false;
    goXlrChannelLabel[0] = '\0';
    goXlrChannelPercent = 0;
    nowPlayingActive = false;
    nowPlaying[0] = '\0';
    nowPlayingOffset = 0;
    nowPlayingLastFrameMs = 0;
    nowPlayingHoldUntilMs = 0;
    nowPlayingHoldingAtEnd = false;
    if (!gameCardVisible && !actionGlyphVisible)
      defaultCanvas->fillRect(iBox2(196, 319, 190, 239), RGB32(8, 13, 18));
  }
  if (gameCardVisible || actionGlyphVisible || goXlrVisible) {
    return;
  }
  defaultCanvas->fillRect(iBox2(0, 319, 0, 9),
                          connected ? RGB32(216, 58, 4)
                                    : RGB32(72, 24, 8));
}

bool beginGameCard(uint8_t slot, uint8_t gameIndex, const char *title,
                   size_t titleLength, uint16_t width, uint16_t height) {
  const bool textOnly = width == 0 && height == 0;
  const bool validArtworkSize = width > 0 && height > 0 &&
                                width <= GAME_ART_MAX_WIDTH &&
                                height <= GAME_ART_MAX_HEIGHT;
  if (slot >= GAME_ART_CACHE_SLOTS || gameIndex >= cachedGameCount ||
      title == nullptr || titleLength == 0 ||
      titleLength > GAME_TITLE_MAX_LENGTH ||
      (!textOnly && !validArtworkSize)) {
    gameTransferActive = false;
    return false;
  }

  gameArtWidth = width;
  gameArtHeight = height;
  gamePixelsReceived = 0;
  gameTransferIndex = gameIndex;
  gameTransferSlot = slot;
  artworkSlots[slot].valid = false;
  gameTransferActive = !textOnly;
  return true;
}

bool writeGameArtChunk(uint32_t pixelOffset, const uint8_t *data,
                       size_t dataLength) {
  if (!gameTransferActive || data == nullptr || dataLength == 0 ||
      (dataLength % 2) != 0 || pixelOffset != gamePixelsReceived) {
    return false;
  }

  const uint32_t pixelCount = dataLength / 2;
  const uint32_t expectedPixels = gameArtWidth * gameArtHeight;
  if (pixelOffset + pixelCount > expectedPixels) {
    gameTransferActive = false;
    return false;
  }

  for (uint32_t index = 0; index < pixelCount; ++index) {
    gameArtwork[gameTransferSlot][pixelOffset + index] =
        (static_cast<uint16_t>(data[index * 2]) << 8) | data[index * 2 + 1];
  }
  gamePixelsReceived += pixelCount;
  return true;
}

bool commitGameCard() {
  const uint32_t expectedPixels = gameArtWidth * gameArtHeight;
  if (!gameTransferActive || gamePixelsReceived != expectedPixels) {
    return false;
  }

  gameTransferActive = false;
  artworkSlots[gameTransferSlot] = {gameTransferIndex, gameArtWidth,
                                    gameArtHeight, true};
  if (gameCardVisible && gameTransferIndex == selectedGameIndex) {
    gameCardLastInteractionMs = millis();
    renderGameCard(gameTransferSlot);
  }
  return true;
}

void showHomeScreen() {
  const bool wasUsbConnected = usbConnected;
  gameTransferActive = false;
  gameCardVisible = false;
  gameCardLastInteractionMs = 0;
  actionGlyphVisible = false;
  actionGlyphShownMs = 0;
  goXlrVisible = false;
  goXlrShownMs = 0;
  splashScreen();
  setUsbConnected(wasUsbConnected);
}

bool beginGameList(uint8_t expectedCount) {
  if (expectedCount == 0 || expectedCount > GAME_LIST_MAX_ITEMS) return false;
  expectedGameCount = expectedCount;
  cachedGameCount = 0;
  localGameCarouselActive = false;
  for (auto &slot : artworkSlots) slot.valid = false;
  return true;
}

bool addGameListItem(const char *appId, size_t appIdLength, const char *title,
                     size_t titleLength) {
  if (cachedGameCount >= expectedGameCount || appId == nullptr || title == nullptr ||
      appIdLength == 0 || appIdLength > GAME_APP_ID_MAX_LENGTH ||
      titleLength == 0 || titleLength > GAME_TITLE_MAX_LENGTH) return false;
  CachedGame &game = cachedGames[cachedGameCount++];
  memcpy(game.appId, appId, appIdLength);
  game.appId[appIdLength] = '\0';
  memcpy(game.title, title, titleLength);
  game.title[titleLength] = '\0';
  return true;
}

bool commitGameList() {
  if (cachedGameCount == 0 || cachedGameCount != expectedGameCount) return false;
  selectedGameIndex = 0;
  localGameCarouselActive = true;
  return true;
}

bool isLocalGameCarouselActive() { return localGameCarouselActive; }
bool isGameCardVisible() { return gameCardVisible; }

void moveLocalGameSelection(int delta) {
  if (!localGameCarouselActive || cachedGameCount == 0 || delta == 0) return;
  const bool wasVisible = gameCardVisible;
  if (!wasVisible) {
    selectedGameIndex = 0;
  } else {
    int next = static_cast<int>(selectedGameIndex) + delta;
    next %= cachedGameCount;
    if (next < 0) next += cachedGameCount;
    selectedGameIndex = next;
  }
  strncpy(gameTitle, cachedGames[selectedGameIndex].title, GAME_TITLE_MAX_LENGTH);
  gameTitle[GAME_TITLE_MAX_LENGTH] = '\0';
  gameCardVisible = true;
  gameTransferActive = false;
  gameCardLastInteractionMs = millis();
  renderGameCard(findArtworkSlot(selectedGameIndex));
}

bool updateGameCarouselTimeout() {
  const uint32_t now = millis();
  const bool gameExpired = gameCardVisible && gameCardLastInteractionMs != 0 &&
      static_cast<uint32_t>(now - gameCardLastInteractionMs) >=
          GAME_CARD_TIMEOUT_MS;
  const bool actionExpired = actionGlyphVisible && actionGlyphShownMs != 0 &&
      static_cast<uint32_t>(now - actionGlyphShownMs) >=
          ACTION_GLYPH_TIMEOUT_MS;
  if (gameExpired || actionExpired) {
    showHomeScreen();
  }
  return gameExpired;
}

void updateOverlayTimeout() {
  if (goXlrVisible && goXlrShownMs != 0 &&
      static_cast<uint32_t>(millis() - goXlrShownMs) >= GOXLR_TIMEOUT_MS) {
    showHomeScreen();
  }
}

void showGoXlrStatus(bool adjusting, const char *name, size_t nameLength,
                     uint8_t percent) {
  char label[33] = {};
  const size_t copyLength = min(nameLength, sizeof(label) - 1);
  memcpy(label, name, copyLength);
  memcpy(goXlrChannelLabel, label, copyLength + 1);
  goXlrChannelPercent = min<uint8_t>(percent, 100);
  goXlrPercentKnown = true;
  goXlrVisible = false;
  goXlrShownMs = 0;
  drawPersistentGoXlrLabel();
}

bool showActionGlyph(uint8_t glyphId) {
  if (glyphId < 1 || glyphId > 3) return false;
  gameTransferActive = false;
  gameCardVisible = false;
  actionGlyphVisible = true;
  actionGlyphShownMs = millis();

  const RGB32 panel(13, 20, 26);
  const RGB32 panelEdge(54, 66, 74);
  const RGB32 orange(216, 58, 4);
  const RGB32 foreground(245, 240, 220);
  constexpr int left = 68;
  constexpr int right = 251;
  constexpr int top = 47;
  constexpr int bottom = 184;
  defaultCanvas->fillRect(iBox2(left, right, top, bottom), panelEdge);
  defaultCanvas->fillRect(iBox2(left + 3, right - 3, top + 3, bottom - 3), panel);
  defaultCanvas->fillRect(iBox2(left + 3, right - 3, top + 3, top + 8), orange);

  const char *title = glyphId == 1 ? "PREVIOUS"
                      : glyphId == 3 ? "NEXT"
                                     : "MEDIA";
  defaultCanvas->drawText(title, iVec2((320 - textWidth(title)) / 2, 77),
                          *baseFont, orange);
  if (glyphId == 1) {
    defaultCanvas->fillRect(iBox2(92, 105, 101, 157), foreground);
    defaultCanvas->fillTriangle(iVec2(155, 101), iVec2(155, 157),
                                iVec2(108, 129), foreground, foreground, 1.0f);
    defaultCanvas->fillTriangle(iVec2(211, 101), iVec2(211, 157),
                                iVec2(164, 129), foreground, foreground, 1.0f);
  } else if (glyphId == 3) {
    defaultCanvas->fillTriangle(iVec2(108, 101), iVec2(108, 157),
                                iVec2(155, 129), foreground, foreground, 1.0f);
    defaultCanvas->fillTriangle(iVec2(164, 101), iVec2(164, 157),
                                iVec2(211, 129), foreground, foreground, 1.0f);
    defaultCanvas->fillRect(iBox2(214, 227, 101, 157), foreground);
  } else {
    defaultCanvas->fillTriangle(iVec2(94, 101), iVec2(94, 157),
                                iVec2(140, 129), foreground, foreground, 1.0f);
    defaultCanvas->fillRect(iBox2(169, 184, 101, 157), foreground);
    defaultCanvas->fillRect(iBox2(199, 214, 101, 157), foreground);
  }
  return true;
}

void setNowPlaying(bool playing, const char *text, size_t textLength) {
  if (text == nullptr || textLength == 0 || textLength > NOW_PLAYING_MAX_LENGTH)
    return;
  memcpy(nowPlaying, text, textLength);
  nowPlaying[textLength] = '\0';
  nowPlayingOffset = 0;
  const uint32_t now = millis();
  nowPlayingLastFrameMs = now;
  nowPlayingHoldUntilMs = now + 700;
  nowPlayingHoldingAtEnd = false;
  const bool wasActive = nowPlayingActive;
  nowPlayingActive = playing;
  if (!playing && wasActive && !gameCardVisible && !actionGlyphVisible) splashScreen();
  else if (playing) drawNowPlayingTicker();
}

void updateNowPlayingTicker() {
  const uint32_t now = millis();
  if (!nowPlayingActive || gameCardVisible || nowPlaying[0] == '\0' ||
      static_cast<uint32_t>(now - nowPlayingLastFrameMs) < 55)
    return;
  nowPlayingLastFrameMs = now;
  const int width = textWidth(nowPlaying);
  if (width <= 304) return;
  if (static_cast<int32_t>(now - nowPlayingHoldUntilMs) < 0) return;
  if (nowPlayingHoldingAtEnd) {
    nowPlayingOffset = 0;
    nowPlayingHoldingAtEnd = false;
    nowPlayingHoldUntilMs = now + 700;
    drawNowPlayingTicker();
    return;
  }
  const int maxOffset = width - 304;
  nowPlayingOffset = min(nowPlayingOffset + 2, maxOffset);
  drawNowPlayingTicker();
  if (nowPlayingOffset == maxOffset) {
    nowPlayingHoldingAtEnd = true;
    nowPlayingHoldUntilMs = now + 1200;
  }
}

const char *selectedGameAppId() {
  return cachedGameCount == 0 ? "" : cachedGames[selectedGameIndex].appId;
}
} // namespace mkshft_ui
