#include <MapleFreeRTOS900.h>

uint16_t *globalVar = nullptr;

void *operator new(size_t size) { return pvPortMalloc(size); }
void operator delete(void *ptr) { vPortFree(ptr); }

class testClass {
  public:
    testClass() : count(0), test(0) {}
    ~testClass() {}
    uint16_t inc() { return ++count; }
  private:
    uint16_t count;
    uint8_t  test;
};

testClass *globalClass = nullptr;

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  xTaskCreate(vLoop1, "lp1", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
  xTaskCreate(vLoop2, "lp2", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
  xTaskCreate(vLoop3, "lp3", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
  xTaskCreate(vLoop4, "lp4", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
  vTaskStartScheduler();
}

void vLoop1(void *pvParams) {
  while (1) {
    if (!globalVar) {
      vTaskDelay(500);
      globalVar = (uint16_t *)pvPortMalloc(64);
      memset(globalVar, 0, 64);
      Serial.println("pvPortMalloc: globalVar");
    }
    else if(++(*globalVar) & 0x0400) {
      vPortFree(globalVar);
      globalVar = nullptr;
      Serial.println("vPortFree: globalVar");
    }
    vTaskDelay(1);
  }
}

void vLoop2(void *pvParams) {
  uint16_t *localVar = nullptr;
  while (1) {
    if (!localVar) {
      vTaskDelay(500);
      localVar = (uint16_t *)pvPortMalloc(8);
      memset(localVar, 0, 8);
      Serial.println("pvPortMalloc: localVar");
    }
    else if(++(*localVar) & 0x0800) {
      vPortFree(localVar);
      localVar = nullptr;
      Serial.println("vPortFree: localVar");
    }
    vTaskDelay(1);
  }
}

void vLoop3(void *pvParams) {
  while (1) {
    if (!globalClass) {
      vTaskDelay(500);
      globalClass = new testClass();
      Serial.println("new: globalClass");
    }
    else if(globalClass->inc() & 0x1000) {
      delete globalClass;
      globalClass = nullptr;
      Serial.println("delete: globalClass");
    }
    vTaskDelay(1);
  }
}

void vLoop4(void *pvParams) {
  testClass *localClass = nullptr;
  while (1) {
    if (!localClass) {
      vTaskDelay(500);
      localClass = new testClass();
      Serial.println("new: localClass");
    }
    else if(localClass->inc() & 0x2000) {
      delete localClass;
      localClass = nullptr;
      Serial.println("delete: localClass");
    }
    vTaskDelay(1);
  }
}

void loop() {}
