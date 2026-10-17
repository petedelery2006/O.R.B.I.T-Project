/*
  ORBIT: Uno R3 + GY-511 LSM303DLHC + 16x2 LCD + AS7341; serial to Mega.
  All I2C devices: SDA -> A4, SCL -> A5, common ground.
  Preserve the appropriate power connections for your breakout boards.
  Maintained switch is Mega D23 to GND; received on Uno D10 RX / D11 TX.
  No switch or OLED is connected to this Uno. Sensor I2C stays on A4/A5.
  OFF/open: E/M magnetic bars. ON/closed: X/Y/Z in microteslas (uT).

  Install: hd44780 by Bill Perry; Adafruit LSM303DLHC (legacy library);
  Adafruit Unified Sensor; Adafruit AS7341 (+ Adafruit BusIO).
  Install dependencies when offered. SoftwareSerial is built into AVR boards.
  The magnetometer library header is Adafruit_LSM303_U.h.
  Select Arduino Uno. Serial Monitor: 9600 baud.

  During the 5-second startup, keep the box still, away from magnets.
  E = fixed local startup reference (4 of 14 cells).
  M = smoothed field magnitude relative to that reference, 2 uT per cell.
  This reference includes nearby interference; it does not isolate Earth's
  field or perform full hard/soft-iron calibration. Reset to rebaseline.
  XYZ shows the latest unsmoothed axes, at 0.1 uT display resolution.
  Spectrometer sends all 12 raw channel values to the Mega at fixed 16x gain.
  Flicker checks recognize 100/120 Hz, not arbitrary flashing frequencies.
  Both Unos share SW=0 (bars), SW=1 (numeric) from the current Mega sketch.
  Missing link: retain last LCD mode; boot default is bars.
  Keep still for the initial 5-second magnetic reference; reset to rebaseline.
  Hardware testing is still required.
*/
#include <Arduino.h>
#include <SoftwareSerial.h>
#include <stdio.h>
#include <string.h>
#include <Wire.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_LSM303_U.h>
#include <math.h>
#include <Adafruit_AS7341.h>
#include <avr/pgmspace.h>

const uint8_t LCD_ADDRESS = 0x27;
const unsigned long STARTUP_MS = 5000UL;
const float UT_PER_CELL = 2.0f; // Smaller positive value = more bar movement.
const float SMOOTHING = 0.35f;
hd44780_I2Cexp lcd(LCD_ADDRESS);
Adafruit_LSM303_Mag_Unified mag(12345);

bool lcdReady = false, magReady = false, valid = false;
bool baselineReady = false, stableSwitch = HIGH;
bool redraw = true, cacheReady = false;
unsigned long startupAt = 0, sampledAt = 0;
unsigned long lastGood = 0, retryAt = 0, renderedAt = 0;
uint16_t baselineCount = 0;
float baselineSum = 0, baseline = 0, filteredMagnitude = 0;
float axisX = 0, axisY = 0, axisZ = 0, magnitude = 0;
uint8_t previous[2][16];

bool present(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}


// AS7341 spectral and flicker acquisition; no local OLED.
Adafruit_AS7341 spectrum;
bool spectralReady=false,spectralBusy=false,spectralValid=false;
unsigned long spectralStarted=0,spectralSampled=0,spectralGoodAt=0;
uint16_t spectralData[12]={};
unsigned long flickerAt=0,flickerGoodAt=0;
uint8_t flickerPhase=0;
uint16_t flickerHz=0;
bool flickerValid=false;
// Uno D10 RX <- Mega D14 TX3; Uno D11 TX -> Mega D15 RX3; common GND.
// The existing Mega broadcasts identical state on Serial1 and Serial3.
// Sensor/LCD I2C stays LOCAL to this Uno on A4/A5.
SoftwareSerial megaLink(10,11);
char command[20]={0};
byte commandLength=0;
bool discardCommand=false,replyPending=false,sampleActive=false;
bool busTimeoutSeen=false;
unsigned long lastRxByte=0,lastReply=0;
uint16_t rxOverflows=0,badCommands=0;

