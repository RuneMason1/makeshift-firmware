#ifndef MKSHFT_UI_H_
#define MKSHFT_UI_H_

#include <Arduino.h>

#include <map>
#include <string>
#include <vector>


#include <widget.hpp>
#include <mkshft_led.hpp>
#include <splash565.h>

namespace mkshft_ui {
enum SplashImageId : uint8_t {
  SPLASH_HOME_DEFAULT = 0,
  SPLASH_MAKESHIFT = 1,
  SPLASH_EOS = 2,
};
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
void playBootSequence();

void setUsbConnected(bool connected);
bool applyVisualPreferences(uint8_t splashImageId, uint8_t ledR, uint8_t ledG,
                            uint8_t ledB, uint8_t usbConnectedR,
                            uint8_t usbConnectedG, uint8_t usbConnectedB,
                            uint8_t usbDisconnectedR,
                            uint8_t usbDisconnectedG,
                            uint8_t usbDisconnectedB);

// Generic collection-card artwork is transferred in bounded RGB565 chunks over
// the existing SLIP transport. A card is not displayed until the complete
// image commits. The current wire protocol still uses legacy GAME_* packet
// names for compatibility with existing ctrl/agent code.
constexpr uint16_t COLLECTION_ART_MAX_WIDTH = 80;
constexpr uint16_t COLLECTION_ART_MAX_HEIGHT = 80;
// This is storage implementation detail, not a host-visible carousel limit.
// Keep the LED-approved seven-buffer footprint until an expanded-cache build
// has separately passed physical cold-boot validation.
constexpr uint8_t MEDIA_CACHE_SLOTS = 7;
constexpr size_t COLLECTION_TITLE_MAX_LENGTH = 48;
constexpr size_t COLLECTION_ITEM_ID_MAX_LENGTH = 12;
constexpr size_t COLLECTION_LIST_MAX_ITEMS = 64;
constexpr size_t COLLECTION_ACTION_LABEL_MAX_LENGTH = 24;

// Source compatibility for external code that still uses the original names.
constexpr uint16_t GAME_ART_MAX_WIDTH = COLLECTION_ART_MAX_WIDTH;
constexpr uint16_t GAME_ART_MAX_HEIGHT = COLLECTION_ART_MAX_HEIGHT;
constexpr uint8_t GAME_ART_CACHE_SLOTS = MEDIA_CACHE_SLOTS;
constexpr size_t GAME_TITLE_MAX_LENGTH = COLLECTION_TITLE_MAX_LENGTH;
constexpr size_t GAME_APP_ID_MAX_LENGTH = COLLECTION_ITEM_ID_MAX_LENGTH;
constexpr size_t GAME_LIST_MAX_ITEMS = COLLECTION_LIST_MAX_ITEMS;

bool beginCollectionCard(uint8_t slot, uint8_t itemIndex, const char *title,
                         size_t titleLength, uint16_t width,
                         uint16_t height);
bool writeCollectionArtChunk(uint32_t pixelOffset, const uint8_t *data,
                             size_t dataLength);
bool commitCollectionCard();
bool bindCollectionAsset(uint32_t key, uint8_t itemIndex);
bool beginCollectionList(uint8_t expectedCount);
bool addCollectionListItem(const char *itemId, size_t itemIdLength,
                           const char *title, size_t titleLength);
bool commitCollectionList();
bool setCollectionPresentation(const char *idleLabel, size_t idleLength,
                               const char *activeLabel, size_t activeLength);
bool isLocalCollectionActive();
void moveLocalCollectionSelection(int delta);
void showCollectionLaunchFeedback();
bool updateLocalCollectionTimeout();
const char *selectedCollectionItemId();

bool beginGameCard(uint8_t slot, uint8_t gameIndex, const char *title,
                   size_t titleLength, uint16_t width, uint16_t height);
bool writeGameArtChunk(uint32_t pixelOffset, const uint8_t *data,
                       size_t dataLength);
bool commitGameCard();
void showHomeScreen();
bool beginGameList(uint8_t expectedCount);
bool addGameListItem(const char *appId, size_t appIdLength, const char *title,
                     size_t titleLength);
bool commitGameList();
bool isLocalGameCarouselActive();
bool isGameCardVisible();
void moveLocalGameSelection(int delta);
bool updateGameCarouselTimeout();
void showGoXlrStatus(bool adjusting, bool muted, const char *name,
                     size_t nameLength, uint8_t percent);
bool showStatusBadge(uint8_t zone, bool adjusting, bool inactive,
                     const char *name, size_t nameLength, uint8_t percent);
void updateOverlayTimeout();
bool showOverlayGlyph(uint8_t glyphId);
bool showActionGlyph(uint8_t glyphId);
void setNowPlaying(bool playing, const char *text, size_t textLength);
void updateNowPlayingTicker();
const char *selectedGameAppId();
} // namespace mkshft_ui

#endif
