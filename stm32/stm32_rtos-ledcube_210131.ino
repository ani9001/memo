#include <MapleFreeRTOS900.h>

#define BOARD_LED_PIN PC13
#define ADC_PIN       PB0

const uint8 pinsZ[]  = { PB14, PB13, PB12 };
const uint8 pinsXY[] = { PA0, PA1, PA2, PA3, PA4, PA5, PA6, PA7, PA8 };

const float minMag =  1.0;
const float maxMag = 10.0;
const unsigned int cubeMaxValue = 100;
const float dirScale = 100.0;
const int interval = 50;

const unsigned int numsZ  = sizeof(pinsZ)  / sizeof(pinsZ[0]);
const unsigned int numsXY = sizeof(pinsXY) / sizeof(pinsXY[0]);

unsigned int cubeValue[numsZ][numsXY] = { 0 };
float pos[3] = { 0.0,  0.0,  0.0  };
float dir[3] = { 0.05, 0.05, 0.05 };
float mag = maxMag;
float dirMag = (maxMag - minMag) / 20.0;

int boardLed = HIGH;

void setup() {
  pinMode(BOARD_LED_PIN, OUTPUT);
  for (unsigned int i = 0; i < numsZ;  i++) pinMode(pinsZ[i],  OUTPUT);
  for (unsigned int i = 0; i < numsXY; i++) pinMode(pinsXY[i], OUTPUT);
  pinMode(ADC_PIN, INPUT_ANALOG);
  randomSeed(analogRead(ADC_PIN));

  xTaskCreate(vLEDshow,      "led", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
  xTaskCreate(vLEDchangePos, "pos", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
  vTaskStartScheduler();
}

static void vLEDshow(void *pvParams) {
  while (1) {
    for (unsigned int i = 0; i < cubeMaxValue; i++) {
      for (unsigned int z = 0; z < numsZ; z++) {
        clearLED();
        digitalWrite(pinsZ[z], HIGH);
        for (unsigned int xy = 0; xy < numsXY; xy++) digitalWrite(pinsXY[xy], cubeValue[z][xy] > i ? LOW : HIGH);
      }
    }
  }
}

void vLEDchangePos(void *pvParams) {
  while (1) {
    for (unsigned int z = 0; z < numsZ; z++) {
      for (unsigned int xy = 0; xy < numsXY; xy++) {
        float dx = mag * ((float)(xy / numsZ) - pos[0]);
        float dy = mag * ((float)(xy % numsZ) - pos[1]);
        float dz = mag * ((float)z - pos[2]);
        cubeValue[z][xy] = cubeMaxValue / (dx * dx + dy * dy + dz * dz);
      }
    }
    for (unsigned int i = 0; i < 3; i++) {
      if ((pos[i] += dir[i]) > (numsZ - 1)) {
        pos[i] = numsZ - 1;
        dir[i] = (dir[i] > 0 ? -1.0 : 1.0) * (float)random(2, 16) / dirScale;
      } else if (pos[i] < 0) {
        pos[i] = 0.0;
        dir[i] = (dir[i] > 0 ? -1.0 : 1.0) * (float)random(2, 16) / dirScale;
      }
    }
    if ((mag += dirMag) > maxMag) {
      mag = maxMag;
      dirMag = (dirMag > 0 ? -1.0 : 1.0) * (float)random(2, 16) / dirScale;
    } else if (mag < minMag) {
      mag = minMag;
      dirMag = (dirMag > 0 ? -1.0 : 1.0) * (float)random(2, 16) / dirScale;
    }
    vTaskDelay(interval);
    digitalWrite(BOARD_LED_PIN, boardLed = boardLed == HIGH ? LOW : HIGH);
  }
}

void clearLED() {
  for (unsigned int i = 0; i < numsZ;  i++) digitalWrite(pinsZ[i],  LOW);
  for (unsigned int i = 0; i < numsXY; i++) digitalWrite(pinsXY[i], HIGH);
}
