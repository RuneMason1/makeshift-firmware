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

void renderGameCard() {
  defaultCanvas->fillRect(iBox2(0, 319, 0, 239), RGB32(8, 13, 18));
  defaultCanvas->fillRect(iBox2(0, 319, 0, 9), RGB32(255, 184, 77));
  defaultCanvas->drawText("GAME LIBRARY", iVec2(24, 31), *baseFont,
                          RGB32(255, 184, 77));

  const int artX = 16;
  const int artY = 48 + ((160 - gameArtHeight) / 2);
  defaultCanvas->fillRect(iBox2(12, 179, 44, 211), RGB32(18, 29, 38));
  Image<RGB565> artwork(gameArtwork, gameArtWidth, gameArtHeight);
  defaultCanvas->blit(artwork, iVec2(artX + ((160 - gameArtWidth) / 2), artY));

  drawGameTitle(gameTitle);
  defaultCanvas->drawText("<  TURN  >", iVec2(192, 174), *baseFont,
                          RGB32(151, 166, 175));
  defaultCanvas->drawText("PRESS TO PLAY", iVec2(192, 204), *baseFont,
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
  // Keep the first screen independent of the unfinished layout dispatcher so
  // it can also serve as a clear display and orientation diagnostic.
  defaultCanvas->fillRect(iBox2(0, 319, 0, 239), RGB32(8, 13, 18));
  defaultCanvas->fillRect(iBox2(0, 319, 0, 9), RGB32(42, 214, 168));
  defaultCanvas->fillRect(iBox2(20, 299, 28, 211), RGB32(18, 29, 38));

  defaultCanvas->drawText("MAKESHIFT", iVec2(40, 72), *baseFont,
                          RGB32(42, 214, 168));
  defaultCanvas->drawText("CONTROLLER READY", iVec2(40, 112), *baseFont,
                          RGB32(245, 240, 220));
  defaultCanvas->drawText("4 KNOBS  /  12 BUTTONS", iVec2(40, 150), *baseFont,
                          RGB32(151, 166, 175));
  setUsbConnected(false);

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
  if (gameCardVisible) {
    return;
  }
  defaultCanvas->fillRect(iBox2(36, 284, 164, 194), RGB32(18, 29, 38));
  defaultCanvas->drawText(connected ? "USB LINK: CONNECTED" : "USB LINK: WAITING",
                          iVec2(40, 184), *baseFont,
                          connected ? RGB32(42, 214, 168)
                                    : RGB32(255, 184, 77));
}

bool beginGameCard(const char *title, size_t titleLength, uint16_t width,
                   uint16_t height) {
  if (title == nullptr || titleLength == 0 ||
      titleLength > GAME_TITLE_MAX_LENGTH || width == 0 || height == 0 ||
      width > GAME_ART_MAX_WIDTH || height > GAME_ART_MAX_HEIGHT) {
    gameTransferActive = false;
    return false;
  }

  memcpy(gameTitle, title, titleLength);
  gameTitle[titleLength] = '\0';
  gameArtWidth = width;
  gameArtHeight = height;
  gamePixelsReceived = 0;
  gameTransferActive = true;
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
  renderGameCard();
  return true;
}

void showHomeScreen() {
  const bool wasUsbConnected = usbConnected;
  gameTransferActive = false;
  gameCardVisible = false;
  splashScreen();
  setUsbConnected(wasUsbConnected);
}
} // namespace mkshft_ui
