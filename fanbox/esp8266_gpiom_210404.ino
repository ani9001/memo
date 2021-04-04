// 80 MHz, DOUT, 1M (128K SPIFFS)
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#define NO_GLOBAL_MDNS
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#define NO_GLOBAL_ARDUINOOTA
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#define NO_GLOBAL_LLMNR
#include <ESP8266LLMNR.h>

#define PROGMEM_T __attribute__((section(".irom.text.t")))

#define WIFI_SSID          "ssid"
#define WIFI_PASS          "pass"
#define HOSTNAME          "gpiom"
#define MY_WDT_TIMEOUT         25
#define FAILSAFECHECK_SEC      55
#define BAUD_RATE            9600
#define COMMAND_BUFFER_SIZE    16
#define INET_INTERVAL_SEC      58
#define INET_HOST               "himitsu.cloudfront.net"
#define INET_PATH               "/gpiom.ctrl"
#define INET_PATH_RES           "/gpiom.res"
#define SERIAL_AUTO_CLOSE_SEC 300
#define FALESAFE_RESTART_SEC   90
#define SERIAL_INTERVAL_MSEC   10
#define SERIAL_LOOP_COUNT      10
#define WEB_MSG_BUFFER_SIZE     ((sizeof(Dpin) / sizeof(Dpin[0])) * 34 + 19)

#define EEPROM_SIZE      ((1 + sizeof(Dpin) / sizeof(Dpin[0])) << 3)
#define MAGIC_B1         0
#define MAGIC_B2         1
#define FAILSAFE         2
#define FEATURE          3
#define DISABLE_WIFI  0x01
#define ENABLE_WEB    0x02
#define ENABLE_OTA    0x04
#define ENABLE_INET   0x08
#define DIO_ENTRY_OFFSET 8
#define DIO_PINMODE      0
#define DIO_VALUE        1
#define DIO_DUTY         2
#define DIO_TRIG_ACTION  3
#define DIO_FREQ         4

volatile unsigned long myWDTsec      = millis() >> 10;
         unsigned long serialLastsec = millis() >> 10;
         unsigned long inetLastsec   = 0;
int8_t  failSafeCheck   = 1;
uint8_t serverClosing   = 0;
uint8_t cmdBufferPos    = 0;
uint8_t serialDontClose = 0;
char    cmd[COMMAND_BUFFER_SIZE];
uint8_t Dpin[] = { 0, 2, 14, 4 };
uint8_t inTrig[sizeof(Dpin) / sizeof(Dpin[0])] = { 0 };
ESP8266WebServer *server = nullptr;
LLMNRResponder   *LLMNRr = nullptr;
ArduinoOTAClass  *OTA    = nullptr;

extern "C" int startWaveform(uint8_t, uint32_t, uint32_t, uint32_t);
extern "C" int stopWaveform(uint8_t);

void setup() {
  Serial.setRxBufferSize(32);
  Serial.begin(BAUD_RATE);

  EEPROM.begin(EEPROM_SIZE); // 4..4096
  if (EEPROM.read(MAGIC_B1) == 0x90 && EEPROM.read(MAGIC_B2) == 0x01 && EEPROM.read(FAILSAFE) == 0xfa) failSafeOTA();
  if (EEPROM.read(MAGIC_B1) != 0x90 || EEPROM.read(MAGIC_B2) != 0x01 || EEPROM.read(FAILSAFE) != 0x00) eraseEEPROM(false, true);
  EEPROM.write(MAGIC_B1, 0x90); EEPROM.write(MAGIC_B2, 0x01); EEPROM.write(FAILSAFE, 0xfa);
  EEPROM.commit();

  for (uint8_t i = 0; i < (sizeof(Dpin) / sizeof(Dpin[0])); i++) {
    pinMode(Dpin[i], EEPROM.read(DIO_ENTRY_OFFSET + (i << 3) + DIO_PINMODE));
    if (EEPROM.read(DIO_ENTRY_OFFSET + (i << 3) + DIO_PINMODE) & 0x01) {
      if (EEPROM.read(DIO_ENTRY_OFFSET + (i << 3) + DIO_DUTY)) analogWrite3(Dpin[i], EEPROM.read(DIO_ENTRY_OFFSET + (i << 3) + DIO_DUTY), *(float *)(EEPROM.getConstDataPtr() + DIO_ENTRY_OFFSET + (i << 3) + DIO_FREQ));
      else digitalWrite(Dpin[i], EEPROM.read(DIO_ENTRY_OFFSET + (i << 3) + DIO_VALUE));
    } else inTrig[i] = digitalRead(Dpin[i]) & 0x01;
  }

  if (!(EEPROM.read(FEATURE) & DISABLE_WIFI)) {
    connectWiFi();
    delay(350);
    if (EEPROM.read(FEATURE) & ENABLE_WEB) { startWebServer(); delay(250); }
    if (EEPROM.read(FEATURE) & ENABLE_OTA) { startOTA(true); delay(250); }
  } else WiFi.mode(WIFI_OFF);

  noInterrupts();
  timer0_isr_init();
  timer0_attachInterrupt(timer0_ISR);
  timer0_write(ESP.getCycleCount() + (uint32_t)ESP.getCpuFreqMHz() * ((uint32_t)5 << 20)); // 5s
  interrupts();

  showVer();
  Serial.println(F("started."));
  Serial.write('#');
}

