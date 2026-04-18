#include <Arduino.h>
#include <Wire.h>
#include "PWMEngine.h"
#include "PatternEngine.h"
#include "leds.h"
#include "piclient.h"
#include "themes.h"

#define DEBUG 0

// ---- Address & compact write command ----
static constexpr uint8_t I2C_ADDR = 0x08;
static constexpr uint8_t CMD_APPLY_MASK = 0x15;  // [cmd, chan(0..3), m0,m1,m2, B5,B6,B7]

PWMEngine pwm;
PatternEngine patterns(/*activeID=*/0);

volatile int g_activePattern = 0;  // 0..7 (0 = Off)
volatile int g_speedPct = 50;      // 0..100
volatile int o_theme = 12;         // 0..100
volatile int n_theme = 12;         // 0..100


// Read-status flags for onRequest
static volatile bool g_ready = false;
static volatile bool g_shutdownReq = false;
static volatile bool g_allOff = false;

// LED snapshot for REQ_LED_STATE
static uint8_t g_txBuf[NUM_OF_USED_OUTPUTS];
static volatile bool g_ledSnapDirty = true;

// Tiny LED helpers
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
static inline bool bitIsSet(uint32_t m, uint16_t bit) {
  return (m >> bit) & 0x1u;
}

// ------------ onRequest (unchanged) ------------
static void i2c_onRequest() {
  uint8_t req;
  if (!PIClient::takeArmedRequest(req)) {
    uint8_t zero = 0;
    Wire.write(&zero, 1);
    return;
  }

  switch (req) {
    case PIClient::REQ_WAKE_READY:
      {
        uint8_t one = g_ready ? 1 : 0;
        Wire.write(&one, 1);
      }
      break;

    case PIClient::REQ_LED_STATE:
      {
        Wire.write(g_txBuf, (int)(3 * NUM_OF_LEDS));
      }
      break;

    case PIClient::REQ_SHUTDOWN:
      {
        uint8_t one = g_allOff ? 1 : 0;
        Wire.write(&one, 1);
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

// ------------ setup / loop ------------
void setup() {
  pwm.begin();
  patterns.setActive(g_activePattern);
  patterns.setSpeedPercent(g_speedPct);
  // initial snapshot
  Serial.print("LED values: ");
  for (uint16_t i = 0; i < 1; ++i) {
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
  for (uint16_t i = 0; i < NUM_OF_LEDS; ++i) {
    g_txBuf[i * 3 + 0] = (uint8_t)pwm.leds[i].getRed();
    g_txBuf[i * 3 + 1] = (uint8_t)pwm.leds[i].getGreen();
    g_txBuf[i * 3 + 2] = (uint8_t)pwm.leds[i].getBlue();
  }
  PIClient::begin(I2C_ADDR, i2c_onRequest, 100000);
  g_ready = true;

  g_ledSnapDirty = false;
}

static void handleWrite(const PIClient::WriteMsg& m) {
  // STRICT: only accept exactly 8-byte packets
  if (m.len != 8) return;

  const uint8_t* p = m.data;

  const uint8_t cmd = p[0];
  if (cmd != CMD_APPLY_MASK) return;

  const uint8_t chan = p[1];
  const uint8_t m0 = p[2], m1 = p[3], m2 = p[4];
  const uint8_t b5 = p[5], b6 = p[6], b7 = p[7];

  const uint32_t mask = (uint32_t)m0 | ((uint32_t)m1 << 8) | ((uint32_t)m2 << 16);

  if (chan <= 2) {
    // Single-channel write: use b5; b6/b7 ignored
    for (uint16_t i = 0; i < NUM_OF_LEDS; ++i) {
      if (bitIsSet(mask, i)) setSingleChan(i, chan, b5);
    }
    g_ledSnapDirty = true;
  } else if (chan == 3) {
    // RGB write: b5,b6,b7 => R,G,B
    for (uint16_t i = 0; i < NUM_OF_LEDS; ++i) {
      if (bitIsSet(mask, i)) setRGB(i, b5, b6, b7);
    }
    g_ledSnapDirty = true;
  } else if (chan >= 4 && chan <= 16) {
    const uint8_t newTheme = chan - 4;

    // Only repaint & re-prime when the theme actually changes
    if (newTheme != o_theme) {
      n_theme = newTheme;
      Serial.println("New Theme");
      o_theme = n_theme;

      
      patterns.primeFromPWM(pwm);  // capture new base
      g_ledSnapDirty = true;
    }

    // Only change active pattern if the value actually changed
    if (g_activePattern != b5) {
      g_activePattern = b5;  // 0..8
      patterns.setActive(g_activePattern);
    }

    // Clamp and apply speed (treat 0 as intentional pause; otherwise default was 50)
    int newSpeed = (int)b6;
    if (newSpeed < 0) newSpeed = 0;
    if (newSpeed > 100) newSpeed = 100;

    if (g_speedPct != newSpeed) {
      g_speedPct = newSpeed;
      patterns.setSpeedPercent(g_speedPct);
    }
  }
}
// Convert one byte to "01010101"
static inline void byte_to_bits(char out[9], uint8_t b) {
  for (int i = 7; i >= 0; --i) out[7 - i] = (b & (1u << i)) ? '1' : '0';
  out[8] = '\0';
}

unsigned long lastPrintALLLeds = 0;
void printALLLedsValues(unsigned long printSpeedMilliSeconds) {
  unsigned long now = millis();

  if (now - lastPrintALLLeds >= printSpeedMilliSeconds) {  // 1000 ms = 1 second
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
    for (uint16_t i = 0; i < NUM_OF_LEDS; ++i) {
      g_txBuf[i * 3 + 0] = (uint8_t)pwm.leds[i].getRed();
      g_txBuf[i * 3 + 1] = (uint8_t)pwm.leds[i].getGreen();
      g_txBuf[i * 3 + 2] = (uint8_t)pwm.leds[i].getBlue();
    }
    g_ledSnapDirty = false;
  }

  // Optional shutdown path (unchanged)
  if (g_shutdownReq && !g_allOff) {
    Serial.println("Shutting Down");
    for (uint16_t i = 0; i < NUM_OF_LEDS; ++i) {
      pwm.leds[i].setRed(0);
      pwm.leds[i].setGreen(0);
      pwm.leds[i].setBlue(0);
    }
    g_allOff = true;
    g_ledSnapDirty = true;
  }
    
    if (DEBUG)
      printALLLedsValues(1000);// milli seconds

  // If "patterns" is enabled (active != 0), render into pwm.leds[]
  if (g_activePattern > 0) {
    patterns.tick(pwm);
  }
}