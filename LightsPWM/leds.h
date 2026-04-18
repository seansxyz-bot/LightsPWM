#ifndef LEDS_H
#define LEDS_H


class LEDs {
public:
  LEDs() {}

    void setRed(int r);
  void setGreen(int g);
  void setBlue(int b);

  int getRed()   const;
  int getGreen() const;
  int getBlue()  const;
private:
  // Approc 2700K warm white: 255, 214, 170 more warm: 255, 200, 150
  int red = 0;
  int grn = 0;
  int blue = 0;
};

#endif  // LEDS_H