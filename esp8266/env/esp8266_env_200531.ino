// 160 MHz, QIO, 4M (2M SPIFFS)
// Json Assistant: https://arduinojson.org/v5/assistant/
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BME280_MOD-1022.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <time.h>
#include "TM1637.h"
#define NO_GLOBAL_MDNS
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#define NO_GLOBAL_ARDUINOOTA
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include "lwip/ip4_addr.h"
#include "netif/etharp.h"
#include <../include/time.h>
extern "C" {
#include <cont.h>
}
#define PROGMEM_T __attribute__((section(".irom.text.t")))
typedef enum : byte { myWriteAPIKey, ssd1306Contents, tm1637Bright, messageSchedule, strMsgSchedule, arpOct4, awsApiGwHost, longitude, latitude, yolpClientId, msg4CtrlHost, msg4CtrlPath,
  crc16info } configFlashParam;

#define PIN_I2C_SDA     0
#define PIN_I2C_SCL     2
#define PIN_1WIRE       4
#define PIN_TM1637_CLK  3
#define PIN_TM1637_DIO  1
#define PIN_MSG_LED    13
#define PIN_MSG2_LED   12
#define PIN_MSG3_LED   14
#define PIN_MSG4_LED   15

#define WIFI_SSID                         ""
#define WIFI_PASS                         ""
#define MAX_SSD1306CONTENT                12
#define MAX_MSG_SCHEDULE                  10
#define MAX_RAINFALL_DATA                 25
#define MAX_API_HOSTNAME_LEN              64
#define MAX_LONGITUDE_LATITUDE_LEN         8
#define MAX_YOLP_CLIENT_ID_LEN            64
#define MAX_MSG4CTRL_HOST_LEN             48
#define MAX_MSG4CTRL_PATH_LEN             16
#define ADD_BUF_SIZE_ROOTHTML            200
#define MY_WDT_TIMEOUT                   115
#define FAILSAFECHECK_COUNT                3
#define FALESAFE_RESTART_SEC             300
#define REBOOT_HOUR                        3
#define REBOOT_MIN                         0
#define CONFIG_PREFIX                     ""
#define CONFIG_SUFFIX                 "$env"
#define THINGSPEAK_HOST "api.thingspeak.com"
#if (REBOOT_MIN > 57)
#undef  REBOOT_MIN
#define REBOOT_MIN 57
#endif

volatile unsigned long myWDTsec = millis() >> 10;
float fTemperature, fHumidity, fPressure, fTemperatureO;
float *rainFall = nullptr;
const uint32_t configFlashStartAddr = (ESP.getSketchSize() + FLASH_SECTOR_SIZE - 1) & (~(FLASH_SECTOR_SIZE - 1));
char           *thingSpeakFieldData = (char *)malloc(160); //       20      * 8
char           *postMessage         = (char *)malloc(232); // 16 + (19 + 8) * 8
time_t t;
struct tm *tm;
int8_t  lastminute            = -1;
int8_t  failSafeCheck         = FAILSAFECHECK_COUNT;
uint8_t irregularResetCheck   =  0;
int16_t thingspeakLastResult  = -1;
uint8_t msgOverrideRemainTime =  0;
uint8_t serverClosing         =  0;
uint8_t enableRainFeature     =  0;
uint8_t noRain                =  1;
int8_t  rainfallLastResult    = -1;
uint8_t ntpFailedAfterMin     =  0;
int8_t  ntpFailedRtcOffset    =  0;
WiFiClientSecure secClient;
ESP8266WebServer *server       = nullptr;
File             *fsUploadFile = nullptr;
ArduinoOTAClass  *OTA          = nullptr;
Adafruit_SSD1306  display(128, 64, &Wire, -1);
OneWire           oneWire(PIN_1WIRE);
DallasTemperature ds18b20(&oneWire);
TM1637            tm1637(PIN_TM1637_CLK, PIN_TM1637_DIO);
ADC_MODE(ADC_VCC);

int8_t timeDisp[4];
char cTemperature[5], cHumidity[5], cTemperatureO[5];
char msgDisp[6];

extern "C" int startWaveform(uint8_t, uint32_t, uint32_t, uint32_t);
extern "C" int stopWaveform(uint8_t);
extern "C" void sntp_init();
extern "C" void sntp_stop();
extern "C" void espconn_dns_setserver(uint8, ip_addr_t *);

void setup() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  display.begin(SSD1306_SWITCHCAPVCC, 0x78 >> 1);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);

  display.print(F(WIFI_SSID));
  display.display();
  WiFi.mode(WIFI_STA);
  WiFi.begin(String(F(WIFI_SSID)).c_str(), String(F(WIFI_PASS)).c_str());
  uint8_t iWiFiwait = 0;
  while (WiFi.status() != WL_CONNECTED) {
    display.print(F(".")); display.display();
    if (++iWiFiwait > 20) ESP.restart();
    delay(1000);
  }
  display.println();
  display.println(WiFi.localIP().toString());
  display.println();
  display.display();

  BME280.readCompensationParams();
  BME280.writeStandbyTime(tsb_1000ms);
  BME280.writeFilterCoefficient(fc_16);
  BME280.writeOversamplingPressure(os16x);
  BME280.writeOversamplingTemperature(os2x);
  BME280.writeOversamplingHumidity(os1x);
  BME280.writeMode(smNormal);
  display.printf_P(PSTR("bme280 %02x"), BME280.readChipId());
  display.display();

  ds18b20.setResolution(12); // 9..12[.5C-.0625C]
  display.println(F("  ds18b20")); display.println(); display.display();

  EEPROM.begin(4); // 4..4096
  if (!thingSpeakFieldData || !postMessage || (EEPROM.read(0) == 0x90 && EEPROM.read(1) == 0x01 && EEPROM.read(2) == 0xfa)) failSafeOTA();
  if (EEPROM.read(0) != 0x90 || EEPROM.read(1) != 0x01 || (EEPROM.read(2) != 0x00 && EEPROM.read(2) != 0xfb)) EEPROM.write(3, 0x03);
  if (EEPROM.read(2) == 0xfb) failSafeCheck = (FAILSAFECHECK_COUNT ? FAILSAFECHECK_COUNT - 1 : 0);
  EEPROM.write(0, 0x90); EEPROM.write(1, 0x01); EEPROM.write(2, 0xfa); // magic: 0x9001, 0xfa: failsafe-check
  EEPROM.commit();

  loadConfig(false);
  display.printf_P(PSTR("%04x %04x  a%d\n"),
    pgm_read_word((const void*)configFlashParamAddr(crc16info)), pgm_read_word((const void*)configFlashParamAddr(crc16info) + 2), pgm_read_byte((const void*)configFlashParamAddr(arpOct4)));
  display.display();

  pinMode(PIN_MSG_LED,  OUTPUT);
  pinMode(PIN_MSG2_LED, OUTPUT);
  pinMode(PIN_MSG3_LED, OUTPUT);
  pinMode(PIN_MSG4_LED, OUTPUT);
  digitalWrite(PIN_MSG_LED,  LOW);
  digitalWrite(PIN_MSG2_LED, LOW);
  digitalWrite(PIN_MSG3_LED, LOW);
  digitalWrite(PIN_MSG4_LED, LOW);

  configTime(3600 * 9, 0, String(F("ntp.nict.jp")).c_str(), String(F("pool.ntp.org")).c_str(), String(F("jp.pool.ntp.org")).c_str());
  uint16_t timeout = 0x1000; // * 10 msec
  do {
    t = time(NULL);
    tm = localtime(&t);
    if (tm->tm_year > 116 || !(timeout & 0xfff0)) break;
    if (!(timeout & 0x03ff)) {
      ip_addr_t dnsserver;
      IP4_ADDR(&dnsserver, 8, 8, 8, 8);
      espconn_dns_setserver(0, &dnsserver);
      IP4_ADDR(&dnsserver, 1, 1, 1, 1);
      espconn_dns_setserver(1, &dnsserver);
      sntp_stop();
      sntp_init();
    }
    delay(10);
    timeout--;
  } while (timeout & 0xffe0);
  if (tm->tm_year < 117) {
    ntpFailedAfterMin = 1;
    setenv(String(F("TZ")).c_str(), String(F("JST-9")).c_str(), 1);
    tzset();
    t = getRTCtime() + 51;
    timeval tv;
    tv.tv_sec = t;
    settimeofday(&tv, NULL);
    t = time(NULL);
    ntpFailedRtcOffset = t > getRTCtime() ? -(int8_t)((t - getRTCtime() + 1800) / 3600) : (int8_t)((getRTCtime() - t + 1800) / 3600);
    tm = localtime(&t);
    if (tm->tm_year > 116) display.print(F("rtc."));
    else {
      tv.tv_sec = 0;
      settimeofday(&tv, NULL);
      display.print(F("ntpFail."));
    }
  } else display.print(F("time."));
  display.display();

  noInterrupts();
  timer0_isr_init();
  timer0_attachInterrupt(timer0_ISR);
  timer0_write(ESP.getCycleCount() + (uint32_t)ESP.getCpuFreqMHz() * ((uint32_t)15 << 20)); // 15s
  interrupts();

  memset(msgDisp, 0, sizeof(msgDisp));
  if (!(EEPROM.read(3) & 0x01)) { display.clearDisplay(); display.display(); }
  if (!(EEPROM.read(3) & 0x02)) { tm1637.set(BRIGHT_DARKEST); tm1637.point(POINT_OFF); tm1637.clearDisplay(); }
}