// 9600 baud; newline-delimited protocol version 1.
// RX: SW=0 -> bars; SW=1 -> XYZ. SAMPLE=0/1 records Mega fan state.
// TX: ACK (recognized by the current Mega), followed by:
// MAG,1,ready,valid,baselineReady,x10,y10,z10,magnitude10,baseline10
//     Units = tenths of a microtesla; invalid fields = -32768.
// SP1,1,ready,valid,F1,F2,F3,F4,clearLow,NIRlow
// SP2,1,ready,valid,F5,F6,F7,F8,clearHigh,NIRhigh
//     All AS7341 values are raw counts, NOT lux or calibrated irradiance.
//     Channel-group integration: ATIME=29, ASTEP=599, gain=16x.
//     Low and high channel groups are measured sequentially, not simultaneously.
//     Invalid channel fields = -1; validity includes a 2.5-second age limit.
// FLK,1,valid,hz   (hz=0 none detected; 1 unknown frequency; 100 or 120 Hz)
// BUS,1,sdaIdle,sclIdle,timeoutEver,rxOverflows,badCommands
// CTRL,1,switchOn,sampleActive
// Idle line levels are HIGH/LOW snapshots, not a capture of I2C traffic.
// timeoutEver is sticky until reset. Link-error counters saturate at 65535.
// Replies occur only after a Mega command burst, with a 25 ms quiet gap.
// Mega's 1-second repeated state recovers updates lost during SoftwareSerial TX.
// Future Mega firmware can parse all telemetry; today's Mega only parses ACK.
void incrementCounter(uint16_t &v) {if(v<65535U) ++v;}
void captureBusTimeout() {
  if(Wire.getWireTimeoutFlag()) busTimeoutSeen=true;
}
void clearBusTimeout() {
  captureBusTimeout();Wire.clearWireTimeoutFlag();
}
void acceptMegaCommand() {
  command[commandLength]=0;
  bool recognized=true;
  if(strcmp(command,"SW=0")==0 || strcmp(command,"SW=1")==0) {
    bool state=command[3]=='1'?LOW:HIGH;
    if(stableSwitch!=state) {stableSwitch=state;redraw=true;}
  } else if(strcmp(command,"SAMPLE=0")==0 || strcmp(command,"SAMPLE=1")==0) {
    sampleActive=command[7]=='1';
  } else recognized=false;
  if(recognized) replyPending=true;
  else if(commandLength) incrementCounter(badCommands);
}
void receiveMega() {
  if(megaLink.overflow()) {incrementCounter(rxOverflows);commandLength=0;discardCommand=true;}
  if(commandLength && millis()-lastRxByte>150UL) {
    commandLength=0;discardCommand=true;incrementCounter(badCommands);
  }
  byte budget=64;
  while(budget-- && megaLink.available()) {
    char c=megaLink.read();lastRxByte=millis();
    if(c=='\r') continue;
    if(c=='\n') {
      if(!discardCommand) acceptMegaCommand();
      commandLength=0;discardCommand=false;
    } else if(!discardCommand) {
      if(commandLength<sizeof(command)-1) command[commandLength++]=c;
      else {commandLength=0;discardCommand=true;incrementCounter(badCommands);}
    }
  }
}
long tenths(float value) {return lroundf(value*10.0f);}
void sendSpectrumGroup(byte group,bool ok) {
  megaLink.print(group==0?F("SP1,1,"):F("SP2,1,"));
  megaLink.print(spectralReady?1:0);megaLink.print(',');megaLink.print(ok?1:0);
  for(byte i=0;i<6;i++) {
    megaLink.print(',');
    if(ok) megaLink.print(spectralData[group*6+i]);else megaLink.print(-1);
  }
  megaLink.print('\n');
}
void sendDiagnostics() {
  captureBusTimeout();
  unsigned long now=millis();
  if(!replyPending || megaLink.available() || now-lastRxByte<25UL || now-lastReply<400UL) return;
  bool magValid=magReady && valid && now-lastGood<1500UL;
  bool specValid=spectralReady && spectralValid && now-spectralGoodAt<2500UL;
  byte sda=digitalRead(A4),scl=digitalRead(A5);
  char line[80];
  megaLink.print(F("ACK\n"));
  snprintf_P(line,sizeof(line),PSTR("MAG,1,%u,%u,%u,%ld,%ld,%ld,%ld,%ld\n"),
    (unsigned)magReady,(unsigned)magValid,(unsigned)baselineReady,
    magValid?tenths(axisX):-32768L,magValid?tenths(axisY):-32768L,
    magValid?tenths(axisZ):-32768L,magValid?tenths(magnitude):-32768L,
    baselineReady?tenths(baseline):-32768L);
  megaLink.print(line);
  sendSpectrumGroup(0,specValid);sendSpectrumGroup(1,specValid);
  bool flickerFresh=flickerValid && now-flickerGoodAt<10000UL;
  snprintf_P(line,sizeof(line),PSTR("FLK,1,%u,%d\n"),
    (unsigned)flickerFresh,flickerFresh?(int)flickerHz:-1);megaLink.print(line);
  snprintf_P(line,sizeof(line),PSTR("BUS,1,%u,%u,%u,%u,%u\n"),
    (unsigned)sda,(unsigned)scl,(unsigned)busTimeoutSeen,
    (unsigned)rxOverflows,(unsigned)badCommands);megaLink.print(line);
  snprintf_P(line,sizeof(line),PSTR("CTRL,1,%u,%u\n"),
    (unsigned)(stableSwitch==LOW),(unsigned)sampleActive);megaLink.print(line);
  replyPending=false;lastReply=millis();
}