void loop() {
  uint8_t serialLoopCount = 0;
  do {
    delayWithHandle(SERIAL_INTERVAL_MSEC);
  } while (++serialLoopCount < SERIAL_LOOP_COUNT);

  if (OTA) OTA->handle();
  if (server) server->handleClient();
  if (server && serverClosing) {
    server->stop();
    delete server;
    server = nullptr;
    serverClosing = 0;
  }
  if (!serialDontClose && (millis() >> 10) - serialLastsec > SERIAL_AUTO_CLOSE_SEC) Serial.end();
  if ((EEPROM.read(FEATURE) & ENABLE_INET) && (millis() >> 10) - inetLastsec > INET_INTERVAL_SEC) {
    inetLastsec = millis() >> 10;
    WiFiClient client;
    uint8_t waitCount = 0;
    client.connect(String(F(INET_HOST)).c_str(), 80);
    do { delayWithHandle(100); } while (!client.connected() && waitCount++ < 100); // 100ms * 100 = 10s
    if (client.connected()) {
      client.print(F("GET "INET_PATH" HTTP/1.1\r\nHost: "INET_HOST"\r\nConnection: close\r\n\r\n"));
      waitCount = 0;
      do { delayWithHandle(100); } while (!client.available() && waitCount++ < 150); // 100ms * 150 = 15s
      String content = String();
      if (client.available()) {
        client.find(String(F("HTTP/1.1")).c_str());
        if (client.parseInt() == 200) {
          client.find(String(F("\n\r\n")).c_str());
          while (client.available()) content.length() < 64 ? (content += client.readString()) : client.readString();
        }
      }
      const char *c = content.c_str();
      if (*c) {
        for (uint8_t i = 0; i < (sizeof(Dpin) / sizeof(Dpin[0])); i++) {
          if (!*c) break;
          if (EEPROM.read(DIO_ENTRY_OFFSET + (i << 3) + DIO_PINMODE) & 0x01) {
            if        (*c == '0' || *c == '1') setDpinVal(i, *c - '0');
            else if   (*c == 'b' || *c == 'B') {
              setDpinDuty(i, 50);
              setDpinFreq(i, (*c == 'B') ? 4.0f : 1.0f);
            } else if (*c == 'z' || *c == 'Z') {
              setDpinDuty(i, 50);
              setDpinFreq(i, (*c == 'Z') ? 4000.0f : 2000.0f);
            }
          }
          c++;
        }
        if (*c) {
          char *msg = (char *)malloc(WEB_MSG_BUFFER_SIZE);
          webDebugMsg(msg, WEB_MSG_BUFFER_SIZE);
          if (msg && (client.connected() || client.connect(String(F(INET_HOST)).c_str(), 80))) {
            if (client.print(F("POST "INET_PATH_RES" HTTP/1.1\r\nHost: "INET_HOST"\r\nConnection: close\r\nContent-Length: "))
             && client.print(strlen(msg) + 1 + strlen(c))
             && client.print(F("\r\n\r\n"))
             && client.print(msg)
             && client.print(F(","))
             && client.print(c)) {
              do { delayWithHandle(100); } while (!client.available() && waitCount++ < 150); // 100ms * 150 = 15s
              if (client.available()) {
                client.find(String(F("\n\r\n")).c_str());
                client.find(String(F("\n")).c_str());
              }
            }
          }
          free(msg);
        }
        if (client.connected() || client.connect(String(F(INET_HOST)).c_str(), 80)) {
          if (client.print(F("POST "INET_PATH" HTTP/1.1\r\nHost: "INET_HOST"\r\nConnection: close\r\nContent-Length: 0\r\n\r\n"))) {
            do { delayWithHandle(100); } while (!client.available() && waitCount++ < 150); // 100ms * 150 = 15s
            if (client.available()) {
              client.find(String(F("\n\r\n")).c_str());
              client.find(String(F("\n")).c_str());
            }
          }
        }
      }
    }
    client.stop();
  }

  myWDTsec = millis() >> 10;
  if (failSafeCheck && myWDTsec > FAILSAFECHECK_SEC) {
    EEPROM.write(FAILSAFE, 0x00);
    EEPROM.commit();
    failSafeCheck = 0;
  }
}