void loop() {
  do {
    delay(200);
    setRTCtime();
    tm = localtime(&t);
    if (OTA) OTA->handle();
    if (!server && etharpFindAddr(pgm_read_byte((const void*)configFlashParamAddr(arpOct4))) > -1) startWebServer();
    if (server) server->handleClient();
    if (server && serverClosing) {
      server->stop();
      delete server;
      server = nullptr;
      if (etharpFindAddr(pgm_read_byte((const void*)configFlashParamAddr(arpOct4))) > -1) etharp_cleanup_netif(netif_default);
      serverClosing = 0;
    }
    if ((EEPROM.read(3) & 0x02) && tm->tm_sec >= 30 && tm1637._PointFlag == POINT_OFF) dispTime();
    setRTCtime();
    myWDTsec = millis() >> 10;
  } while (tm->tm_min == lastminute || tm->tm_sec > 9);
  if (EEPROM.read(3) & 0x02) dispTime();

  if (!msgOverrideRemainTime) {
    for (uint8_t i = 0; i < MAX_MSG_SCHEDULE; i++) {
      if (pgm_read_byte((const void*)configFlashParamAddr(messageSchedule) + (i << 2) + 3) != 0xff) {
        if (pgm_read_byte((const void*)configFlashParamAddr(messageSchedule) + (i << 2)) == tm->tm_hour && pgm_read_byte((const void*)configFlashParamAddr(messageSchedule) + (i << 2) + 1) == tm->tm_min) {
          analogWrite3(PIN_MSG_LED, pgm_read_byte((const void*)configFlashParamAddr(messageSchedule) + (i << 2) + 3),
            (pgm_read_byte((const void*)configFlashParamAddr(messageSchedule) + (i << 2) + 2) > 0xf0) ? 128.0 : (float)pgm_read_byte((const void*)configFlashParamAddr(messageSchedule) + (i << 2) + 2) / 8.0);
        }
      }
    }
  } else msgOverrideRemainTime--;

  ds18b20.requestTemperatures();
  fTemperatureO = ds18b20.getTempCByIndex(0);

  uint8_t iIsMeasuringwait = 0;
  while (iIsMeasuringwait++ < 20 && BME280.isMeasuring()) { delay(25); }
  BME280.readMeasurements();
  iIsMeasuringwait = 0;
  while (iIsMeasuringwait++ < 20 && BME280.isMeasuring()) { delay(25); }
  fTemperature = BME280.getTemperature();
  fHumidity    = BME280.getHumidity();
  fPressure    = BME280.getPressure();
  dtostrf(fTemperature,  sizeof(cTemperature) - 1,  1, cTemperature);
  dtostrf(fHumidity,     sizeof(cHumidity) - 1,     1, cHumidity);
  dtostrf(fTemperatureO, sizeof(cTemperatureO) - 1, 1, cTemperatureO);

  memset(thingSpeakFieldData, 0, 160); // 20 * 8
  if (-10.0 <= fTemperature  && fTemperature  <=   50.0) thingSpeakSetFieldFloat(1, fTemperature);
  if (  0.0 <= fHumidity     && fHumidity     <=  100.0) thingSpeakSetFieldFloat(2, fHumidity);
  if (940.0 <= fPressure     && fPressure     <= 1040.0) thingSpeakSetFieldFloat(3, fPressure);
  if (enableRainFeature && rainFall && !rainfallLastResult && tm->tm_min % 5 == 1 && 0.0 <= rainFall[MAX_RAINFALL_DATA >> 1] && rainFall[MAX_RAINFALL_DATA >> 1] <= 400.0)
                                                         thingSpeakSetFieldFloat(4, rainFall[MAX_RAINFALL_DATA >> 1]);
  if (-15.0 <= fTemperatureO && fTemperatureO <=   50.0) thingSpeakSetFieldFloat(6, fTemperatureO);
                                                         thingSpeakSetFieldInt  (8, WiFi.RSSI());
  thingspeakLastResult = thingSpeakWriteFields();
  setRTCtime();

  if (enableRainFeature && rainFall && (!(tm->tm_min % 5) || rainfallLastResult) && !(tm->tm_hour == REBOOT_HOUR && tm->tm_min == REBOOT_MIN && EEPROM.read(2) != 0xfa)) {
    if (!(rainfallLastResult = yolpGetRainfall())) {
      if (noRain) analogWrite3(PIN_MSG2_LED, 0, 1.0f);
      else {
        float rainIndex = 0.0f;
        for (uint8_t i = 0; i < MAX_RAINFALL_DATA; i++) { rainIndex += rainFall[i]; }
        rainIndex = sqrt(rainIndex / MAX_RAINFALL_DATA) * 16.0f;
        if      (rainIndex <   4.0f) rainIndex =   4.0f;
        else if (rainIndex > 128.0f) rainIndex = 128.0f;
        analogWrite3(PIN_MSG2_LED, (uint8_t)rainIndex, 64.0f);
      }
    }
    setRTCtime();
  }

  if ((tm->tm_hour == REBOOT_HOUR && tm->tm_min == REBOOT_MIN && EEPROM.read(2) != 0xfa) || (tm->tm_year < 117 && (tm->tm_min >> 3) && !failSafeCheck)) {
    delay(10 << 10);
    setRTCtime();
    ESP.restart();
  }
  lastminute = tm->tm_min;
  if (irregularResetCheck) irregularResetCheck--;
  if (failSafeCheck) {
    if (failSafeCheck == FAILSAFECHECK_COUNT && !(tm->tm_hour == REBOOT_HOUR && tm->tm_min <= REBOOT_MIN + 2)) irregularResetCheck = 30;
    if (!(--failSafeCheck)) {
      EEPROM.write(2, 0x00); // 0x00: failsafe-check
      EEPROM.commit();
    }
  }
  if (ntpFailedAfterMin) {
    if (ntpFailedAfterMin++ > 5) ESP.restart();
  }
  if (EEPROM.read(3) & 0x01) showOLED();
  if (pgm_read_byte((const void*)configFlashParamAddr(msg4CtrlHost)) && tm->tm_year > 117 && tm->tm_min % 5 == 2) msg4Ctrl();
  if (tm->tm_year < 117) { sntp_stop(); sntp_init(); }
}

void dispTime() {
  char tm1637BrightChar = (char)pgm_read_byte((const void*)configFlashParamAddr(tm1637Bright) + tm->tm_hour) - '0';
  if      (tm->tm_year < 117) tm1637.set(BRIGHT_DARKEST);
  else if (((tm->tm_min >= 30) ? tm1637BrightChar % 3 : tm1637BrightChar / 3) == 2) tm1637.set(BRIGHTEST);
  else if (((tm->tm_min >= 30) ? tm1637BrightChar % 3 : tm1637BrightChar / 3) == 1) tm1637.set(BRIGHT_TYPICAL);
  else tm1637.set(BRIGHT_DARKEST);
  timeDisp[0] = ntpFailedAfterMin ? 0x0e : (tm->tm_hour > 9 && tm->tm_year > 116) ? tm->tm_hour / 10 : 0x7f;
  timeDisp[1] = (tm->tm_year > 116) ? tm->tm_hour % 10 : 0x7f;
  timeDisp[2] = (tm->tm_year > 116) ? tm->tm_min  / 10 : 0x7f;
  timeDisp[3] =                       tm->tm_min  % 10;
  tm1637.point(tm->tm_sec < 30 ? POINT_OFF : POINT_ON);
  tm1637.display(timeDisp);
}

void eraseEEPROM() {
  if ((EEPROM.read(0) & EEPROM.read(1) & EEPROM.read(2) & EEPROM.read(3)) == 0xff) return;
  EEPROM.write(0, 0xff); EEPROM.write(1, 0xff); EEPROM.write(2, 0xff); EEPROM.write(3, 0xff);
  EEPROM.commit();
}

