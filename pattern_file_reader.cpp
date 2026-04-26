#include "pattern_file_reader.h"

#include <SD.h>

static const char* kPatternFilePath = "/ptrns";

void PatternFileReader::loadDefaults_() {
  table_ = PatternSpeedTable{};
}

uint8_t PatternFileReader::getSpeed(uint8_t patternId) const {
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

void PatternFileReader::setSpeed(uint8_t patternId, uint8_t speed) {
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

bool PatternFileReader::parseLine_(const String& line) {
  String s = line;
  s.trim();
  if (s.length() == 0) return true;

  char buf[48];
  s.toCharArray(buf, sizeof(buf));

  char tag[8] = {0};
  int speed = -1;

  const int count = sscanf(buf, "%7s %d", tag, &speed);
  if (count < 2) return false;

  const uint8_t pct = PatternSpeedTable::clampPct(speed);

  if      (strcmp(tag, "chas") == 0) table_.chase = pct;
  else if (strcmp(tag, "comt") == 0) table_.comet = pct;
  else if (strcmp(tag, "wave") == 0) table_.waves = pct;
  else if (strcmp(tag, "slgl") == 0) table_.sloglo = pct;
  else if (strcmp(tag, "twnk") == 0) table_.twinkle = pct;
  else if (strcmp(tag, "fade") == 0) table_.slowfade = pct;
  else if (strcmp(tag, "altn") == 0) table_.alternate = pct;
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