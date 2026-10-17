# Wiring reference

Derived from the supplied 1.0 sketches. This records firmware pin assignments; it is not a complete electrical schematic.

## Mega 2560

| Connection | Pin / address |
| --- | --- |
| Software I2C SDA / SCL | A4 / A5 |
| LCD | 0x27 on software I2C |
| Small SSD1306 OLED | 0x3C on software I2C |
| Large SSD1309 OLED | 0x3D on software I2C |
| AHT30 | 0x38 on software I2C |
| INA219 | 0x40 on software I2C |
| BMP280 SDA / SCL | D20 / D21, hardware I2C; auto-detect 0x77 or 0x76 |
| Fan control outputs 1 / 2 | D49 / D47 |
| Power LED / RGB green / RGB blue | D43 / D41 / D39 |
| Buzzer | D38 |
| Air-sample button | D6 → GND |
| LCD maintained switch | D23 → GND |
| OLED button | D24 → GND |

RGB defaults to common cathode. Fan pins are control outputs; the PCB/motor driver and supply wiring require their own electrical verification.

## Uno serial links

| Mega port | Mega TX → Uno RX | Uno TX → Mega RX | Intended board |
| --- | --- | --- | --- |
| Serial1 | D18 → D10 | D11 → D19 | SGP30 Uno |
| Serial3 | D14 → D10 | D11 → D15 | Magnetic/light Uno |

Use common ground. Both links run at 9600 baud. The Mega identifies sensor records by packet type, allowing the Uno port assignments to be swapped.

Keep all three boards' I2C buses separate. The Mega repeats SW=0/1 and SAMPLE=0/1 each second and on changes. SAMPLE reports the fan state; the SGP30 continues its periodic measurements.

## Local Uno I2C

Both Unos use A4 for SDA and A5 for SCL on separate local buses.

- SGP30 Uno: LCD 0x27, SSD1306 OLED 0x3C, SGP30 0x58.
- Magnetic/light Uno: LCD configured at 0x27; LSM303DLHC and AS7341 use their library-managed addresses. No local OLED or switch is assigned in this sketch.

Confirm breakout power requirements, pull-ups, actual addresses, and the physical wiring before use.

## Power and calibration assumptions

The Mega code assumes an INA219 on the battery side with a standard R100 shunt, measuring all battery loads. Usage totals reset at startup. Runtime estimates are unreliable while charging.

The supplied parts list says “5V 5AH” battery; the code says one 12 V 5 Ah LiFePO4 pack with a spare and uses 12.8 × 5 = 64 Wh. Resolve the actual battery specification and power topology before relying on estimates.

BMP280 altitude uses SEA_LEVEL_HPA = 1013.25 by default, a standard reference rather than local QNH. Set the reference to the applicable sea-level pressure for calibrated altitude.
