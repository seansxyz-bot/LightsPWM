#include "LEDs.h"


void LEDs::setRed(int r)        { red = r; }
void LEDs::setGreen(int g)      { grn = g; }
void LEDs::setBlue(int b)       { blue = b; }

int LEDs::getRed()   const { return  red ; }   // <-- const
int LEDs::getGreen() const { return  grn ; }   // <-- const
int LEDs::getBlue()  const { return  blue;  }  // <-- const