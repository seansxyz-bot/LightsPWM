#pragma once

#include <Arduino.h>

#include "file_protocol.h"
#include "pattern_file_reader.h"

class PatternFileWriter {
public:
  PatternFileWriter();

  void reset();
  uint8_t status() const { return status_; }

  bool beginFile(uint8_t patternId, uint8_t expectedLines, uint8_t version);
  bool pushChunk(uint8_t speed);
  bool endFile(uint8_t expectedLines, PatternFileReader& reader);
  void abortFile();

private:
  bool writePatternFileToDisk_(PatternFileReader& reader);

  uint8_t status_ = FileProto::FILE_IDLE;
  uint8_t patternId_ = 0;
  uint8_t expectedLines_ = 0;
  uint8_t receivedLines_ = 0;
  uint8_t version_ = 0;
  uint8_t staging_[7] = {50, 50, 50, 50, 50, 50, 50};
};