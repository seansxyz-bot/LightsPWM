#include "theme_file_writer.h"

ThemeFileWriter::ThemeFileWriter() {
  reset();
}

void ThemeFileWriter::reset() {
  status_ = FileProto::FILE_IDLE;
  fileType_ = 0;
  fileId_ = 0;
  expectedLines_ = 0;
  receivedLines_ = 0;
  version_ = 0;

  for (uint8_t i = 0; i < ThemeFileReader::MAX_COLORS_PER_THEME; ++i) {
    staging_[i] = {0, 0, 0};
  }
}

uint8_t ThemeFileWriter::status() const {
  return status_;
}

bool ThemeFileWriter::beginFile(uint8_t fileType, uint8_t fileId, uint8_t lineCount, uint8_t version) {
  reset();

  if (fileType != FileProto::FILE_THEME) {
    status_ = FileProto::FILE_ERROR;
    return false;
  }

  if (lineCount == 0 || lineCount > ThemeFileReader::MAX_COLORS_PER_THEME) {
    status_ = FileProto::FILE_ERROR;
    return false;
  }

  fileType_ = fileType;
  fileId_ = fileId;
  expectedLines_ = lineCount;
  version_ = version;
  receivedLines_ = 0;
  status_ = FileProto::FILE_RECEIVING;
  return true;
}

bool ThemeFileWriter::pushChunk(uint8_t b1, uint8_t b2, uint8_t b3,
                                uint8_t, uint8_t, uint8_t, uint8_t) {
  if (status_ != FileProto::FILE_RECEIVING) {
    status_ = FileProto::FILE_ERROR;
    return false;
  }

  if (fileType_ != FileProto::FILE_THEME) {
    status_ = FileProto::FILE_ERROR;
    return false;
  }

  if (receivedLines_ >= expectedLines_ ||
      receivedLines_ >= ThemeFileReader::MAX_COLORS_PER_THEME) {
    status_ = FileProto::FILE_ERROR;
    return false;
  }

  staging_[receivedLines_] = {b1, b2, b3};
  ++receivedLines_;
  return true;
}

bool ThemeFileWriter::endFile(uint8_t expectedLines, ThemeFileReader& reader) {
  if (status_ != FileProto::FILE_RECEIVING) {
    status_ = FileProto::FILE_ERROR;
    return false;
  }

  if (expectedLines != expectedLines_ || receivedLines_ != expectedLines_) {
    status_ = FileProto::FILE_ERROR;
    return false;
  }

  if (!reader.commitTheme(fileId_, staging_, receivedLines_)) {
    status_ = FileProto::FILE_ERROR;
    return false;
  }

  status_ = FileProto::FILE_SUCCESS;
  return true;
}

void ThemeFileWriter::abortFile() {
  reset();
  status_ = FileProto::FILE_IDLE;
}
