#include "pattern_file_writer.h"

#include <SD.h>

static const char *kPatternFilePath = "/ptrns";
static constexpr uint8_t kPatternBulkFileId = 1;
static constexpr uint8_t kPatternSpeedCount = 7; // IDs 2..8

PatternFileWriter::PatternFileWriter() { reset(); }

void PatternFileWriter::reset() {
  status_ = FileProto::FILE_IDLE;
  patternId_ = 0;
  expectedLines_ = 0;
  receivedLines_ = 0;
  version_ = 0;

  for (uint8_t &s : stagedSpeeds_)
    s = 50;
  for (bool &received : received_)
    received = false;
}

void PatternFileWriter::abortFile() { reset(); }

bool PatternFileWriter::beginFile(uint8_t patternId, uint8_t expectedLines,
                                  uint8_t version) {
  reset();

  patternId_ = patternId;
  expectedLines_ = expectedLines;
  version_ = version;

  // Bulk pattern-speed file. Each chunk carries patternId + speed.
  if (patternId_ != kPatternBulkFileId) {
    status_ = FileProto::FILE_ERROR;
    return false;
  }

  if (expectedLines_ != kPatternSpeedCount) {
    status_ = FileProto::FILE_ERROR;
    return false;
  }

  status_ = FileProto::FILE_RECEIVING;
  return true;
}

bool PatternFileWriter::pushChunk(uint8_t patternId, uint8_t speed) {
  if (status_ != FileProto::FILE_RECEIVING)
    return false;

  if (patternId < 2 || patternId > 8 || receivedLines_ >= expectedLines_) {
    status_ = FileProto::FILE_ERROR;
    return false;
  }

  const uint8_t index = patternId - 2;
  if (received_[index]) {
    status_ = FileProto::FILE_ERROR;
    return false;
  }

  stagedSpeeds_[index] = PatternSpeedTable::clampPct(speed);
  received_[index] = true;
  receivedLines_++;
  return true;
}

bool PatternFileWriter::writePatternFileToDisk_(PatternFileReader &reader) {
  SD.remove(kPatternFilePath);

  File f = SD.open(kPatternFilePath, FILE_WRITE);
  if (!f)
    return false;

  const PatternSpeedTable &t = reader.table();

  f.print("chas ");
  f.println(t.chase);
  f.print("comt ");
  f.println(t.comet);
  f.print("wave ");
  f.println(t.waves);
  f.print("slgl ");
  f.println(t.sloglo);
  f.print("twnk ");
  f.println(t.twinkle);
  f.print("fade ");
  f.println(t.slowfade);
  f.print("altn ");
  f.println(t.alternate);

  f.flush();
  f.close();
  return true;
}

bool PatternFileWriter::endFile(uint8_t expectedLines,
                                PatternFileReader &reader) {
  if (status_ != FileProto::FILE_RECEIVING)
    return false;

  if (expectedLines != expectedLines_ || receivedLines_ != expectedLines_) {
    status_ = FileProto::FILE_ERROR;
    return false;
  }

  for (uint8_t i = 0; i < kPatternSpeedCount; ++i) {
    if (!received_[i]) {
      status_ = FileProto::FILE_ERROR;
      return false;
    }
  }

  for (uint8_t i = 0; i < kPatternSpeedCount; ++i) {
    const uint8_t patternId = i + 2;
    reader.setSpeed(patternId, stagedSpeeds_[i]);
  }

  if (!writePatternFileToDisk_(reader)) {
    status_ = FileProto::FILE_ERROR;
    return false;
  }

  status_ = FileProto::FILE_SUCCESS;
  return true;
}
