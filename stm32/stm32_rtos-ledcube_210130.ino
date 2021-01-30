#include <MapleFreeRTOS900.h>

#define BOARD_LED_PIN PC13

uint8 pinsZ[] = { PB14, PB13, PB12 };
uint8 pinsXY[] = { PA0, PA1, PA2, PA3, PA6, PA7, PA8, PA9, PA10 };

unsigned long animation[] = { 0x00400000, 0x00017400, 0x000001ff, 0x003de00, 0x05140000, 0, 0 };

const unsigned int numsZ = sizeof(pinsZ) / sizeof(pinsZ[0]);
const unsigned int numsXY = sizeof(pinsXY) / sizeof(pinsXY[0]);
const unsigned int numsSlide = sizeof(animation) / sizeof(animation[0]);

void setup() {
  pinMode(BOARD_LED_PIN, OUTPUT);

  xTaskCreate(vLEDtask, "led", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
  vTaskStartScheduler();
}

static void vLEDtask(void *pvParams) {
  int boardLed = HIGH;

  for (unsigned int i = 0; i < numsZ;  i++) pinMode(pinsZ[i],  OUTPUT);
  for (unsigned int i = 0; i < numsXY; i++) pinMode(pinsXY[i], OUTPUT);

  while (1) {
    for (unsigned int s = 0; s < numsSlide; s++) {
      for (unsigned int i = 0; i < 100; i++) {
        for (unsigned int z = 0; z < numsZ; z++) {
          clearLED();
          digitalWrite(pinsZ[z], HIGH);
          for (unsigned int xy = 0; xy < numsXY; xy++) digitalWrite(pinsXY[xy], (animation[s] >> (z * 9 + xy) & 0x01) ? LOW : HIGH);
          vTaskDelay(1);
        }
      }
    }
    digitalWrite(BOARD_LED_PIN, boardLed);
    boardLed = boardLed == HIGH ? LOW : HIGH;
  }
}

void clearLED() {
  for (unsigned int i = 0; i < numsZ;  i++) digitalWrite(pinsZ[i],  LOW);
  for (unsigned int i = 0; i < numsXY; i++) digitalWrite(pinsXY[i], HIGH);
}