void handleSerial() {
  while (Serial.available()) {
    char cmdChar = Serial.read();
    if (0x1f < cmdChar && cmdChar < 0x7f) {
      if (cmdBufferPos == COMMAND_BUFFER_SIZE - 1) {
        Serial.write(0x08);
        cmdBufferPos--;
      }
      cmd[cmdBufferPos++] = cmdChar;
    } else if (cmdChar == '\n') {
      cmd[cmdBufferPos] = '\0';
      cmdBufferPos = 0;
    } else if (cmdChar == 0x08) cmdBufferPos--;
    Serial.write(cmdChar);
    if (cmdChar == '\n') {
      if (cmd[0]) {
        bool syntaxError = false;
        if ((cmd[0] == 'd' || cmd[0] == 'D') && 0x2f < cmd[1] && cmd[1] < 0x30 + sizeof(Dpin) / sizeof(Dpin[0])) {
          uint8_t pin = cmd[1] - 0x30;
          if        (cmd[2] == 'm' || cmd[2] == 'M') {
            if (!cmd[3]) Serial.println(EEPROM.read(DIO_ENTRY_OFFSET + (pin << 3) + DIO_PINMODE));
            else if (!setDpinMode(pin, cmd[3] - 0x30))  syntaxError = true;
          } else if (cmd[2] == 'v' || cmd[2] == 'V') {
            if (!cmd[3]) Serial.println((EEPROM.read(DIO_ENTRY_OFFSET + (pin << 3) + DIO_PINMODE) & 0xf9) ? EEPROM.read(DIO_ENTRY_OFFSET + (pin << 3) + DIO_VALUE) : digitalRead(Dpin[pin]));
            else if (!setDpinVal(pin, cmd[3] - 0x30)) syntaxError = true;
          } else if (cmd[2] == 'd' || cmd[2] == 'D') {
            if (!cmd[3]) Serial.println(EEPROM.read(DIO_ENTRY_OFFSET + (pin << 3) + DIO_DUTY));
            else if (!setDpinDuty(pin, atoi(cmd + 3) & 0x7f)) syntaxError = true;
          } else if (cmd[2] == 'f' || cmd[2] == 'F') {
            if (!cmd[3]) {
              char cFreq[8];
              dtostrf(*(float *)(EEPROM.getConstDataPtr() + DIO_ENTRY_OFFSET + (pin << 3) + DIO_FREQ), - (sizeof(cFreq) - 1),  2, cFreq);
              Serial.println(cFreq);
            } else if (!setDpinFreq(pin, atof(cmd + 3))) syntaxError = true;
          } else if (cmd[2] == 't' || cmd[2] == 'T') {
            if (!cmd[3]) Serial.println(inTrig[pin] >> 1);
            else if (!(cmd[3] == 'c' || cmd[3] == 'C') || cmd[4]) syntaxError = true;
            else inTrig[pin] &= 0x01;
          } else if (cmd[2] == 'a' || cmd[2] == 'A') {
            if (!cmd[3]) Serial.println(EEPROM.read(DIO_ENTRY_OFFSET + (pin << 3) + DIO_TRIG_ACTION));
            setDpinAction(pin, atoi(cmd + 3) & 0xff);
          }

        } else if (!strncasecmp_P(cmd, PSTR("a0v"), COMMAND_BUFFER_SIZE)) {
          Serial.println(analogRead(0));

        } else if (!strncasecmp_P(cmd, PSTR("status"), COMMAND_BUFFER_SIZE)) {
          static char onStr[]  PROGMEM_T = "ON";
          static char offStr[] PROGMEM_T = "off";
          char cFreq[8];
          for (uint8_t i = 0; i < (sizeof(Dpin) / sizeof(Dpin[0])); i++) {
            dtostrf(*(float *)(EEPROM.getConstDataPtr() + DIO_ENTRY_OFFSET + (i << 3) + DIO_FREQ), sizeof(cFreq) - 1,  2, cFreq);
            Serial.printf_P(PSTR("D%d(GPIO%-2d) mode%2x val%2x duty%3d freq%s trig%2d act %02x\n"),
              i, Dpin[i],
              EEPROM.read(DIO_ENTRY_OFFSET + (i << 3) + DIO_PINMODE),
              (EEPROM.read(DIO_ENTRY_OFFSET + (i << 3) + DIO_PINMODE) & 0xf9) ? EEPROM.read(DIO_ENTRY_OFFSET + (i << 3) + DIO_VALUE) : digitalRead(Dpin[i]),
              EEPROM.read(DIO_ENTRY_OFFSET + (i << 3) + DIO_DUTY),
              cFreq,
              inTrig[i] >> 1,
              EEPROM.read(DIO_ENTRY_OFFSET + (i << 3) + DIO_TRIG_ACTION));
          }
          Serial.printf_P(PSTR("wifi:%s%s web:%s ota:%s inet:%s failsafe:%s uptime:%dm\n"),
            String((EEPROM.read(FEATURE) & DISABLE_WIFI) ? FPSTR(offStr) : FPSTR(onStr)).c_str(), (WiFi.status() == WL_CONNECTED) ? (String(F("/")) + WiFi.localIP().toString()).c_str() : '\0',
            String((EEPROM.read(FEATURE) & ENABLE_WEB  ) ? FPSTR(onStr) : FPSTR(offStr)).c_str(),
            String((EEPROM.read(FEATURE) & ENABLE_OTA  ) ? FPSTR(onStr) : FPSTR(offStr)).c_str(),
            String((EEPROM.read(FEATURE) & ENABLE_INET ) ? FPSTR(onStr) : FPSTR(offStr)).c_str(),
            failSafeCheck ? String(F("RUNNING")).c_str() : String(F("stopped")).c_str(),
            millis() / 60000);

        } else if (!strncasecmp_P(cmd, PSTR("modelist"), COMMAND_BUFFER_SIZE)) {
          Serial.println(F("0 INPUT\n2 INPUT_PULLUP\n4 INPUT_PULLDOWN_16 // PULLDOWN only possible for pin16\n1 OUTPUT\n3 OUTPUT_OPEN_DRAIN"));
        } else if (!strncasecmp_P(cmd, PSTR("serial forever"), COMMAND_BUFFER_SIZE)) {
          serialDontClose = 1;

        } else if (!strncasecmp_P(cmd, PSTR("wifi on"), COMMAND_BUFFER_SIZE)) {
          EEPROM.write(FEATURE, (EEPROM.read(FEATURE) & (~DISABLE_WIFI)));
          EEPROM.commit();
          connectWiFi();
        } else if (!strncasecmp_P(cmd, PSTR("wifi off"), COMMAND_BUFFER_SIZE)) {
          EEPROM.write(FEATURE, (EEPROM.read(FEATURE) | DISABLE_WIFI));
          EEPROM.commit();
          WiFi.disconnect(); 
          WiFi.mode(WIFI_OFF);
          Serial.println(F("wifi disconnected."));

        } else if (!strncasecmp_P(cmd, PSTR("web on"), COMMAND_BUFFER_SIZE)) {
          EEPROM.write(FEATURE, (EEPROM.read(FEATURE) | ENABLE_WEB));
          EEPROM.commit();
          startWebServer();
        } else if (!strncasecmp_P(cmd, PSTR("web off"), COMMAND_BUFFER_SIZE)) {
          EEPROM.write(FEATURE, (EEPROM.read(FEATURE) & (~ENABLE_WEB)));
          EEPROM.commit();
          if (LLMNRr) {
            delete LLMNRr;
            LLMNRr = nullptr;
          }
          if (server) {
            server->stop();
            delete server;
            server = nullptr;
          }
          Serial.println(F("web stopped."));

        } else if (!strncasecmp_P(cmd, PSTR("ota on"), COMMAND_BUFFER_SIZE)) {
          EEPROM.write(FEATURE, (EEPROM.read(FEATURE) | ENABLE_OTA));
          EEPROM.commit();
          startOTA(true);
        } else if (!strncasecmp_P(cmd, PSTR("ota off"), COMMAND_BUFFER_SIZE)) {
          EEPROM.write(FEATURE, (EEPROM.read(FEATURE) & (~ENABLE_OTA)));
          EEPROM.commit();
          if (OTA) {
            delete OTA;
            OTA = nullptr;
          }
          Serial.println(F("ota stopped."));

        } else if (!strncasecmp_P(cmd, PSTR("inet on"), COMMAND_BUFFER_SIZE)) {
          enableInet();
        } else if (!strncasecmp_P(cmd, PSTR("inet off"), COMMAND_BUFFER_SIZE)) {
          disableInet();

        } else if (!strncmp_P(cmd, PSTR("?"), COMMAND_BUFFER_SIZE)) {
          Serial.println(F("d<pin>{m[0-4]|v[0|1]|d[0-100]|f[.0005-4000]|t[c]|a[0-255]}, a0v\nstatus, modelist, wifi|web|ota|inet {on|off}\nserial forever, ver, RESET, ERASEeeprom"));
        } else if (!strncasecmp_P(cmd, PSTR("ver"), COMMAND_BUFFER_SIZE)) {
          showVer();
        } else if (!strncmp_P(cmd, PSTR("RESET"), COMMAND_BUFFER_SIZE)) {
          EEPROM.write(FAILSAFE, 0x00);
          EEPROM.commit();
          Serial.println(F("restarting..."));
          delay(100);
          ESP.restart();
        } else if (!strncmp_P(cmd, PSTR("ERASEeeprom"), COMMAND_BUFFER_SIZE)) {
          eraseEEPROM(true, false);
          Serial.println(F("eeprom erase is done. please restart."));
        } else Serial.println(F("! invalid cmd."));
        if (syntaxError) Serial.println(F("! syntax error."));
      }
      Serial.write('#');
    }
    serialLastsec = millis() >> 10;
  }
}