void startSpectrum() {
  spectralReady=present(0x39) && spectrum.begin();
  if(spectralReady) {
    spectralReady=spectrum.setATIME(29) && spectrum.setASTEP(599)
      && spectrum.setGain(AS7341_GAIN_16X);
    spectrum.enableLED(false);
  }
  Wire.setClock(100000UL);Wire.setWireTimeout(25000UL,true);
  Serial.println(spectralReady?F("AS7341 ready"):F("AS7341 init failed; check and reset"));
}

// Nonblocking version of the AS7341 flicker sequence. Spectral and flicker
// measurements share the SMUX and must never run at the same time.
bool spectralWrite(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(0x39); Wire.write(reg); Wire.write(value);
  return Wire.endTransmission() == 0;
}
bool spectralRead(uint8_t reg, uint8_t &value) {
  Wire.beginTransmission(0x39); Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)0x39, (uint8_t)1) != 1) return false;
  value = Wire.read(); return true;
}
void endFlicker(bool ok) {
  bool stopped = spectralWrite(AS7341_ENABLE, 0x01);
  flickerPhase = 0; flickerValid = ok && stopped;
  if (flickerValid) flickerGoodAt = millis();
  flickerAt = millis();
  // startReading() restores the spectral SMUX on the next pass.
}
void beginFlicker() {
  flickerValid = false;
  bool ok = spectrum.setBank(false);
  ok = ok && spectralWrite(AS7341_ENABLE, 0x01);
  ok = ok && spectralWrite(AS7341_CFG6, 0x10);
  for (uint8_t i=0; i<20 && ok; ++i)
    ok = spectralWrite(i, i == 19 ? 0x60 : 0x00);
  ok = ok && spectralWrite(AS7341_ENABLE, 0x11); // Power + SMUX command
  if (!ok) { endFlicker(false); return; }
  flickerPhase = 1; flickerAt = millis();
}
void pollFlicker() {
  uint8_t value;
  if (flickerPhase == 1) {
    if (!spectralRead(AS7341_ENABLE, value)) { endFlicker(false); return; }
    if (!(value & 0x10)) {
      // Clear latched status before collecting a fresh result.
      if (!spectralWrite(AS7341_FD_STATUS, 0x3C) ||
          !spectralWrite(AS7341_ENABLE, 0x41)) { endFlicker(false); return; }
      flickerPhase = 2; flickerAt = millis();
    } else if (millis() - flickerAt > 100UL) endFlicker(false);
  } else if (flickerPhase == 2 && millis() - flickerAt >= 500UL) {
    if (!spectralRead(AS7341_FD_STATUS, value)) { endFlicker(false); return; }
    // Same 100/120 Hz result codes as Adafruit detectFlickerHz().
    if (value == 45) flickerHz = 100;
    else if (value == 46) flickerHz = 120;
    else if (value == 44) flickerHz = 1; // Detected, frequency unknown
    else flickerHz = 0;
    endFlicker(true);
  }
}
void pollSpectrum() {
  unsigned long now = millis();
  if (spectralReady) {
    if (flickerPhase) pollFlicker();
    else if (spectralBusy) {
      clearBusTimeout();
      bool complete = spectrum.checkReadingProgress();
      if (Wire.getWireTimeoutFlag()) { spectralValid=false; spectralBusy=false; }
      else if (complete) {
        spectralValid = spectrum.getAllChannels(spectralData) && present(0x39)
          && !Wire.getWireTimeoutFlag();
        if(spectralValid) spectralGoodAt=millis();
        spectralBusy = false;
      } else if (millis() - spectralStarted > 1500UL) {
        spectralBusy = false; spectralValid = false;
        Serial.println(F("AS7341 read timeout"));
      }
    } else if (spectralValid && millis()-flickerAt >= 3000UL) beginFlicker();
    else if (millis() - spectralSampled >= 500UL) {
      spectralSampled = millis();
      spectralBusy = spectrum.startReading(); spectralStarted = millis();
      if (!spectralBusy) spectralValid = false;
    }
  }

}

