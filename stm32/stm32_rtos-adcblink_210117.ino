#include <MapleFreeRTOS900.h>
#include <STM32ADC.h>

#define BOARD_LED_PIN PC13

uint8 pins[] = { PA0, PA1, PA2, PA3, PA4, PA5, PA6, PA7 };
const int numPins = sizeof(pins) / sizeof(pins[0]);
uint16_t adcVals[numPins];
int dp[] = { adcVals, numPins };
STM32ADC adc(ADC1);
int ledDelay = 500;

void setup() {
  Serial.begin(115200);
  pinMode(BOARD_LED_PIN, OUTPUT);

  adc.calibrate();
  for (unsigned int j = 0; j < numPins; j++) pinMode(pins[j], INPUT_ANALOG);
  adc.setSampleRate(ADC_SMPR_71_5);  // 71.5 ADC cycles
  adc.setScanMode();
  adc.setPins(pins, numPins);
  adc.setContinuous();
  adc.setDMA(adcVals, numPins, (DMA_MINC_MODE | DMA_CIRC_MODE), NULL);
  adc.startConversion();

  xTaskCreate(vLEDtask,      "led",  configMINIMAL_STACK_SIZE,      &ledDelay, tskIDLE_PRIORITY,     NULL);
  xTaskCreate(vADCtask,      "adc",  configMINIMAL_STACK_SIZE + 40, dp,        tskIDLE_PRIORITY + 1, NULL);
  xTaskCreate(vLEDdelaytask, "ldly", configMINIMAL_STACK_SIZE,      &ledDelay, tskIDLE_PRIORITY,     NULL);
  vTaskStartScheduler();
}

static void vLEDdelaytask(void *pvParams) {
  while (1) {
    vTaskDelay(3000);
    if((*(int *)pvParams -= 100) <= 0) *(int *)pvParams = 500;
  }
}

static void vADCtask(void *pvParams) {
  while (1) {
    vTaskDelay(1000);
    for(unsigned int i = 0; i < ((int *)pvParams)[1]; i++) {
      char str[12];
      sprintf(str, "%d %4d, ", i, ((uint16_t *)(((int *)pvParams)[0]))[i]);
      Serial.print(str);
    }
    Serial.println();
  }
}

static void vLEDtask(void *pvParams) {
  while (1) {
    vTaskDelay(*(int *)pvParams);
    digitalWrite(BOARD_LED_PIN, HIGH);
    vTaskDelay(*(int *)pvParams);
    digitalWrite(BOARD_LED_PIN, LOW);
  }
}