void handlePinTrig() {
  for (uint8_t i = 0; i < (sizeof(Dpin) / sizeof(Dpin[0])); i++) {
    if (!(EEPROM.read(DIO_ENTRY_OFFSET + (i << 3) + DIO_PINMODE) & 0xf9)) {
      uint8_t val = digitalRead(Dpin[i]) & 0x01;
      if ((inTrig[i] & 0x01) != val) {
        uint8_t count = inTrig[i] >> 1;
        if (++count & 0x80) count = 0x7f;
        inTrig[i] = (count << 1) | val;
        if (EEPROM.read(DIO_ENTRY_OFFSET + (i << 3) + DIO_TRIG_ACTION) && count >= (0x01 << ((uint8_t)EEPROM.read(DIO_ENTRY_OFFSET + (i << 3) + DIO_TRIG_ACTION) >> 5))) {
          uint8_t pin = (EEPROM.read(DIO_ENTRY_OFFSET + (i << 3) + DIO_TRIG_ACTION) & 0x1e) >> 1;
          if (pin < sizeof(Dpin) / sizeof(Dpin[0])
          && EEPROM.read(DIO_ENTRY_OFFSET + (pin << 3) + DIO_PINMODE) & 0x01
          && EEPROM.read(DIO_ENTRY_OFFSET + (pin << 3) + DIO_VALUE) != (EEPROM.read(DIO_ENTRY_OFFSET + (i << 3) + DIO_TRIG_ACTION) & 0x01)) {
            setDpinVal(pin, EEPROM.read(DIO_ENTRY_OFFSET + (i << 3) + DIO_TRIG_ACTION) & 0x01);
          }
        }
      }
    }
  }
}