void textRow(uint8_t *destination, const char *text) {
  memset(destination, ' ', 16);
  size_t n = strlen(text);
  if (n > 16) n = 16;
  memcpy(destination, text, n);
}

void flushFrame(uint8_t frame[2][16]) {
  if (!lcdReady) return;
  for (uint8_t row = 0; row < 2; ++row) {
    if (!cacheReady || memcmp(previous[row], frame[row], 16) != 0) {
      lcd.setCursor(0, row);
      for (uint8_t col = 0; col < 16; ++col) lcd.write(frame[row][col]);
      memcpy(previous[row], frame[row], 16);
    }
  }
  cacheReady = true;
}

void startLCD() {
  Serial.println(F("Starting LCD at 0x27")); Serial.flush();
  lcdReady = present(LCD_ADDRESS) && lcd.begin(16, 2) == 0;
  Wire.setWireTimeout(25000UL, true);
  if (!lcdReady) { Serial.println(F("LCD initialization failed")); return; }
  lcd.backlight(); lcd.display(); lcd.clear();
  for (uint8_t width = 1; width <= 5; ++width) {
    uint8_t glyph[8];
    for (uint8_t row = 0; row < 8; ++row)
      glyph[row] = (uint8_t)((0x1F << (5 - width)) & 0x1F);
    lcd.createChar(width, glyph);
  }
  cacheReady = false;
  uint8_t frame[2][16];
  textRow(frame[0], "O.R.B.I.Tstartup");
  textRow(frame[1], "Starting in 5s");
  flushFrame(frame);
}

void startMagnet() {
  Serial.println(F("Starting GY-511 magnetometer at 0x1E")); Serial.flush();
  clearBusTimeout();
  magReady = present(0x1E) && mag.begin();
  if (Wire.getWireTimeoutFlag()) magReady = false;
  Wire.setClock(100000UL); Wire.setWireTimeout(25000UL, true);
  if (magReady) {
    mag.enableAutoRange(false);
    mag.setMagGain(LSM303_MAGGAIN_1_3);
    startupAt = millis();
    baselineCount = 0; baselineSum = 0;
    baselineReady = false; valid = false;
    Serial.println(F("Magnetometer ready. Hold still for startup reference."));
  } else Serial.println(F("Magnetometer initialization failed"));
}

bool readMagnet() {
  clearBusTimeout();
  if (!present(0x1E)) return false;
  // Checked six-byte read avoids the legacy driver's unbounded wait on faults.
  // LSM303DLHC magnet registers are big-endian X, Z, Y, starting at 0x03.
  Wire.beginTransmission(0x1E);Wire.write((uint8_t)0x03);
  if(Wire.endTransmission(false)!=0) return false;
  if(Wire.requestFrom((uint8_t)0x1E,(uint8_t)6)!=6) {
    while(Wire.available()) Wire.read();
    return false;
  }
  uint8_t b[6];for(byte i=0;i<6;i++) b[i]=Wire.read();
  int16_t rx=(int16_t)(((uint16_t)b[0]<<8)|b[1]);
  int16_t rz=(int16_t)(((uint16_t)b[2]<<8)|b[3]);
  int16_t ry=(int16_t)(((uint16_t)b[4]<<8)|b[5]);
  if(Wire.getWireTimeoutFlag() || rx==-4096 || ry==-4096 || rz==-4096) return false;
  // Same 1.3-gauss sensitivity as the legacy library: 1100 XY / 980 Z LSB/G.
  float x=rx*(100.0f/1100.0f),y=ry*(100.0f/1100.0f),z=rz*(100.0f/980.0f);
  if(!isfinite(x) || !isfinite(y) || !isfinite(z) ||
     fabsf(x)>300.0f || fabsf(y)>300.0f || fabsf(z)>300.0f) return false;
  axisX = x; axisY = y; axisZ = z;
  magnitude = sqrtf(x*x + y*y + z*z);
  return isfinite(magnitude);
}

