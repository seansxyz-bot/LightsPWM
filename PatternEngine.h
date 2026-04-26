#pragma once

#include <Arduino.h>
#include <math.h>

#include "PWMEngine.h"
#include "pattern_file_reader.h"

namespace {
inline float clamp01f(float v) {
  return v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
}

inline int wrap_dist(int i, int j, int n) {
  int d = abs(i - j);
  return (d < (n - d)) ? d : (n - d);
}

inline float comet_brightness(int i, int head, int n) {
  switch (wrap_dist(i, head, n)) {
    case 0: return 1.00f;
    case 1: return 0.75f;
    case 2: return 0.50f;
    case 3: return 0.25f;
    default: return 0.18f;
  }
}
}

class PatternEngine {
public:
  // IDs:
  // 0=Off, 1=Combo, 2=Chase, 3=Comet, 4=Waves, 5=Slo-Glo,
  // 6=Twinkle, 7=Slow Fade, 8=In Waves, 9=Alternate
  explicit PatternEngine(int activeID = 0) {
    setActive(activeID);
  }

  void setActive(int activeID) {
    activeID_ = activeID;
    combo_mode_ = 0;
    combo_repeat_count_ = 0;
    combo_elapsed_s_ = 0.f;
  }

  void setSpeedTable(const PatternSpeedTable& table) {
    speeds_ = table;
  }

  void primeFromPWM(const PWMEngine& pwm) {
    for (int i = 0; i < kDots; ++i) {
      base_[i][0] = (uint8_t)pwm.leds[i].getRed();
      base_[i][1] = (uint8_t)pwm.leds[i].getGreen();
      base_[i][2] = (uint8_t)pwm.leds[i].getBlue();
      twinkle_[i] = 0.f;
      twinkle2_[i] = 0.f;
    }
    primed_ = true;
  }

  void tick(PWMEngine& pwm) {
    if (activeID_ == 0) return;
    if (!primed_) primeFromPWM(pwm);
    if (frameTimer_ < framePeriodUs_) return;

    const float dt_raw = frameTimer_ / 1e6f;
    frameTimer_ = 0;

    const uint8_t pct = currentSpeedPct_();
    const float rate = (float)pct / 50.f;
    const float dt = dt_raw * rate;

    if (rate > 0.f) {
      t_ += dt;
      phase_ += 6.2831853f * (1.f / 3.f) * dt;

      chase_accum_ += dt;
      if (chase_accum_ >= 0.25f) {
        chase_accum_ = 0.f;
        chase_idx_ = (chase_idx_ + 1) % kDots;
      }

      if (activeID_ == 1) {
        combo_elapsed_s_ += dt;
        const float one_repeat = comboRepeatLenSeconds_(combo_mode_);
        if (combo_elapsed_s_ >= one_repeat) {
          combo_elapsed_s_ = 0.f;
          if (++combo_repeat_count_ >= 5) {
            combo_repeat_count_ = 0;
            combo_mode_ = (combo_mode_ + 1) % 7;
          }
        }
      }
    }

    const int parity = (int)floorf(t_ / 0.5f) & 1;
    const int step = 1;
    const int frameOffset = (parity == 0) ? 0 : step;

    for (int i = 0; i < kDots; ++i) {
      float b = 1.f;

      switch (activeID_) {
        case 1:
          {
            switch (combo_mode_) {
              case 0:  // Chase, id 2
                b = (i == chase_idx_) ? 1.f : 0.18f;
                break;

              case 1:  // Comet, id 3
                b = comet_brightness(i, chase_idx_, kDots);
                break;

              case 2:  // Waves, id 4
                b = 0.35f + 0.65f * (0.5f * (1.f + sinf(phase_ + i * 0.45f)));
                break;

              case 3:  // Slo-Glo, id 5
                b = 0.22f + 0.78f * (0.5f * (1.f + sinf(t_ * 0.6f)));
                break;

              case 4:  // Twinkle, id 6
                b = twinkleStep_(twinkle2_[i], i);
                break;

              case 5:  // Fade, id 7
                b = 0.20f + 0.80f * (0.5f * (1.f + sinf(t_ * 0.6f)));
                break;

              case 6:
                {  // Alternate, id 8
                  const int src = ((i - frameOffset) % kDots + kDots) % kDots;
                  pwm.leds[i].setRed(base_[src][0]);
                  pwm.leds[i].setGreen(base_[src][1]);
                  pwm.leds[i].setBlue(base_[src][2]);
                  continue;
                }
            }
          }
          break;
        case 2:
          b = (i == chase_idx_) ? 1.f : 0.18f;
          break;
        case 3:
          b = comet_brightness(i, chase_idx_, kDots);
          break;
        case 4:
          b = 0.35f + 0.65f * (0.5f * (1.f + sinf(phase_ + i * 0.5f)));
          break;
        case 5:
          b = 0.25f + 0.75f * (0.5f * (1.f + sinf(t_ * 0.8f)));
          break;
        case 6:
          b = twinkleStep_(twinkle_[i], i);
          break;
        case 7:
          b = 0.20f + 0.80f * (0.5f * (1.f + sinf(t_ * 0.6f)));
          break;
        case 8:
          {  // Alternate
            int src = i - frameOffset;
            if (src < 0) src += kDots;

            pwm.leds[i].setRed(base_[src][0]);
            pwm.leds[i].setGreen(base_[src][1]);
            pwm.leds[i].setBlue(base_[src][2]);
            continue;
          }

        default:
          break;
      }

      const uint8_t r0 = base_[i][0];
      const uint8_t g0 = base_[i][1];
      const uint8_t b0 = base_[i][2];

      const uint8_t R = (uint8_t)roundf((float)r0 * clamp01f(b));
      const uint8_t G = (uint8_t)roundf((float)g0 * clamp01f(b));
      const uint8_t B = (uint8_t)roundf((float)b0 * clamp01f(b));

      pwm.leds[i].setRed(R);
      pwm.leds[i].setGreen(G);
      pwm.leds[i].setBlue(B);
    }
  }

private:
  static constexpr int kDots = NUM_OF_LEDS;

