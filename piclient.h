#pragma once
#include <Arduino.h>
#include <Wire.h>

namespace PIClient {
// Read-requests (unchanged)
static constexpr uint8_t REQ_WAKE_READY = 0xF0;
static constexpr uint8_t REQ_LED_STATE = 0xF1;
static constexpr uint8_t REQ_ALL_OFF_STATUS = 0xF2;
static constexpr uint8_t REQ_FILE_STATUS = 0xF3;

struct WriteMsg {
  uint8_t len;      // always 8 for valid packets
  uint8_t data[8];  // exactly 8B payload
};

// --- ring buffer for write packets ---
static constexpr uint8_t Q_CAP = 16;
static volatile uint8_t qHead = 0;  // ISR writes
static volatile uint8_t qTail = 0;  // loop reads
static WriteMsg qBuf[Q_CAP];

// --- request arm state (set by onReceive when first byte is REQ_*) ---
static volatile uint8_t g_lastReq = 0;
static volatile bool g_haveReq = false;

// --------- helpers you call from loop()/onRequest ----------
inline bool pop(WriteMsg &out) {
  if (qTail == qHead) return false;
  out = qBuf[qTail];
  qTail = (uint8_t)((qTail + 1) % Q_CAP);
  return true;
}

// Atomically consume a pending read-request arm (for your onRequest)
inline bool takeArmedRequest(uint8_t &req) {
  if (!g_haveReq) return false;
  req = g_lastReq;
  g_haveReq = false;  // one response per arm
  return true;
}

// --------- ISR side: onReceive handler (STRICT 8-BYTE WRITES) ----------
static void onReceiveISR(int n) {
  if (n <= 0) return;

  // First byte chooses: arm read-page OR a write command to queue
  uint8_t first = (uint8_t)Wire.read();
  --n;

  if (first == REQ_WAKE_READY || 
      first == REQ_LED_STATE || 
      first == REQ_ALL_OFF_STATUS || 
      first == REQ_FILE_STATUS) {
    g_lastReq = first;
    g_haveReq = true;
    while (n-- > 0) (void)Wire.read();  // drain extras
    return;
  }

  // Otherwise: we expect exactly 7 more bytes to make 8 total
  uint8_t tmp[8];
  tmp[0] = first;
  uint8_t i = 1;
  for (; i < 8 && n > 0; ++i, --n) tmp[i] = (uint8_t)Wire.read();
  while (n-- > 0) (void)Wire.read();  // drain overflow safely

  if (i != 8) {
    // Invalid length (not exactly 8) — drop packet
    return;
  }

  // Queue it; if full, drop oldest
  uint8_t next = (uint8_t)((qHead + 1) % Q_CAP);
  if (next == qTail) qTail = (uint8_t)((qTail + 1) % Q_CAP);

  WriteMsg &m = qBuf[qHead];
  m.len = 8;
  for (uint8_t k = 0; k < 8; ++k) m.data[k] = tmp[k];
  qHead = next;
}

// --------- begin: set address, speed, and your onRequest callback ----------
inline void begin(uint8_t addr, void (*onRequestCb)(), uint32_t hz = 100000) {
  Wire.begin(addr);
  Wire.setClock(hz);
  Wire.onReceive(onReceiveISR);
  Wire.onRequest(onRequestCb);  // your onRequest lives in lights.ino
}
}  // namespace PIClient,
