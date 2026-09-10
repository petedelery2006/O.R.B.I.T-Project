# O.R.B.I.T.

An Arduino-based environmental sensing and electronics project with multiple displays and a custom enclosure. Previously called Space Sense; the current prototype is referred to as O.R.B.I.T. 0.1v.

## Current progress

- Magnetic-field sensing with the HiLetgo GY-511 / LSM303DLHC has been reported working, including a response test with a magnet.
- Temperature and humidity readings have been reported working on an LCD.
- Arduino Uno and Mega 2560 boards, 16×2 LCDs, and 0.96-inch OLED displays are marked as tested in the supplied parts list.
- Three prototype STL models are included: an enclosure floor, front wall, and sensor test tower.

These are builder-reported results, not independent accuracy or calibration certifications. The “8 sensor” model names describe the prototype designs; they do not establish that eight sensors are installed and working.

## Project files

| Location | Contents |
| --- | --- |
| [Parts and status](docs/parts-and-status.md) | Organized hardware inventory and purchase status |
| [Original sensor notes](docs/sensors-original.md) | Original uploaded parts list, unchanged |
| [Progress log](docs/progress.md) | Reported milestones and next steps |
| [Hardware models](hardware/models/README.md) | STL files and original filename mapping |
| [Firmware](firmware/README.md) | Status of Arduino source files |

## Next steps

- Add the actual working Arduino sketches and their required libraries.
- Record the current pin connections, board assignments, and display addresses.
- Confirm enclosure fit and printing settings.
- Develop the shared power supply and additional sensor modules.
- Record calibration procedures and comparison measurements.

The repository currently documents hardware and progress. It does not yet contain Arduino firmware or a complete, verified wiring guide.
