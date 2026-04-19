#include "theme_file_reader.h"
#include "theme_storage.h"

#include <SD.h>

ThemeFileReader::ThemeFileReader() {
  clear();
}

void ThemeFileReader::clear() {
  for (uint8_t t = 0; t < MAX_THEMES; ++t) {
    counts_[t] = 0;
    valid_[t] = false;
    names_[t][0] = '\0';

    for (uint8_t i = 0; i < MAX_COLORS_PER_THEME; ++i) {
      themes_[t][i] = {0, 0, 0};
    }
  }
}

void ThemeFileReader::setThemeName(uint8_t themeId, const char* name) {
  if (themeId >= MAX_THEMES) return;

  if (!name || !*name) {
    ThemeStorage::defaultNameForId(themeId, names_[themeId], sizeof(names_[themeId]));
    return;
  }

  strncpy(names_[themeId], name, MAX_THEME_NAME_LEN);
  names_[themeId][MAX_THEME_NAME_LEN] = '\0';
}

bool ThemeFileReader::commitTheme(uint8_t themeId, const char* name, const RGBColor* colors, uint8_t count) {
  if (themeId >= MAX_THEMES) return false;
  if (!colors) return false;
  if (count == 0 || count > MAX_COLORS_PER_THEME) return false;

  for (uint8_t i = 0; i < count; ++i) {
    themes_[themeId][i] = colors[i];
  }

  counts_[themeId] = count;
  valid_[themeId] = true;
  setThemeName(themeId, name);
  return true;
}

bool ThemeFileReader::commitTheme(uint8_t themeId, const RGBColor* colors, uint8_t count) {
  if (themeId >= MAX_THEMES) return false;

  if (names_[themeId][0] != '\0') {
    return commitTheme(themeId, names_[themeId], colors, count);
  }

  char fallback[MAX_THEME_NAME_LEN + 1];
  ThemeStorage::defaultNameForId(themeId, fallback, sizeof(fallback));
  return commitTheme(themeId, fallback, colors, count);
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

const char* ThemeFileReader::themeName(uint8_t themeId) const {
  if (themeId >= MAX_THEMES || !valid_[themeId]) return nullptr;
  return names_[themeId];
}

static bool readLine(File& f, char* out, size_t outSize) {
  if (!out || outSize == 0) return false;

  size_t idx = 0;
  while (f.available()) {
    int c = f.read();
    if (c < 0) break;
    if (c == '\r') continue;
    if (c == '\n') break;

    if (idx + 1 < outSize) {
      out[idx++] = (char)c;
    }
  }

  out[idx] = '\0';
  return idx > 0 || !f.available();
}

bool ThemeFileReader::loadThemeFromDisk(uint8_t themeId) {
  if (themeId >= MAX_THEMES) return false;
  if (!ThemeStorage::begin()) return false;

  char path[8];
  ThemeStorage::makePath(themeId, path, sizeof(path));

  File f = SD.open(path, FILE_READ);
  if (!f) {
    return false;
  }

  char line[64];
  char nameBuf[MAX_THEME_NAME_LEN + 1];

  if (!readLine(f, nameBuf, sizeof(nameBuf))) {
    f.close();
    return false;
  }

  if (!readLine(f, line, sizeof(line))) {
    f.close();
    return false;
  }

  long parsedCount = strtol(line, nullptr, 10);
  if (parsedCount <= 0 || parsedCount > MAX_COLORS_PER_THEME) {
    f.close();
    return false;
  }

  RGBColor tmp[MAX_COLORS_PER_THEME];

  for (int i = 0; i < parsedCount; ++i) {
    if (!readLine(f, line, sizeof(line))) {
      f.close();
      return false;
    }

    int r = 0, g = 0, b = 0;
    if (sscanf(line, "%d %d %d", &r, &g, &b) != 3) {
      f.close();
      return false;
    }

    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
      f.close();
      return false;
    }

    tmp[i] = {
      (uint8_t)r,
      (uint8_t)g,
      (uint8_t)b
    };
  }

  f.close();
  return commitTheme(themeId, nameBuf, tmp, (uint8_t)parsedCount);
}

void ThemeFileReader::loadAllFromDisk() {
  if (!ThemeStorage::begin()) return;

  for (uint8_t themeId = 0; themeId < MAX_THEMES; ++themeId) {
    loadThemeFromDisk(themeId);
  }
}