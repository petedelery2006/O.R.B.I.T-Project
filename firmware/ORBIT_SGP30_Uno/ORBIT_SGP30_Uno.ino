/* ORBIT - Uno R3 / SGP30 / 16x2 I2C LCD / SSD1306 128x64 OLED
   SDA=A4, SCL=A5. LCD switch is on MEGA D23 to GND, received over serial.
   Uno D10=RX, D11=TX, 9600 baud. D5 is no longer used.
   Retains last received LCD view if Mega disconnects; defaults to bar at boot.
   Reports SGP30 readings and I2C idle-line diagnostics to the Mega.
   No Mega firmware change is required for switch control or ACK status.
   Addresses: LCD 0x27, OLED 0x3C, SGP30 0x58.
   Libraries: Adafruit SGP30 (+ dependencies), LiquidCrystal I2C
   by Frank de Brabander, U8g2 by oliver.
   U8x8 text mode avoids a full OLED framebuffer on the Uno.
   eCO2 is an estimate, not a direct CO2 measurement.
   OLED shows a custom TVOC-based status, then raw H2 and raw ethanol.
   Labels are a rough VOC indicator, NOT a health/safety assessment.
   Custom bands: Healthy <200 ppb; Mediocre 200-499; Unhealthy >=500.
   These are UI thresholds, not official AQI or exposure limits.
   Switch OFF: relative TVOC load bar, 0-1000 ppb. ON: LCD eCO2/TVOC.
   Changed-character writes prevent blanking/flicker during updates.
   The startup display lasts 5 seconds; sensor conditioning continues separately.
   The first 16 seconds are marked warming, not Healthy or calibrated.
*/
#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include <SoftwareSerial.h>
#include <avr/pgmspace.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_SGP30.h>
#include <U8x8lib.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);
U8X8_SSD1306_128X64_NONAME_HW_I2C oled(U8X8_PIN_NONE);
Adafruit_SGP30 sgp;

const unsigned long STARTUP_MS = 5000UL;
const unsigned long SENSOR_WARMUP_MS = 16000UL;
bool sensorOK = false, readingOK = false, oledOK = false;
bool switchState = HIGH;
unsigned long startedAt=0, lastSample=0, lastDraw=0;
uint16_t tvoc = 0, eco2 = 400, rawH2 = 0, rawEthanol = 0;
bool rawOK = false;
char lcdCache[2][16] = {};
char oledCache[4][16] = {};
// Inverse TVOC bar: more filled blocks means fewer detected VOCs.
// This is not an official AQI or a safety assessment.
const uint16_t BAR_MAX_PPB = 1000;
// Adjustable UI thresholds; SGP30 does not determine whether air is safe.
const uint16_t TVOC_MEDIOCRE_PPB = 200;
const uint16_t TVOC_UNHEALTHY_PPB = 500;
const char* airQualityStatus() {
  if(tvoc >= TVOC_UNHEALTHY_PPB) return "Unhealthy";
  if(tvoc >= TVOC_MEDIOCRE_PPB) return "Mediocre";
  return "Healthy";
}
byte fullBlock[8] = {31,31,31,31,31,31,31,31};

// Mega link: Uno D10 RX <- Mega D18 TX1 (or D14 TX3).
//            Uno D11 TX -> Mega D19 RX1 (or D15 RX3); common GND.
// Do NOT join the boards' I2C buses. The Uno's sensor/display bus stays A4/A5.
const byte MEGA_RX=10, MEGA_TX=11;
SoftwareSerial megaLink(MEGA_RX,MEGA_TX);
char command[20]={0};
byte commandLength=0;
bool discardCommand=false,replyPending=false,displayDirty=false;
bool sampleActive=false,haveMega=false,busTimeoutSeen=false;
unsigned long lastRxByte=0,lastMegaCommand=0,lastReply=0;
uint16_t rxOverflows=0,badCommands=0;

// Version 1 protocol, 9600 baud, ASCII lines terminated with '\n':
// Receive SW=0 (bar view), SW=1 (numeric view). Mega debounces D23.
// Receive SAMPLE=0/1 (Mega fan state only; SGP30 still measures each second).
// Reply after input has been quiet for 25 ms, at most once per 200 ms:
// ACK
// SGP,1,initOK,iaqOK,rawOK,warming,eco2ppm,tvocppb,rawH2,rawEthanol
// BUS,1,sdaIdle,sclIdle,timeoutEver,rxOverflows,badCommands
// CTRL,1,switchOn,sampleActive
// -1 in a measurement field means INVALID, never an old reading passed as fresh.
// BUS levels are shared-I2C IDLE snapshots, NOT decoded SDA/SCL traffic.
// HIGH/HIGH alone does not prove the sensor is healthy; check SGP flags.
// timeoutEver latches until reset; counters saturate at 65535.
// Current Mega understands ACK; future firmware can parse the other lines.
// Reply-only scheduling avoids continuous TX on half-duplex SoftwareSerial.
// Mega repeats state each second, recovering a command lost during a reply.
void incrementCounter(uint16_t &value) {if(value<65535U) ++value;}

