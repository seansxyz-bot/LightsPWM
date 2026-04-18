#include "PWMEngine.h"

PWMEngine* PWMEngine::s_instance = nullptr;

// ----- helpers -----
inline void PWMEngine::latchPulse() {
  digitalWriteFast(LATCH_PIN, LOW);
  digitalWriteFast(LATCH_PIN, HIGH);
  asm volatile("nop\nnop\nnop\n");
  digitalWriteFast(LATCH_PIN, LOW);  // idle LOW between frames
}

void PWMEngine::chToByteBit(uint16_t ch, uint16_t& byteIndex, uint8_t& bitMask) {
  byteIndex = ch / 8;
  uint8_t bitInByte = ch % 8;
  // Map logical 0 → Q7, 1 → Q6 … 7 → Q0
  bitMask = (uint8_t)(1u << (7 - bitInByte));
}
// Reverse 8 bits in a byte
static inline uint8_t rev8(uint8_t x) {
  x = (x & 0xF0) >> 4 | (x & 0x0F) << 4;
  x = (x & 0xCC) >> 2 | (x & 0x33) << 2;
  x = (x & 0xAA) >> 1 | (x & 0x55) << 1;
  return x;
}

bool reverse = 1;
// Build drawBuf for a given PWM slice from current LED values
void PWMEngine::buildSliceFrame(uint8_t slice) {
  uint8_t temp[NUM_OF_SHIFT_REGS];
  if (reverse)
    memset(temp, 0x00, sizeof(drawBuf));
  else
    memset(drawBuf, 0x00, sizeof(drawBuf));

  for (uint16_t led = 0; led < NUM_OF_LEDS; ++led) {
    // getters already zero if power is off
    const int r = leds[led].getBlue();    // 0..255 or 0 if off :contentReference[oaicite:2]{index=2}
    const int g = leds[led].getGreen();  // 0..255 or 0 if off :contentReference[oaicite:3]{index=3}
    const int b = leds[led].getRed();   // 0..255 or 0 if off :contentReference[oaicite:4]{index=4}

    const uint16_t chBase = led * 3;
    const uint16_t chR = chBase + 0;
    const uint16_t chG = chBase + 1;
    const uint16_t chB = chBase + 2;

    if (r > slice) {
      uint16_t bi;
      uint8_t bm;
      chToByteBit(chR, bi, bm);
      if (bi < NUM_OF_SHIFT_REGS) (reverse ? temp[bi] : drawBuf[bi]) |= bm;
    }
    if (g > slice) {
      uint16_t bi;
      uint8_t bm;
      chToByteBit(chG, bi, bm);
      if (bi < NUM_OF_SHIFT_REGS) (reverse ? temp[bi] : drawBuf[bi]) |= bm;
    }
    if (b > slice) {
      uint16_t bi;
      uint8_t bm;
      chToByteBit(chB, bi, bm);
      if (bi < NUM_OF_SHIFT_REGS) (reverse ? temp[bi] : drawBuf[bi]) |= bm;
    }
  }

  if (reverse) {
    // Reverse the ENTIRE 8*NUM_OF_SHIFT_REGS bitstream from temp -> drawBuf
    for (uint16_t i = 0, j = NUM_OF_SHIFT_REGS - 1; i < NUM_OF_SHIFT_REGS; ++i, --j) {
      drawBuf[i] = rev8(temp[j]);
    }
  }
}

// ----- DMA flow -----
void PWMEngine::onSpiDmaDoneStatic(EventResponder& /*er*/) {
  if (s_instance) s_instance->onSpiDmaDone();
}

void PWMEngine::onSpiDmaDone() {
  // DMA finished shifting activeBuf; now latch and queue the next slice.
  SPI.endTransaction();
  latchPulse();

  if (!running) {
    xferInFlight = false;
    return;
  }

  // Advance slice (wrap 0..255)
  pwmSlice = (uint8_t)(pwmSlice + 1);

  // Build next slice into drawBuf
  buildSliceFrame(pwmSlice);

  // Kick next DMA frame using drawBuf (it becomes the new active)
  startDmaFrame(drawBuf);

  // Copy for record/debug (optional; safe since DMA uses a separate buffer)
  memcpy(activeBuf, drawBuf, sizeof(activeBuf));
}

void PWMEngine::startDmaFrame(const uint8_t* buf) {
  // Latch must be LOW during shift; discipline here and in latchPulse()
  digitalWriteFast(LATCH_PIN, LOW);

  xferInFlight = true;

  dmaDoneEvt.detach();
  dmaDoneEvt.attach(onSpiDmaDoneStatic);

  SPI.beginTransaction(SPISettings(PWM_SPI_HZ, MSBFIRST, PWM_SPI_MODE));
  // Async DMA: send exactly NUM_OF_SHIFT_REGS bytes
  SPI.transfer((const void*)buf, nullptr, (size_t)NUM_OF_SHIFT_REGS, dmaDoneEvt);
}

// ----- Public API -----
bool PWMEngine::begin() {
  s_instance = this;

  pinMode(LATCH_PIN, OUTPUT);
  digitalWriteFast(LATCH_PIN, LOW);

  SPI.begin();

  memset(activeBuf, 0x00, sizeof(activeBuf));
  memset(drawBuf, 0x00, sizeof(drawBuf));

  // Start at slice 0 with current LED values
  pwmSlice = 0;
  buildSliceFrame(pwmSlice);

  running = true;

  // Queue first DMA frame using drawBuf; copy to active for reference
  startDmaFrame(drawBuf);
  memcpy(activeBuf, drawBuf, sizeof(activeBuf));

  return true;
}

void PWMEngine::stop() {
  running = false;
  // Let any in-flight DMA finish; outputs stay at last latched state
}

void PWMEngine::start() {
  if (running) return;
  running = true;

  // Rebuild from current LED values at current slice and fire DMA
  buildSliceFrame(pwmSlice);
  startDmaFrame(drawBuf);
  memcpy(activeBuf, drawBuf, sizeof(activeBuf));
}

// In PWMEngine.cpp
void PWMEngine::allOffLatch() {
  // Push static zeros once, no DMA
  uint8_t zeros[NUM_OF_SHIFT_REGS];
  memset(zeros, 0x00, sizeof(zeros));

  digitalWriteFast(LATCH_PIN, LOW);
  SPI.beginTransaction(SPISettings(PWM_SPI_HZ, MSBFIRST, PWM_SPI_MODE));
  for (int i = 0; i < NUM_OF_SHIFT_REGS; ++i) SPI.transfer((uint8_t)0x00);
  SPI.endTransaction();
  latchPulse();  // updates Q0–Q7 on every 595 to 0
}

void PWMEngine::shutdown() {
  // optional: zero your logical RGBs for next boot
  for (int i = 0; i < NUM_OF_LEDS; ++i) {
    leds[i].setRed(0);
    leds[i].setGreen(0);
    leds[i].setBlue(0);
  }
  running = false;  // stop queueing new DMA slices
  allOffLatch();    // force outputs LOW once
}