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
bool collectionLaunching = false;
char collectionIdleAction[COLLECTION_ACTION_LABEL_MAX_LENGTH + 1] =
    "SELECT TO ACTIVATE";
char collectionActiveAction[COLLECTION_ACTION_LABEL_MAX_LENGTH + 1] =
    "ACTIVATING...";
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
struct StatusBadgeState {
  char label[33] = {};
  uint8_t percent = 0;
  bool inactive = false;
  bool known = false;
};

StatusBadgeState lowerLeftStatus;
StatusBadgeState lowerRightStatus;
constexpr size_t NOW_PLAYING_MAX_LENGTH = 160;
char nowPlaying[NOW_PLAYING_MAX_LENGTH + 1] = {};
bool nowPlayingActive = false;
int nowPlayingOffset = 0;
uint32_t nowPlayingLastFrameMs = 0;
uint32_t nowPlayingHoldUntilMs = 0;
bool nowPlayingHoldingAtEnd = false;

constexpr uint32_t GAME_CARD_TIMEOUT_MS = 5000;
constexpr uint32_t ACTION_GLYPH_TIMEOUT_MS = 1500;
constexpr uint16_t BOOT_EOS_HOLD_MS = 900;
constexpr uint16_t BOOT_MAKESHIFT_HOLD_MS = 1200;
uint8_t homeSplashImageId = SPLASH_HOME_DEFAULT;
RGB32 usbConnectedColor(216, 58, 4);
RGB32 usbDisconnectedColor(72, 24, 8);

