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

  for (int i = 0; i < 7; ++i) {
    staging_[i] = 50;
  }
}

void PatternFileWriter::abortFile() {
  reset();
  }

bool PatternFileWriter::beginFile(uint8_t patternId, uint8_t expectedLines, uint8_t version) {
  reset();

  patternId_ = patternId;
  expectedLines_ = expectedLines;
  version_ = version;

  if (patternId_ == 1) {
    if (expectedLines_ == 0 || expectedLines_ > 7) {
      status_ = FileProto::FILE_ERROR;
      return false;
    }
  } else {
    if (expectedLines_ != 1) {
      status_ = FileProto::FILE_ERROR;
      return false;
    }
  }

  status_ = FileProto::FILE_RECEIVING;
  return true;
}

bool PatternFileWriter::pushChunk(uint8_t speed) {
  if (status_ != FileProto::FILE_RECEIVING) return false;

  if (patternId_ == 1) {
    if (receivedLines_ >= 7) {
      return true; // ignore extras for combo
    }
    staging_[receivedLines_++] = PatternSpeedTable::clampPct(speed);
    return true;
  }

  // standalone patterns only accept first one
  if (receivedLines_ == 0) {
    staging_[0] = PatternSpeedTable::clampPct(speed);
    receivedLines_ = 1;
  }

  return true; // ignore extras
}

bool PatternFileWriter::writePatternFileToDisk_(PatternFileReader& reader) {
  SD.remove(kPatternFilePath);

  File f = SD.open(kPatternFilePath, FILE_WRITE);
  if (!f) return false;

  const PatternSpeedTable& t = reader.table();

  f.print("cmbo ");
  for (int i = 0; i < 7; ++i) {
    f.print(t.combo[i]);
    if (i < 6) f.print(' ');
  }
  f.println();

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
  if (expectedLines != expectedLines_) {
    status_ = FileProto::FILE_ERROR;
    return false;
  }

  if (patternId_ == 1) {
    for (uint8_t i = 0; i < receivedLines_ && i < 7; ++i) {
      reader.setComboSpeed(i, staging_[i]);
    }
  } else {
    if (receivedLines_ >= 1) {
      reader.setStandaloneSpeed(patternId_, staging_[0]);
    }
  }

  if (!writePatternFileToDisk_(reader)) {
    status_ = FileProto::FILE_ERROR;
    return false;
  }

   if (!writePatternFileToDisk_(reader)) {
    status_ = FileProto::FILE_ERROR;
    return false;
  }

  status_ = FileProto::FILE_IDLE;
  return true;
}