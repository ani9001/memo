#include <MapleFreeRTOS900.h>
#include <SerialCommand.h>

#define BOARD_LED_PIN PC13
#define ADC4RAND_PIN  PB0

const uint8 pinsZ[]  = { PB14, PB13, PB12 };
const uint8 pinsXY[] = { PA0, PA1, PA2, PA3, PA4, PA5, PA6, PA7, PA8 };

const unsigned int speedTimer = 10000;
const unsigned int minInterval = 3;
const unsigned int maxInterval = 3 << 6;
const unsigned int cubeMaxValue = 100;
const int minMag =  1 << 7;
const int maxMag = 10 << 7;

const unsigned int numsZ  = sizeof(pinsZ)  / sizeof(pinsZ[0]);
const unsigned int numsXY = sizeof(pinsXY) / sizeof(pinsXY[0]);

unsigned int cubeValue[numsZ][numsXY] = { 0 };
int pos[3] = { 0, 0, 0 };
int dir[3] = { 6, 6, 6 };
int mag = maxMag;
int dirMag = (maxMag - minMag) / 20;
unsigned int interval = maxInterval;
int dirInterval = -1;
int enLED = 1;
int enAutoChgSpeed = 1;

SerialCommand sCmd;
int boardLed = HIGH;

void setup() {
  Serial.begin(115200);
  pinMode(BOARD_LED_PIN, OUTPUT);
  for (unsigned int i = 0; i < numsZ;  i++) pinMode(pinsZ[i],  OUTPUT);
  for (unsigned int i = 0; i < numsXY; i++) pinMode(pinsXY[i], OUTPUT);
  pinMode(ADC4RAND_PIN, INPUT_ANALOG);
  randomSeed(analogRead(ADC4RAND_PIN));

  sCmd.addCommand("1", []() { enLED = 1; Serial.println("on"); });
  sCmd.addCommand("0", []() { enLED = 0; Serial.println("off"); });
  sCmd.addCommand("+", []() { enAutoChgSpeed = 0; interval = (interval >>= 1) < minInterval ? minInterval : interval; Serial.println(interval); });
  sCmd.addCommand("-", []() { enAutoChgSpeed = 0; interval = (interval <<= 1) > maxInterval ? maxInterval : interval; Serial.println(interval); });
  sCmd.addCommand("*", []() { enAutoChgSpeed = 1; Serial.println(interval); });
  sCmd.setDefaultHandler([]() { Serial.println("cmd: <1|0|+|-|*>"); });

  xTaskCreate(vLEDshow,      "led", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 2, NULL);
  xTaskCreate(vLEDchangePos, "pos", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL);
  xTaskCreate(vLEDchangeSpd, "spd", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY,     NULL);
  xTaskCreate(vSerialRead,   "ser", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY,     NULL);
  vTaskStartScheduler();
}

void vLEDshow(void *pvParams) {
  while (1) {
    if (enLED) {
      for (unsigned int i = 0; i < cubeMaxValue; i++) {
        for (unsigned int z = 0; z < numsZ; z++) {
          clearLED();
          digitalWrite(pinsZ[z], HIGH);
          for (unsigned int xy = 0; xy < numsXY; xy++) digitalWrite(pinsXY[xy], cubeValue[z][xy] > i ? LOW : HIGH);
        }
      }
    }
    vTaskDelay(1);
  }
}

void vLEDchangePos(void *pvParams) {
  while (1) {
    if (enLED) {
      for (unsigned int z = 0; z < numsZ; z++) {
        for (unsigned int xy = 0; xy < numsXY; xy++) {
          unsigned int dx = (xy / numsZ << 7) - pos[0];
          unsigned int dy = (xy % numsZ << 7) - pos[1];
          unsigned int dz =          (z << 7) - pos[2];
          unsigned int dist = ((dx * dx + dy * dy + dz * dz) * mag >> 14) * mag >> 14;
          cubeValue[z][xy] = cubeMaxValue / (dist ? dist : 1);
        }
      }
      for (unsigned int i = 0; i < 3; i++) {
        if ((pos[i] += dir[i]) > (int)(numsZ - 1 << 7)) {
          pos[i] = numsZ - 1 << 7;
          dir[i] = -random(2, 16);
        } else if (pos[i] < 0) {
          pos[i] = 0;
          dir[i] = random(2, 16);
        }
      }
      if ((mag += dirMag) > maxMag) {
        mag = maxMag;
        dirMag = -random(2, 16);
      } else if (mag < minMag) {
        mag = minMag;
        dirMag = random(2, 16);
      }
    }
    vTaskDelay(interval);
    digitalWrite(BOARD_LED_PIN, boardLed = boardLed == HIGH ? LOW : HIGH);
  }
}

void vLEDchangeSpd(void *pvParams) {
  while (1) {
    vTaskDelay(speedTimer);
    if (enLED && enAutoChgSpeed) {
      if (dirInterval > 0 && (interval <<= 1) > maxInterval) {
        interval = maxInterval;
        dirInterval = -1;
      } else if (dirInterval < 0 && (interval >>= 1) < minInterval) {
        interval = minInterval;
        dirInterval = 1;
      }
    }
  }
}

void clearLED() {
  for (unsigned int i = 0; i < numsZ;  i++) digitalWrite(pinsZ[i],  LOW);
  for (unsigned int i = 0; i < numsXY; i++) digitalWrite(pinsXY[i], HIGH);
}

void vSerialRead(void *pvParams) {
  while (1) {
    sCmd.readSerial();
    vTaskDelay(100);
  }
}
