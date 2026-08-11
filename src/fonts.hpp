#ifndef FONTS_H_
#define FONTS_H_

#include <Arduino.h>
#include <tgx.h>
#include <map>
#include <string>
#include <Fonts/FreeSans9pt7b.h>

#include <fonts/iosevka-mkshft-thin8pt7b.h>
#include <fonts/iosevka-mkshft-bold8pt7b.h>

#include <fonts/iosevka-mkshft-regular9pt7b.h>
#include <fonts/iosevka-mkshft-thin7pt7b.h>

inline namespace fonts {

#define defFont(STYLE) &iosevka_mkshft_##STYLE##pt7b
#define defAdafruitFont(NAME) &NAME

extern const GFXfont * baseFont;
// Note to self: const <T>* is a non-constant pointer to a constant type,
// while <T> * const is a constant pointer to a non-constant type.

} // namespace fonts

#endif // FONTS_H_
