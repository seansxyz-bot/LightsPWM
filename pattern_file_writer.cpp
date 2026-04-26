#include "pattern_file_writer.h"

#include <SD.h>

static const char* kPatternFilePath = "/ptrns";

PatternFileWriter::PatternFileWriter() {
  reset();
}

void PatternFileWriter::reset() {
  status_ = FileProto::FILE_IDLE;
  patternId_ = 0;
  expectedLines_ = 0;
  receivedLines_ = 0;
  version_ = 0;
  stagedSpeed_ = 50;
}

void PatternFileWriter::abortFile() {
  reset();
}

bool PatternFileWriter::beginFile(uint8_t patternId, uint8_t expectedLines, uint8_t version) {
  reset();

  patternId_ = patternId;
  expectedLines_ = expectedLines;
  version_ = version;

  // Only standalone pattern IDs 2-7 are writable.
  // Combo ID 1 has no file/speed payload anymore.
  if (patternId_ < 2 || patternId_ > 8) {
    status_ = FileProto::FILE_ERROR;
    return false;
  }

  // Every pattern has exactly one speed.
  if (expectedLines_ != 1) {
    status_ = FileProto::FILE_ERROR;
    return false;
  }

  status_ = FileProto::FILE_RECEIVING;
  return true;
}

bool PatternFileWriter::pushChunk(uint8_t speed) {
  if (status_ != FileProto::FILE_RECEIVING) return false;

  // Only first speed counts. Extras are ignored.
  if (receivedLines_ == 0) {
    stagedSpeed_ = PatternSpeedTable::clampPct(speed);
    receivedLines_ = 1;
  }

  return true;
}

bool PatternFileWriter::writePatternFileToDisk_(PatternFileReader& reader) {
  SD.remove(kPatternFilePath);

  File f = SD.open(kPatternFilePath, FILE_WRITE);
  if (!f) return false;

  const PatternSpeedTable& t = reader.table();

  f.print("chas "); f.println(t.chase);
  f.print("comt "); f.println(t.comet);
  f.print("wave "); f.println(t.waves);
  f.print("slgl "); f.println(t.sloglo);
  f.print("twnk "); f.println(t.twinkle);
  f.print("fade "); f.println(t.slowfade);
  f.print("altn "); f.println(t.alternate);

  f.flush();
  f.close();
  return true;
}

bool PatternFileWriter::endFile(uint8_t expectedLines, PatternFileReader& reader) {
  if (status_ != FileProto::FILE_RECEIVING) return false;

  if (expectedLines != expectedLines_ || receivedLines_ != expectedLines_) {
    status_ = FileProto::FILE_ERROR;
    return false;
  }

  reader.setSpeed(patternId_, stagedSpeed_);

  if (!writePatternFileToDisk_(reader)) {
    status_ = FileProto::FILE_ERROR;
    return false;
  }

  status_ = FileProto::FILE_IDLE;
  return true;
}