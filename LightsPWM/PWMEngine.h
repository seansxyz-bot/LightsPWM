#pragma once
#include <Arduino.h>
#include <SPI.h>
#include "leds.h"

// ===== Config (tune as needed) =====
#define NUM_OF_LEDS             19
#define NUM_OF_SHIFT_REGS       8
#define MAX_FRAMES              256                 // 8-bit PWM slices

#define NUM_OF_OUTPUTS          (NUM_OF_SHIFT_REGS * 8) // 48
#define NUM_OF_USED_OUTPUTS (NUM_OF_LEDS * 3)   // RGBRGB...
#define NUM_OF_UNUSED_OUTPUTS   (NUM_OF_OUTPUTS - NUM_OF_USED_OUTPUTS)

// Pins / SPI
#define LATCH_PIN               2
#define PWM_SPI_HZ              1000000
#define PWM_SPI_MODE            SPI_MODE0

class PWMEngine {
public:
  uint8_t pwmSlice = 0;   // 0..255
  LEDs leds[NUM_OF_LEDS];

  // Call once in setup(). Starts continuous DMA-driven PWM.
  bool begin();

  // Optional: stop / start controls (no loop polling required)
  void stop();
  void start();   // resumes from current slice
  void shutdown();
void allOffLatch();

private:
  // DMA plumbing
  static void onSpiDmaDoneStatic(EventResponder &er);
  void onSpiDmaDone();
  void startDmaFrame(const uint8_t* buf);

  // Latch line
  inline void latchPulse();

  // Build the next frame (for 'slice') into drawBuf
  void buildSliceFrame(uint8_t slice);

  // Map logical channel -> (byte, bitmask)
  static inline void chToByteBit(uint16_t ch, uint16_t& byteIndex, uint8_t& bitMask);

  // Instance handle for static callback
  static PWMEngine* s_instance;

  // State
  volatile bool running       = false;
  volatile bool xferInFlight  = false;


  EventResponder dmaDoneEvt;

  // Double buffers: one byte per 595
  uint8_t activeBuf[NUM_OF_SHIFT_REGS];
  uint8_t drawBuf  [NUM_OF_SHIFT_REGS];
};