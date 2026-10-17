# Arduino firmware

The three supplied sketches are archived unchanged in matching Arduino sketch folders.

| Sketch | Arduino board selection | Required external libraries |
| --- | --- | --- |
| [ORBIT_Mega_Analog_I2C](ORBIT_Mega_Analog_I2C/ORBIT_Mega_Analog_I2C.ino) | Arduino Mega or Mega 2560 / ATmega2560 | U8g2, SoftwareWire by Testato, Adafruit BMP280 Library |
| [ORBIT_SGP30_Uno](ORBIT_SGP30_Uno/ORBIT_SGP30_Uno.ino) | Arduino Uno | Adafruit SGP30, LiquidCrystal I2C by Frank de Brabander, U8g2 |
| [ORBIT_Uno_Magnet_LCD](ORBIT_Uno_Magnet_LCD/ORBIT_Uno_Magnet_LCD.ino) | Arduino Uno | hd44780 by Bill Perry, legacy Adafruit LSM303DLHC (Adafruit_LSM303_U.h), Adafruit Unified Sensor, Adafruit AS7341 |

Install library dependencies when offered, including Adafruit BusIO and Adafruit Unified Sensor as required. Wire, SoftwareSerial, EEPROM, and AVR headers come with the Arduino AVR board package. Exact library versions were not supplied.

## Upload and startup

1. Open the .ino inside its matching folder in Arduino IDE.
2. Install the libraries above and select the target board and USB port.
3. Verify and upload each sketch to its assigned board.
4. Check the [code-derived wiring reference](../docs/wiring.md).
5. Keep the magnetic sensor still and away from magnets during its initial 5-second reference period. Reset to rebaseline. The SGP30 marks its first 16 seconds as warming.

The Mega uses software I2C on A4/A5 for its main shared bus, and hardware I2C on D20/D21 for the BMP280 only. Each Uno uses its own A4/A5 bus. Serial links use 9600 baud and newline-delimited version-1 telemetry.

The current Mega parser handles ACK, SGP, MAG, SP1, SP2, FLK, BUS, and CTRL records. Some comments in the Uno sources still describe an older ACK-only Mega; the supplied Mega implementation has the expanded parser. The source files are preserved as supplied.

## Validation status

These files were inspected and copied byte-for-byte. Compilation, upload, memory use, and physical operation have not been verified in this repository update; exact library versions and hardware test records remain to be added.
