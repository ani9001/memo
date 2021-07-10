// 72MHz, 3v3
// STM32-SHIFT1_PIN --- SCK -+- 3k3 -- SI  -- 100pf(101) -+- GND
//                   74HC595 `- 56k -- RCK -- 100pf(101) -'

#include <MapleFreeRTOS900.h>

#define SHIFT1_PIN   PA0
#define NUM_LED      7
#define LED_DELAY1   15
#define LED_DELAY0   5
#define MODE_DELAY_B 10
#define MODE_DELAY_R 12
#define ADC4RAND_PIN PB0

#define SHIFT1_MASK  digitalPinToBitMask(SHIFT1_PIN)
#define NOP          asm volatile ("nop");                   // 13.9 ns @ 72MHz
#define NOP10        NOP NOP NOP NOP NOP NOP NOP NOP NOP NOP // 139 ns @ 72MHz

uint8_t levelNow[NUM_LED], levelMax[NUM_LED], dir[NUM_LED];
uint8_t mode = 0;
uint8_t LedPos[3] = { 0, 3, 6 };
uint8_t fadeCount = 0;

void setup() {
  pinMode(SHIFT1_PIN, OUTPUT);
  pinMode(ADC4RAND_PIN, INPUT_ANALOG);
  randomSeed(analogRead(ADC4RAND_PIN));
  GPIOA->regs->BSRR = SHIFT1_MASK;
  delay(1);
  xTaskCreate(vShowLed,        "led", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
  xTaskCreate(vChangeLedLevel, "chg", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
  xTaskCreate(vChangeLedMode,  "mod", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
  vTaskStartScheduler();
}

void vShowLed(void *pvParams) {
  while (1) {
    for (uint8_t lv = 0; lv < 160; lv = lv < 32 ? lv + 1 : lv + 2) {
      uint8_t data = 0;
      for (uint8_t i = 0; i < NUM_LED; i++) if (levelNow[i] > lv) data |= 1 << i;
      shift1_senddata(data);
    }
    vTaskDelay(1);
  }
}

void vChangeLedLevel(void *pvParams) {
  while (1) {
    if (mode) {
      for (uint8_t i = 0; i < NUM_LED; i++) {
        if (!levelNow[i] && !dir[i]) {
          levelMax[i] = 64 + random(96);
          dir[i] = 1;
        } else if (!dir[i]) levelNow[i]--;
        else if (++levelNow[i] == levelMax[i]) dir[i] = 0;
      }
    } else {
      for (uint8_t i = 0; i < 3; i++) {
        if (                 LedPos[i] < NUM_LED    ) levelNow[LedPos[i]    ] =       fadeCount;
        if (LedPos[i] > 0 && LedPos[i] < NUM_LED + 1) levelNow[LedPos[i] - 1] = 160 - fadeCount;
      }
      if (++fadeCount > 160) {
        for (uint8_t i = 0; i < 3; i++) if (++LedPos[i] > 8) LedPos[i] = 0;
        fadeCount = 0;
      }
    }
    vTaskDelay(mode ? LED_DELAY1 : LED_DELAY0);
  }
}

void vChangeLedMode(void *pvParams) {
  while (1) {
    mode = mode ? 0 : 1;
    for (uint8_t i = 0; i < NUM_LED; i++) levelNow[i] = levelMax[i] = dir[i] = 0;
    vTaskDelay((MODE_DELAY_B + random(MODE_DELAY_R)) << 10);
  }
}

void shift1_senddata(uint8_t data) {
  for (uint8_t i = 0; i < 7; i++) shift1_send((data <<= 1) & 0x80, false);
  shift1_send(false, true);
}

void shift1_send(bool dat1) { shift1_send(dat1, false); }
void shift1_send(bool dat1, bool latch) {
  GPIOA->regs->BRR = SHIFT1_MASK;
  if (dat1) {} else if (latch) delayMicroseconds(10); else { NOP10 }
  GPIOA->regs->BSRR = SHIFT1_MASK;
  delayMicroseconds(latch ? 20 : 1);
}
// dat1: GPIOA->regs->BRR = SHIFT1_MASK; NOP NOP NOP NOP GPIOA->regs->BSRR = SHIFT1_MASK;
// dat0: GPIOA->regs->BRR = SHIFT1_MASK; NOP10 NOP10 NOP10 GPIOA->regs->BSRR = SHIFT1_MASK;

void loop() {}