void delayWithHandle(uint16_t msec) {
  handleSerial();
  handlePinTrig();
  delay(msec);
}

bool setDpinMode(uint8_t pin, uint8_t modenum) {
  if (modenum > 4) return false;
  EEPROM.write(DIO_ENTRY_OFFSET + (pin << 3) + DIO_PINMODE, modenum);
  EEPROM.commit();
  pinMode(Dpin[pin], modenum);
  if (modenum & 0x01) {
    if (EEPROM.read(DIO_ENTRY_OFFSET + (pin << 3) + DIO_DUTY)) analogWrite3(Dpin[pin], EEPROM.read(DIO_ENTRY_OFFSET + (pin << 3) + DIO_DUTY), *(float *)(EEPROM.getConstDataPtr() + DIO_ENTRY_OFFSET + (pin << 3) + DIO_FREQ));
    else digitalWrite(Dpin[pin], EEPROM.read(DIO_ENTRY_OFFSET + (pin << 3) + DIO_VALUE));
  } else inTrig[pin] = digitalRead(Dpin[pin]) & 0x01;
  return true;
}

bool setDpinVal(uint8_t pin, uint8_t val) {
  if (val > 1) return false;
  EEPROM.write(DIO_ENTRY_OFFSET + (pin << 3) + DIO_VALUE, val);
  EEPROM.write(DIO_ENTRY_OFFSET + (pin << 3) + DIO_DUTY, 0x00);
  EEPROM.commit();
  if (EEPROM.read(DIO_ENTRY_OFFSET + (pin << 3) + DIO_PINMODE) & 0x01) digitalWrite(Dpin[pin], val);
  return true;
}

bool setDpinDuty(uint8_t pin, uint8_t duty) {
  if (duty > 100) return false;
  EEPROM.write(DIO_ENTRY_OFFSET + (pin << 3) + DIO_DUTY, duty);
  if (!duty) EEPROM.write(DIO_ENTRY_OFFSET + (pin << 3) + DIO_VALUE, 0x00);
  EEPROM.commit();
  if (EEPROM.read(DIO_ENTRY_OFFSET + (pin << 3) + DIO_PINMODE) & 0x01) {
    if (duty) analogWrite3(Dpin[pin], duty, *(float *)(EEPROM.getConstDataPtr() + DIO_ENTRY_OFFSET + (pin << 3) + DIO_FREQ));
    else digitalWrite(Dpin[pin], 0x00);
  }
  return true;
}

bool setDpinFreq(uint8_t pin, float freq) {
  if (freq <= 0) return false;
  for (uint8_t i = 0; i < 4; i++) EEPROM.write(DIO_ENTRY_OFFSET + (pin << 3) + DIO_FREQ + i, *((char *)(&freq) + i));
  EEPROM.commit();
  if (EEPROM.read(DIO_ENTRY_OFFSET + (pin << 3) + DIO_PINMODE) & 0x01) {
    if (EEPROM.read(DIO_ENTRY_OFFSET + (pin << 3) + DIO_DUTY)) analogWrite3(Dpin[pin], EEPROM.read(DIO_ENTRY_OFFSET + (pin << 3) + DIO_DUTY), freq);
    else digitalWrite(Dpin[pin], EEPROM.read(DIO_ENTRY_OFFSET + (pin << 3) + DIO_VALUE));
  }
  return true;
}

