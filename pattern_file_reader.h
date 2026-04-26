#pragma once

#include <Arduino.h>

struct PatternSpeedTable {
  uint8_t chase = 50;      // id 2
  uint8_t comet = 50;      // id 3
  uint8_t waves = 50;      // id 4
  uint8_t sloglo = 50;     // id 5
  uint8_t twinkle = 50;    // id 6
  uint8_t slowfade = 50;   // id 7
  uint8_t alternate = 50;  // id 8

  static uint8_t clampPct(int v) {
    if (v < 0) return 0;
    if (v > 100) return 100;
    return (uint8_t)v;
  }
};

class PatternFileReader {
public:
  bool loadFromDisk();
  const PatternSpeedTable& table() const {
    return table_;
  }

  uint8_t getSpeed(uint8_t patternId) const;
  void setSpeed(uint8_t patternId, uint8_t speed);

private:
  PatternSpeedTable table_;

  void loadDefaults_();
  bool parseLine_(const String& line);
};