void showOLED() {
  display.clearDisplay();
  char tm1637BrightChar = (char)pgm_read_byte((const void*)configFlashParamAddr(tm1637Bright) + tm->tm_hour) - '0';
  if ((tm->tm_min >= 30) ? tm1637BrightChar % 3 : tm1637BrightChar / 3) display.dim(false);
  else display.dim(true);
  display.setTextColor(WHITE);
  static char wday[] PROGMEM_T = "SMTWHFA";
  static char p2d[]  PROGMEM_T = "%2d";
  char ssd1306ContentChar[2];
  uint8_t suppressIfNoRain = 0;
  for (uint8_t i = 0; i < MAX_SSD1306CONTENT; i++) {
    ssd1306ContentChar[0] = (char)pgm_read_byte((const void*)configFlashParamAddr(ssd1306Contents) + (i << 3) + 3);
    if (*ssd1306ContentChar) {
      display.setCursor(pgm_read_byte((const void*)configFlashParamAddr(ssd1306Contents) + (i << 3)), pgm_read_byte((const void*)configFlashParamAddr(ssd1306Contents) + (i << 3) + 1));
      display.setTextSize(pgm_read_byte((const void*)configFlashParamAddr(ssd1306Contents) + (i << 3) + 2));
      ssd1306ContentChar[1] = (char)pgm_read_byte((const void*)configFlashParamAddr(ssd1306Contents) + (i << 3) + 4);
      if (!ssd1306ContentChar[1]) {
        if        (*ssd1306ContentChar == 'm') { display.printf_P((PGM_P)p2d, tm->tm_mon + 1);
        } else if (*ssd1306ContentChar == 'd') { display.printf_P((PGM_P)p2d, tm->tm_mday);
        } else if (*ssd1306ContentChar == 'H') { display.print(cHumidity[0]); display.print(cHumidity[1]);
        } else if (*ssd1306ContentChar == 'w' || (*ssd1306ContentChar == 'k' && !noRain)) { display.print((char)pgm_read_byte(wday + tm->tm_wday));
        } else if (*ssd1306ContentChar == 'W' || (*ssd1306ContentChar == 'k' &&  noRain)) {
          display.setCursor(pgm_read_byte((const void*)configFlashParamAddr(ssd1306Contents) + (i << 3)) + tm->tm_wday * 6, pgm_read_byte((const void*)configFlashParamAddr(ssd1306Contents) + (i << 3) + 1));
          display.print((char)pgm_read_byte(wday + tm->tm_wday));
          if (*ssd1306ContentChar == 'k') suppressIfNoRain = 1;
        } else if (*ssd1306ContentChar == 's') {
          if (thingspeakLastResult != 200) display.print(F("t"));
          if (EEPROM.read(2) == 0xfa) {
            display.setCursor(pgm_read_byte((const void*)configFlashParamAddr(ssd1306Contents) + (i << 3)) + 5, pgm_read_byte((const void*)configFlashParamAddr(ssd1306Contents) + (i << 3) + 1));
            display.print(F("f"));
          }
          if (server) {
            display.setCursor(pgm_read_byte((const void*)configFlashParamAddr(ssd1306Contents) + (i << 3))    , pgm_read_byte((const void*)configFlashParamAddr(ssd1306Contents) + (i << 3) + 1));
            if (OTA) display.print(F("o"));
            else     display.print(F("w"));
          }
          if (irregularResetCheck) {
            display.setCursor(pgm_read_byte((const void*)configFlashParamAddr(ssd1306Contents) + (i << 3)) + 5, pgm_read_byte((const void*)configFlashParamAddr(ssd1306Contents) + (i << 3) + 1) + 7);
            display.print(F("e"));
          }
          if (enableRainFeature) {
            display.setCursor(pgm_read_byte((const void*)configFlashParamAddr(ssd1306Contents) + (i << 3))    , pgm_read_byte((const void*)configFlashParamAddr(ssd1306Contents) + (i << 3) + 1) + 7);
            if (!rainFall)               display.print(F("R"));
            else if (rainfallLastResult) display.print(F("r"));
          }
        } else if (*ssd1306ContentChar == 'g') {
          if (pgm_read_byte((const void*)configFlashParamAddr(ssd1306Contents) + (i << 3) + 2) == 1) {
            display.print(msgDisp);
            display.setCursor(pgm_read_byte((const void*)configFlashParamAddr(ssd1306Contents) + (i << 3)), pgm_read_byte((const void*)configFlashParamAddr(ssd1306Contents) + (i << 3) + 1) + 8);
            display.print(&msgDisp[3]);
          } else display.print(*msgDisp);
        } else if (*ssd1306ContentChar == 'r') {
          if (noRain && suppressIfNoRain) continue;
          for (uint8_t j = 0; j < MAX_RAINFALL_DATA; j++) {
            uint8_t height = (uint8_t)(rainFall[j] * (float)pgm_read_byte((const void*)configFlashParamAddr(ssd1306Contents) + (i << 3) + 2)) + 1;
            if (rainFall[j] == 0.0f) height = 0;
            else if (height >= (pgm_read_byte((const void*)configFlashParamAddr(ssd1306Contents) + (i << 3) + 2) << 3)) height = (pgm_read_byte((const void*)configFlashParamAddr(ssd1306Contents) + (i << 3) + 2) << 3) - 1;
            for (uint8_t k = 0; k < pgm_read_byte((const void*)configFlashParamAddr(ssd1306Contents) + (i << 3) + 2); k++) {
              display.drawFastVLine(pgm_read_byte((const void*)configFlashParamAddr(ssd1306Contents) + (i << 3)) + pgm_read_byte((const void*)configFlashParamAddr(ssd1306Contents) + (i << 3) + 2) * j + k,
                pgm_read_byte((const void*)configFlashParamAddr(ssd1306Contents) + (i << 3) + 1) + (pgm_read_byte((const void*)configFlashParamAddr(ssd1306Contents) + (i << 3) + 2) << 3) - 1 - height, height, INVERSE);
            }
          }
          display.drawFastVLine(pgm_read_byte((const void*)configFlashParamAddr(ssd1306Contents) + (i << 3))
            + pgm_read_byte((const void*)configFlashParamAddr(ssd1306Contents) + (i << 3) + 2) * (MAX_RAINFALL_DATA >> 1)
            + pgm_read_byte((const void*)configFlashParamAddr(ssd1306Contents) + (i << 3) + 2) - 1,
            pgm_read_byte((const void*)configFlashParamAddr(ssd1306Contents) + (i << 3) + 1), (pgm_read_byte((const void*)configFlashParamAddr(ssd1306Contents) + (i << 3) + 2) << 3) - 1, INVERSE);
        } else display.print(*ssd1306ContentChar);
      } else if (*ssd1306ContentChar == 'T' && !pgm_read_byte((const void*)configFlashParamAddr(ssd1306Contents) + (i << 3) + 5)) {
        if        (ssd1306ContentChar[1] == 'O') { display.print(cTemperatureO[0]); display.print(cTemperatureO[1]);
        } else if (ssd1306ContentChar[1] == 'o') { display.print(cTemperatureO[3]);
        } else if (ssd1306ContentChar[1] == 'I') { display.print(cTemperature[0]);  display.print(cTemperature[1]);
        } else if (ssd1306ContentChar[1] == 'i') { display.print(cTemperature[3]);
        } else display.print(String(FPSTR(configFlashParamAddr(ssd1306Contents) + (i << 3) + 3)));
      } else display.print(String(FPSTR(configFlashParamAddr(ssd1306Contents) + (i << 3) + 3)));
    }
  }
  display.display();
}

void handleFileList() {
  static char htmlHeader[] PROGMEM_T = R"(<!DOCTYPE html><html><head><title>file manager</title><style>
body{background-color:#f39800;font-family:Arial,Helvetica,Sans-Serif;Color:#046;}
</style></head><body>
<h1>file manager</h1>
)";
  static char htmlFooter[] PROGMEM_T = R"(<form method='post' enctype='multipart/form-data'>
<input type='file' name='name'><input class='button' type='submit' value='upload'>
</form></body></html>)";
  String fileListHTML = String();
  FSInfo fs_info;
  String delFile = server->arg(String(F("del")));
  SPIFFS.begin();
  if(delFile.length()) SPIFFS.remove(delFile);
  Dir dir = SPIFFS.openDir(String(F("/")));
  while (dir.next()) {
    fileListHTML += String(F("<p><b><a href='")) + dir.fileName() + String(F("'>")) + dir.fileName() + String(F("</a></b>, "));
    File f = dir.openFile(String(F("r")).c_str());
    if (f) {
      fileListHTML += String(f.size()) + String(F(", "));
      f.close();
    }
    fileListHTML += String(F("<a href='/upload?del=")) + dir.fileName() + String(F("'>[del]</a></p>\n"));
  }
  SPIFFS.info(fs_info);
  fileListHTML += String(F("<hr><p>used / total = ")) + String(fs_info.usedBytes) + String(F(" / ")) + fs_info.totalBytes + String(F("</p>\n"));
  SPIFFS.end();
  server->setContentLength(strlen_P((PGM_P)htmlHeader) + strlen_P((PGM_P)htmlFooter) + fileListHTML.length());
  server->send_P(200, PSTR("text/html"), PSTR(""));
  server->sendContent_P((PGM_P)htmlHeader);
  server->sendContent_P(fileListHTML.c_str());
  server->sendContent_P((PGM_P)htmlFooter);
}

