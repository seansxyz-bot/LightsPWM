#pragma once
#include <Arduino.h>

namespace FileProto {

static constexpr uint8_t CMD_APPLY_MASK  = 0x15;
static constexpr uint8_t CMD_PATTERN_SPEED = 0x16;
static constexpr uint8_t CMD_LED_FRAME = 0x17;

static constexpr uint8_t CMD_BEGIN_FILE  = 0x20;
static constexpr uint8_t CMD_FILE_CHUNK  = 0x21;
static constexpr uint8_t CMD_END_FILE    = 0x22;
static constexpr uint8_t CMD_ABORT_FILE  = 0x23;

static constexpr uint8_t REQ_WAKE_READY  = 0xF0;
static constexpr uint8_t REQ_LED_STATE   = 0xF1;
static constexpr uint8_t REQ_SHUTDOWN    = 0xF2;
static constexpr uint8_t REQ_FILE_STATUS = 0xF3;

static constexpr uint8_t FILE_THEME      = 0x01;
static constexpr uint8_t FILE_PATTERN    = 0x02;

static constexpr uint8_t FILE_IDLE       = 0;
static constexpr uint8_t FILE_RECEIVING  = 1;
static constexpr uint8_t FILE_SUCCESS    = 2;
static constexpr uint8_t FILE_ERROR      = 3;

} // namespace FileProto