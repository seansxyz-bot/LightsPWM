#pragma once
#include <Arduino.h>

struct RGBColor {
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
};

class ThemeFileReader {
public:
  static constexpr uint8_t MAX_THEMES = 32;
  static constexpr uint8_t MAX_COLORS_PER_THEME = 64;

  ThemeFileReader();

  void clear();

  bool commitTheme(uint8_t themeId, const RGBColor* colors, uint8_t count);

  bool hasTheme(uint8_t themeId) const;
  uint8_t colorCount(uint8_t themeId) const;
  const RGBColor* colors(uint8_t themeId) const;
  RGBColor firstColor(uint8_t themeId) const;

private:
  RGBColor themes_[MAX_THEMES][MAX_COLORS_PER_THEME];
  uint8_t counts_[MAX_THEMES];
  bool valid_[MAX_THEMES];
};