void handleFileUpload() {
  HTTPUpload& upload = server->upload();
  if (upload.status == UPLOAD_FILE_START) {
    static char pathDelim[] PROGMEM_T = "/";
    String filename = upload.filename;
    if (!filename.startsWith(FPSTR(pathDelim))) filename = String(FPSTR(pathDelim)) + filename;
    SPIFFS.begin();
    fsUploadFile = new File(SPIFFS.open(filename, String(F("w")).c_str()));
    filename = String();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (fsUploadFile) fsUploadFile->write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile) {
      fsUploadFile->close();
      delete fsUploadFile;
      fsUploadFile = nullptr;
      server->sendHeader(String(F("Location")), String(F("/upload")));
      server->send(303);
    } else server->send_P(500, PSTR("text/plain"), PSTR("500: couldn't create file"));
    SPIFFS.end();
  }
}

String getContentType(String filename) {
  static char extHtm[]  PROGMEM_T = ".htm";
  static char extHtml[] PROGMEM_T = ".html";
  static char extCss[]  PROGMEM_T = ".css";
  static char extJs[]   PROGMEM_T = ".js";
  static char extPng[]  PROGMEM_T = ".png";
  static char extGif[]  PROGMEM_T = ".gif";
  static char extJpg[]  PROGMEM_T = ".jpg";
  static char extIco[]  PROGMEM_T = ".ico";
  static char extXml[]  PROGMEM_T = ".xml";
  static char extPdf[]  PROGMEM_T = ".pdf";
  static char extZip[]  PROGMEM_T = ".zip";
  static char textHtml[] PROGMEM_T = "text/html";
  static char textCss[]  PROGMEM_T = "text/css";
  static char appJava[]  PROGMEM_T = "application/javascript";
  static char imgPng[]   PROGMEM_T = "image/png";
  static char imgGif[]   PROGMEM_T = "image/gif";
  static char imgJpeg[]  PROGMEM_T = "image/jpeg";
  static char imgIcon[]  PROGMEM_T = "image/x-icon";
  static char textXml[]  PROGMEM_T = "text/xml";
  static char appPdf[]   PROGMEM_T = "application/x-pdf";
  static char appZip[]   PROGMEM_T = "application/x-zip";
  static PGM_P contentTypeArray[22] PROGMEM_T = {
    extHtm,  textHtml,
    extHtml, textHtml,
    extCss,  textCss,
    extJs,   appJava,
    extPng,  imgPng,
    extGif,  imgGif,
    extJpg,  imgJpeg,
    extIco,  imgIcon,
    extXml,  textXml,
    extPdf,  appPdf,
    extZip,  appZip
  };
  for (uint8_t i = 0; i < (sizeof(contentTypeArray) / sizeof(contentTypeArray[0])); i += 2) if (filename.endsWith(FPSTR(contentTypeArray[i]))) return FPSTR(contentTypeArray[i + 1]);
  return F("text/plain");
}

void handleWebRequests() {
  String path = server->uri();
  String dataType = getContentType(path);
  SPIFFS.begin();
  File dataFile = SPIFFS.open(path.c_str(), String(F("r")).c_str());
  if (!dataFile) server->send_P(404, PSTR("text/plain"), PSTR("404: not found"));
  else {
    server->streamFile(dataFile, dataType);
    dataFile.close();
  }
  SPIFFS.end();
}

