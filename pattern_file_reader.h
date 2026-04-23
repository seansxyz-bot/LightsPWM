#pragma once

#include <Arduino.h>

struct PatternSpeedTable {
  uint8_t combo[7] = {50, 50, 50, 50, 50, 50, 50};

  uint8_t chase     = 50;
  uint8_t comet     = 50;
  uint8_t waves     = 50;
  uint8_t sloglo    = 50;
  uint8_t twinkle   = 50;
  uint8_t slowfade  = 50;
  uint8_t inwaves   = 50;
  uint8_t alternate = 50;

  static uint8_t clampPct(int v) {
    if (v < 0) return 0;
    if (v > 100) return 100;
    return (uint8_t)v;
  }
};

class PatternFileReader {
public:
  bool loadFromDisk();
  const PatternSpeedTable& table() const { return table_; }

  uint8_t getStandaloneSpeed(uint8_t patternId) const;
  uint8_t getComboSpeed(uint8_t comboIndex) const;

  void setStandaloneSpeed(uint8_t patternId, uint8_t speed);
  void setComboSpeed(uint8_t comboIndex, uint8_t speed);

private:
  PatternSpeedTable table_;

  void loadDefaults_();
  bool parseLine_(const String& line);
};