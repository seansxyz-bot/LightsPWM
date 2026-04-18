#pragma once
#include <Arduino.h>
#include "PWMEngine.h"

namespace {
inline float clamp01f(float v) {
  return v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
}

// ring distance helper for Comet
inline int wrap_dist(int i, int j, int n) {
  int d = abs(i - j);
  return (d < (n - d)) ? d : (n - d);
}

// brightness kernel: head + 3-step tails; background 0.18
inline float comet_brightness(int i, int head, int n) {
  switch (wrap_dist(i, head, n)) {
    case 0: return 1.00f;
    case 1: return 0.75f;
    case 2: return 0.50f;
    case 3: return 0.25f;
    default: return 0.18f;
  }
}
}  // namespace

class PatternEngine {
public:
  // IDs: 0=Off, 1=Combo, 2=Chase, 3=Comet, 4=Waves, 5=Slo-Glo, 6=Twinkle,
  //      7=Slow Fade, 8=In Waves, 9=Alternate
  explicit PatternEngine(int activeID = 0) {
    setActive(activeID);
  }

  void setActive(int activeID) {
    activeID_ = activeID;
    combo_mode_ = 0;
    // reset combo cycling state
    combo_repeat_count_ = 0;
    combo_elapsed_s_ = 0.f;
    // if turning on and we haven't captured a base yet, caller should call primeFromPWM()
  }

  void setSpeedPercent(int pct) {
    speedPct_ = (pct < 0) ? 0 : ((pct > 100) ? 100 : pct);
  }

  // Copy CURRENT pwm.leds[] into our base_ buffer. Call this whenever the “theme” or raw LED colors change.
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

