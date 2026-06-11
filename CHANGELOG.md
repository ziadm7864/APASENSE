# Changelog — APASENSE

## [1.1.1] — 2026-06-11

### Added
- Library logo (`extras/apasense-logo.png`) — now displayed in README header.

## [1.1.0] — 2026-06-10

### Added
- `BuzzerAlert` enum: `BUZZER_INFO`, `BUZZER_WARNING`, `BUZZER_ALARM`
- `alert(severity, repeat)` — plays a rhythm pattern matched to alert severity; `repeat=true` loops until `stopAlert()`
- `stopAlert()` — stops any active repeating alert
- `getPower()` — returns instantaneous power in watts (V × I RMS); returns −1.0 if voltage not configured
- `APASENSE_MAINS_EU` (230 V) and `APASENSE_MAINS_US` (120 V) convenience constants

### Changed
- `enableCurrent()` gains optional third parameter `voltageV` (default 0.0 — no power calc, fully backward compatible)
- `setBuzzer(false)` now also cancels any active `alert()` pattern
- `beep()` now cancels any active pattern before starting a single beep

## [1.0.0] — 2026-06-10

### Added
- ADS1015 four-channel ADC driver (raw Wire I2C, no Adafruit dependency)
- Pressure transducer reading — parametric `maxBar`, auto zero-cal, EEPROM persistence
- AC current measurement (ACS712) — true RMS via non-blocking 32-sample accumulator
- LDR solar irradiance — 0–100 % output, user-supplied calibration constants
- AUX raw voltage input
- PCF8574AT I/O expander — 4 tank-empty sensor inputs (configurable polarity) + 4 status LEDs
- Buzzer management — `setBuzzer()` direct and `beep()` non-blocking single beep
- `onPumpState()` bridge for APAPUMP — triggers 30 s pressure re-zero after pump stop
- EEPROM persistence for pressure zero-point (6 bytes at address 582)
- All 5 platform builds verified: AVR Uno, Mega 2560, ESP32, ESP8266, STM32
- 4 examples: LDR calibration, minimal, pool sensors, full APA integration
