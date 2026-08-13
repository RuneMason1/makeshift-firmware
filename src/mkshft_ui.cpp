#include <mkshft_ui.hpp>
#include <mkshft_assets.hpp>
#include <mkshft_display.hpp>
#include <mkshft_media_cache.hpp>
#include <eos_splash565.h>
#include <makeshift_boot_splash565.h>

namespace mkshft_ui {

Layout *currentLayout;
std::map<std::string, Layout> layouts;

char gameTitle[GAME_TITLE_MAX_LENGTH + 1] = {};
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
bool goXlrChannelMuted = false;
bool goXlrPercentKnown = false;
char lowerLeftStatusLabel[33] = {};
uint8_t lowerLeftStatusPercent = 0;
bool lowerLeftStatusInactive = false;
bool lowerLeftStatusKnown = false;
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
constexpr uint16_t BOOT_EOS_HOLD_MS = 900;
constexpr uint16_t BOOT_MAKESHIFT_HOLD_MS = 1200;
uint8_t homeSplashImageId = SPLASH_HOME_DEFAULT;
RGB32 usbConnectedColor(216, 58, 4);
RGB32 usbDisconnectedColor(72, 24, 8);

namespace {
const Image<RGB565> &selectedHomeSplash() {
  switch (homeSplashImageId) {
  case SPLASH_MAKESHIFT:
    return makeshift_boot_splash565;
  case SPLASH_EOS:
    return eos_splash565;
  case SPLASH_HOME_DEFAULT:
  default:
    return splash565;
  }
}

void presentBootSplash(const Image<RGB565> &image) {
  defaultCanvas->blit(image, iVec2(0, 0));
  if (mkshft_display::displayReady) {
    mkshft_display::tft.update(mkshft_display::fb, true);
  }
}

void presentBootStep(uint8_t, uint8_t) {
}

bool drawCachedGlyph(uint8_t assetId, int centerX, int centerY,
                     const RGB32 &color, uint8_t scale = 1) {
  const mkshft_assets::Asset *asset = mkshft_assets::find(assetId);
  if (asset == nullptr || asset->format != mkshft_assets::Format::MONO_1BPP)
    return false;
  const int left = centerX - (asset->width * scale) / 2;
  const int top = centerY - (asset->height * scale) / 2;
  for (uint16_t bit = 0; bit < asset->width * asset->height; ++bit) {
    if ((asset->data[bit / 8] & (0x80U >> (bit % 8))) == 0) continue;
    const int x = left + (bit % asset->width) * scale;
    const int y = top + (bit / asset->width) * scale;
    defaultCanvas->fillRect(iBox2(x, x + scale - 1, y, y + scale - 1), color);
  }
  return true;
}

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
  constexpr int right = 304;
  constexpr int labelBaseline = 230;
  constexpr int mutedIndicatorLeft = 310;
  constexpr int mutedIndicatorRight = 316;
  constexpr int mutedIndicatorTop = 220;
  constexpr int mutedIndicatorBottom = 226;
  char percent[8] = {};
  snprintf(percent, sizeof(percent), "%u%%", goXlrChannelPercent);
  const int labelWidth = textWidth(goXlrChannelLabel);
  const int percentWidth = textWidth(percent);
  const RGB32 mutedColor(255, 64, 64);
  const RGB32 normalColor(216, 58, 4);
  // Restore the zone from the selected wallpaper so shorter labels cannot
  // leave pixels behind without placing an opaque panel over the logo.
  const Image<RGB565> background =
      selectedHomeSplash().getCrop(iBox2(190, 319, 190, 239));
  defaultCanvas->blit(background, iVec2(190, 190));
  defaultCanvas->drawText(percent, iVec2(right - percentWidth, 207),
                          *baseFont, RGB32(245, 240, 220));
  if (goXlrChannelMuted) {
    defaultCanvas->fillRect(
        iBox2(mutedIndicatorLeft, mutedIndicatorRight, mutedIndicatorTop,
              mutedIndicatorBottom),
        mutedColor);
  }
  defaultCanvas->drawText(goXlrChannelLabel,
                          iVec2(right - labelWidth, labelBaseline),
                          *baseFont,
                          goXlrChannelMuted ? mutedColor : normalColor);
}

void drawLowerLeftStatusBadge() {
  if (!usbConnected || !lowerLeftStatusKnown) return;
  constexpr int left = 15;
  constexpr int labelBaseline = 230;
  constexpr int inactiveIndicatorLeft = 3;
  constexpr int inactiveIndicatorRight = 9;
  constexpr int inactiveIndicatorTop = 220;
  constexpr int inactiveIndicatorBottom = 226;
  char percent[8] = {};
  snprintf(percent, sizeof(percent), "%u%%", lowerLeftStatusPercent);
  const RGB32 inactiveColor(255, 64, 64);
  const RGB32 normalColor(216, 58, 4);
  const Image<RGB565> background =
      selectedHomeSplash().getCrop(iBox2(0, 129, 190, 239));
  defaultCanvas->blit(background, iVec2(0, 190));
  defaultCanvas->drawText(percent, iVec2(left, 207), *baseFont,
                          RGB32(245, 240, 220));
  if (lowerLeftStatusInactive) {
    defaultCanvas->fillRect(
        iBox2(inactiveIndicatorLeft, inactiveIndicatorRight,
              inactiveIndicatorTop, inactiveIndicatorBottom),
        inactiveColor);
  }
  defaultCanvas->drawText(lowerLeftStatusLabel, iVec2(left, labelBaseline),
                          *baseFont,
                          lowerLeftStatusInactive ? inactiveColor : normalColor);
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
  return mkshft_media_cache::findSlot(gameIndex);
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
  const mkshft_media_cache::SlotInfo *slotData =
      slot >= 0 ? mkshft_media_cache::slotInfo(slot) : nullptr;
  const uint16_t *pixels =
      slot >= 0 ? mkshft_media_cache::slotPixels(slot) : nullptr;
  if (slotData != nullptr && pixels != nullptr) {
    const uint16_t width = slotData->width;
    const uint16_t height = slotData->height;
    Image<RGB565> artwork(pixels, width, height);
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
  presentBootSplash(eos_splash565);
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
  defaultCanvas->blit(selectedHomeSplash(), iVec2(0, 0));
  defaultCanvas->fillRect(
      iBox2(0, 319, 0, 9),
      usbConnected ? usbConnectedColor : usbDisconnectedColor);
  drawNowPlayingTicker();
  drawLowerLeftStatusBadge();
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

void playBootSequence() {
  presentBootSplash(eos_splash565);
  delay(BOOT_EOS_HOLD_MS);
  presentBootSplash(makeshift_boot_splash565);
  delay(BOOT_MAKESHIFT_HOLD_MS);
  mkshft_ledMatrix::playBootSequence(&presentBootStep);
  splashScreen();
  if (mkshft_display::displayReady) {
    mkshft_display::tft.update(mkshft_display::fb, true);
  }
}

void setUsbConnected(bool connected) {
  usbConnected = connected;
  if (!connected) {
    goXlrPercentKnown = false;
    goXlrChannelLabel[0] = '\0';
    goXlrChannelPercent = 0;
    goXlrChannelMuted = false;
    lowerLeftStatusKnown = false;
    lowerLeftStatusLabel[0] = '\0';
    lowerLeftStatusPercent = 0;
    lowerLeftStatusInactive = false;
    nowPlayingActive = false;
    nowPlaying[0] = '\0';
    nowPlayingOffset = 0;
    nowPlayingLastFrameMs = 0;
    nowPlayingHoldUntilMs = 0;
    nowPlayingHoldingAtEnd = false;
    gameCardVisible = false;
    gameCardLastInteractionMs = 0;
    actionGlyphVisible = false;
    actionGlyphShownMs = 0;
    goXlrVisible = false;
    goXlrShownMs = 0;
    splashScreen();
    return;
  }
  if (gameCardVisible || actionGlyphVisible || goXlrVisible) {
    return;
  }
  defaultCanvas->fillRect(iBox2(0, 319, 0, 9),
                          connected ? usbConnectedColor : usbDisconnectedColor);
}

bool applyVisualPreferences(uint8_t splashImageId, uint8_t ledR, uint8_t ledG,
                            uint8_t ledB, uint8_t connectedR,
                            uint8_t connectedG, uint8_t connectedB,
                            uint8_t disconnectedR,
                            uint8_t disconnectedG,
                            uint8_t disconnectedB) {
  if (splashImageId > SPLASH_EOS) return false;

  homeSplashImageId = splashImageId;
  usbConnectedColor = RGB32(connectedR, connectedG, connectedB);
  usbDisconnectedColor = RGB32(disconnectedR, disconnectedG, disconnectedB);
  mkshft_ledMatrix::setBaseColor(ledR, ledG, ledB);

  if (!gameCardVisible && !actionGlyphVisible && !goXlrVisible) {
    splashScreen();
  }
  return true;
}

bool beginGameCard(uint8_t slot, uint8_t gameIndex, const char *title,
                   size_t titleLength, uint16_t width, uint16_t height) {
  return beginCollectionCard(slot, gameIndex, title, titleLength, width, height);
}

bool beginCollectionCard(uint8_t slot, uint8_t itemIndex, const char *title,
                         size_t titleLength, uint16_t width, uint16_t height) {
  const bool textOnly = width == 0 && height == 0;
  const bool validArtworkSize = width > 0 && height > 0 &&
                                width <= GAME_ART_MAX_WIDTH &&
                                height <= GAME_ART_MAX_HEIGHT;
  if (slot >= GAME_ART_CACHE_SLOTS || itemIndex >= cachedGameCount ||
      title == nullptr || titleLength == 0 ||
      titleLength > GAME_TITLE_MAX_LENGTH) {
    return false;
  }

  if (!textOnly && !validArtworkSize) return false;
  return mkshft_media_cache::beginWrite(slot, itemIndex, width, height);
}

bool writeGameArtChunk(uint32_t pixelOffset, const uint8_t *data,
                       size_t dataLength) {
  return writeCollectionArtChunk(pixelOffset, data, dataLength);
}

bool writeCollectionArtChunk(uint32_t pixelOffset, const uint8_t *data,
                             size_t dataLength) {
  return mkshft_media_cache::writeChunk(pixelOffset, data, dataLength);
}

bool commitGameCard() {
  return commitCollectionCard();
}

bool commitCollectionCard() {
  const bool committed = mkshft_media_cache::commitWrite();
  if (!committed) return false;

  if (gameCardVisible && findArtworkSlot(selectedGameIndex) >= 0) {
    gameCardLastInteractionMs = millis();
    renderGameCard(findArtworkSlot(selectedGameIndex));
  }
  return committed;
}

void showHomeScreen() {
  gameCardVisible = false;
  gameCardLastInteractionMs = 0;
  actionGlyphVisible = false;
  actionGlyphShownMs = 0;
  goXlrVisible = false;
  goXlrShownMs = 0;
  splashScreen();
}

bool beginGameList(uint8_t expectedCount) {
  return beginCollectionList(expectedCount);
}

bool beginCollectionList(uint8_t expectedCount) {
  if (expectedCount == 0 || expectedCount > GAME_LIST_MAX_ITEMS) return false;
  expectedGameCount = expectedCount;
  cachedGameCount = 0;
  localGameCarouselActive = false;
  mkshft_media_cache::invalidateAll();
  return true;
}

bool addGameListItem(const char *appId, size_t appIdLength, const char *title,
                     size_t titleLength) {
  return addCollectionListItem(appId, appIdLength, title, titleLength);
}

bool addCollectionListItem(const char *itemId, size_t itemIdLength,
                           const char *title, size_t titleLength) {
  if (cachedGameCount >= expectedGameCount || itemId == nullptr || title == nullptr ||
      itemIdLength == 0 || itemIdLength > GAME_APP_ID_MAX_LENGTH ||
      titleLength == 0 || titleLength > GAME_TITLE_MAX_LENGTH) return false;
  CachedGame &game = cachedGames[cachedGameCount++];
  memcpy(game.appId, itemId, itemIdLength);
  game.appId[itemIdLength] = '\0';
  memcpy(game.title, title, titleLength);
  game.title[titleLength] = '\0';
  return true;
}

bool commitGameList() {
  return commitCollectionList();
}

bool commitCollectionList() {
  if (cachedGameCount == 0 || cachedGameCount != expectedGameCount) return false;
  selectedGameIndex = 0;
  localGameCarouselActive = true;
  return true;
}

bool isLocalGameCarouselActive() { return localGameCarouselActive; }
bool isLocalCollectionActive() { return isLocalGameCarouselActive(); }
bool isGameCardVisible() { return gameCardVisible; }

void moveLocalGameSelection(int delta) {
  moveLocalCollectionSelection(delta);
}

void moveLocalCollectionSelection(int delta) {
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
  gameCardLastInteractionMs = millis();
  renderGameCard(findArtworkSlot(selectedGameIndex));
}

bool updateGameCarouselTimeout() {
  return updateLocalCollectionTimeout();
}

bool updateLocalCollectionTimeout() {
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

void showGoXlrStatus(bool adjusting, bool muted, const char *name,
                     size_t nameLength, uint8_t percent) {
  showStatusBadge(2, adjusting, muted, name, nameLength, percent);
}

bool showStatusBadge(uint8_t zone, bool, bool inactive, const char *name,
                     size_t nameLength, uint8_t percent) {
  if ((zone != 1 && zone != 2) || name == nullptr || nameLength == 0)
    return false;
  char label[33] = {};
  const size_t copyLength = min(nameLength, sizeof(label) - 1);
  memcpy(label, name, copyLength);
  if (zone == 1) {
    memcpy(lowerLeftStatusLabel, label, copyLength + 1);
    lowerLeftStatusPercent = min<uint8_t>(percent, 100);
    lowerLeftStatusInactive = inactive;
    lowerLeftStatusKnown = true;
    drawLowerLeftStatusBadge();
    return true;
  }
  memcpy(goXlrChannelLabel, label, copyLength + 1);
  goXlrChannelPercent = min<uint8_t>(percent, 100);
  goXlrChannelMuted = inactive;
  goXlrPercentKnown = true;
  goXlrVisible = false;
  goXlrShownMs = 0;
  drawPersistentGoXlrLabel();
  return true;
}

bool showOverlayGlyph(uint8_t glyphId) {
  if (glyphId < 1 || glyphId > 5) return false;
  gameCardVisible = false;
  actionGlyphVisible = true;
  actionGlyphShownMs = millis();

  const RGB32 shadow(3, 5, 7);
  const RGB32 panel(43, 43, 43);
  const RGB32 foreground(248, 248, 246);
  constexpr int left = 72;
  constexpr int right = 247;
  constexpr int top = 46;
  constexpr int bottom = 193;
  constexpr int radius = 20;
  defaultCanvas->fillRoundRect(iBox2(left + 4, right + 4, top + 5, bottom + 5),
                               radius, shadow, 0.45f);
  defaultCanvas->fillRoundRect(iBox2(left, right, top, bottom), radius, panel,
                               0.72f);

  if (drawCachedGlyph(glyphId, 160, 120, foreground, 2)) {
    return true;
  } else if (glyphId == 1) {
    defaultCanvas->fillRect(iBox2(106, 113, 92, 148), foreground);
    defaultCanvas->fillTriangle(iVec2(157, 92), iVec2(157, 148),
                                iVec2(110, 120), foreground, foreground, 1.0f);
    defaultCanvas->fillTriangle(iVec2(213, 92), iVec2(213, 148),
                                iVec2(166, 120), foreground, foreground, 1.0f);
  } else if (glyphId == 3) {
    defaultCanvas->fillTriangle(iVec2(100, 92), iVec2(100, 148),
                                iVec2(147, 120), foreground, foreground, 1.0f);
    defaultCanvas->fillTriangle(iVec2(156, 92), iVec2(156, 148),
                                iVec2(203, 120), foreground, foreground, 1.0f);
    defaultCanvas->fillRect(iBox2(206, 219, 92, 148), foreground);
  } else if (glyphId == 4) {
    defaultCanvas->fillRect(iBox2(108, 127, 105, 135), foreground);
    defaultCanvas->fillTriangle(iVec2(127, 105), iVec2(127, 135),
                                iVec2(153, 120), foreground, foreground, 1.0f);
    for (int offset = -2; offset <= 2; ++offset) {
      defaultCanvas->drawLine(iVec2(174 + offset, 106),
                              iVec2(202 + offset, 134), foreground);
      defaultCanvas->drawLine(iVec2(202 + offset, 106),
                              iVec2(174 + offset, 134), foreground);
    }
  } else if (glyphId == 5) {
    defaultCanvas->fillRect(iBox2(108, 127, 105, 135), foreground);
    defaultCanvas->fillTriangle(iVec2(127, 105), iVec2(127, 135),
                                iVec2(153, 120), foreground, foreground, 1.0f);
    Image<RGB565> waves(*defaultCanvas, iBox2(154, 211, 88, 152));
    for (int radius = 15; radius <= 17; ++radius)
      waves.drawCircle(iVec2(0, 32), radius, foreground);
    for (int radius = 27; radius <= 29; ++radius)
      waves.drawCircle(iVec2(0, 32), radius, foreground);
  } else {
    defaultCanvas->fillTriangle(iVec2(99, 92), iVec2(99, 148),
                                iVec2(145, 120), foreground, foreground, 1.0f);
    defaultCanvas->fillRect(iBox2(174, 189, 92, 148), foreground);
    defaultCanvas->fillRect(iBox2(204, 219, 92, 148), foreground);
  }
  return true;
}

bool showActionGlyph(uint8_t glyphId) { return showOverlayGlyph(glyphId); }

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
  return selectedCollectionItemId();
}

const char *selectedCollectionItemId() {
  return cachedGameCount == 0 ? "" : cachedGames[selectedGameIndex].appId;
}
} // namespace mkshft_ui
