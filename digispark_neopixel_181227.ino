// 8mhz - No USB
#include <Adafruit_NeoPixel.h>

#define PIN_NEOPIXEL 1
#define NUM_PIXELS  15

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUM_PIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
uint8_t color[NUM_PIXELS], levelNow[NUM_PIXELS], levelMax[NUM_PIXELS], direction[NUM_PIXELS];

void setup() {
  pixels.begin();
  pixels.setBrightness(80);
  pixels.show(); // Initialize all pixels to 'off'
  for (uint8_t i = 0; i < NUM_PIXELS; i++) color[i] = levelNow[i] = levelMax[i] = direction[i] = 0;
  randomSeed(analogRead(0));
}

void loop() {
  randomWhite((18 << 10) + random(12 << 10), false);
  rainbow(18 + random(21));
  rainbow(18 + random(21));
  randomWhite((30 << 10) + random(10 << 10), true);
  chase();
  randomWhite((15 << 10) + random(10 << 10), true);
  rainbowCycle(18 + random(21));
}

void colorWipe(uint32_t c, uint8_t wait) {
  for (uint8_t i = 0; i < pixels.numPixels(); i++) {
    pixels.setPixelColor(i, c);
    pixels.show();
    delay(wait);
  }
}

void rainbow(uint8_t wait) {
  uint8_t i;
  uint16_t j;
  for (j = 0; j < 256; j++) {
    for (i = 0; i < pixels.numPixels(); i++) pixels.setPixelColor(i, Wheel((i + j) & 0xff));
    pixels.show();
    delay(wait);
  }
}

void rainbowCycle(uint8_t wait) {
  uint16_t i, j;
  for (j = 0; j < (5 << 8); j++) { // 5 cycles of all colors on wheel
    for (i = 0; i < pixels.numPixels(); i++) pixels.setPixelColor(i, Wheel((((i << 8) / pixels.numPixels()) + j) & 0xff));
    pixels.show();
    delay(wait);
  }
}

void chase() {
  uint8_t LedPos[6];
  for (uint8_t i = 0; i < 3; i++) {
    LedPos[(i << 1) + 1] =  pixels.numPixels() / 3 * i;
    LedPos[ i << 1] = LedPos[(i << 1) + 1] + 2;
  }
  uint8_t ColorType = 1;
  uint32_t LastChangeTime = millis();
  uint32_t duration = (10 << 10) + random(12 << 10);
  uint16_t fadeCount = 0;
  uint8_t wait = 2;
  colorWipe(pixels.Color(0, 0, 0), 0);
  while (1) {
    if (millis() - LastChangeTime > duration) {
      if (++ColorType & 0x4) break;
      LastChangeTime = millis();
      wait = 5 + random(7);
    }
    switch (ColorType) {
      case 1: // B
        for (uint8_t i = 0; i < 3; i++) {
          pixels.setPixelColor(LedPos[ i << 1 ],     pixels.Color(0, 0, fadeCount));
          pixels.setPixelColor(LedPos[(i << 1) + 1], pixels.Color(0, 0, 255 - fadeCount));
        }
        pixels.show();
        break;
      case 2: // CMY
        pixels.setPixelColor(LedPos[0], pixels.Color(fadeCount,       fadeCount,       0));
        pixels.setPixelColor(LedPos[1], pixels.Color(255 - fadeCount, 255 - fadeCount, 0));
        pixels.setPixelColor(LedPos[2], pixels.Color(0,               fadeCount,       fadeCount));
        pixels.setPixelColor(LedPos[3], pixels.Color(0,               255 - fadeCount, 255 - fadeCount));
        pixels.setPixelColor(LedPos[4], pixels.Color(fadeCount,       0,               fadeCount));
        pixels.setPixelColor(LedPos[5], pixels.Color(255 - fadeCount, 0,               255 - fadeCount));
        pixels.show();
        break;
      case 3: // Yellow/White/Orange, Blue
        pixels.setPixelColor(LedPos[0], pixels.Color(fadeCount,       fadeCount,              32));
        pixels.setPixelColor(LedPos[1], pixels.Color(255 - fadeCount, 255 - fadeCount,        32));
        pixels.setPixelColor(LedPos[2], pixels.Color(fadeCount,       fadeCount,              (fadeCount > 32) ? fadeCount : 32));
        pixels.setPixelColor(LedPos[3], pixels.Color(255 - fadeCount, 255 - fadeCount,        (255 - fadeCount > 32) ? 255 - fadeCount : 32));
        pixels.setPixelColor(LedPos[4], pixels.Color(fadeCount,       fadeCount >> 1,         32));
        pixels.setPixelColor(LedPos[5], pixels.Color(255 - fadeCount, (255 - fadeCount) >> 1, 32));
        pixels.show();
        break;
    }
    if (++fadeCount & 0x100) {
      for (uint8_t i = 0; i < 6; i++) if (++LedPos[i] >= pixels.numPixels()) LedPos[i] = 0;
      fadeCount = 0;
    }
    delay(wait);
  }
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if (WheelPos < 85) return pixels.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  if (WheelPos < 170) {
    WheelPos -= 85;
    return pixels.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return pixels.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

void randomWhite(uint32_t timeMills, bool rgb) {
  uint32_t startTime = millis();
  do {
    for (uint8_t i = 0; i < pixels.numPixels(); i++) {
      if (!levelNow[i] && !direction[i]) {
        color[i] = rgb ? random(6) : random(3);
        levelMax[i] = 192 + random(64);
        direction[i] = 1;
      } else if (!direction[i]) levelNow[i]--;
      else if (++levelNow[i] == levelMax[i]) direction[i] = 0;
    setPixelWhite(i, color[i], levelNow[i]);
    }
    pixels.show();
    delay(20);
  } while (millis() - startTime < timeMills);
}

void setPixelWhite(uint8_t pos, uint8_t color, uint8_t level) {
  pixels.setPixelColor(pos, (color == 1) ? pixels.Color(255 - (level >> 2), 255 -  level,       255 - (level >> 2))
                          : (color == 2) ? pixels.Color(255 - (level >> 2), 255 - (level >> 2), 255 -  level)
                          : (color == 3) ? pixels.Color(255 - (level >> 2), 255 -  level,       255 -  level)
                          : (color == 4) ? pixels.Color(255 -  level,       255 - (level >> 2), 255 -  level)
                          : (color == 5) ? pixels.Color(255 -  level,       255 -  level,       255 - (level >> 2))
                          :                pixels.Color(255 -  level,       255 - (level >> 2), 255 - (level >> 2))); // color == 0
}
