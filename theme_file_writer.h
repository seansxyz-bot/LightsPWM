#pragma once
#include <Arduino.h>
#include "file_protocol.h"
#include "theme_file_reader.h"

class ThemeFileWriter {
public:
  ThemeFileWriter();

  void reset();

  uint8_t status() const;

  bool beginFile(uint8_t fileType, uint8_t fileId, uint8_t lineCount, uint8_t version);
  bool pushChunk(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5, uint8_t b6, uint8_t b7);
  bool endFile(uint8_t expectedLines, ThemeFileReader& reader);
  void abortFile();

private:
  uint8_t status_ = FileProto::FILE_IDLE;
  uint8_t fileType_ = 0;
  uint8_t fileId_ = 0;
  uint8_t expectedLines_ = 0;
  uint8_t receivedLines_ = 0;
  uint8_t version_ = 0;

  RGBColor staging_[ThemeFileReader::MAX_COLORS_PER_THEME];
};