  uint8_t currentPatternId_() const {
    if (activeID_ != 1) {
      return activeID_;
    }

    // Combo cycles pattern IDs 2-7.
    // combo_mode_: 0..5 -> patternId: 2..7
    return (uint8_t)(combo_mode_ + 2);
  }

  uint8_t currentSpeedPct_() const {
    return speedForPatternId_(currentPatternId_());
  }

  uint8_t speedForPatternId_(uint8_t patternId) const {
    switch (patternId) {
      case 2: return speeds_.chase;
      case 3: return speeds_.comet;
      case 4: return speeds_.waves;
      case 5: return speeds_.sloglo;
      case 6: return speeds_.twinkle;
      case 7: return speeds_.slowfade;
      case 8: return speeds_.alternate;
      default: return 50;
    }
  }

  float comboRepeatLenSeconds_(int mode) const {
    switch (mode) {
      case 0:
      case 1:
        return 0.25f * kDots;
      case 2:
        return 3.0f;
      case 3:
        return 6.2831853f / 0.6f;
      case 4:
        return 2.0f;
      case 5:
        return 6.2831853f / 1.2f;
      case 6:
        return 1.0f;
      default:
        return 2.0f;
    }
  }

  float twinkleStep_(float& store, int i) const {
    store = max(0.06f, store * 0.92f);

    const float pct = (float)currentSpeedPct_();
    const float spark_win = (1.f / 30.f) * (pct / 50.f);
    const bool spark = fmodf(t_ + i * 0.07f, 0.2f) < spark_win;

    if (spark) store = 1.f;
    return store;
  }

  PatternSpeedTable speeds_{};

  int activeID_ = 0;
  bool primed_ = false;
  float t_ = 0.f;
  float phase_ = 0.f;
  float chase_accum_ = 0.f;
  int chase_idx_ = 0;
  int combo_mode_ = 0;

  int combo_repeat_count_ = 0;
  float combo_elapsed_s_ = 0.f;

  float twinkle_[kDots] = { 0 };
  float twinkle2_[kDots] = { 0 };

  uint8_t base_[kDots][3] = { { 0, 0, 0 } };

  elapsedMicros frameTimer_;
  static constexpr uint32_t framePeriodUs_ = 1000000 / 60;
};