void acceptMegaCommand() {
  command[commandLength]=0;
  bool recognized=true;
  if(strcmp(command,"SW=0")==0 || strcmp(command,"SW=1")==0) {
    // Preserve the existing LCD code's electrical convention: LOW = ON.
    bool newState=command[3]=='1'?LOW:HIGH;
    if(switchState!=newState) {switchState=newState;displayDirty=true;}
  } else if(strcmp(command,"SAMPLE=0")==0 || strcmp(command,"SAMPLE=1")==0) {
    sampleActive=command[7]=='1';
  } else recognized=false;
  if(recognized) {haveMega=true;lastMegaCommand=millis();replyPending=true;}
  else if(commandLength) incrementCounter(badCommands);
}
void receiveMega() {
  unsigned long now=millis();
  if(megaLink.overflow()) {
    incrementCounter(rxOverflows);commandLength=0;discardCommand=true;
  }
  if(commandLength && now-lastRxByte>150UL) {
    commandLength=0;discardCommand=true;incrementCounter(badCommands);
  }
  byte budget=64;
  while(budget-- && megaLink.available()) {
    char ch=megaLink.read();lastRxByte=millis();
    if(ch=='\r') continue;
    if(ch=='\n') {
      if(!discardCommand) acceptMegaCommand();
      commandLength=0;discardCommand=false;
    } else if(!discardCommand) {
      if(commandLength<sizeof(command)-1) command[commandLength++]=ch;
      else {commandLength=0;discardCommand=true;incrementCounter(badCommands);}
    }
  }
}
void sendDiagnostics() {
#if defined(WIRE_HAS_TIMEOUT)
  if(Wire.getWireTimeoutFlag()) {busTimeoutSeen=true;Wire.clearWireTimeoutFlag();}
#endif
  unsigned long now=millis();
  if(!replyPending || megaLink.available() || now-lastRxByte<25UL || now-lastReply<200UL) return;
  // No I2C transaction is active here. Read levels without changing pin modes.
  byte sdaIdle=digitalRead(A4),sclIdle=digitalRead(A5);
  bool iaqValid=sensorOK && readingOK && now-lastSample<2500UL;
  bool rawValid=sensorOK && rawOK && now-lastSample<2500UL;
  char line[72];
  megaLink.print(F("ACK\n"));
  snprintf_P(line,sizeof(line),PSTR("SGP,1,%u,%u,%u,%u,%ld,%ld,%ld,%ld\n"),
    (unsigned)sensorOK,(unsigned)iaqValid,(unsigned)rawValid,
    (unsigned)(now-startedAt<SENSOR_WARMUP_MS),
    iaqValid?(long)eco2:-1L,iaqValid?(long)tvoc:-1L,
    rawValid?(long)rawH2:-1L,rawValid?(long)rawEthanol:-1L);
  megaLink.print(line);
  snprintf_P(line,sizeof(line),PSTR("BUS,1,%u,%u,%u,%u,%u\n"),
    (unsigned)sdaIdle,(unsigned)sclIdle,(unsigned)busTimeoutSeen,
    (unsigned)rxOverflows,(unsigned)badCommands);
  megaLink.print(line);
  snprintf_P(line,sizeof(line),PSTR("CTRL,1,%u,%u\n"),
    (unsigned)(switchState==LOW),(unsigned)sampleActive);
  megaLink.print(line);
  replyPending=false;lastReply=millis();
}