void setDpinAction(uint8_t pin, uint8_t action) {
  EEPROM.write(DIO_ENTRY_OFFSET + (pin << 3) + DIO_TRIG_ACTION, action);
  EEPROM.commit();
}

void enableInet() {
  EEPROM.write(FEATURE, (EEPROM.read(FEATURE) | ENABLE_INET));
  EEPROM.commit();
}

void disableInet() {
  EEPROM.write(FEATURE, (EEPROM.read(FEATURE) & (~ENABLE_INET)));
  EEPROM.commit();
}

void eraseEEPROM(bool withCommit, bool isInit) {
  for (uint8_t i = 0; i < EEPROM_SIZE; i++) EEPROM.write(i, isInit ? 0x0 : 0xff);
  EEPROM.write(FEATURE, (EEPROM.read(FEATURE) | ENABLE_WEB | ENABLE_INET | ENABLE_OTA));
  if (withCommit) EEPROM.commit();
}

void handleRoot() {
  static char rootHTML[] PROGMEM_T = R"(<!DOCTYPE html><html><head><title>esp8266-gpiom</title><style>
body{background-color:#dce;font-family:Arial,Helvetica,Sans-Serif;Color:#326;font-size:16px;}
button{background-color:#54a;border:none;color:white;padding:10px 24px;text-align:center;text-decoration:none;
 display:inline-block;font-size:12px;margin:4px 2px;cursor:pointer;border-radius:4px;box-shadow:0 4px #999;}
.admin{background-color:#933;padding:6px 14px;font-size:10px;}
button:active{box-shadow:0 1px #888;transform:translateY(3px);}
hr{border-style:dashed;color:#406;height:0px;}
</style></head><body><form action='' method='post'>
<h1>esp8266-gpiom</h1>
<a href='api' target='_blank'>api</a>
<hr>
<button name='inet' value='on'>enable inet</button> <button name='inet' value='off'>disable inet</button>
<button name='ota' value='begin'>&#x1f528;begin ota</button> <button name='svc' value='close'>close services</button>
<button name='eeprom' value='erase' class='admin'>erase eeprom</button> <button name='reset' value='go' class='admin'>restart</button>
</form></body></html>)";
  setCORS();
  if (server->method() == HTTP_POST) {
    if (server->arg(String(F("inet")))   == String(F("on")))    enableInet();
    if (server->arg(String(F("inet")))   == String(F("off")))   disableInet();
    if (server->arg(String(F("ota")))    == String(F("begin"))) startOTA(false);
    if (server->arg(String(F("svc")))    == String(F("close"))) { closeServices(); return; }
    if (server->arg(String(F("eeprom"))) == String(F("erase"))) eraseEEPROM(true, false);
    if (server->arg(String(F("reset")))  == String(F("go")))    {
      EEPROM.write(FAILSAFE, 0x00);
      EEPROM.commit();
      delay(100);
      ESP.restart();
    }
  }
  server->send_P(200, PSTR("text/html"), (PGM_P)rootHTML);
}

void failSafeOTA() {
  Serial.println(F("*** starting FAILSAFE ota"));
  connectWiFi();
  delay(1000);
  OTA = new ArduinoOTAClass();
  OTA->setHostname(String(F("esp8266-gpiom-failsafe")).c_str());
  OTA->begin();
  EEPROM.write(MAGIC_B1, 0x90); EEPROM.write(MAGIC_B2, 0x01); EEPROM.write(FAILSAFE, 0x00);
  EEPROM.commit();
  Serial.println(F("*** FAILSAFE ota started ***"));
  pinMode(2, OUTPUT);
  uint16_t otaLoopCount = 0;
  do {
    OTA->handle();
    digitalWrite(2, HIGH);
    delay(100);
    OTA->handle();
    digitalWrite(2, LOW);
    delay(100);
  } while (otaLoopCount++ < (FALESAFE_RESTART_SEC * 5)); // 200msec/loop
  Serial.println(F("*** Auto Restarting... ***"));
  delay(100);
  ESP.restart();
}

void analogWrite3(uint8_t pin, uint8_t duty, float freq) {
  if (pin > 16) return;
  float analogPeriod = 1000000.0 / ((freq < 0.00048828125) ? 0.00048828125 : ((freq > 4096.0) ? 4096.0 : freq));
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

void apiExec() {
  static char textPlain[] PROGMEM_T = "text/plain";
  setCORS();
  if (!server->args()) {
    server->send_P(200, (PGM_P)textPlain, PSTR("usage; d=[0-3],a=0,mode=[0-4],val=[0|1],duty=[0-100%],freq=[.0005-4000hz],action=[0-255],trig=0,status=1,ota=1,inet=[0|1],debug=1"));
    return;
  }
  static char aStr[]    PROGMEM_T = "a";
  static char modeStr[] PROGMEM_T = "mode";
  static char valStr[]  PROGMEM_T = "val";
  static char dutyStr[] PROGMEM_T = "duty";
  static char freqStr[] PROGMEM_T = "freq";
  static char inetStr[] PROGMEM_T = "inet";
  static char actStr[]  PROGMEM_T = "action";
  static char okStr[]   PROGMEM_T = "200: ok";
  static char format[]  PROGMEM_T = "%d,%d,%d,%s,%d,%02x"; // mode, val, duty, freq, trig, action
  if (atoi(server->arg(String(F("ota"))).c_str())) startOTA(false);
  if (server->arg(String(FPSTR(inetStr))).length()) { if (atoi(server->arg(String(FPSTR(inetStr))).c_str())) { enableInet(); } else { disableInet(); } }
  if (atoi(server->arg(String(F("debug"))).c_str()) > 0) {
    char *msg = (char *)malloc(WEB_MSG_BUFFER_SIZE);
    if (!msg) {
      server->send_P(503, (PGM_P)textPlain, PSTR("503: not enough memory"));
      return;
    }
    webDebugMsg(msg, WEB_MSG_BUFFER_SIZE);
    server->send_P(200, (PGM_P)textPlain, msg);
    free(msg);
    return;
  }
  bool statusOnly = atoi(server->arg(String(F("status"))).c_str()) > 0;
  if (server->arg(String(FPSTR(aStr))).length()) {
    if (server->arg(String(FPSTR(aStr))) == String(F("0")) && statusOnly) server->send_P(200, (PGM_P)textPlain, String(analogRead(0)).c_str());
    else server->send_P(200, (PGM_P)textPlain, (PGM_P)okStr);
    return;
  }
  uint8_t pin = atoi(server->arg(String(F("d"))).c_str()) & 0xff;
  if (pin >= sizeof(Dpin) / sizeof(Dpin[0])) {
    server->send_P(200, (PGM_P)textPlain, (PGM_P)okStr);
    return;
  }
  if (strlen(server->arg(String(F("trig"))).c_str()) > 0) inTrig[pin] &= 0x01;
  if (statusOnly) {
    char cFreq[8];
    dtostrf(*(float *)(EEPROM.getConstDataPtr() + DIO_ENTRY_OFFSET + (pin << 3) + DIO_FREQ), - (sizeof(cFreq) - 1),  2, cFreq);
    char *msg;
    uint8_t msg_size = strlen_P((PGM_P)format) + 8;
    msg = (char *)malloc(msg_size);
    if (!msg) {
      server->send_P(503, (PGM_P)textPlain, PSTR("503: not enough memory"));
      return;
    }
    snprintf_P(msg, msg_size, (PGM_P)format, 
      EEPROM.read(DIO_ENTRY_OFFSET + (pin << 3) + DIO_PINMODE),
      (EEPROM.read(DIO_ENTRY_OFFSET + (pin << 3) + DIO_PINMODE) & 0xf9) ? EEPROM.read(DIO_ENTRY_OFFSET + (pin << 3) + DIO_VALUE) : digitalRead(Dpin[pin]),
      EEPROM.read(DIO_ENTRY_OFFSET + (pin << 3) + DIO_DUTY),
      cFreq,
      inTrig[pin] >> 1,
      EEPROM.read(DIO_ENTRY_OFFSET + (pin << 3) + DIO_TRIG_ACTION));
    server->send_P(200, (PGM_P)textPlain, msg);
    free(msg);
    return;
  }
  if (server->arg(String(FPSTR(modeStr))).length()) setDpinMode(  pin, atoi(server->arg(String(FPSTR(modeStr))).c_str()) & 0xff);
  if (server->arg(String(FPSTR(valStr ))).length()) setDpinVal(   pin, atoi(server->arg(String(FPSTR(valStr ))).c_str()) & 0xff);
  if (server->arg(String(FPSTR(dutyStr))).length()) setDpinDuty(  pin, atoi(server->arg(String(FPSTR(dutyStr))).c_str()) & 0x7f);
  if (server->arg(String(FPSTR(freqStr))).length()) setDpinFreq(  pin, atof(server->arg(String(FPSTR(freqStr))).c_str()));
  if (server->arg(String(FPSTR(actStr ))).length()) setDpinAction(pin, atoi(server->arg(String(FPSTR(actStr ))).c_str()) & 0xff);
  server->send_P(200, (PGM_P)textPlain, (PGM_P)okStr);
}

void webDebugMsg(char *msg, uint16_t msgSize) {
  if (!msg) return;
  char cFreq[8], *pmsg;
  pmsg = msg;
  for (uint8_t i = 0; i < (sizeof(Dpin) / sizeof(Dpin[0])); i++) {
    dtostrf(*(float *)(EEPROM.getConstDataPtr() + DIO_ENTRY_OFFSET + (i << 3) + DIO_FREQ), - (sizeof(cFreq) - 1),  2, cFreq);
    snprintf_P(pmsg, msgSize - (pmsg - msg), PSTR("%d,%d,%d,%s,%d,%02x"), // mode, val, duty, freq, trig, action
      EEPROM.read(DIO_ENTRY_OFFSET + (i << 3) + DIO_PINMODE),
      (EEPROM.read(DIO_ENTRY_OFFSET + (i << 3) + DIO_PINMODE) & 0xf9) ? EEPROM.read(DIO_ENTRY_OFFSET + (i << 3) + DIO_VALUE) : digitalRead(Dpin[i]),
      EEPROM.read(DIO_ENTRY_OFFSET + (i << 3) + DIO_DUTY),
      cFreq,
      inTrig[i] >> 1,
      EEPROM.read(DIO_ENTRY_OFFSET + (i << 3) + DIO_TRIG_ACTION));
    pmsg += strlen(pmsg);
    if (msgSize - (pmsg - msg) > 0) *(pmsg++) = ';';
  }
  strncpy(pmsg, String(analogRead(0)).c_str(), msgSize - (pmsg - msg));
  msg[msgSize - 1] = '\0';
  pmsg += strlen(pmsg);
  if (msgSize - (pmsg - msg) > 0) *(pmsg++) = ';';
  snprintf_P(pmsg, msgSize - (pmsg - msg), PSTR("%c%c%c%c,%d"),
    (EEPROM.read(FEATURE) & ENABLE_OTA) ? 'O' : '_', failSafeCheck ? 'F' : '_', Serial ? 'S' : '_', (EEPROM.read(FEATURE) & ENABLE_INET) ? 'I' : '_', millis() / 1000);
}

void timer0_ISR() {
  timer0_write(ESP.getCycleCount() + (uint32_t)ESP.getCpuFreqMHz() * ((uint32_t)5 << 20)); // 5s
  if ((millis() >> 10) - myWDTsec > MY_WDT_TIMEOUT) ESP.restart();
}

void startWebServer() {
  if (server) return;
  Serial.print(F("starting web "));
  server = new ESP8266WebServer(80);
  server->on(String(F("/")),    handleRoot);
  server->on(String(F("/api")), apiExec);
  server->onNotFound(handleNotFound);
  server->begin();
  if (!LLMNRr) {
    LLMNRr = new LLMNRResponder();
    LLMNRr->begin(String(F(HOSTNAME)).c_str());
  }
  Serial.println(F(": started"));
}

void closeServices() {
  if (OTA) {
    delete OTA;
    OTA = nullptr;
  }
  if (LLMNRr) {
    delete LLMNRr;
    LLMNRr = nullptr;
  }
  if (server) {
    server->send_P(200, PSTR("text/plain"), PSTR("close services..."));
    delay(1000);
    serverClosing = 1;
  }
}

void connectWiFi() {
  Serial.print(F("connecting wifi "));
  WiFi.mode(WIFI_STA);
  WiFi.begin(String(F(WIFI_SSID)).c_str(), String(F(WIFI_PASS)).c_str());
  uint8_t iWiFiwait = 0;
  while (WiFi.status() != WL_CONNECTED) {
    if (++iWiFiwait > 20) ESP.restart();
    delay(1000);
  }
  Serial.println(F(": connected"));
}

void startOTA(bool serialPrint) {
  if (OTA) return;
  if (serialPrint) Serial.print(F("starting ota "));
  OTA = new ArduinoOTAClass();
  OTA->setHostname(String(F("esp8266-gpiom")).c_str());
  OTA->setPassword(String(F("gpiom")).c_str());
  OTA->onStart([] {
    timer0_detachInterrupt();
    for (uint8_t i = 0; i < (sizeof(Dpin) / sizeof(Dpin[0])); i++) stopWaveform(Dpin[i]);
    Serial.end();
  });
  OTA->onError([](ota_error_t e) {
    Serial.begin(BAUD_RATE);
    timer0_attachInterrupt(timer0_ISR);
  });
  OTA->onEnd([] { 
    EEPROM.write(FAILSAFE, 0x00);
    EEPROM.commit();
    delay(100);
  });
  OTA->begin();
  if (serialPrint) Serial.println(F(": started"));
}

void showVer() {
  Serial.print(F("gpiom: "));
  Serial.print(F(__DATE__));
  Serial.print(F(" "));
  Serial.println(F(__TIME__));
}

void handleNotFound() {
  if (server->method() == HTTP_OPTIONS) {
    setCORS();
    server->send(204);
  } else server->send(404);
}

void setCORS() {
  server->sendHeader(F("Access-Control-Allow-Origin"),  F("*"));
  server->sendHeader(F("Access-Control-Max-Age"),       F("7200"));
  server->sendHeader(F("Access-Control-Allow-Methods"), F("PUT,POST,GET,OPTIONS"));
  server->sendHeader(F("Access-Control-Allow-Headers"), F("*"));
}
