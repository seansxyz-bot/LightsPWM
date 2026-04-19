#pragma once
#include <Arduino.h>
#include <SD.h>

// Teensy 4.1 built-in SD.
// If you use a different storage device later, change this one file.
namespace ThemeStorage {

inline bool begin() {
  static bool started = false;
  if (!started) {
    started = SD.begin(BUILTIN_SDCARD);
  }
  return started;
}

inline void makePath(uint8_t themeId, char* out, size_t outSize) {
  snprintf(out, outSize, "/%u", (unsigned)themeId);
}

inline void defaultNameForId(uint8_t themeId, char* out, size_t outSize) {
  snprintf(out, outSize, "%u", (unsigned)themeId);
}

} // namespace ThemeStorage