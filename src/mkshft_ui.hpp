#ifndef MKSHFT_UI_H_
#define MKSHFT_UI_H_

#include <Arduino.h>

#include <map>
#include <string>
#include <vector>


#include <widget.hpp>
#include <splash565.h>

namespace mkshft_ui {
/**
 * Layout widget
 */
class Layout {
public:
  Layout(std::string id) : id(id){};

  std::vector<std::string> renderingOrder;
  std::map<std::string, Widget *> renderedWidgets;
  std::map<std::string, WProgressBar> progressBars;
  std::map<std::string, WTextBox> textBoxes;
  std::map<std::string, WCircle> circles;
  std::map<std::string, WTriangle> triangles;
  std::map<std::string, WBox> boxes;


  std::string getId() { return id; };

  bool addWidget(WidgetType, std::string);
  template <typename T> bool addWidget(T);
  void removeWidget(std::string);
  std::vector<std::string> getRenderStackWidgetIds();

protected:
  std::string id;
  WBox *_emplaceWidget(WBox);
  WCircle *_emplaceWidget(WCircle);
  WTriangle *_emplaceWidget(WTriangle);
  WProgressBar *_emplaceWidget(WProgressBar);
  WTextBox *_emplaceWidget(WTextBox);
  };

extern Layout *currentLayout;
extern std::map<std::string, Layout> layouts;

void init(Image<RGB565> *cnv);

void renderUI();

void splashScreen();

void setUsbConnected(bool connected);
} // namespace mkshft_ui

#endif
