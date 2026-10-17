# Current snapshot — October 17, 2026

The supplied 1.0 files replace the earlier 0.1 snapshot as the current project reference.

- Added the original Mega 2560, SGP30 Uno, and magnetic/light Uno firmware, with matching Arduino sketch folders.
- Added nine 1.0 enclosure STL parts and the original Pad2Pad fan-control PCB/backup.
- Added startup and air-sample GIF previews to the project page.
- Updated the parts list to include all current checkmarked entries and explicit quantities.
- Documented code-derived wiring, display addresses, controls, serial telemetry, dependencies, and measurement limitations.
- Preserved earlier parts notes, models, and milestones below as historical information.

## Remaining build records

- [ ] Record exact library versions, successful compile/upload results, and memory use.
- [ ] Confirm physical sensor/display operation and add wiring/assembly photos.
- [ ] Resolve the battery voltage discrepancy between the parts list and Mega code.
- [ ] Verify power distribution, fan driver/PCB, and INA219 measurement placement.
- [ ] Record enclosure scale, print settings, and mechanical fit.
- [ ] Add calibration and reference measurements.

The source files show implemented behavior; they do not provide new hardware test results.

---
# Historical progress log (September 2026)

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
