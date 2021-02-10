#include <MapleFreeRTOS900.h>
#include <SerialCommand.h>

#define ADC4RAND_PIN  PB0

const uint8_t pinsZ[]  = { PB14, PB13, PB12 }; // only GPIOB
const uint8_t pinsXY[] = { PA0, PA1, PA2, PA3, PA4, PA5, PA6, PA7, PA8 }; //only GPIOA

const uint32_t speedTimer = 10000;
const uint32_t minInterval = 3;
const uint32_t maxInterval = minInterval << 6;
const uint32_t cubeMaxValue = 100;
const int32_t  minMag =  1 << 7;
const int32_t  maxMag = 10 << 7;

const uint32_t numsZ  = sizeof(pinsZ)  / sizeof(pinsZ[0]);
const uint32_t numsXY = sizeof(pinsXY) / sizeof(pinsXY[0]);

uint32_t cubeValue[numsZ][numsXY] = { 0 };
int32_t  pos[3] = { 0, 0, 0 };
int32_t  dir[3] = { 6, 6, 6 };
int32_t  mag = maxMag;
int32_t  dirMag = (maxMag - minMag) / 20;
uint32_t interval = maxInterval;
int32_t  dirInterval = -1;
int32_t  enLED = 1;
int32_t  enAutoChgSpeed = 1;

uint32_t pinMaskZ[numsZ];
uint32_t pinMaskXY[numsXY];
uint32_t pinMaskAllZ = 0;
uint32_t pinMaskAllXY = 0;
SerialCommand sCmd;
int32_t boardLed = HIGH;

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(ADC4RAND_PIN, INPUT_ANALOG);
  randomSeed(analogRead(ADC4RAND_PIN));
  for (uint32_t i = 0; i < numsZ;  i++) { pinMode(pinsZ[i],  OUTPUT); pinMaskAllZ  |= pinMaskZ[i]  = digitalPinToBitMask(pinsZ[i]);  }
  for (uint32_t i = 0; i < numsXY; i++) { pinMode(pinsXY[i], OUTPUT); pinMaskAllXY |= pinMaskXY[i] = digitalPinToBitMask(pinsXY[i]); }

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
      for (uint32_t i = cubeMaxValue - 1; i > 0; i--) {
        for (uint32_t z = 0; z < numsZ; z++) {
          uint32_t pinMaskXYshow = 0;
          for (uint32_t xy = 0; xy < numsXY; xy++) if (cubeValue[z][xy] > i) pinMaskXYshow |= pinMaskXY[xy];
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
      for (uint32_t z = 0; z < numsZ; z++) {
        for (uint32_t xy = 0; xy < numsXY; xy++) {
          uint32_t dx = (xy / numsZ << 7) - pos[0];
          uint32_t dy = (xy % numsZ << 7) - pos[1];
          uint32_t dz =          (z << 7) - pos[2];
          uint32_t dist = ((dx * dx + dy * dy + dz * dz) * mag >> 14) * mag >> 14;
          cubeValue[z][xy] = cubeMaxValue / (dist ? dist : 1);
        }
      }
      for (uint32_t i = 0; i < 3; i++) {
        if ((pos[i] += dir[i]) > (int32_t)(numsZ - 1 << 7)) {
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
