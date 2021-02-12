// PA3 --[R]-+-|>|-- PA4
// PA2 ------’

uint32_t *DWT_CONTROL = (volatile uint32_t *) 0xe0001000;
uint32_t *DWT_CYCCNT  = (volatile uint32_t *) 0xe0001004;
uint32_t *DEMCR       = (volatile uint32_t *) 0xe000edfc;
uint32_t *DWT_LAR     = (volatile uint32_t *) 0xe0001fb0;

int32_t sensorCount = 0;
int32_t sensorCountThresh = 0xffffffff;
uint32_t loopCount = 0;
int32_t lastAnalogValue = 0;
int32_t analogBase = 0;

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);

  *DEMCR |= 0x01000000;
  *DWT_LAR = 0xc5acce55;
  *DWT_CYCCNT = 0;
  *DWT_CONTROL |= 1;

  GPIOA->regs->CRL = 0x44433044; // PA0-1,5-7:input-floating, PA3,4:output-push-pull, PA2:input-analog
  GPIOA->regs->BRR = 0x000000ff; // PA3,4:low
}

void loop() {
  GPIOA->regs->CRL = GPIOA->regs->CRL & 0xfff00fff | 0x00033000; // PA3,4:output-push-pull
  GPIOA->regs->BRR  = 0x00000008; // PA3:low
  GPIOA->regs->BSRR = 0x00000010; // PA4:high (led:reverse bias)
  delay(1);

  *DWT_CYCCNT = 0;
  GPIOA->regs->CRL = GPIOA->regs->CRL & 0xffff0fff | 0x00004000; // PA4:input-floating
  while (!(GPIOA->regs->IDR & 0x0008)) {} // while PA3 is low (led:discharge/measure)
  sensorCount += *DWT_CYCCNT;
  sensorCount = sensorCount * 29 / 30;

  if (sensorCount > sensorCountThresh) {
      GPIOA->regs->CRL = GPIOA->regs->CRL & 0xfff00fff | 0x00033000; // PA3,4:output-push-pull
      GPIOA->regs->BRR  = 0x00000010; // PA4:low
      GPIOA->regs->BSRR = 0x00000008; // PA3:high (led:light)
      delay(3);
      lastAnalogValue = analogRead(PA2);
    }

  if (++loopCount == 50) {
    if (sensorCountThresh == 0xffffffff) sensorCountThresh = sensorCount * 8 / 10;
    if (analogBase == 0 && lastAnalogValue != 0) analogBase = lastAnalogValue;
    Serial.print("light:");
    Serial.print(sensorCountThresh - sensorCount);
    Serial.print(", temp:");
    Serial.println(analogBase - lastAnalogValue);
    loopCount = 0;
  }
}
