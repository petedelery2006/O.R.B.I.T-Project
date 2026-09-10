# Progress log

Compiled 2026-09-10 from the supplied files and visible project conversation. Dates identify reported milestones, not independently recorded test runs.

## Reported milestones

- **2026-09-01:** Initial Space Sense magnetometer and LCD prototype worked and responded to a magnet. An I2C scan at that stage found addresses `0x19`, `0x1E`, and `0x27`. Those observations should not be treated as a complete address map for the present build.
- **2026-09-02:** Project renamed ORBIT. A Mega 2560 and temperature/humidity sensor were ordered, and multiple display layouts were discussed.
- **2026-09-07:** Three screens were reported lighting up successfully. Temperature and humidity were subsequently reported displaying correctly on an LCD connected to an Uno R3. The builder named the current version O.R.B.I.T. 0.1v.
- **2026-09-10:** A parts/status list and three STL designs were supplied for repository organization. Original model bytes and original sensor notes are preserved in this snapshot.

## Planned or exploratory work

- Additional gas/air-quality sensing, with a larger readings display and smaller overall-rating display.
- Barometric pressure and altitude sensing.
- Possible AS7341 light-sensing integration and cautious interpretation of room lighting conditions.
- Startup audio and a sensor-readiness sequence.
- Shared power for multiple Arduino boards. A 12 V 5 Ah LiFePO4 battery was selected in discussion; purchase and installation are not confirmed here. The uploaded list marks the buck converter as not purchased.
- Custom lighting PCBs, diffusers, and enclosure development.

These items are plans, not completed capabilities. The final sensor roster, code, wiring, power measurements, and calibration records still need to be captured.

## Next documentation tasks

- [ ] Upload working `.ino` sketch folders.
- [ ] Identify which sketch belongs to each physical board.
- [ ] Record Arduino board settings and library names/versions.
- [ ] Photograph the current wiring and record pin assignments.
- [ ] Record installed sensor/display quantities and I2C addresses.
- [ ] Confirm STL units, print settings, and physical fit.
- [ ] Add reference measurements and calibration notes.
