#include <Arduino.h>
#include <Wire.h>

#include "PWMEngine.h"
#include "PatternEngine.h"
#include "leds.h"
#include "piclient.h"

#include "file_protocol.h"
#include "theme_storage.h"
#include "pattern_file_reader.h"
#include "pattern_file_writer.h"
#include "theme_file_reader.h"
#include "theme_file_writer.h"

#define DEBUG 0

static constexpr uint8_t I2C_ADDR = 0x08;

PWMEngine pwm;
PatternEngine patterns(/*activeID=*/0);

PatternFileReader g_patternReader;
PatternFileWriter g_patternWriter;

ThemeFileReader g_themeReader;
ThemeFileWriter g_themeWriter;

volatile int g_activePattern = 0;
volatile int g_speedPct = 50;
volatile int g_currentTheme = 0;

static volatile bool g_ready = false;
static volatile bool g_shutdownReq = false;
static volatile bool g_allOff = false;

static uint8_t g_txBuf[NUM_OF_USED_OUTPUTS];
static volatile bool g_ledSnapDirty = true;

static constexpr const char* MANUAL_FILE_PATH = "/manual";
static constexpr uint32_t MANUAL_SAVE_DELAY_MS = 1500;

static bool g_manualDirty = false;
static uint32_t g_manualDirtyAt = 0;

static inline void setSingleChan(uint16_t idx, uint8_t chan, uint8_t val) {
  if (idx >= NUM_OF_LEDS) return;

  if (chan == 0) pwm.leds[idx].setRed(val);
  else if (chan == 1) pwm.leds[idx].setGreen(val);
  else pwm.leds[idx].setBlue(val);
}

static inline void setRGB(uint16_t idx, uint8_t r, uint8_t g, uint8_t b) {
  if (idx >= NUM_OF_LEDS) return;
  pwm.leds[idx].setRed(r);
  pwm.leds[idx].setGreen(g);
  pwm.leds[idx].setBlue(b);
}

static void markManualDirty() {
  g_manualDirty = true;
  g_manualDirtyAt = millis();
}

static bool saveManualToDisk() {
  if (!ThemeStorage::begin()) {
    Serial.println("Manual save failed: storage not ready");
    return false;
  }

  SD.remove(MANUAL_FILE_PATH);

  File f = SD.open(MANUAL_FILE_PATH, FILE_WRITE);
  if (!f) {
    Serial.println("Manual save failed: could not open /manual");
    return false;
  }

  for (uint16_t i = 0; i < NUM_OF_LEDS; ++i) {
    f.print((int)pwm.leds[i].getRed());
    f.print(" ");
    f.print((int)pwm.leds[i].getGreen());
    f.print(" ");
    f.println((int)pwm.leds[i].getBlue());
  }

  f.close();
  Serial.println("Manual LED values saved");
  return true;
}

static bool loadManualFromDisk() {
  if (!ThemeStorage::begin()) {
    Serial.println("Manual load failed: storage not ready");
    return false;
  }

  File f = SD.open(MANUAL_FILE_PATH, FILE_READ);
  if (!f) {
    Serial.println("No /manual file found");
    return false;
  }

  uint16_t led = 0;

  while (f.available() && led < NUM_OF_LEDS) {
    int r = f.parseInt();
    int g = f.parseInt();
    int b = f.parseInt();

    r = constrain(r, 0, 255);
    g = constrain(g, 0, 255);
    b = constrain(b, 0, 255);

    setRGB(led, (uint8_t)r, (uint8_t)g, (uint8_t)b);
    led++;
  }

  f.close();

  if (led > 0) {
    patterns.primeFromPWM(pwm);
    g_ledSnapDirty = true;
    Serial.print("Manual LED values loaded: ");
    Serial.println(led);
    return true;
  }

  Serial.println("/manual was empty or invalid");
  return false;
}

static inline bool bitIsSet(uint32_t m, uint16_t bit) {
  return (m >> bit) & 0x1u;
}

static void refreshLedSnapshot() {
  for (uint16_t i = 0; i < NUM_OF_LEDS; ++i) {
    g_txBuf[i * 3 + 0] = (uint8_t)pwm.leds[i].getRed();
    g_txBuf[i * 3 + 1] = (uint8_t)pwm.leds[i].getGreen();
    g_txBuf[i * 3 + 2] = (uint8_t)pwm.leds[i].getBlue();
  }
  g_ledSnapDirty = false;
}

static void applyThemeToLeds(uint8_t themeId) {
  if (!g_themeReader.hasTheme(themeId)) {
    Serial.print("Theme not loaded: ");
    Serial.println(themeId);
    return;
  }

  const RGBColor* colors = g_themeReader.colors(themeId);
  const uint8_t count = g_themeReader.colorCount(themeId);
  if (!colors || count == 0) return;

  for (uint16_t i = 0; i < NUM_OF_LEDS; ++i) {
    const RGBColor& c = colors[i % count];
    setRGB(i, c.r, c.g, c.b);
  }

  patterns.primeFromPWM(pwm);
  g_ledSnapDirty = true;
}

