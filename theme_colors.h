#pragma once
#include <cstdint>
#include <cstddef>

struct Color {
  constexpr Color(uint8_t r_, uint8_t g_, uint8_t b_)
    : r(r_), g(g_), b(b_) {}
  constexpr uint8_t red() const {
    return r;
  }
  constexpr uint8_t grn() const {
    return g;
  }
  constexpr uint8_t blu() const {
    return b;
  }
private:
  uint8_t r, g, b;
};

struct Theme {
  uint8_t size;
  const Color* colors;
  constexpr Theme(uint8_t s = 0, const Color* c = nullptr)
    : size(s), colors(c) {}
};

namespace ThemeDefs {
// Inline constexpr gives header-only, single-definition semantics.
inline constexpr Color NewYears[] = {
  Color(255, 215, 0),    // Gold
  Color(255, 0, 255),    // Magenta
  Color(192, 192, 192),  // Silver
  Color(255, 80, 160),   // pink
  Color(255, 255, 255),  // White
  Color(125, 0, 100),    // Black
  Color(0, 0, 255),    // Blue
};

inline constexpr Color Valentines[] = {
  Color(220, 20, 60),    // Crimson
  Color(255, 105, 180),  // Hot Pink
  Color(255, 255, 255),  // White
  Color(255, 0, 255),    // Magenta
  Color(199, 21, 133)    // Deep Rose
};

inline constexpr Color StPatricks[] = {
  Color(0, 155, 72),     // Irish Green
  Color(48, 191, 81),    // Shamrock
  Color(255, 215, 0),    // Gold
  Color(255, 255, 255),  // White
  Color(255, 140, 0)     // Orange
};

inline constexpr Color MyBDay[] = {
  Color(0, 155, 72),
  Color(48, 191, 81),
  Color(255, 215, 0),
  Color(255, 255, 255),
  Color(255, 140, 0)
};

inline constexpr Color Easter[] = {
  Color(255, 182, 193),  // Pastel Pink
  Color(255, 239, 161),  // Pastel Yellow
  Color(230, 230, 250),  // Lavender
  Color(135, 206, 250),  // Sky Blue
  Color(189, 252, 201)   // Mint
};

inline constexpr Color Memorial[] = {
  Color(178, 34, 34),    // Red
  Color(255, 255, 255),  // White
  Color(0, 0, 128)       // Navy
};

inline constexpr Color ID4[] = {
  Color(220, 20, 60),    // Red
  Color(255, 255, 255),  // White
  Color(0, 0, 255)       // Blue
};

inline constexpr Color Labor[] = {
  Color(178, 34, 34),    // Red
  Color(255, 255, 255),  // White
  Color(0, 0, 139)       // Dark Blue
};

inline constexpr Color Halloween[] = {
  Color(255, 61, 5),  // Pumpkin Orange
  Color(128, 0, 128),   // Purple
  Color(138, 3, 3)      // Blood Red
};

inline constexpr Color Thanksgiving[] = {
  Color(255, 117, 24),  // Pumpkin
  Color(139, 69, 19),   // Brown
  Color(128, 0, 0),     // Maroon
  Color(218, 165, 32),  // Goldenrod
  Color(128, 128, 0)    // Olive
};

inline constexpr Color Christmas[] = {
  Color(220, 20, 60),    // Red
  Color(255, 215, 0),    // Gold
  Color(0, 255, 0),      // Green
  Color(192, 192, 192),  // Silver
  Color(0, 0, 255),      // Blue
  Color(255, 0, 255)     // Purple
};

inline constexpr Color Cops[] = {
  Color(255, 0, 0),
  Color(0, 0, 255)
};

inline constexpr Color Off[] = {
  Color(0, 0, 0)
};
}  // namespace ThemeDefs

class Themes {
  template<size_t N>
  static constexpr Theme make(const Color (&arr)[N]) {
    return Theme((uint8_t)N, arr);
  }
public:
  constexpr Themes() {}

  constexpr Theme getTheme(int idx) const {
    switch (idx) {
      case 0: return make(ThemeDefs::NewYears);
      case 1: return make(ThemeDefs::Valentines);
      case 2: return make(ThemeDefs::StPatricks);
      case 3: return make(ThemeDefs::MyBDay);
      case 4: return make(ThemeDefs::Easter);
      case 5: return make(ThemeDefs::Memorial);
      case 6: return make(ThemeDefs::ID4);
      case 7: return make(ThemeDefs::Labor);
      case 8: return make(ThemeDefs::Halloween);
      case 9: return make(ThemeDefs::Thanksgiving);
      case 10: return make(ThemeDefs::Christmas);
      case 11: return make(ThemeDefs::Cops);
      case 12:
      default: return make(ThemeDefs::Off);
    }
  }
};