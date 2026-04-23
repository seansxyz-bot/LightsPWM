#include "pattern_file_reader.h"

#include <SD.h>

static const char* kPatternFilePath = "/ptrns";

void PatternFileReader::loadDefaults_() {
  table_ = PatternSpeedTable{};
}

uint8_t PatternFileReader::getStandaloneSpeed(uint8_t patternId) const {
  switch (patternId) {
    case 2: return table_.chase;
    case 3: return table_.comet;
    case 4: return table_.waves;
    case 5: return table_.sloglo;
    case 6: return table_.twinkle;
    case 7: return table_.slowfade;
    case 8: return table_.alternate;
    default: return 50;
  }
}

uint8_t PatternFileReader::getComboSpeed(uint8_t comboIndex) const {
  if (comboIndex >= 7) return 50;
  return table_.combo[comboIndex];
}

void PatternFileReader::setStandaloneSpeed(uint8_t patternId, uint8_t speed) {
  speed = PatternSpeedTable::clampPct(speed);

  switch (patternId) {
    case 2: table_.chase = speed; break;
    case 3: table_.comet = speed; break;
    case 4: table_.waves = speed; break;
    case 5: table_.sloglo = speed; break;
    case 6: table_.twinkle = speed; break;
    case 7: table_.slowfade = speed; break;
    case 8: table_.alternate = speed; break;
    default: break;
  }
}

void PatternFileReader::setComboSpeed(uint8_t comboIndex, uint8_t speed) {
  if (comboIndex >= 7) return;
  table_.combo[comboIndex] = PatternSpeedTable::clampPct(speed);
}

bool PatternFileReader::parseLine_(const String& line) {
  String s = line;
  s.trim();
  if (s.length() == 0) return true;

  char buf[96];
  s.toCharArray(buf, sizeof(buf));

  char tag[8] = {0};
  int a = -1, b = -1, c = -1, d = -1, e = -1, f = -1, g = -1;

  const int count = sscanf(buf, "%7s %d %d %d %d %d %d %d",
                           tag, &a, &b, &c, &d, &e, &f, &g);

  if (count < 2) return false;

  if (strcmp(tag, "cmbo") == 0) {
    if (count < 8) return false;
    table_.combo[0] = PatternSpeedTable::clampPct(a);
    table_.combo[1] = PatternSpeedTable::clampPct(b);
    table_.combo[2] = PatternSpeedTable::clampPct(c);
    table_.combo[3] = PatternSpeedTable::clampPct(d);
    table_.combo[4] = PatternSpeedTable::clampPct(e);
    table_.combo[5] = PatternSpeedTable::clampPct(f);
    table_.combo[6] = PatternSpeedTable::clampPct(g);
    return true;
  }

  const uint8_t speed = PatternSpeedTable::clampPct(a);

  if      (strcmp(tag, "chas") == 0) table_.chase = speed;
  else if (strcmp(tag, "comt") == 0) table_.comet = speed;
  else if (strcmp(tag, "wave") == 0) table_.waves = speed;
  else if (strcmp(tag, "slgl") == 0) table_.sloglo = speed;
  else if (strcmp(tag, "twnk") == 0) table_.twinkle = speed;
  else if (strcmp(tag, "fade") == 0) table_.slowfade = speed;
  else if (strcmp(tag, "altn") == 0) table_.alternate = speed;
  else return false;

  return true;
}

bool PatternFileReader::loadFromDisk() {
  loadDefaults_();

  File f = SD.open(kPatternFilePath, FILE_READ);
  if (!f) {
    return false;
  }

  String line;
  while (f.available()) {
    char ch = (char)f.read();
    if (ch == '\n') {
      parseLine_(line);
      line = "";
    } else if (ch != '\r') {
      line += ch;
    }
  }

  if (line.length() > 0) {
    parseLine_(line);
  }

  f.close();
  return true;
}