static void handleApplyMaskPacket(const uint8_t* p) {
  const uint8_t chan = p[1];
  const uint8_t m0 = p[2];
  const uint8_t m1 = p[3];
  const uint8_t m2 = p[4];
  const uint8_t b5 = p[5];
  const uint8_t b6 = p[6];
  const uint8_t b7 = p[7];

  const uint32_t mask = (uint32_t)m0 | ((uint32_t)m1 << 8) | ((uint32_t)m2 << 16);

  if (chan <= 2) {
    for (uint16_t i = 0; i < NUM_OF_LEDS; ++i) {
      if (bitIsSet(mask, i)) setSingleChan(i, chan, b5);
    }

    patterns.primeFromPWM(pwm);
    g_ledSnapDirty = true;
    markManualDirty();
    return;
  }

  if (chan == 3) {
    for (uint16_t i = 0; i < NUM_OF_LEDS; ++i) {
      if (bitIsSet(mask, i)) setRGB(i, b5, b6, b7);
    }

    patterns.primeFromPWM(pwm);
    g_ledSnapDirty = true;
    markManualDirty();
    return;
  }

  if (chan >= 4) {
  const uint8_t newTheme = chan - 4;
  if (newTheme >= ThemeFileReader::MAX_THEMES) {
    return;
  }

  if (newTheme != g_currentTheme) {
    g_currentTheme = newTheme;

    if (g_currentTheme == 0) {
      Serial.println("Applying manual LEDs");
      loadManualFromDisk();
    } else {
      Serial.print("Applying theme ");
      Serial.println(g_currentTheme);
      applyThemeToLeds(g_currentTheme);
    }
  }

  if (g_activePattern != b5) {
    g_activePattern = b5;
    patterns.setActive(g_activePattern);
  }

    if (g_activePattern != b5) {
      g_activePattern = b5;
      patterns.setActive(g_activePattern);
    }

    int newSpeed = (int)b6;
    if (newSpeed < 0) newSpeed = 0;
    if (newSpeed > 100) newSpeed = 100;

    if (g_speedPct != newSpeed) {
      g_speedPct = newSpeed;
    }
  }
}

static void handleLedFramePacket(const uint8_t* p) {
  const uint8_t ledIndex = p[1];
  const uint8_t r = p[2];
  const uint8_t g = p[3];
  const uint8_t b = p[4];

  if (ledIndex >= NUM_OF_LEDS)
    return;

  // LightShow is live-only. Do not write to disk.
  // Also stop pattern engine from immediately overwriting this frame.
  if (g_activePattern != 0) {
    g_activePattern = 0;
    patterns.setActive(0);
  }

  setRGB(ledIndex, r, g, b);
  g_ledSnapDirty = true;
}

static void handleFilePacket(const uint8_t* p) {
  const uint8_t cmd = p[0];

  switch (cmd) {
    case FileProto::CMD_BEGIN_FILE:
      {
        const uint8_t fileType = p[1];

        if (fileType == FileProto::FILE_THEME) {
          // keep your existing theme begin logic here
          if (!g_themeWriter.beginFile(p[1], p[2], p[3], p[4])) {
            Serial.println("BEGIN_FILE theme failed");
          }
        } else if (fileType == FileProto::FILE_PATTERN) {
          // p[2] = pattern id
          // p[3] = expected lines
          // p[4] = version
          if (!g_patternWriter.beginFile(p[2], p[3], p[4])) {
            Serial.println("BEGIN_FILE pattern failed");
          }
        }
        break;
      }

    case FileProto::CMD_FILE_CHUNK:
      {
        if (g_themeWriter.status() == FileProto::FILE_RECEIVING) {
          // keep your existing theme chunk logic
          if (!g_themeWriter.pushChunk(p[1], p[2], p[3], p[4], p[5], p[6], p[7])) {
            Serial.println("FILE_CHUNK theme failed");
          }
        } else if (g_patternWriter.status() == FileProto::FILE_RECEIVING) {
          // only p[1] matters for pattern speed chunks
          if (!g_patternWriter.pushChunk(p[1])) {
            Serial.println("FILE_CHUNK pattern failed");
          }
        }
        break;
      }

    case FileProto::CMD_END_FILE:
      {
        if (g_themeWriter.status() == FileProto::FILE_RECEIVING) {
          if (!g_themeWriter.endFile(p[1], g_themeReader)) {
            Serial.println("END_FILE theme failed");
          } else {
            // keep whatever theme refresh logic you already have
          }
        } else if (g_patternWriter.status() == FileProto::FILE_RECEIVING) {
          if (!g_patternWriter.endFile(p[1], g_patternReader)) {
            Serial.println("END_FILE pattern failed");
          } else {
            patterns.setSpeedTable(g_patternReader.table());
            Serial.println("Pattern speeds updated");
          }
        }
        break;
      }

    case FileProto::CMD_ABORT_FILE:
      {
        g_themeWriter.abortFile();
        g_patternWriter.abortFile();
        Serial.println("FILE aborted");
        break;
      }

    default:
      break;
  }
}

