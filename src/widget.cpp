#include <widget.hpp>

namespace mkshft_ui {

Image<RGB565> *defaultCanvas = nullptr;

const WidgetType WBox::type = W_BOX;
const WidgetType WCircle::type = W_CIRCLE;
const WidgetType WTriangle::type = W_TRIANGLE;
const WidgetType WTextBox::type = W_TEXT_BOX;
const WidgetType WProgressBar::type = W_PROGRESS_BAR;

void _printBox(iBox2 b) {
#ifdef DEBUG
  Serial.print("min: (");
  Serial.print(b.minX);
  Serial.print(",");
  Serial.print(b.minY);
  Serial.print(") | max: (");
  Serial.print(b.maxX);
  Serial.print(",");
  Serial.print(b.maxY);
  Serial.print(")");
#endif
}

void Widget::setAnchor(uint16_t x, uint16_t y) { anchor = iVec2(x, y); }

void Widget::setSize(uint16_t lx, uint16_t ly) {
#ifdef DEBUG
  Serial.print("Setting size of widget ");
  Serial.print(id.data());
  Serial.print(" to : ");
  Serial.print(lx);
  Serial.print("x");
  Serial.print(ly);
  Serial.println();
#endif
  box = iBox2(anchor.x, lx, anchor.y, ly);
  center = box.center();
  dimensions = iVec2(box.lx(), box.ly());

  // this generates new border box
  setBorderWidth(borderWidth);
}

void Widget::setMargin(uint16_t all) { setMargin(all, all, all, all); }

void Widget::setMargin(uint16_t vert, uint16_t horz) {
  setMargin(vert, horz, vert, horz);
}

void Widget::setMargin(uint16_t top, uint16_t horz, uint16_t bottom) {
  setMargin(top, horz, bottom, horz);
}

void Widget::setMargin(uint16_t top, uint16_t right, uint16_t bottom,
                       uint16_t left) {
  margin[0] = top;
  margin[1] = right;
  margin[2] = bottom;
  margin[3] = left;
}

void Widget::setBorderWidth(int bw) {
  borderWidth = bw;
  borderBox = iBox2(box);
  borderBox.minX -= borderWidth;
  borderBox.minY -= borderWidth;
  borderBox.maxX += borderWidth;
  borderBox.maxY += borderWidth;
}

void Widget::setCornerRadius(int r) {
  cornerRadius = r;
  if (cornerRadius > 0) {
    borderRadius = cornerRadius + borderWidth;
  }
}

void Widget::render() {
#ifdef DEBUG_LOOP
  Serial.print("Rendering widget \"");
  Serial.print(id.data());
  Serial.print("\" : ");
  _printBox(box);
  Serial.println();
#endif
  if (borderWidth > 0) {
    canvas->fillRoundRect(borderBox, borderRadius, borderColor, opacity);
  }
  canvas->fillRoundRect(box, cornerRadius, fillColor, opacity);
}

void Widget::_generateParameters() {
  int minx = anchor.x;
  int maxx = anchor.x + dimensions.x;
  int miny = anchor.y;
  int maxy = anchor.y + dimensions.y;
  cornerRadius = 0;
  box = iBox2(minx, maxx, miny, maxy);
  center = box.center();
  fillColor = RGB32(0, 0, 0);
  borderColor = RGB32(0, 0, 0);
  borderRadius = 0;
  borderWidth = 0;
  opacity = 1.0f;
  borderBox = iBox2(box);

#ifdef DEBUG
  Serial.print("Generating parameters for widget \"");
  Serial.print(id.data());
  Serial.print("\"...");
  Serial.println();
  // _printBox(iBox2(anchor.x, dimensions.x, anchor.y, dimensions.y));
  // Serial.println();
#endif
}

void WTriangle::render() {
  // triangles do not have a borderless fill function
  canvas->fillTriangle(pA, pB, pC, fillColor, fillColor, opacity);
}

void WTextBox::setSize(uint16_t lx, uint16_t ly) {
  box = iBox2(anchor.x, lx, anchor.y, ly);
  center = box.center();
  dimensions = iVec2(box.lx(), box.ly());

  maxCharsInLine = lx / fontSz.x;
  maxLines = ly / fontSz.y;

  setBorderWidth(borderWidth);

  refitText();
}

void WTextBox::fastSetText(std::string txt) {
  txt = removeControlChars(txt);
  if (txt.size() <= maxCharsInLine) {
    contents.clear();
    contents.append(removeControlChars(txt));
    contentByLine.clear();
    contentByLine.push_back(contents);
  }
}

void WTextBox::setText(std::string txt) {
  contents.clear();
  contents.append(removeControlChars(txt));
  refitText();
}

void WTextBox::refitText() {
  int16_t lines = 0;
  int16_t charsInLine = 0;

  Serial.println("Refitting text: ");
  Serial.println(contents.data());
  Serial.print("maxlines: ");
  Serial.print(maxLines);
  Serial.print(" | maxchars: ");
  Serial.println(maxCharsInLine);

  contentByLine.clear();
  contentByLine.push_back("");
  auto currentLine = contentByLine.begin();
  for (auto curChar : contents) {
    if (lines == maxLines) {
      Serial.print("breaking at line ");
      Serial.println(lines);
      break;
    }
    if (charsInLine >= maxCharsInLine) {
      charsInLine = 0;
      ++lines;

      contentByLine.push_back(std::string());

      // resetting the invalidated currentLine iterator to the last element
      // of the contentByLine vector
      currentLine = contentByLine.end();
      --currentLine;
    }
    currentLine->push_back(curChar);
    switch (curChar) {
    case '\n': {
      charsInLine = 0;
      ++lines;
      break;
    }
    default: {
      ++charsInLine;
      break;
    }
    }
  }

  Serial.println("Refitting complete");
}

void WTextBox::render() {
  auto line = contentByLine.begin();
  auto end = contentByLine.end();
  uint8_t lineCount = 0;
  iVec2 cursor = iVec2(anchor.x, anchor.y + fontSz.y);

  // while loop avoids rendering empty text boxes
  while (line != end && lineCount < maxLines) {
    canvas->drawText(line->data(), cursor, *fontFace, fillColor);
    ++line;
    ++lineCount;
    cursor.y += fontSz.y;
  }
}

void WTextBox::_generateExtraParameters() {
  fontFace = baseFont;

#ifdef DEBUG
  Serial.print("Calculating font size of textbox \"");
  Serial.print(id.data());
  Serial.print("\"... ");
  Serial.println();
#endif

  // setting the x-width through the graphics library
  int xadv = 0;
  auto fontBox = canvas->measureChar('A', iVec2(0, 0), *fontFace,
                                     DEFAULT_TEXT_ANCHOR, &xadv);
  fontSz.x = xadv;
  fontSz.y = fontFace->yAdvance;

  clip = true;
  wrap = true;

  Serial.print("got: ");
  Serial.print(fontSz.x);
  Serial.print("x");
  Serial.print(fontSz.y);
  Serial.println();

  Serial.print("Max lines shown in box: ");
  Serial.print(maxLines);
  Serial.println();
}

void WProgressBar::setFillDirection(FILL_DIRECTION newFD) {
  if (fillDirection != newFD) {
    RGB32 tempFill = fillColor;
    fillColor = RGB32_Transparent;
    // reset progressBox to max size
    setSize(box.lx(), box.ly());
    switch (newFD) {
    case UP:
      progressBoxMax = progressBox.ly();
      break;
    case RIGHT:
      progressBoxMax = progressBox.lx();
      break;
    default:
      // do nothing for now
      break;
    }
    setProgress(progress);
    fillDirection = newFD;
    fillColor = tempFill;
  }
}

void WProgressBar::setSize(uint16_t lx, uint16_t ly) {
  box = iBox2(anchor.x, dimensions.x, anchor.y, dimensions.y);
  progressBox = iBox2(box);

  progressBox.minX += 8;
  progressBox.maxX -= 8;
  progressBox.minY += 8;
  progressBox.maxY -= 8;
  Serial.print("Setting progress box: ");
  _printBox(progressBox);
  Serial.println();

  switch (fillDirection) {
  case RIGHT:
  case LEFT:
    fProgressRatio = (float)progressBox.lx() / 100.0f;
    break;
  case UP:
  case DOWN:
    fProgressRatio = (float)progressBox.ly() / 100.0f;
    break;
  default:
    break;
  }
}

void WProgressBar::setProgress(int prog) {
  if (prog >= 100) {
    progress = 100;
  } else if (prog < 0) {
    progress = 0;
  } else {
    progress = prog;
  }

  Serial.print("Setting progress: ");
  Serial.println(progress);

  float fProgGap = (float)progress * fProgressRatio;
  int iProgGap = round(fProgGap);

  switch (fillDirection) {
  case RIGHT:
    progressBox.maxX = progressBox.minX + iProgGap;
    break;
  case UP:
    progressBox.minY = progressBox.maxY - iProgGap;
    break;
  default:
    break;
  }
  Serial.println('c');
}

void WProgressBar::render() {
  // if (borderWidth > 0) {
  //   canvas->fillRoundRect(borderBox, borderRadius, borderColor, opacity);
  // }
  // canvas->fillRoundRect(box, cornerRadius, backgroundColor, opacity);
  canvas->fillRect(progressBox, backgroundColor);

  _printBox(progressBox);
  Serial.println();

  // canvas->fillRoundRect(progressBox, cornerRadius, fillColor, opacity);
}

void setDefaultCanvas(Image<RGB565> *cnv) { defaultCanvas = cnv; }

uint8_t smallest(uint8_t a, uint8_t b, uint8_t c) {
  if (a < b) {
    if (a < c) {
      return a;
    } else {
      return c;
    }
  } else { // if (b < a)
    if (b < c) {
      return b;
    } else {
      return c;
    }
  }
}
uint8_t largest(uint8_t a, uint8_t b, uint8_t c) {
  if (a > b) {
    if (a > c) {
      return a;
    } else {
      return c;
    }
  } else { // if (b > a)
    if (b > c) {
      return b;
    } else {
      return c;
    }
  }
}

std::string removeControlChars(std::string txt) {
  auto end = txt.size();
  auto curChar = txt.begin();

  for (int n = 0; n != end; ++n) {
    if (std::iscntrl(*curChar) && *curChar != '\n') {
      curChar = txt.erase(curChar);
      --end;
    } else {
      ++curChar;
    }
  }
  return txt;
}

} // namespace mkshft_ui
