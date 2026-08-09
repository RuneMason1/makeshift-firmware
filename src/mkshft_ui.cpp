#include <mkshft_ui.hpp>

namespace mkshft_ui {

Layout *currentLayout;
std::map<std::string, Layout> layouts;

DMAMEM uint16_t gameArtwork[GAME_ART_MAX_WIDTH * GAME_ART_MAX_HEIGHT];
char gameTitle[GAME_TITLE_MAX_LENGTH + 1] = {};
uint16_t gameArtWidth = 0;
uint16_t gameArtHeight = 0;
uint32_t gamePixelsReceived = 0;
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

constexpr uint32_t GAME_CARD_TIMEOUT_MS = 5000;
constexpr uint32_t ACTION_GLYPH_TIMEOUT_MS = 1500;

namespace {
void drawGameTitle(const char *title) {
  constexpr size_t lineLength = 17;
  char line[lineLength + 1] = {};
  size_t source = 0;
  int y = 82;

  for (uint8_t lineNumber = 0; lineNumber < 3 && title[source] != '\0';
       ++lineNumber) {
    size_t count = 0;
    while (count < lineLength && title[source] != '\0') {
      line[count++] = title[source++];
    }
    line[count] = '\0';
    defaultCanvas->drawText(line, iVec2(192, y), *baseFont,
                            RGB32(245, 240, 220));
    y += 24;
  }
}

void renderGameCard(bool artworkReady) {
  defaultCanvas->fillRect(iBox2(0, 319, 0, 239), RGB32(8, 13, 18));
  defaultCanvas->fillRect(iBox2(0, 319, 0, 9), RGB32(255, 184, 77));
  defaultCanvas->drawText("GAME LIBRARY", iVec2(24, 31), *baseFont,
                          RGB32(255, 184, 77));

  const int artX = 16;
  const int artY = 48;
  defaultCanvas->fillRect(iBox2(12, 179, 44, 211), RGB32(18, 29, 38));
  if (artworkReady) {
    Image<RGB565> artwork(gameArtwork, gameArtWidth, gameArtHeight);
    for (int y = 0; y < 160; ++y) {
      const int sourceY = (y * gameArtHeight) / 160;
      for (int x = 0; x < 160; ++x) {
        const int sourceX = (x * gameArtWidth) / 160;
        defaultCanvas->drawPixel<false>(iVec2(artX + x, artY + y),
                                        artwork(sourceX, sourceY));
      }
    }
  } else {
    defaultCanvas->drawText("LOADING ART", iVec2(48, 132), *baseFont,
                            RGB32(151, 166, 175));
  }

  drawGameTitle(gameTitle);
  defaultCanvas->drawText("<  TURN  >", iVec2(192, 174), *baseFont,
                          RGB32(151, 166, 175));
  defaultCanvas->drawText("PRESS TO PLAY", iVec2(192, 204), *baseFont,
                          RGB32(42, 214, 168));
}

void renderTextGameTitle() {
  defaultCanvas->fillRect(iBox2(20, 299, 54, 171), RGB32(18, 29, 38));

  const GFXfont &titleFont = iosevka_mkshft_regular9pt7b;
  constexpr size_t lineLength = 25;
  char lines[3][lineLength + 1] = {};
  uint8_t lineCount = 0;
  const char *cursor = gameTitle;

  while (*cursor != '\0' && lineCount < 3) {
    while (*cursor == ' ') ++cursor;
    size_t used = 0;
    while (*cursor != '\0') {
      const char *wordStart = cursor;
      while (*cursor != '\0' && *cursor != ' ') ++cursor;
      const size_t wordLength = cursor - wordStart;
      const size_t separator = used == 0 ? 0 : 1;
      if (used > 0 && used + separator + wordLength > lineLength) break;
      if (separator) lines[lineCount][used++] = ' ';
      const size_t copyLength = min(wordLength, lineLength - used);
      memcpy(lines[lineCount] + used, wordStart, copyLength);
      used += copyLength;
      if (copyLength < wordLength) cursor = wordStart + copyLength;
      while (*cursor == ' ') ++cursor;
      if (used == lineLength) break;
    }
    lines[lineCount][used] = '\0';
    ++lineCount;
  }

  const int firstY = 87 + ((3 - lineCount) * 14);
  for (uint8_t line = 0; line < lineCount; ++line) {
    int advance = 0;
    defaultCanvas->measureChar('A', iVec2(0, 0), titleFont,
                               DEFAULT_TEXT_ANCHOR, &advance);
    const int x = max(12, (320 - static_cast<int>(strlen(lines[line])) * advance) / 2);
    defaultCanvas->drawText(lines[line], iVec2(x, firstY + line * 30),
                            titleFont, RGB32(245, 240, 220));
  }
}

void renderTextGameCard() {
  defaultCanvas->fillRect(iBox2(0, 319, 0, 239), RGB32(8, 13, 18));
  defaultCanvas->fillRect(iBox2(0, 319, 0, 9), RGB32(255, 184, 77));
  defaultCanvas->drawText("GAME LIBRARY", iVec2(24, 31), *baseFont,
                          RGB32(255, 184, 77));
  renderTextGameTitle();

  defaultCanvas->drawText("< TURN >", iVec2(36, 204), *baseFont,
                          RGB32(151, 166, 175));
  defaultCanvas->drawText("PRESS TO PLAY", iVec2(174, 204), *baseFont,
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
  if (gameCardVisible || actionGlyphVisible) {
    return;
  }
  defaultCanvas->fillRect(iBox2(0, 319, 0, 9),
                          connected ? RGB32(216, 58, 4)
                                    : RGB32(72, 24, 8));
}

bool beginGameCard(const char *title, size_t titleLength, uint16_t width,
                   uint16_t height) {
  const bool textOnly = width == 0 && height == 0;
  const bool validArtworkSize = width > 0 && height > 0 &&
                                width <= GAME_ART_MAX_WIDTH &&
                                height <= GAME_ART_MAX_HEIGHT;
  if (title == nullptr || titleLength == 0 ||
      titleLength > GAME_TITLE_MAX_LENGTH ||
      (!textOnly && !validArtworkSize)) {
    gameTransferActive = false;
    return false;
  }

  memcpy(gameTitle, title, titleLength);
  gameTitle[titleLength] = '\0';
  gameArtWidth = width;
  gameArtHeight = height;
  gamePixelsReceived = 0;
  gameTransferActive = !textOnly;
  gameCardVisible = true;
  actionGlyphVisible = false;
  gameCardLastInteractionMs = millis();
  if (textOnly) renderTextGameCard();
  else renderGameCard(false);
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
    gameArtwork[pixelOffset + index] =
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
  gameCardVisible = true;
  gameCardLastInteractionMs = millis();
  renderGameCard(true);
  return true;
}

void showHomeScreen() {
  const bool wasUsbConnected = usbConnected;
  gameTransferActive = false;
  gameCardVisible = false;
  gameCardLastInteractionMs = 0;
  actionGlyphVisible = false;
  actionGlyphShownMs = 0;
  splashScreen();
  setUsbConnected(wasUsbConnected);
}

bool beginGameList(uint8_t expectedCount) {
  if (expectedCount == 0 || expectedCount > GAME_LIST_MAX_ITEMS) return false;
  expectedGameCount = expectedCount;
  cachedGameCount = 0;
  localGameCarouselActive = false;
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
  if (wasVisible) renderTextGameTitle();
  else renderTextGameCard();
}

void updateGameCarouselTimeout() {
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
}

void showPlayPauseGlyph() {
  gameTransferActive = false;
  gameCardVisible = false;
  actionGlyphVisible = true;
  actionGlyphShownMs = millis();

  const RGB32 background(8, 13, 18);
  const RGB32 orange(216, 58, 4);
  const RGB32 foreground(245, 240, 220);
  defaultCanvas->fillRect(iBox2(0, 319, 0, 239), background);
  defaultCanvas->fillRect(iBox2(0, 319, 0, 9),
                          usbConnected ? orange : RGB32(72, 24, 8));
  defaultCanvas->drawText("MEDIA", iVec2(132, 42), *baseFont, orange);
  defaultCanvas->fillTriangle(iVec2(67, 76), iVec2(67, 178),
                              iVec2(151, 127), foreground, foreground, 1.0f);
  defaultCanvas->fillRect(iBox2(184, 211, 76, 178), foreground);
  defaultCanvas->fillRect(iBox2(231, 258, 76, 178), foreground);
}

const char *selectedGameAppId() {
  return cachedGameCount == 0 ? "" : cachedGames[selectedGameIndex].appId;
}
} // namespace mkshft_ui
