#include "theme_file_reader.h"

ThemeFileReader::ThemeFileReader() {
  clear();
}

void ThemeFileReader::clear() {
  for (uint8_t t = 0; t < MAX_THEMES; ++t) {
    counts_[t] = 0;
    valid_[t] = false;
    for (uint8_t i = 0; i < MAX_COLORS_PER_THEME; ++i) {
      themes_[t][i] = {0, 0, 0};
    }
  }
}

bool ThemeFileReader::commitTheme(uint8_t themeId, const RGBColor* colors, uint8_t count) {
  if (themeId >= MAX_THEMES) return false;
  if (!colors) return false;
  if (count == 0 || count > MAX_COLORS_PER_THEME) return false;

  for (uint8_t i = 0; i < count; ++i) {
    themes_[themeId][i] = colors[i];
  }

  counts_[themeId] = count;
  valid_[themeId] = true;
  return true;
}

bool ThemeFileReader::hasTheme(uint8_t themeId) const {
  if (themeId >= MAX_THEMES) return false;
  return valid_[themeId];
}

uint8_t ThemeFileReader::colorCount(uint8_t themeId) const {
  if (themeId >= MAX_THEMES || !valid_[themeId]) return 0;
  return counts_[themeId];
}

const RGBColor* ThemeFileReader::colors(uint8_t themeId) const {
  if (themeId >= MAX_THEMES || !valid_[themeId]) return nullptr;
  return themes_[themeId];
}

RGBColor ThemeFileReader::firstColor(uint8_t themeId) const {
  if (themeId >= MAX_THEMES || !valid_[themeId] || counts_[themeId] == 0) {
    return {0, 0, 0};
  }
  return themes_[themeId][0];
}