  // ~60 FPS; writes scaled RGB into pwm.leds[i]
  void tick(PWMEngine& pwm) {
    if (activeID_ == 0) return;       // Off = do nothing
    if (!primed_) primeFromPWM(pwm);  // safety net
    if (frameTimer_ < framePeriodUs_) return;

    const float dt_raw = frameTimer_ / 1e6f;
    frameTimer_ = 0;
    const float rate = speedPct_ / 50.f;  // 50% slider = 1x
    const float dt = dt_raw * rate;

    if (rate > 0.f) {
      t_ += dt;
      phase_ += 6.2831853f * (1.f / 3.f) * dt;  // ~3s period @ 1x
      chase_accum_ += dt;
      if (chase_accum_ >= 0.25f) {  // head steps ~4/s @ 1x
        chase_accum_ = 0.f;
        chase_idx_ = (chase_idx_ + 1) % kDots;
      }

      // Combo: time-based repeats; after 5 repeats, advance to next submode
      if (activeID_ == 1) {
        combo_elapsed_s_ += dt;  // dt is already scaled by speedPct_
        const float one_repeat = comboRepeatLenSeconds_(combo_mode_);
        if (combo_elapsed_s_ >= one_repeat) {
          combo_elapsed_s_ = 0.f;
          if (++combo_repeat_count_ >= 5) {
            combo_repeat_count_ = 0;
            combo_mode_ = (combo_mode_ + 1) % 7;  // Chase, Comet, Waves, Slo-Glo, Twinkle, In Waves, Alternate
          }
        }
      }
    }

    // Alternate ping-pong (used by Alternate pattern and by Combo->Alternate):
    // 1x: flip every 0.5 s; full cycle 1.0 s
    const int parity = (int)floorf(t_ / 0.5f) & 1;  // 0,1,0,1,...
    const int step = 1;                             // shift by 1
    const int frameOffset = (parity == 0) ? 0 : step;

    for (int i = 0; i < kDots; ++i) {
      float b = 1.f;

      switch (activeID_) {
        case 1:
          {  // Combination
            switch (combo_mode_) {
              case 0:  // Chase (single head)
                b = (i == chase_idx_) ? 1.f : 0.18f;
                break;

              case 1:  // Comet (head + 3 tails)
                b = comet_brightness(i, chase_idx_, kDots);
                break;

              case 2:  // Waves (match preview spacing)
                b = 0.35f + 0.65f * (0.5f * (1.f + sinf(phase_ + i * 0.45f)));
                break;

              case 3:  // Slo-Glo
                b = 0.22f + 0.78f * (0.5f * (1.f + sinf(t_ * 0.6f)));
                break;

              case 4:  // Twinkle
                b = twinkleStep_(twinkle2_[i], t_, i);
                break;

              case 5:  // In Waves
                b = 0.25f + 0.75f * (0.5f * (1.f + sinf(t_ * 1.2f + ((float)i / 2.0f) * 0.9f)));
                break;

              case 6:
                {  // Alternate: colors move right then back left
                  const int src = ((i - frameOffset) % kDots + kDots) % kDots;
                  pwm.leds[i].setRed(base_[src][0]);
                  pwm.leds[i].setGreen(base_[src][1]);
                  pwm.leds[i].setBlue(base_[src][2]);
                  continue;  // already wrote RGB; skip brightness scaling
                }
            }
          }
          break;

        case 2:  // Chase
          b = (i == chase_idx_) ? 1.f : 0.18f;
          break;

        case 3:  // Comet
          b = comet_brightness(i, chase_idx_, kDots);
          break;

        case 4:  // Waves (standalone uses 0.5 spacing in your preview)
          b = 0.35f + 0.65f * (0.5f * (1.f + sinf(phase_ + i * 0.5f)));
          break;

        case 5:  // Slo-Glo
          b = 0.25f + 0.75f * (0.5f * (1.f + sinf(t_ * 0.8f)));
          break;

        case 6:  // Twinkle
          b = twinkleStep_(twinkle_[i], t_, i);
          break;

        case 7:  // Slow Fade
          b = 0.20f + 0.80f * (0.5f * (1.f + sinf(t_ * 0.6f)));
          break;

        case 8:  // In Waves
          b = 0.25f + 0.75f * (0.5f * (1.f + sinf(t_ * 1.2f + ((float)i / 2.0f) * 0.9f)));
          break;

        case 9:
          {  // Alternate
            const int src = ((i - frameOffset) % kDots + kDots) % kDots;
            pwm.leds[i].setRed(base_[src][0]);
            pwm.leds[i].setGreen(base_[src][1]);
            pwm.leds[i].setBlue(base_[src][2]);
            continue;  // skip brightness path
          }

        default:
          break;
      }

      // Scale from BASE colors (captured from pwm.leds) – no palette.
      const uint8_t r0 = base_[i][0], g0 = base_[i][1], b0 = base_[i][2];
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

  // --- Combo cycling state ---
  int combo_repeat_count_ = 0;   // how many repeats we've done in this submode
  float combo_elapsed_s_ = 0.f;  // time accumulated within this repeat

  // Define one "repeat" length for each combo submode, at 1× (50% speed knob).
  // We accumulate with dt (already scaled by speedPct_), so this auto-scales.
  float comboRepeatLenSeconds_(int mode) const {
    switch (mode) {
      case 0:                  // Chase: head makes a full lap (head steps every 0.25s at 1×)
      case 1:                  // Comet: same head stepping
        return 0.25f * kDots;  // full lap time @ 1×

      case 2:  // Waves (phase_ += 2π/3 * dt) → full cycle ~3s @ 1×
        return 3.0f;

      case 3:                      // Slo-Glo (sin(t_*0.6)) → period = 2π/0.6
        return 6.2831853f / 0.6f;  // ≈ 10.47 s

      case 4:  // Twinkle (stochastic) → pick a musical feel
        return 2.0f;

      case 5:                      // In Waves (sin(t_*1.2 + ...)) → period = 2π/1.2
        return 6.2831853f / 1.2f;  // ≈ 5.24 s

      case 6:  // Alternate ping-pong (flip every 0.5s; full ping-pong ≈ 1.0s)
        return 1.0f;

      default:
        return 2.0f;
    }
  }

  float twinkleStep_(float& store, float /*t*/, int i) const {
    store = max(0.06f, store * 0.92f);
    const float spark_win = (1.f / 30.f) * (speedPct_ / 50.f);
    const bool spark = fmodf(t_ + i * 0.07f, 0.2f) < spark_win;
    if (spark) store = 1.f;
    return store;
  }

  // State
  int activeID_ = 0;
  int speedPct_ = 50;
  bool primed_ = false;

  float t_ = 0.f;
  float phase_ = 0.f;
  float chase_accum_ = 0.f;
  int chase_idx_ = 0;
  int combo_mode_ = 0;

  float twinkle_[kDots] = { 0 };
  float twinkle2_[kDots] = { 0 };

  // Base colors captured from pwm.leds[]
  uint8_t base_[kDots][3] = { { 0, 0, 0 } };

  elapsedMicros frameTimer_;
  static constexpr uint32_t framePeriodUs_ = 1000000 / 60;  // 60 FPS
};