namespace {
enum HomeDirty : uint8_t {
  DIRTY_NONE = 0,
  DIRTY_WALLPAPER = 1U << 0,
  DIRTY_CONNECTION = 1U << 1,
  DIRTY_UPPER = 1U << 2,
  DIRTY_LOWER_LEFT = 1U << 3,
  DIRTY_LOWER_RIGHT = 1U << 4,
  DIRTY_ALL = DIRTY_WALLPAPER | DIRTY_CONNECTION | DIRTY_UPPER |
              DIRTY_LOWER_LEFT | DIRTY_LOWER_RIGHT,
};

uint8_t dirtyHomeZones = DIRTY_ALL;

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

void invalidateHome(uint8_t zones) { dirtyHomeZones |= zones; }

void restoreWallpaperRegion(const iBox2 &region) {
  const Image<RGB565> background = selectedHomeSplash().getCrop(region);
  defaultCanvas->blit(background, iVec2(region.minX, region.minY));
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
  if (asset == nullptr) return false;
  if (asset->format == mkshft_assets::Format::VECTOR_COMMANDS) {
    const int left = centerX - asset->width / 2;
    const int top = centerY - asset->height / 2;
    auto thickLine = [&color](int x1, int y1, int x2, int y2, uint8_t width) {
      const int half = width / 2;
      for (int offset = -half; offset <= half; ++offset) {
        defaultCanvas->drawLine(iVec2(x1 + offset, y1),
                                iVec2(x2 + offset, y2), color);
        defaultCanvas->drawLine(iVec2(x1, y1 + offset),
                                iVec2(x2, y2 + offset), color);
      }
    };
    uint16_t cursor = 0;
    while (cursor < asset->length) {
      const uint8_t operation = asset->data[cursor++];
      if (operation == 0) return cursor == asset->length;
      if (operation == 1 && cursor + 4 <= asset->length) {
        const uint8_t *p = asset->data + cursor;
        defaultCanvas->fillRect(iBox2(left + p[0], left + p[0] + p[2] - 1,
                                     top + p[1], top + p[1] + p[3] - 1), color);
        cursor += 4;
      } else if (operation == 2 && cursor + 6 <= asset->length) {
        const uint8_t *p = asset->data + cursor;
        defaultCanvas->fillTriangle(iVec2(left + p[0], top + p[1]),
                                    iVec2(left + p[2], top + p[3]),
                                    iVec2(left + p[4], top + p[5]),
                                    color, color, 1.0f);
        cursor += 6;
      } else if (operation == 3 && cursor + 5 <= asset->length) {
        const uint8_t *p = asset->data + cursor;
        thickLine(left + p[0], top + p[1], left + p[2], top + p[3], p[4]);
        cursor += 5;
      } else if (operation == 4 && cursor + 6 <= asset->length) {
        const uint8_t *p = asset->data + cursor;
        const float start = static_cast<int8_t>(p[3]) * 2.0f * PI / 180.0f;
        float end = static_cast<int8_t>(p[4]) * 2.0f * PI / 180.0f;
        if (end < start) end += 2.0f * PI;
        int previousX = left + p[0] + lroundf(cosf(start) * p[2]);
        int previousY = top + p[1] + lroundf(sinf(start) * p[2]);
        for (float angle = start + 0.04f; angle <= end + 0.001f; angle += 0.04f) {
          const int x = left + p[0] + lroundf(cosf(angle) * p[2]);
          const int y = top + p[1] + lroundf(sinf(angle) * p[2]);
          thickLine(previousX, previousY, x, y, p[5]);
          previousX = x;
          previousY = y;
        }
        cursor += 6;
      } else {
        return false;
      }
    }
    return true;
  }
  if (asset->format == mkshft_assets::Format::ALPHA_4BPP) {
    const int left = centerX - asset->width / 2;
    const int top = centerY - asset->height / 2;
    for (uint16_t pixel = 0; pixel < asset->width * asset->height; ++pixel) {
      const uint8_t packed = asset->data[pixel / 2];
      const uint8_t alpha = (pixel & 1) == 0 ? packed >> 4 : packed & 0x0F;
      if (alpha == 0) continue;
      defaultCanvas->drawPixel<false>(
          iVec2(left + pixel % asset->width, top + pixel / asset->width),
          color, static_cast<float>(alpha) / 15.0f);
    }
    return true;
  }
  if (asset->format != mkshft_assets::Format::MONO_1BPP) return false;
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
  if (!usbConnected || !lowerRightStatus.known) return;
  constexpr int right = 304;
  constexpr int labelBaseline = 230;
  constexpr int mutedIndicatorLeft = 310;
  constexpr int mutedIndicatorRight = 316;
  constexpr int mutedIndicatorTop = 220;
  constexpr int mutedIndicatorBottom = 226;
  char percent[8] = {};
  snprintf(percent, sizeof(percent), "%u%%", lowerRightStatus.percent);
  const int labelWidth = textWidth(lowerRightStatus.label);
  const int percentWidth = textWidth(percent);
  const RGB32 mutedColor(255, 64, 64);
  const RGB32 normalColor(216, 58, 4);
  defaultCanvas->drawText(percent, iVec2(right - percentWidth, 207),
                          *baseFont, RGB32(245, 240, 220));
  if (lowerRightStatus.inactive) {
    defaultCanvas->fillRect(
        iBox2(mutedIndicatorLeft, mutedIndicatorRight, mutedIndicatorTop,
              mutedIndicatorBottom),
        mutedColor);
  }
  defaultCanvas->drawText(lowerRightStatus.label,
                          iVec2(right - labelWidth, labelBaseline),
                          *baseFont,
                          lowerRightStatus.inactive ? mutedColor : normalColor);
}

void drawLowerLeftStatusBadge() {
  if (!usbConnected || !lowerLeftStatus.known) return;
  constexpr int left = 15;
  constexpr int labelBaseline = 230;
  constexpr int inactiveIndicatorLeft = 3;
  constexpr int inactiveIndicatorRight = 9;
  constexpr int inactiveIndicatorTop = 220;
  constexpr int inactiveIndicatorBottom = 226;
  char percent[8] = {};
  snprintf(percent, sizeof(percent), "%u%%", lowerLeftStatus.percent);
  const RGB32 inactiveColor(255, 64, 64);
  const RGB32 normalColor(216, 58, 4);
  defaultCanvas->drawText(percent, iVec2(left, 207), *baseFont,
                          RGB32(245, 240, 220));
  if (lowerLeftStatus.inactive) {
    defaultCanvas->fillRect(
        iBox2(inactiveIndicatorLeft, inactiveIndicatorRight,
              inactiveIndicatorTop, inactiveIndicatorBottom),
        inactiveColor);
  }
  defaultCanvas->drawText(lowerLeftStatus.label, iVec2(left, labelBaseline),
                          *baseFont,
                          lowerLeftStatus.inactive ? inactiveColor : normalColor);
}

void composeHome() {
  if (dirtyHomeZones == DIRTY_NONE || gameCardVisible || actionGlyphVisible)
    return;

  uint8_t zones = dirtyHomeZones;
  dirtyHomeZones = DIRTY_NONE;
  if ((zones & DIRTY_WALLPAPER) != 0) {
    defaultCanvas->blit(selectedHomeSplash(), iVec2(0, 0));
    zones |= DIRTY_CONNECTION | DIRTY_UPPER | DIRTY_LOWER_LEFT |
             DIRTY_LOWER_RIGHT;
  }
  if ((zones & DIRTY_CONNECTION) != 0) {
    defaultCanvas->fillRect(
        iBox2(0, 319, 0, 9),
        usbConnected ? usbConnectedColor : usbDisconnectedColor);
  }
  if ((zones & DIRTY_UPPER) != 0) {
    restoreWallpaperRegion(iBox2(0, 319, 10, 31));
    drawNowPlayingTicker();
  }
  if ((zones & DIRTY_LOWER_LEFT) != 0) {
    restoreWallpaperRegion(iBox2(0, 129, 190, 239));
    drawLowerLeftStatusBadge();
  }
  if ((zones & DIRTY_LOWER_RIGHT) != 0) {
    restoreWallpaperRegion(iBox2(190, 319, 190, 239));
    drawPersistentGoXlrLabel();
  }
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

  const char *action = collectionLaunching ? collectionActiveAction
                                           : collectionIdleAction;
  defaultCanvas->drawText(action, iVec2((320 - textWidth(action)) / 2, 226), *baseFont,
                          collectionLaunching ? RGB32(255, 184, 77)
                                              : RGB32(42, 214, 168));
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
  composeHome();
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
  invalidateHome(DIRTY_ALL);
  composeHome();

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
    // Collection input bindings are host-session state. Do not let a stale
    // carousel consume dial/button events after Ctrl reconnects.
    localGameCarouselActive = false;
    gameCardVisible = false;
    collectionLaunching = false;
    lowerLeftStatus = {};
    lowerRightStatus = {};
    nowPlayingActive = false;
    nowPlaying[0] = '\0';
    nowPlayingOffset = 0;
    nowPlayingLastFrameMs = 0;
    nowPlayingHoldUntilMs = 0;
    nowPlayingHoldingAtEnd = false;
    gameCardVisible = false;
    collectionLaunching = false;
    gameCardLastInteractionMs = 0;
    actionGlyphVisible = false;
    actionGlyphShownMs = 0;
    splashScreen();
    return;
  }
  if (gameCardVisible || actionGlyphVisible) {
    return;
  }
  invalidateHome(DIRTY_CONNECTION);
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

