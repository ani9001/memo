#include <MapleFreeRTOS900.h>
#include <SerialCommand.h>

#define ADC4RAND_PIN  PB0

const uint8 pinsZ[]  = { PB14, PB13, PB12 }; // only GPIOB
const uint8 pinsXY[] = { PA0, PA1, PA2, PA3, PA4, PA5, PA6, PA7, PA8 }; //only GPIOA

const unsigned int speedTimer = 10000;
const unsigned int minInterval = 3;
const unsigned int maxInterval = minInterval << 6;
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

uint32 pinMaskZ[numsZ];
uint32 pinMaskXY[numsXY];
uint32 pinMaskAllZ = 0;
uint32 pinMaskAllXY = 0;
SerialCommand sCmd;
int boardLed = HIGH;

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(ADC4RAND_PIN, INPUT_ANALOG);
  randomSeed(analogRead(ADC4RAND_PIN));
  for (unsigned int i = 0; i < numsZ;  i++) { pinMode(pinsZ[i],  OUTPUT); pinMaskAllZ  |= pinMaskZ[i]  = digitalPinToBitMask(pinsZ[i]);  }
  for (unsigned int i = 0; i < numsXY; i++) { pinMode(pinsXY[i], OUTPUT); pinMaskAllXY |= pinMaskXY[i] = digitalPinToBitMask(pinsXY[i]); }

  sCmd.addCommand("1", []() { enLED = 1; Serial.println("on"); });
  sCmd.addCommand("0", []() { enLED = 0; Serial.println("off"); });
  sCmd.addCommand("+", []() { enAutoChgSpeed = 0; interval = (interval >>= 1) < minInterval ? minInterval : interval; Serial.println(interval); });
  sCmd.addCommand("-", []() { enAutoChgSpeed = 0; interval = (interval <<= 1) > maxInterval ? maxInterval : interval; Serial.println(interval); });
  sCmd.addCommand("*", []() { enAutoChgSpeed = 1; Serial.println(interval); });
  sCmd.setDefaultHandler([](const char *c) { Serial.println("cmd: <1|0|+|-|*>"); });

  xTaskCreate(vLEDshow,      "led", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL);
  xTaskCreate(vLEDchangePos, "pos", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL);
  xTaskCreate(vLEDchangeSpd, "spd", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY,     NULL);
  xTaskCreate(vSerialRead,   "ser", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY,     NULL);
  vTaskStartScheduler();
}

void vLEDshow(void *pvParams) {
  while (1) {
    if (enLED) {
      for (unsigned int i = cubeMaxValue - 1; i > 0; i--) {
        for (unsigned int z = 0; z < numsZ; z++) {
          uint32 pinMaskXYshow = 0;
          for (unsigned int xy = 0; xy < numsXY; xy++) if (cubeValue[z][xy] > i) pinMaskXYshow |= pinMaskXY[xy];
          GPIOB->regs->BRR  = pinMaskAllZ;
          GPIOA->regs->BSRR = pinMaskAllXY;
          GPIOB->regs->BSRR = pinMaskZ[z];
          GPIOA->regs->BRR  = pinMaskXYshow;
        }
      }
      GPIOB->regs->BRR  = pinMaskAllZ;
      GPIOA->regs->BSRR = pinMaskAllXY;
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
    digitalWrite(LED_BUILTIN, boardLed = boardLed == HIGH ? LOW : HIGH);
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

void vSerialRead(void *pvParams) {
  while (1) {
    sCmd.readSerial();
    vTaskDelay(100);
  }
}

void loop() {}