bool responds(byte address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

// Write only characters that changed; never erase a row before repainting it.
// Padding removes old trailing digits when a reading gets shorter.
void lcdRow(byte row, const char *text) {
  bool ended = false;
  for (byte col = 0; col < 16; ++col) {
    char ch = ' ';
    if (!ended) {
      if (*text) ch = *text++;
      else ended = true;
    }
    if (lcdCache[row][col] != ch) {
      lcd.setCursor(col, row);
      lcd.write((uint8_t)ch);
      lcdCache[row][col] = ch;
    }
  }
}

void oledRow(byte slot, const char *text) {
  if (!oledOK) return;
  bool ended = false;
  for (byte col = 0; col < 16; ++col) {
    char ch = ' ';
    if (!ended) {
      if (*text) ch = *text++;
      else ended = true;
    }
    if (oledCache[slot][col] != ch) {
      oled.setCursor(col, slot * 2);
      oled.write((uint8_t)ch);
      oledCache[slot][col] = ch;
    }
  }
}

void startup(unsigned long elapsed) {
  unsigned long remaining=elapsed<STARTUP_MS?(STARTUP_MS-elapsed+999UL)/1000UL:0;
  char line[17];
  snprintf_P(line,sizeof(line),PSTR("Starting in %lus"),remaining);
  lcdRow(0,"O.R.B.I.Tstartup");
  lcdRow(1,line);
  oledRow(0,"O.R.B.I.Tstartup");
  oledRow(1,line);
  oledRow(2,"");
  oledRow(3,"");
}

void showReadings() {
  char line[17];
  if (!sensorOK || !readingOK) {
    lcdRow(0, "SGP30 ERROR");
    lcdRow(1, sensorOK ? "Read failed" : "Check / reset");
    oledRow(0, "Air (TVOC est.)");
    oledRow(1, "Sensor error");
  } else if(millis()-startedAt<SENSOR_WARMUP_MS) {
    lcdRow(0,"SGP30 warming up");
    unsigned long left=(SENSOR_WARMUP_MS-(millis()-startedAt)+999UL)/1000UL;
    snprintf_P(line,sizeof(line),PSTR("Wait %lus"),left);
    lcdRow(1,line);
    oledRow(0,"Air (TVOC est.)");oledRow(1,"Warming up");
  } else {
    if (switchState == HIGH) {
      // Fourteen segments: full at 0 ppb, empty at 1000 ppb and above.
      lcdRow(0, "Air Quality(TVOC)");
      byte blocks = tvoc >= BAR_MAX_PPB ? 0 : 14 - uint32_t(tvoc) * 14UL / BAR_MAX_PPB;
      line[0] = '[';
      for (byte i = 0; i < 14; ++i) line[i+1] = i < blocks ? char(1) : ' ';
      line[15] = ']'; line[16] = '\0';
      lcdRow(1, line);
    } else {
      snprintf_P(line, sizeof(line), PSTR("eCO2:%5u ppm"), (unsigned int)eco2);
      lcdRow(0, line);
      snprintf_P(line, sizeof(line), PSTR("TVOC:%5u ppb"), (unsigned int)tvoc);
      lcdRow(1, line);
    }
    oledRow(0, "Air (TVOC est.)");
    oledRow(1, airQualityStatus());
  }
  // Raw signals are unitless sensor outputs, not gas concentrations.
  if (rawOK) {
    snprintf_P(line, sizeof(line), PSTR("H2 raw: %5u"), (unsigned int)rawH2);
    oledRow(2, line);
    snprintf_P(line, sizeof(line), PSTR("EtOH raw:%5u"), (unsigned int)rawEthanol);
    oledRow(3, line);
  } else {
    oledRow(2, "H2 raw: --");
    oledRow(3, "EtOH raw: --");
  }
}

void setup() {
  Serial.begin(115200);
  megaLink.begin(9600);
  megaLink.listen();
  Wire.begin();
  Wire.setClock(100000);
#if defined(WIRE_HAS_TIMEOUT)
  Wire.setWireTimeout(25000, true);
#endif
  lcd.init(); lcd.backlight(); lcd.createChar(1, fullBlock);
  oledOK = responds(0x3C);
  if (oledOK) {
    oled.setI2CAddress(0x3C << 1);
    oled.setBusClock(100000);
    oled.begin();
    oled.setFont(u8x8_font_chroma48medium8_r);
    oled.clearDisplay();
  }
  startup(0);
  Serial.println(F("ORBIT SGP30 UNO - A4/A5 I2C; D10 RX/D11 TX; switch from Mega"));
  sensorOK = sgp.begin();
  Serial.println(sensorOK ? F("SGP30 initialized") : F("SGP30 init FAILED: check wiring/power, then reset"));
  startedAt = millis();
  lastSample = startedAt;
  lastDraw = startedAt;
  // Commands buffered during initialization are handled by loop().
}

void loop() {
  unsigned long now = millis();
  receiveMega();
  sendDiagnostics();
  now=millis();
  bool changed=displayDirty;displayDirty=false;
  // Keep measuring every second, including during the startup animation.
  if (sensorOK && now - lastSample >= 1000UL) {
    lastSample = now;
    readingOK = sgp.IAQmeasure();
    if (readingOK) {
      tvoc = sgp.TVOC; eco2 = sgp.eCO2;
      Serial.print(F("TVOC: ")); Serial.print(tvoc);
      Serial.print(F(" ppb | eCO2: ")); Serial.print(eco2); Serial.println(F(" ppm"));
    } else Serial.println(F("SGP30 measurement FAILED"));
    rawOK = sgp.IAQmeasureRaw();
    if (rawOK) {
      rawH2 = sgp.rawH2;
      rawEthanol = sgp.rawEthanol;
    } else Serial.println(F("SGP30 raw measurement FAILED"));
  }
  receiveMega();
  if(displayDirty) {changed=true;displayDirty=false;}
  sendDiagnostics();
  now=millis();
  static bool startupFinished = false;
  if (now - startedAt < STARTUP_MS) {
    if (now - lastDraw >= 200UL) { lastDraw = now; startup(now - startedAt); }
    return;
  }
  if (!startupFinished) {
    startupFinished = true; changed = true;
  }
  if (changed || now - lastDraw >= 1000UL) { lastDraw = now; showReadings(); }
}