  if (!gameCardVisible && !actionGlyphVisible) {
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

bool bindCollectionAsset(uint32_t key, uint8_t itemIndex) {
  if (itemIndex >= cachedGameCount ||
      !mkshft_media_cache::bindKeyToItem(key, itemIndex)) return false;
  if (localGameCarouselActive && gameCardVisible && selectedGameIndex == itemIndex) {
    gameCardLastInteractionMs = millis();
    renderGameCard(findArtworkSlot(selectedGameIndex));
  }
  return true;
}

void showHomeScreen() {
  gameCardVisible = false;
  collectionLaunching = false;
  gameCardLastInteractionMs = 0;
  actionGlyphVisible = false;
  actionGlyphShownMs = 0;
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
  // A new collection must not inherit item-index artwork from the previous
  // one, but keyed assets remain reusable through CACHE_FILE_BIND.
  mkshft_media_cache::invalidateBindings();
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
  // Opening a collection is itself a visible state transition. Previously
  // the first encoder detent only committed the list; a second detent was
  // required before the selected item was drawn.
  strncpy(gameTitle, cachedGames[selectedGameIndex].title, GAME_TITLE_MAX_LENGTH);
  gameTitle[GAME_TITLE_MAX_LENGTH] = '\0';
  gameCardVisible = true;
  collectionLaunching = false;
  gameCardLastInteractionMs = millis();
  renderGameCard(findArtworkSlot(selectedGameIndex));
  return true;
}

bool setCollectionPresentation(const char *idleLabel, size_t idleLength,
                               const char *activeLabel, size_t activeLength) {
  if (idleLabel == nullptr || activeLabel == nullptr || idleLength == 0 ||
      activeLength == 0 || idleLength > COLLECTION_ACTION_LABEL_MAX_LENGTH ||
      activeLength > COLLECTION_ACTION_LABEL_MAX_LENGTH) {
    return false;
  }
  memcpy(collectionIdleAction, idleLabel, idleLength);
  collectionIdleAction[idleLength] = '\0';
  memcpy(collectionActiveAction, activeLabel, activeLength);
  collectionActiveAction[activeLength] = '\0';
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
  collectionLaunching = false;
  gameCardLastInteractionMs = millis();
  renderGameCard(findArtworkSlot(selectedGameIndex));
}

void showCollectionLaunchFeedback() {
  if (!gameCardVisible) return;
  collectionLaunching = true;
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
  StatusBadgeState &status = zone == 1 ? lowerLeftStatus : lowerRightStatus;
  memcpy(status.label, label, copyLength + 1);
  status.percent = min<uint8_t>(percent, 100);
  status.inactive = inactive;
  status.known = true;
  invalidateHome(zone == 1 ? DIRTY_LOWER_LEFT : DIRTY_LOWER_RIGHT);
  return true;
}

bool showOverlayGlyph(uint8_t glyphId) {
  // Built-in transport fallbacks cover IDs 1-5. Cue-owned assets may use any
  // non-zero cache ID, including the seek glyphs assigned after the original UI.
  if (glyphId == 0 ||
      (glyphId > 5 && mkshft_assets::find(glyphId) == nullptr))
    return false;
  gameCardVisible = false;
  collectionLaunching = false;
  actionGlyphVisible = false;
  invalidateHome(DIRTY_ALL);
  composeHome();
  actionGlyphVisible = true;
  actionGlyphShownMs = millis();

  const RGB32 shadow(3, 5, 7);
  const RGB32 panel(43, 43, 43);
  const RGB32 foreground(248, 248, 246);
  // Keep overlays focused on their 64px catalog glyph rather than obscuring
  // most of the wallpaper with the previous full-size panel.
  constexpr int left = 112;
  constexpr int right = 207;
  constexpr int top = 78;
  constexpr int bottom = 161;
  constexpr int radius = 14;
  defaultCanvas->fillRoundRect(iBox2(left + 3, right + 3, top + 4, bottom + 4),
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
  if (wasActive || playing) invalidateHome(DIRTY_UPPER);
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
    invalidateHome(DIRTY_UPPER);
    return;
  }
  const int maxOffset = width - 304;
  nowPlayingOffset = min(nowPlayingOffset + 2, maxOffset);
  invalidateHome(DIRTY_UPPER);
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