static uint8_t currentFileStatus() {
  const uint8_t ts = g_themeWriter.status();
  const uint8_t ps = g_patternWriter.status();

  if (ts == FileProto::FILE_ERROR || ps == FileProto::FILE_ERROR)
    return FileProto::FILE_ERROR;

  if (ts == FileProto::FILE_RECEIVING || ps == FileProto::FILE_RECEIVING)
    return FileProto::FILE_RECEIVING;

  if (ts == FileProto::FILE_SUCCESS || ps == FileProto::FILE_SUCCESS)
    return FileProto::FILE_SUCCESS;

  return FileProto::FILE_IDLE;
}

static void i2c_onRequest() {
  uint8_t req;
  if (!PIClient::takeArmedRequest(req)) {
    uint8_t zero = 0;
    Wire.write(&zero, 1);
    return;
  }

  switch (req) {
    case FileProto::REQ_WAKE_READY:
      {
        uint8_t one = g_ready ? 1 : 0;
        Wire.write(&one, 1);
      }
      break;

    case FileProto::REQ_LED_STATE:
      {
        Wire.write(g_txBuf, (int)(3 * NUM_OF_LEDS));
      }
      break;

    case FileProto::REQ_SHUTDOWN:
      {
        uint8_t one = g_allOff ? 1 : 0;
        Wire.write(&one, 1);
      }
      break;

    case FileProto::REQ_FILE_STATUS:
      {
        Wire.write(currentFileStatus());
      }
      break;

    default:
      {
        uint8_t zero = 0;
        Wire.write(&zero, 1);
      }
      break;
  }
}

void setup() {
  Serial.begin(115200);

  pwm.begin();
  patterns.setActive(g_activePattern);

  if (ThemeStorage::begin()) {
    Serial.println("Theme storage ready");
    g_themeReader.loadAllFromDisk();

    g_patternReader.loadFromDisk();
    patterns.setSpeedTable(g_patternReader.table());
  } else {
    Serial.println("Theme storage init failed");
  }

  if (!loadManualFromDisk()) {
    if (g_themeReader.hasTheme((uint8_t)g_currentTheme)) {
      applyThemeToLeds((uint8_t)g_currentTheme);
    }
  }

  refreshLedSnapshot();

  PIClient::begin(I2C_ADDR, i2c_onRequest, 100000);

  g_ready = true;
  g_ledSnapDirty = false;

  Serial.println("LightsPWM ready");
}

static void handleWrite(const PIClient::WriteMsg& m) {
  if (m.len != 8) return;

  const uint8_t* p = m.data;
  const uint8_t cmd = p[0];

  switch (cmd) {
    case FileProto::CMD_APPLY_MASK:
      handleApplyMaskPacket(p);
      break;

    case FileProto::CMD_PATTERN_SPEED:
      {
        const uint8_t patternId = p[1];
        const uint8_t speed = p[2];
        g_patternReader.setSpeed(patternId, speed);
        patterns.setSpeedTable(g_patternReader.table());
        break;
      }

    case FileProto::CMD_LED_FRAME:
      handleLedFramePacket(p);
      break;

    case FileProto::CMD_BEGIN_FILE:
    case FileProto::CMD_FILE_CHUNK:
    case FileProto::CMD_END_FILE:
    case FileProto::CMD_ABORT_FILE:
      handleFilePacket(p);
      break;

    default:
      break;
  }
}

unsigned long lastPrintALLLeds = 0;

void printALLLedsValues(unsigned long printSpeedMilliSeconds) {
  unsigned long now = millis();
  if (now - lastPrintALLLeds >= printSpeedMilliSeconds) {
    lastPrintALLLeds = now;

    Serial.print("LED values: ");
    for (uint16_t i = 0; i < NUM_OF_LEDS; ++i) {
      Serial.print("[");
      Serial.print(pwm.leds[i].getRed());
      Serial.print(",");
      Serial.print(pwm.leds[i].getGreen());
      Serial.print(",");
      Serial.print(pwm.leds[i].getBlue());
      Serial.print("]");
      if (i < NUM_OF_LEDS - 1) Serial.print(" ");
    }
    Serial.println();
  }
}

void loop() {
  bool applied = false;
  PIClient::WriteMsg msg;

  while (PIClient::pop(msg)) {
    handleWrite(msg);
    applied = true;
  }

  if (applied || g_ledSnapDirty) {
    refreshLedSnapshot();
  }

  if (g_shutdownReq && !g_allOff) {
    Serial.println("Shutting Down");
    for (uint16_t i = 0; i < NUM_OF_LEDS; ++i) {
      setRGB(i, 0, 0, 0);
    }
    g_allOff = true;
    g_ledSnapDirty = true;
  }

  if (DEBUG) {
    printALLLedsValues(1000);
  }

  if (g_manualDirty && (millis() - g_manualDirtyAt >= MANUAL_SAVE_DELAY_MS)) {
    if (saveManualToDisk()) {
      g_manualDirty = false;
    } else {
      g_manualDirtyAt = millis();
    }
  }

  if (g_activePattern > 0) {
    patterns.tick(pwm);
    g_ledSnapDirty = true;
  }
}