void makeBar(uint8_t *row, char label, float cells) {
  memset(row, ' ', 16); row[0] = label; row[1] = ':';
  int pixels = (int)(constrain(cells, 0.0f, 14.0f) * 5.0f + 0.5f);
  for (uint8_t i = 0; i < 14; ++i) {
    int fill = constrain(pixels - i*5, 0, 5);
    row[i + 2] = fill ? (uint8_t)fill : (uint8_t)' ';
  }
}

void render() {
  uint8_t frame[2][16];
  char line[17];
  if (!magReady || !valid || millis() - lastGood > 1500UL) {
    textRow(frame[0], "Mag read/range!");
    textRow(frame[1], "See Serial 9600");
  } else if (!baselineReady) {
    unsigned long elapsed = millis() - startupAt;
    unsigned long remaining = elapsed < STARTUP_MS ? (STARTUP_MS - elapsed + 999UL)/1000UL : 0;
    textRow(frame[0], "O.R.B.I.Tstartup");
    if(remaining) snprintf(line, sizeof(line), "Starting in %lus", remaining);
    else snprintf(line, sizeof(line), "Collecting ref.");
    textRow(frame[1], line);
  } else if (stableSwitch == LOW) {
    char x[10], y[10], z[10];
    dtostrf(axisX, 6, 1, x); dtostrf(axisY, 6, 1, y); dtostrf(axisZ, 6, 1, z);
    snprintf(line, sizeof(line), "X%s Y%s", x, y); textRow(frame[0], line);
    snprintf(line, sizeof(line), "Z%s uT", z); textRow(frame[1], line);
  } else {
    makeBar(frame[0], 'E', 4.0f);
    makeBar(frame[1], 'M', 4.0f + (filteredMagnitude - baseline)/UT_PER_CELL);
  }
  flushFrame(frame);
}

void setup() {
  Serial.begin(9600);
  megaLink.begin(9600);megaLink.listen();
  Wire.begin(); Wire.setClock(100000UL); Wire.setWireTimeout(25000UL, true);
  delay(100);
  Serial.println(F("ORBIT UNO MAG+AS7341: A4/A5 I2C; D10 RX/D11 TX; Mega switch"));
  startLCD(); startMagnet();
  startSpectrum();
  flickerAt = millis();
}

void loop() {
  receiveMega();
  pollSpectrum();
  receiveMega();
  if (magReady && millis() - sampledAt >= 100UL) {
    sampledAt = millis();
    valid = readMagnet();
    if (valid) {
      lastGood = millis();
      if (!baselineReady) {
        baselineSum += magnitude; ++baselineCount;
        if (millis() - startupAt >= STARTUP_MS && baselineCount >= 20) {
          baseline = baselineSum / baselineCount;
          filteredMagnitude = magnitude;
          baselineReady = true;
          Serial.print(F("Local reference uT: ")); Serial.println(baseline, 2);
        }
      } else filteredMagnitude += SMOOTHING * (magnitude - filteredMagnitude);
    }
    redraw = true;
  }
  if (redraw && millis() - renderedAt >= 100UL) {
    renderedAt = millis(); redraw = false; render();
  }
  if (millis() - retryAt >= 3000UL) {
    retryAt = millis();
    if (!lcdReady) startLCD();
    if (!magReady) startMagnet();
    else if (!valid) Serial.println(F("Mag read failed or out of range; move away from strong magnets."));
    redraw = true;
  }
  receiveMega();sendDiagnostics();
}
