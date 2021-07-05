// 16mhz - No USB

// PB0: 14 SI
// PB1: 11 SCK
// PB2: 12 RCK
// PB3: 13 ~G
// PB4: 10 ~SCLR
#define PIN_SI   0b00000001
#define PIN_SCK  0b00000010
#define PIN_RCK  0b00000100
#define PIN_G    0b00001000
#define PIN_SCLR 0b00010000

const uint8_t interval = 8;
const uint8_t cubeMaxValue = 100;
const int8_t  minMag =  7 << 1;
const int8_t  maxMag = 12 << 1;
const uint8_t ledShowDelayUs = 5;

const uint8_t numsZ  = 3;
const uint8_t numsXY = 3 * 3;

uint8_t cubeValue[numsZ][numsXY] = { 0 };
int8_t  pos[3] = { 0, 0, 0 };
int8_t  dir[3] = { 3, 3, 3 };
int8_t  mag = maxMag;
int8_t  dirMag = (maxMag - minMag) / 20;
uint8_t showCount = 0;

void setup() {
  PORTB = PIN_G | PIN_SCLR;
  DDRB = 0x1f;  // PB4-0:OUTPUT, PB5:INPUT
  randomSeed(analogRead(PB5));
}

void loop() {
  showLED();
  if (showCount++ == interval) {
    changePos();
    showCount = 0;
  }
}

void showLED() {
  for (uint8_t i = cubeMaxValue - 1; i > 0; i--) {
    for (uint8_t z = 0; z < numsZ; z++) {
      for (uint8_t xy = 0; xy < numsXY; xy++) {
        if (cubeValue[z][xy] > i) PORTB &= ~PIN_SI; else PORTB |= PIN_SI;
        PORTB |= PIN_SCK;
        PORTB &=~PIN_SCK;
      }
      for (uint8_t zl = 0; zl < numsZ; zl++) {
        if (zl != z) PORTB &= ~PIN_SI; else PORTB |= PIN_SI;
        PORTB |= PIN_SCK;
        PORTB &=~PIN_SCK;
      }
      PORTB |= PIN_RCK;
      PORTB &=~PIN_RCK;
      PORTB &=~PIN_G;
      delayMicroseconds(ledShowDelayUs);
      PORTB |= PIN_G;
    }
  }
}

void changePos() {
  for (int8_t z = 0; z < numsZ; z++) {
    for (int8_t xy = 0; xy < numsXY; xy++) {
      int8_t dx = (xy / (int8_t)numsZ << 4) - pos[0];
      int8_t dy = (xy % (int8_t)numsZ << 4) - pos[1];
      int8_t dz =                  (z << 4) - pos[2];
      if (dx < 0) dx = -dx;
      if (dy < 0) dy = -dy;
      if (dz < 0) dz = -dz;
      uint8_t dist = (uint8_t)dx + (uint8_t)dy + (uint8_t)dz;
      if (dist & 0xf0 && mag > 3) dist = 0xff;
      else if (dist & 0xfc) dist = (dist >> 5) * (uint8_t)mag;
      else dist = dist * (uint8_t)mag >> 5;
      cubeValue[z][xy] = cubeMaxValue / (dist ? dist : 1);
    }
  }
  for (uint8_t i = 0; i < 3; i++) {
    if ((pos[i] += dir[i]) > ((int8_t)numsZ - 1 << 4)) {
      pos[i] = (int8_t)numsZ - 1 << 4;
      dir[i] = -random(1, 7);
    } else if (pos[i] < 0) {
      pos[i] = 0;
      dir[i] = random(1, 7);
    }
  }
  if ((mag += dirMag) > maxMag) {
    mag = maxMag;
    dirMag = -random(1, 4);
  } else if (mag < minMag) {
    mag = minMag;
    dirMag = random(1, 4);
  }
}
