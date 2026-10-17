# O.R.B.I.T. 1.0

An Arduino-based environmental sensing project combining air-quality, temperature, humidity, pressure, magnetic-field, light-spectrum, and power readings in a custom enclosure.

The current 1.0 files use **one Mega 2560 and two Uno boards**, with three 16×2 LCDs, two small OLEDs, and a 2.42-inch OLED. This repository includes the supplied firmware, nine enclosure STL files, a fan-control PCB design, and display animation previews.

## Display previews

### Startup
![O.R.B.I.T. startup animation](docs/media/ORBIT_Startup_Preview.gif)

### Air sampling
![O.R.B.I.T. air-sample animation](docs/media/ORBIT_Air_Sample_Preview.gif)

These GIFs preview the display animations; they are not recordings of a hardware test.

## Inside the current firmware

| Board | Responsibilities |
| --- | --- |
| **Mega 2560** | AHT30 temperature/humidity, BMP280 pressure/estimated altitude, INA219 power monitoring, main OLED menus and diagnostics, startup animation, sound settings, and two-fan control |
| **Uno — SGP30** | TVOC, estimated CO2 (eCO2), raw hydrogen/ethanol readings, LCD bar/numeric views, and local OLED status |
| **Uno — magnetic field and light** | LSM303DLHC magnetic-field bars/XYZ readings, AS7341 raw spectral channels and 100/120 Hz flicker detection, and local LCD |

The Mega exchanges switch state, sampling state, acknowledgements, and sensor/diagnostic telemetry with the Unos over two 9600-baud serial links. Each board keeps its own local I2C bus.

## Controls

- **Air-sample button (Mega D6 → GND):** starts the physical fans for 30 seconds; the display animation lasts 6 seconds.
- **OLED button (Mega D24 → GND):** tap to advance; hold for 2 seconds to open/select. Menus include power, debugging, and sounds.
- **LCD switch (Mega D23 → GND):** open shows temperature/humidity, magnetic bars, and the TVOC bar; closed shows altitude/pressure, magnetic XYZ, and numeric eCO2/TVOC.
- **Sounds:** separate startup and air-sample tune selection, with 12 tunes and global mute.

## Build files

| Location | Contents |
| --- | --- |
| [Firmware and setup](firmware/README.md) | Three original sketches, board assignments, and required libraries |
| [Wiring reference](docs/wiring.md) | Pin assignments and I2C addresses taken from the supplied code |
| [Parts and status](docs/parts-and-status.md) | Current component list and supplied quantities |
| [Current original parts notes](docs/sensors-current.md) | Supplied sensors.md, preserved unchanged |
| [1.0 enclosure models](hardware/models/1.0/README.md) | Base, battery base, four walls, roof, connector, and fan mounts |
| [Fan-control PCB](hardware/pcb/README.md) | Original Pad2Pad .pcb and .bak files |
| [Progress log](docs/progress.md) | Current snapshot and earlier project history |
| [Earlier prototype models](hardware/models/README.md) | Previous designs retained for reference |

## Measurement and build status

Updated from the supplied files on **October 17, 2026**. The current parts list marks all listed components with a checkmark. Firmware capabilities above describe the code; end-to-end hardware operation and calibration are not established by the files alone.

SGP30 eCO2 is an estimate. The on-screen “Healthy / Mediocre / Unhealthy” labels use custom TVOC thresholds, not an official AQI or safety assessment. Magnetic bars use a local startup reference; light readings are raw counts, and location/day-night labels are uncertain guesses. Altitude depends on the configured sea-level pressure. Battery percentage and runtime are rough estimates.

**Power detail to resolve:** the parts list labels the battery “5V 5AH,” while the Mega firmware assumes a 12 V 5 Ah LiFePO4 battery (12.8 V nominal, 64 Wh). Confirm the actual pack before using the power estimates.

Next build records to add: library versions and successful compile/upload results, wiring photos, battery specification confirmation, enclosure print settings/fit checks, and calibration/comparison measurements.