void handleRoot() {
  static char rootHTML[] PROGMEM_T = R"(<!DOCTYPE html><html><head><title>esp8266-env</title><style>
body{background-color:#fdb700;font-family:Arial,Helvetica,Sans-Serif;Color:#026;font-size:16px;}
button{background-color:#4c50af;border:none;color:white;padding:10px 24px;text-align:center;text-decoration:none;
 display:inline-block;font-size:12px;margin:4px 2px;cursor:pointer;border-radius:4px;box-shadow:0 4px #999;}
.admin{background-color:#af4c4c;padding:6px 14px;font-size:10px;}
button:active{box-shadow:0 1px #666;transform:translateY(3px);}
hr{border-style:dashed;color:#006;height:0px;}
</style></head><body><form action='' method='post'>
<h1>esp8266-env</h1>
<b>oled</b> : <b>%s</b> <button name='oled' value='1'>&#x1f321;on</button> <button name='oled' value='0'>off</button><br>
<b>clock</b> : <b>%s</b> <button name='clock' value='1'>&#x23f0;on</button> <button name='clock' value='0'>off</button><br>
<br>
<b>bme280</b> : temp= <b>%s</b> c, humidity= <b>%s</b> %%, pressure= <b>%d</b> hpa<br>
<b>ds18b20</b> : temp= <b>%s</b> c<br>
<b>rainfall</b>(yolp) : <b><tt>%s</tt></b> mm/h, past 60min + fore 60min<br>
<br>
<button name='msg' value='clear'>clear msg led</button> overrideRemain=<b>%d</b> m, <a href='msg' target='_blank'>msg api</a><br>
msg schedule : <b>%s</b><br>
<br>
<hr>
<a href='upload' target='_blank'>&#x1f4c2;file man</a> <button name='conf' value='reload'>&#x1f4dd;config reload</button>
<button name='ota' value='begin'>&#x1f528;enable ota</button> <button name='svc' value='close'>close services</button>
<button name='eeprom' value='erase' class='admin'>erase eeprom</button> <button name='reset' value='go' class='admin'>restart</button><br>
<br>
vcc= %d mv, rssi= %d dbm@ch.%d, localtime: %d/%02d/%02d %02d:%02d:%02d, uptime: <b>%d:%02d:%02d</b>, build: %s, %s<br>
tsLastResult=<b>%d</b>, rainLastResult=<b>%d</b>, failsafe=%d, myWDTdiff=%ld<br>
eeprom: magic=%02x%02x, failsafe=<b>%02x</b>, cfg=%02x | irregularResetCheck=<b>%d</b>, rtcUsrMemT=%d, ntpFail=<b>%d</b><br>
reset reason: <b>%s</b>, exccause=%d, reason=%d/%s<br>
<br>
unmodified, free stack= %d, <b>%d</b> | free heap= <b>%d</b> | sketch, free, flashReal/flash size= <b>%d</b>, %d, %d/%d KB<br>
cpu / flash spd= %d / %d mhz, flash mode= %02x/%s, crc16: conf=%04x, datetime=%04x<br>
ver: %s<br>
</form></body></html>)";
  static char oled[]  PROGMEM_T = "oled";
  static char clock[] PROGMEM_T = "clock";
  static char n1[]    PROGMEM_T = "1";
  static char n0[]    PROGMEM_T = "0";
  if (server->method() == HTTP_POST) {
    if        (server->arg(String(FPSTR(oled))) == String(FPSTR(n1))) {
      showOLED();
      EEPROM.write(3, EEPROM.read(3) | 0x01);
      EEPROM.commit();
    } else if (server->arg(String(FPSTR(oled))) == String(FPSTR(n0))) {
      display.clearDisplay();
      display.display();
      EEPROM.write(3, EEPROM.read(3) & 0xfe);
      EEPROM.commit();
    }
    if        (server->arg(String(FPSTR(clock))) == String(FPSTR(n1))) {
      dispTime();
      EEPROM.write(3, EEPROM.read(3) | 0x02);
      EEPROM.commit();
    } else if (server->arg(String(FPSTR(clock))) == String(FPSTR(n0))) {
      tm1637.point(POINT_OFF);
      tm1637.clearDisplay();
      EEPROM.write(3, EEPROM.read(3) & 0xfd);
      EEPROM.commit();
    }
    if (server->arg(String(F("msg")))    == String(F("clear")))  {
      analogWrite3(PIN_MSG_LED,  0, 1.0);
      analogWrite3(PIN_MSG2_LED, 0, 1.0);
      analogWrite3(PIN_MSG3_LED, 0, 1.0);
      analogWrite3(PIN_MSG4_LED, 0, 1.0);
      msgOverrideRemainTime = 0;
      memset(msgDisp, 0, sizeof(msgDisp));
    }
    if (server->arg(String(F("conf")))   == String(F("reload"))) loadConfig(true);
    if (server->arg(String(F("ota")))    == String(F("begin")))  {
      OTA = new ArduinoOTAClass();
      OTA->setHostname(String(F("esp8266-env")).c_str());
      OTA->setPassword(String(F("env")).c_str());
      OTA->onStart([]() {
        stopWaveform(PIN_MSG_LED);
        stopWaveform(PIN_MSG2_LED);
        stopWaveform(PIN_MSG3_LED);
        stopWaveform(PIN_MSG4_LED);
        timer0_detachInterrupt();
      });
      OTA->onError([](ota_error_t e) { timer0_attachInterrupt(timer0_ISR); });
      OTA->onEnd([] { 
        EEPROM.write(2, 0xfb); // 0xfb: failsafe-check
        EEPROM.commit();
        delay(100);
        });
      OTA->begin();
    }
    if (server->arg(String(F("svc")))    == String(F("close")))  { closeServices(); return; }
    if (server->arg(String(F("eeprom"))) == String(F("erase")))  eraseEEPROM();
    if (server->arg(String(F("reset")))  == String(F("go")))     {
      EEPROM.write(2, 0xfb); // 0xfb: failsafe-check
      EEPROM.commit();
      delay(100);
      ESP.restart();
    }
  }
  static char onStr[]  PROGMEM_T = "on";
  static char offStr[] PROGMEM_T = "off";
  static char textPlain[] PROGMEM_T = "text/plain";
  uint32_t sec = millis() / 1000;
  uint32_t min = sec / 60;
  uint16_t hr  = min / 60;
  extern cont_t *g_pcont;
  register uint32_t *sp asm("a1");
  char *strRainfall = (char *)malloc(MAX_RAINFALL_DATA + 1);
  if (strRainfall && enableRainFeature && rainFall) {
    for (uint8_t i = 0; i < MAX_RAINFALL_DATA; i++) {
      if      (rainFall[i] == 0.0f)  strRainfall[i] = '_';
      else if (rainFall[i] <  1.0f)  strRainfall[i] = '.';
      else if (rainFall[i] >  9.99f) strRainfall[i] = '@';
      else                           strRainfall[i] = '0' + (uint8_t)rainFall[i];
    }
    strRainfall[MAX_RAINFALL_DATA] = '\0';
  }
  char *msg;
  uint16_t msg_size = strlen_P((PGM_P)rootHTML) + ADD_BUF_SIZE_ROOTHTML + (MAX_MSG_SCHEDULE << 4) + MAX_RAINFALL_DATA * 5;
  msg = (char *)malloc(msg_size);
  if (!msg) {
    free(strRainfall);
    server->send_P(503, (PGM_P)textPlain, PSTR("503: not enough memory"));
    return;
  }
  snprintf_P(msg, msg_size, (PGM_P)rootHTML,
    String(EEPROM.read(3) & 0x1 ? FPSTR(onStr) : FPSTR(offStr)).c_str(),
    String(EEPROM.read(3) & 0x2 ? FPSTR(onStr) : FPSTR(offStr)).c_str(),
    cTemperature,
    cHumidity,
    (uint16_t)fPressure,
    cTemperatureO,
    enableRainFeature ? rainFall ? strRainfall ? strRainfall : String(F("cant alloc. tmp mem")).c_str() : String(F("cant alloc. mem")).c_str() : String(F("disabled")).c_str(),
    msgOverrideRemainTime,
    String(FPSTR(configFlashParamAddr(strMsgSchedule))).c_str(),
    ESP.getVcc(),
    WiFi.RSSI(),
    WiFi.channel(),
    tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
    tm->tm_hour,        tm->tm_min,     tm->tm_sec,
    hr,                 min % 60,       sec % 60,
    String(F(__DATE__)).c_str(),
    String(F(__TIME__)).c_str(),
    thingspeakLastResult,
    rainfallLastResult,
    failSafeCheck,
    (millis() >> 10) - myWDTsec,
    EEPROM.read(0), EEPROM.read(1), EEPROM.read(2), EEPROM.read(3),
    irregularResetCheck,
    getRTCtime(),
    ntpFailedAfterMin,
    ESP.getResetReason().c_str(),
    ESP.getResetInfoPtr()->exccause,
    ESP.getResetInfoPtr()->reason,
    String((ESP.getResetInfoPtr()->reason == 0 ? F("default") : ESP.getResetInfoPtr()->reason == 1 ? F("wdt") : ESP.getResetInfoPtr()->reason == 2 ? F("exception") : ESP.getResetInfoPtr()->reason == 3 ? F("soft_wdt")
      : ESP.getResetInfoPtr()->reason == 4 ? F("soft_restart") : ESP.getResetInfoPtr()->reason == 5 ? F("deep_sleep_awake") : ESP.getResetInfoPtr()->reason == 6 ? F("ext_sys_rst") : F("???"))).c_str(),
    cont_get_free_stack(g_pcont),
    (sp - g_pcont->stack) << 2,
    ESP.getFreeHeap(),
    ESP.getSketchSize() >> 10,
    ESP.getFreeSketchSpace() >> 10,
    ESP.getFlashChipRealSize() >> 10,
    ESP.getFlashChipSize() >> 10,
    ESP.getCpuFreqMHz(),
    ESP.getFlashChipSpeed() / 1000000,
    ESP.getFlashChipMode(),
    String(ESP.getFlashChipMode() == 0 ? F("qio") : ESP.getFlashChipMode() == 1 ? F("qout") : ESP.getFlashChipMode() == 2 ? F("dio") : ESP.getFlashChipMode() == 3 ? F("dout") : F("unknown")).c_str(),
    pgm_read_word((const void*)configFlashParamAddr(crc16info)),
    pgm_read_word((const void*)configFlashParamAddr(crc16info) + 2),
    ESP.getFullVersion().c_str());
  free(strRainfall);
  server->send_P(200, PSTR("text/html"), msg);
  free(msg);
}

void loadConfig(bool force) {
  SPIFFS.begin();
  File configFile = SPIFFS.open(String(F("/"CONFIG_PREFIX"config"CONFIG_SUFFIX".json")), String(F("r")).c_str());
  uint16_t crc16infoUint16t[2];
  size_t size = configFile.size();
  if (size >> 10) size = 0x3ff;
  char *_buf = (char *)malloc(size + 1);
  if (_buf) {
    _buf[configFile.readBytes(_buf, size)] = '\0';
    crc16infoUint16t[0] = crc16(0, _buf, size);
    free(_buf);
  } else crc16infoUint16t[0] = 0xeeee;
  uint32_t configFlashMagic[2];
  ESP.flashRead(configFlashStartAddr, (uint32_t *)configFlashMagic, sizeof(configFlashMagic));
  crc16infoUint16t[1] = crc16(0, String(F(__DATE__)).c_str());
  crc16infoUint16t[1] = crc16(crc16infoUint16t[1], String(F(__TIME__)).c_str());
  if (configFlashMagic[0] == (0x90010000 | crc16infoUint16t[0]) && configFlashMagic[1] == (0x09010000 | crc16infoUint16t[1]) && !force) {
    configFile.close();
    SPIFFS.end();
    resetRainfallFeature();
    return;
  }
  configFile.seek(0, SeekSet);
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(configFile);
  configFile.close();
  SPIFFS.end();
  static char dStr[] PROGMEM_T = "d";
  static char mStr[] PROGMEM_T = "m";
  static char rStr[] PROGMEM_T = "r";
  static char iStr[] PROGMEM_T = "i";
  ESP.flashEraseSector(configFlashStartAddr / FLASH_SECTOR_SIZE);
  configFlashMagic[0] = 0x90010000 | crc16infoUint16t[0];
  configFlashMagic[1] = 0x09010000 | crc16infoUint16t[1];
  ESP.flashWrite(configFlashStartAddr, (uint32_t *)configFlashMagic, sizeof(configFlashMagic));
  configFlashParamWrite(myWriteAPIKey, json[F("t")] | String().c_str());
  char *ssd1306ContentsChar = (char *)malloc(MAX_SSD1306CONTENT << 3);
  if (ssd1306ContentsChar) {
    for (uint8_t i = 0; i < MAX_SSD1306CONTENT; i++) {
      ssd1306ContentsChar[ i << 3 ]     = (char)((json[FPSTR(dStr)][i << 1] | 0) / 1000);
      ssd1306ContentsChar[(i << 3) + 1] = (char)((json[FPSTR(dStr)][i << 1] | 0) / 10 - (uint16_t)ssd1306ContentsChar[i << 3] * 100);
      ssd1306ContentsChar[(i << 3) + 2] = (char)((json[FPSTR(dStr)][i << 1] | 0) % 10);
      strlcpy(&ssd1306ContentsChar[(i << 3) + 3], json[FPSTR(dStr)][(i << 1) + 1] | String().c_str(), 5);
    }
    configFlashParamWrite(ssd1306Contents, ssd1306ContentsChar, MAX_SSD1306CONTENT << 3);
    free(ssd1306ContentsChar);
  } else configFlashParamWrite(ssd1306Contents, String().c_str());
  char *tm1637BrightChar = (char *)malloc(25);
  if (tm1637BrightChar) {
    strlcpy(tm1637BrightChar, json[F("c")] | String(F("000002888888888888800000")).c_str(), 25);
    configFlashParamWrite(tm1637Bright, tm1637BrightChar, 24);
    free(tm1637BrightChar);
  } else configFlashParamWrite(tm1637Bright, String().c_str());
  char *messageScheduleChar = (char *)malloc(MAX_MSG_SCHEDULE << 2);
  if (messageScheduleChar) {
    for (uint8_t i = 0; i < MAX_MSG_SCHEDULE; i++) {
      messageScheduleChar[ i << 2 ]     =               (json[FPSTR(mStr)][3 * i]     | 2500) / 100;
      messageScheduleChar[(i << 2) + 1] =               (json[FPSTR(mStr)][3 * i]     | 99)   % 100;
      messageScheduleChar[(i << 2) + 2] = (char)((float)(json[FPSTR(mStr)][3 * i + 1] | 1.0) * 8.0);
      messageScheduleChar[(i << 2) + 3] =                json[FPSTR(mStr)][3 * i + 2] | 50;
      if (messageScheduleChar[i << 2] > 23 || messageScheduleChar[(i << 2) + 1] > 59) messageScheduleChar[(i << 2) + 3] = 0xff;
    }
    configFlashParamWrite(messageSchedule, messageScheduleChar, MAX_MSG_SCHEDULE << 2);
    free(messageScheduleChar);
  } else configFlashParamWrite(messageSchedule, String().c_str());
  uint8_t arpOct4Uint8t = json[F("a")] | 1;
  configFlashParamWrite(arpOct4, (char *)&arpOct4Uint8t, 1);
  char *awsApiGwHostChar = (char *)malloc(MAX_API_HOSTNAME_LEN);
  if (awsApiGwHostChar) {
    strlcpy(awsApiGwHostChar, json[FPSTR(rStr)][0] | String().c_str(), MAX_API_HOSTNAME_LEN);
    configFlashParamWrite(awsApiGwHost, awsApiGwHostChar, MAX_API_HOSTNAME_LEN);
    free(awsApiGwHostChar);
  } else configFlashParamWrite(awsApiGwHost, String().c_str());
  char *longitudeChar = (char *)malloc(MAX_LONGITUDE_LATITUDE_LEN);
  if (longitudeChar) {
    strlcpy(longitudeChar, json[FPSTR(rStr)][1] | String().c_str(), MAX_LONGITUDE_LATITUDE_LEN);
    configFlashParamWrite(longitude, longitudeChar, MAX_LONGITUDE_LATITUDE_LEN);
    free(longitudeChar);
  } else configFlashParamWrite(longitude, String().c_str());
  char *latitudeChar = (char *)malloc(MAX_LONGITUDE_LATITUDE_LEN);
  if (latitudeChar) {
    strlcpy(latitudeChar, json[FPSTR(rStr)][2] | String().c_str(), MAX_LONGITUDE_LATITUDE_LEN);
    configFlashParamWrite(latitude, latitudeChar, MAX_LONGITUDE_LATITUDE_LEN);
    free(latitudeChar);
  } else configFlashParamWrite(latitude, String().c_str());
  char *yolpClientIdChar = (char *)malloc(MAX_YOLP_CLIENT_ID_LEN);
  if (yolpClientIdChar) {
    strlcpy(yolpClientIdChar, json[FPSTR(rStr)][3] | String().c_str(), MAX_YOLP_CLIENT_ID_LEN);
    configFlashParamWrite(yolpClientId, yolpClientIdChar, MAX_YOLP_CLIENT_ID_LEN);
    free(yolpClientIdChar);
  } else configFlashParamWrite(yolpClientId, String().c_str());
  char *msg4CtrlHostChar = (char *)malloc(MAX_MSG4CTRL_HOST_LEN);
  if (msg4CtrlHostChar) {
    strlcpy(msg4CtrlHostChar, json[FPSTR(iStr)][0] | String().c_str(), MAX_MSG4CTRL_HOST_LEN);
    configFlashParamWrite(msg4CtrlHost, msg4CtrlHostChar, MAX_MSG4CTRL_HOST_LEN);
    free(msg4CtrlHostChar);
  } else configFlashParamWrite(msg4CtrlHost, String().c_str());
  char *msg4CtrlPathChar = (char *)malloc(MAX_MSG4CTRL_PATH_LEN);
  if (msg4CtrlPathChar) {
    strlcpy(msg4CtrlPathChar, json[FPSTR(iStr)][1] | String().c_str(), MAX_MSG4CTRL_PATH_LEN);
    configFlashParamWrite(msg4CtrlPath, msg4CtrlPathChar, MAX_MSG4CTRL_PATH_LEN);
    free(msg4CtrlPathChar);
  } else configFlashParamWrite(msg4CtrlPath, String().c_str());
  jsonBuffer.clear();
  resetRainfallFeature();
  String strMsgScheduleF = String();
  for (uint8_t i = 0; i < MAX_MSG_SCHEDULE; i++) {
    if (pgm_read_byte((const void*)configFlashParamAddr(messageSchedule) + (i << 2) + 3) != 0xff) strMsgScheduleF += String((strMsgScheduleF.length() ? F(", ") : FPSTR("")))
      + String(pgm_read_byte((const void*)configFlashParamAddr(messageSchedule) + (i << 2))) + String(F(":")) + ((pgm_read_byte((const void*)configFlashParamAddr(messageSchedule) + (i << 2) + 1) > 9)
      ? String(pgm_read_byte((const void*)configFlashParamAddr(messageSchedule) + (i << 2) + 1)) : String(F("0")) + pgm_read_byte((const void*)configFlashParamAddr(messageSchedule) + (i << 2) + 1)) + String(F("/"))
      + (pgm_read_byte((const void*)configFlashParamAddr(messageSchedule) + (i << 2) + 3) ? (pgm_read_byte((const void*)configFlashParamAddr(messageSchedule) + (i << 2) + 3) == 100) ? String(F("on"))
      : ((pgm_read_byte((const void*)configFlashParamAddr(messageSchedule) + (i << 2) + 2) > 0xf0) ? String(F("128")) : ((pgm_read_byte((const void*)configFlashParamAddr(messageSchedule) + (i << 2) + 2) & 0x7)
      ? String((float)pgm_read_byte((const void*)configFlashParamAddr(messageSchedule) + (i << 2) + 2) / 8.0) : String(pgm_read_byte((const void*)configFlashParamAddr(messageSchedule) + (i << 2) + 2) >> 3)))
      + String(F("hz/")) + pgm_read_byte((const void*)configFlashParamAddr(messageSchedule) + (i << 2) + 3) + String(F("%")) : String(F("off")));
  }
  if (!strMsgScheduleF.length()) strMsgScheduleF = String(F("none"));
  configFlashParamWrite(strMsgSchedule, strMsgScheduleF.c_str());
  configFlashParamWrite(crc16info, (char *)&crc16infoUint16t, 4);
}

void failSafeOTA() {
  OTA = new ArduinoOTAClass();
  OTA->setHostname(String(F("esp8266-env-failsafe")).c_str());
  OTA->begin();
  display.println(F("****====****====****")); display.display();
  display.print(F("FAILSAFE OTA enabled")); display.display();
  setRTCtime(true);
  EEPROM.write(0, 0x90); EEPROM.write(1, 0x01); EEPROM.write(2, 0x00); // magic: 0x9001, 0x00: failsafe-check
  EEPROM.commit();
  uint16_t otaLoopCount = 0;
  do {
    OTA->handle();
    delay(100);
  } while (otaLoopCount++ < (FALESAFE_RESTART_SEC * 10)); // 100msec/loop
  delay(100);
  ESP.restart();
}

void analogWrite3(uint8_t pin, uint8_t duty, float freq) {
  if (pin > 16) return;
  float analogPeriod = 1000000.0 / ((freq < 0.03125) ? 0.03125 : ((freq > 512.0) ? 512.0 : freq));
  if (duty > 100) duty = 100;
  uint32_t high = (analogPeriod * duty) / 100;
  uint32_t low = analogPeriod - high;
  pinMode(pin, OUTPUT);
  if (low == 0) {
    stopWaveform(pin);
    digitalWrite(pin, HIGH);
  } else if (high == 0) {
    stopWaveform(pin);
    digitalWrite(pin, LOW);
  } else startWaveform(pin, high, low, 0);
}

void messageControl() {
  static char textPlain[] PROGMEM_T = "text/plain";
  if (!server->args()) {
    server->send_P(200, (PGM_P)textPlain, PSTR("usage; pin=[1-4],freq=[.0005-400](hz),duty=[0:off-100:on],override=[0-](min),disp=[str]"));
    return;
  }
  static char overrideStr[] PROGMEM_T = "override";
  static char dutyStr[]     PROGMEM_T = "duty";
  static char freqStr[]     PROGMEM_T = "freq";
  static char dispStr[]     PROGMEM_T = "disp";
  if ((server->arg(String(FPSTR(dutyStr)))).length() || (server->arg(String(FPSTR(freqStr)))).length()) {
    uint8_t pin = atoi(server->arg(String(F("pin"))).c_str()) & 0xff;
    analogWrite3(pin == 4 ? PIN_MSG4_LED : pin == 3 ? PIN_MSG3_LED : pin == 2 ? PIN_MSG2_LED : PIN_MSG_LED,
      atoi(server->arg(String(FPSTR(dutyStr))).c_str()) & 0xff, atof(server->arg(String(FPSTR(freqStr))).c_str()));
  }
  if ((server->arg(String(FPSTR(overrideStr)))).length()) msgOverrideRemainTime = atoi(server->arg(String(FPSTR(overrideStr))).c_str()) & 0xff;
  if ((server->arg(String(FPSTR(dispStr)))).length()) {
    if (server->arg(String(FPSTR(dispStr))) == String(F("clear"))) {
      msgDisp[0] = '\0';
      msgDisp[3] = '\0';
    } else {
      strlcpy(msgDisp, server->arg(String(FPSTR(dispStr))).c_str(), 5);
      msgDisp[5] = '\0';
      msgDisp[4] = msgDisp[3];
      msgDisp[3] = msgDisp[2];
      msgDisp[2] = '\0';
    }
    if (EEPROM.read(3) & 0x01) showOLED();
  }
  server->send_P(200, (PGM_P)textPlain, PSTR("200: ok"));
}

void timer0_ISR() {
  timer0_write(ESP.getCycleCount() + (uint32_t)ESP.getCpuFreqMHz() * ((uint32_t)15 << 20)); // 15s
  if ((millis() >> 10) - myWDTsec > MY_WDT_TIMEOUT) ESP.restart();
}

uint32_t configFlashParamAddr(const configFlashParam num) {
  if (num & 0xc0) return 0x40010000;
  uint32_t configFlashParamTable;
  ESP.flashRead(configFlashStartAddr + 8 + ((num >> 1) << 2), &configFlashParamTable, sizeof(configFlashParamTable));
  if (num & 0x1) configFlashParamTable >>= 16;
  if ((configFlashParamTable & 0xffff) == 0xffff) return 0x40010000;
  return 0x40200000 + configFlashStartAddr + ((configFlashParamTable & 0x0000ffc0) >> 4);
}

void configFlashParamWrite(const configFlashParam num, const char *writeParam) { configFlashParamWrite(num, writeParam, 0); }
void configFlashParamWrite(const configFlashParam num, const char *writeParam, uint16_t size) {
  if (num & 0xc0) return;
  uint16_t paramSize;
  if (size) paramSize = (size + 3) & (~3);
  else paramSize = (strlen(writeParam) + 4) & (~3);
  if (paramSize & 0xff00) return;
  uint16_t nextAddr = 8 + 128;
  uint32_t configFlashParamTable;
  for (uint8_t i = 0; i < 32; i++) {
    ESP.flashRead(configFlashStartAddr + 8 + (i << 2), &configFlashParamTable, sizeof(configFlashParamTable));
    for (uint8_t j = 0; j < 2; j++) {
      if ((configFlashParamTable & 0x0000ffff) != 0x0000ffff) {
        if (((i << 1) | j) == num) return;
        if (nextAddr < (((uint16_t)configFlashParamTable & 0xffc0) >> 4) + (((uint16_t)configFlashParamTable & 0x003f) << 2))
            nextAddr = (((uint16_t)configFlashParamTable & 0xffc0) >> 4) + (((uint16_t)configFlashParamTable & 0x003f) << 2);
      }
      configFlashParamTable >>= 16;
    }
  }
  if (((nextAddr + paramSize) & 0xf000) || (nextAddr + paramSize) > FLASH_SECTOR_SIZE) return;
  configFlashParamTable = 0xffff0000 | ((uint32_t)nextAddr << 4) | (uint32_t)(paramSize >> 2);
  if (num & 0x1) configFlashParamTable = (configFlashParamTable << 16) | 0x0000ffff;
  ESP.flashWrite(configFlashStartAddr + 8 + (uint32_t)((num >> 1) << 2), &configFlashParamTable, sizeof(configFlashParamTable));
  for (uint16_t i = 0; i < (paramSize >> 2); i++) {
    uint32_t writeParamUint32t = (uint32_t)writeParam[(i << 2) + 3] << 24 | (uint32_t)writeParam[(i << 2) + 2] << 16 | (uint32_t)writeParam[(i << 2) + 1] << 8 | (uint32_t)writeParam[i << 2];
    ESP.flashWrite(configFlashStartAddr + nextAddr + (i << 2), &writeParamUint32t, sizeof(writeParamUint32t));
  }
}

void thingSpeakSetFieldInt(uint8_t field, int value) {
  if (!field || field > 8) return;
  field--;
  snprintf_P(&thingSpeakFieldData[20 * field], 20, PSTR("%d"), value);
}

void thingSpeakSetFieldFloat(uint8_t field, float value) {
  if (!field || field > 8) return;
  field--;
  if (0 == isinf(value) && (value > 999999000000 || value < -999999000000)) return;
  dtostrf(value, 1, 5, &thingSpeakFieldData[20 * field]);
}

int16_t thingSpeakGetHTTPResponse() {
  long startWaitForResponseAt = millis();
  while (!secClient.available() && millis() - startWaitForResponseAt < 5000) { delay(100); }
  if (!secClient.available()) return -304; // ERR_TIMEOUT
  if (!secClient.find(String(F("HTTP/1.1")).c_str())) return -303; // ERR_BAD_RESPONSE
  int16_t status = secClient.parseInt();
  if (status != 200) return status;
  if (!secClient.find(String(F("\r\n")).c_str()))     return -303; // ERR_BAD_RESPONSE
  if (!secClient.find(String(F("\n\r\n")).c_str()))   return -303; // ERR_BAD_RESPONSE
  uint8_t *_buffer = (uint8_t *)malloc(32);
  if (_buffer) {
    int entryID = -1;
    while (secClient.available()) {
      int8_t actualLength = secClient.read(_buffer, 31);
      if (actualLength <= 0) {
        if (entryID == -1) entryID = 0;
        break;
      }
      if (entryID == -1) {
        _buffer[actualLength] = '\0';
        entryID = atoi((char *)_buffer);
      }
    }
    free(_buffer);
    if (!entryID) return -401; // ERR_NOT_INSERTED
  } else return -701; // cant allocate buffer
  return status;
}

int16_t thingSpeakWriteFields() {
  char *pmsg = postMessage;
  memset(pmsg, 0, 232);  // 16 + (19 + 8) * 8
  for (uint8_t i = 0; i < 8; i++) {
    if (thingSpeakFieldData[20 * i]) {
      if (pmsg != postMessage) *(pmsg++) = '&';
      strncpy_P(pmsg, PSTR("field"), 6);
      while (*++pmsg) ;
      *(pmsg++) = '1' + i;
      *(pmsg++) = '=';
      strncpy(pmsg, (const char *)&thingSpeakFieldData[20 * i], 20);
      while (*++pmsg) ;
    }
  }
  if (!strlen(postMessage)) return -210; // ERR_SETFIELD_NOT_CALLED
  strncpy_P(pmsg, PSTR("&headers=false\n"), 16);
  if (secClient.connected() || secClient.connect(String(F(THINGSPEAK_HOST)).c_str(), 443)) {
    uint8_t abortFlag = 0;
    if (             !secClient.print(F("POST /update HTTP/1.1\r\nHost: "THINGSPEAK_HOST"\r\nX-THINGSPEAKAPIKEY: "))) abortFlag = 1;
    if (abortFlag || !secClient.print(FPSTR(configFlashParamAddr(myWriteAPIKey))))                                    abortFlag = 1;
    if (abortFlag || !secClient.print(F("\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: ")))  abortFlag = 1;
    if (abortFlag || !secClient.print(strlen(postMessage)))                                                           abortFlag = 1;
    if (abortFlag || !secClient.print(F("\r\n\r\n")))                                                                 abortFlag = 1;
    if (abortFlag || !secClient.print(postMessage))                                                                   abortFlag = 1;
    if (abortFlag) {
      secClient.stop();
      return -302; // ERR_UNEXPECTED_FAIL
    }
    int16_t result = thingSpeakGetHTTPResponse();
    if (result != 200) secClient.stop();
    return result;
  }
  return -301; // ERR_CONNECT_FAILED
}

uint16_t crc16(uint16_t crc, const char *ptr) { return crc16(crc, ptr, strlen(ptr)); }
uint16_t crc16(uint16_t crc, const char *ptr, uint16_t len) {
  #define CRC16POLY 0x8408  // CRC-16-CCITT
  crc = ~crc;
  for (uint16_t i = 0; i < len; i++) {
    crc ^= ptr[i];
    for (uint8_t j = 0; j < 8; j++) { if (crc & 1) crc = (crc >> 1) ^ CRC16POLY; else crc >>= 1; }
  }
  return ~crc;
}

int8_t etharpFindAddr(uint8_t ipAddrOct4) {
  struct eth_addr *macAddress;
  ip4_addr_t ipAddr;
  ipAddr.addr = WiFi.localIP();
  ipAddr.addr &= 0x00ffffff;
  ipAddr.addr |= ((uint32_t)ipAddrOct4 << 24);
  const ip4_addr_t *ipAddrR;
  return etharp_find_addr(netif_default, (const ip4_addr_t *)&ipAddr, &macAddress, &ipAddrR);
}

void startWebServer() {
  if (server) return;
  server = new ESP8266WebServer(80);
  static char upload[] PROGMEM_T = "/upload";
  server->on(String(F("/")),        handleRoot);
  server->on(String(FPSTR(upload)), HTTP_GET,  handleFileList);
  server->on(String(FPSTR(upload)), HTTP_POST, [] { server->send(200); }, handleFileUpload);
  server->on(String(F("/msg")),     messageControl);
  server->onNotFound(handleWebRequests);
  server->begin();
}

void closeServices() {
  if (fsUploadFile) {
    fsUploadFile->close();
    delete fsUploadFile;
    fsUploadFile = nullptr;
  }
  if (OTA) {
    delete OTA;
    OTA = nullptr;
  }
  if (server) {
    server->send_P(200, PSTR("text/plain"), PSTR("close services..."));
    delay(1000);
    serverClosing = 1;
  }
}

uint8_t yolpGwGetHTTPResponse(String &response) {
  long startWaitForResponseAt = millis();
  while (secClient.available() == 0 && millis() - startWaitForResponseAt < 5000) { delay(100); }
  if (secClient.available() == 0) return 1;
  if (!secClient.find(const_cast<char *>(String(F("HTTP/1.1")).c_str()))) return 1;
  int16_t status = secClient.parseInt();
  if (status != 200) return 1;
  if (!secClient.find(const_cast<char *>(String(F("\r\n")).c_str()))) return 1;
  if (!secClient.find(const_cast<char *>(String(F("\n\r\n")).c_str()))) return 1;
  String tempString = secClient.readString();
  if (tempString.length() < 256) response = tempString;
  return 0;
}

int8_t yolpGetRainfall() {
  secClient.stop();
  if (secClient.connect(String(FPSTR(configFlashParamAddr(awsApiGwHost))).c_str(), 443)) {
    uint8_t abortFlag = 0;
    if (             !secClient.print(F("GET /prod/")))                           abortFlag = 1;
    if (abortFlag || !secClient.print(FPSTR(configFlashParamAddr(longitude))))    abortFlag = 1;
    if (abortFlag || !secClient.print(F("/")))                                    abortFlag = 1;
    if (abortFlag || !secClient.print(FPSTR(configFlashParamAddr(latitude))))     abortFlag = 1;
    if (abortFlag || !secClient.print(F("?past=1&interval=5&id=")))               abortFlag = 1;
    if (abortFlag || !secClient.print(FPSTR(configFlashParamAddr(yolpClientId)))) abortFlag = 1;
    if (abortFlag || !secClient.print(F(" HTTP/1.1\r\nHost: ")))                  abortFlag = 1;
    if (abortFlag || !secClient.print(FPSTR(configFlashParamAddr(awsApiGwHost)))) abortFlag = 1;
    if (abortFlag || !secClient.print(F("\r\nConnection: close\r\n\r\n")))        abortFlag = 1;
    String content = String();
    if (abortFlag || yolpGwGetHTTPResponse(content))                              abortFlag = 1;
    secClient.stop();
    if (abortFlag) return 21;
    rainfallJsonToFloat(content.c_str());
    return 0;
  } else {
    return 11;
  }
}

void rainfallJsonToFloat(const char *jsonChar) {
  DynamicJsonBuffer jsonBuffer;
  JsonArray& json = jsonBuffer.parseArray(jsonChar);
  noRain = 1;
  if (rainFall) {
    for (uint8_t i = 0; i < MAX_RAINFALL_DATA; i++) {
      if (rainFall[i] = ((String)(json[i] | String(F("x0")).c_str())).substring(1).toFloat()) noRain = 0;
    }
  }
}

void resetRainfallFeature() {
  free(rainFall);
  rainFall = nullptr;
  noRain = 1;
  enableRainFeature = 0;
  rainfallLastResult = -1;
  for (uint8_t i = 0; i < MAX_SSD1306CONTENT; i++) {
    if (pgm_read_byte((const void*)configFlashParamAddr(ssd1306Contents) + (i << 3) + 3) == 'r' && !pgm_read_byte((const void*)configFlashParamAddr(ssd1306Contents) + (i << 3) + 4)) enableRainFeature = 1;
  }
  if (enableRainFeature) {
    rainFall = (float *)malloc(MAX_RAINFALL_DATA << 2); // sizeof(float) * MAX_RAINFALL_DATA
    if (rainFall) for (uint8_t i = 0; i < MAX_RAINFALL_DATA; i++) { rainFall[i] = 0.0f; }
  }
}

void msg4Ctrl() {
  WiFiClient client;
  uint8_t waitCount = 0;
  client.connect(String(FPSTR(configFlashParamAddr(msg4CtrlHost))).c_str(), 80);
  do { delay(100); } while (!client.connected() && waitCount++ < 100); // 100ms * 100 = 10s
  if (client.connected()) {
    uint8_t abortFlag = 0;
    if (             !client.print(F("GET /")))                                abortFlag = 1;
    if (abortFlag || !client.print(FPSTR(configFlashParamAddr(msg4CtrlPath)))) abortFlag = 1;
    if (abortFlag || !client.print(F(" HTTP/1.1\r\nHost: ")))                  abortFlag = 1;
    if (abortFlag || !client.print(FPSTR(configFlashParamAddr(msg4CtrlHost)))) abortFlag = 1;
    if (abortFlag || !client.print(F("\r\nConnection: close\r\n\r\n")))        abortFlag = 1;
    if (abortFlag) { client.stop(); return; }
    waitCount = 0;
    do { delay(100); } while (!client.available() && waitCount++ < 150); // 100ms * 150 = 15s
    String content = String();
    if (client.available()) {
      client.find(String(F("HTTP/1.1")).c_str());
      if (client.parseInt() == 200) {
        client.find(String(F("\n\r\n")).c_str());
        while (client.available()) content += client.readString();
      }
    }
    if (*content.c_str()) {
      if (*content.c_str() == '0' || *content.c_str() == '1') analogWrite3(PIN_MSG4_LED, *content.c_str() == '1' ? 50 : 0, 4.0f);
      if (client.connected() || client.connect(String(FPSTR(configFlashParamAddr(msg4CtrlHost))).c_str(), 80)) {
        if (             !client.print(F("POST /")))                               abortFlag = 1;
        if (abortFlag || !client.print(FPSTR(configFlashParamAddr(msg4CtrlPath)))) abortFlag = 1;
        if (abortFlag || !client.print(F(" HTTP/1.1\r\nHost: ")))                  abortFlag = 1;
        if (abortFlag || !client.print(FPSTR(configFlashParamAddr(msg4CtrlHost)))) abortFlag = 1;
        if (abortFlag || !client.print(F("\r\nConnection: close\r\nContent-Length: 0\r\n\r\n"))) abortFlag = 1;
        if (abortFlag) { client.stop(); return; }
        do { delay(100); } while (!client.available() && waitCount++ < 150); // 100ms * 150 = 15s
        if (client.available()) {
          client.find(String(F("\n\r\n")).c_str());
          client.find(String(F("\n")).c_str());
        }
      }
    }
  }
  client.stop();
}

uint32_t getRTCtime() {
  uint32_t rtc[2];
  ESP.rtcUserMemoryRead(0, rtc, sizeof(rtc));
  if ((rtc[0] >> 16) == 0x9001) return rtc[1];
  else return 3600 * 9;
}

void setRTCtime() { setRTCtime(false); }
void setRTCtime(bool invalidate) {
  uint32_t rtc[2];
  t = time(NULL);
  if (ntpFailedAfterMin && ntpFailedRtcOffset) t += 3600 * ntpFailedRtcOffset;
  rtc[0] = invalidate ? 0 : 0x90010000;
  rtc[1] = (uint32_t)t;
  ESP.rtcUserMemoryWrite(0, rtc, sizeof(rtc));
}
