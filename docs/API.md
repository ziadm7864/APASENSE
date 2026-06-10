# APASENSE — API Reference

**Version 1.0.0** · [GitHub](https://github.com/apadevices/APASENSE)

---

## Quick start

```cpp
#include <APASENSE.h>

ApaSense adc;              // ADS1015=0x4B, PCF8574AT=0x3B (hardware defaults)

void setup() {
    adc.enablePressure();  // enable channels BEFORE begin()
    adc.enableCurrent();
    adc.begin();           // initialises I2C, zeroes current sensor
}

void loop() {
    adc.update();          // call every loop() — never use delay()

    float bar  = adc.getPressure();   // -1.0 until calibrated
    float amps = adc.getCurrent();    // -1.0 until 32 samples collected
}
```

---

## Setup order

Always follow this order in `setup()`:

| Step | Call | Why |
|------|------|-----|
| 1 | `adc.enablePressure()` etc. | Sets channel assignments and parameters |
| 2 | `adc.enableTankSensor()` etc. | Arms PCF inputs with correct polarity |
| 3 | `adc.enableBuzzer(pin)` | Configures output pin |
| 4 | `adc.begin()` | Starts Wire, inits PCF, auto-zeroes current |
| 5 | Wire APAPUMP callbacks | `pump.enablePressure([](){...})` etc. |

**Current zero-cal happens inside `begin()`** — if `enableCurrent()` is called after `begin()`, the current zero will be 0 (no calibration). Always call all `enable*()` methods before `begin()`.

---

## Constructors

```cpp
ApaSense adc;
```
Default constructor. Uses ADS1015 at address `0x4B` and PCF8574AT at `0x3B` — matches the APA sensing board hardware.

```cpp
ApaSense adc(uint8_t adsAddr);
```
Custom ADS1015 address, PCF at default `0x3B`.

```cpp
ApaSense adc(uint8_t adsAddr, uint8_t pcfAddr);
```
Both addresses custom. Pass `APASENSE_NO_PCF` (0xFF) as `pcfAddr` to disable PCF support entirely — useful when only the ADS1015 is present.

---

## Core

### `begin()`
```cpp
void begin();
```
Initialises the library. Must be called once in `setup()` after all `enable*()` calls.

- Calls `Wire.begin()` (safe to call even if the sketch also calls it)
- On ESP32/ESP8266: calls `EEPROM.begin(600)`
- Loads stored pressure zero-point from EEPROM (if valid)
- Writes `0xFF` to the PCF — arms P0–P3 as inputs and turns off all LEDs
- Takes 8 blocking samples (≈8 ms) to measure the current sensor zero offset

---

### `update()`
```cpp
void update();
```
Advances all non-blocking state machines. **Must be called every `loop()` iteration.** Never use `delay()` in a sketch that uses APASENSE.

Each `update()` call:
1. Checks and fires the beep timer
2. Checks and fires the 30-second pressure settle timer
3. Reads a completed ADC conversion (if 1 ms has elapsed since it started)
4. Starts the next ADC conversion on the next enabled sensor

---

## ADS1015 channel enable

All channels are disabled by default. Call the relevant `enable*()` method for each sensor you have connected.

### `enablePressure()`
```cpp
void enablePressure(uint8_t channel = 0, float maxBar = APASENSE_DEFAULT_MAX_BAR);
```
Enables pressure transducer reading on the specified ADS1015 channel.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `channel` | uint8_t | 0 | ADS1015 channel (0–3) |
| `maxBar` | float | 6.9 | Sensor full-scale in bar (6.9 ≈ 100 PSI) |

The sensor must be a 0.5–4.5 V ratiometric type. The 0.5 V zero offset is absorbed automatically by `calibratePressureZero()`. For a 150 PSI sensor use `maxBar = 10.3f`; for 200 PSI use `13.8f`.

---

### `enableCurrent()`
```cpp
void enableCurrent(uint8_t channel = 1, float sensitivity = APASENSE_DEFAULT_CURR_SENS);
```
Enables AC current measurement via ACS712 on the specified channel.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `channel` | uint8_t | 1 | ADS1015 channel (0–3) |
| `sensitivity` | float | 0.100 | Sensor output in V/A |

ACS712 sensitivity values: 5 A → 0.185, **20 A → 0.100** (default), 30 A → 0.066.

`getCurrent()` returns −1.0 until the first 32-sample RMS cycle completes (approximately one second at a typical update rate).

---

### `enableAux()`
```cpp
void enableAux(uint8_t channel = 2);
```
Enables raw voltage reading on the specified channel. No processing — `getAuxVoltage()` returns the measured voltage in volts.

---

### `enableLDR()`
```cpp
void enableLDR(uint8_t channel = 3, int16_t rawDark = 0, int16_t rawSun = 1667);
```
Enables solar irradiance reading via the LDR circuit.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `channel` | uint8_t | 3 | ADS1015 channel (0–3) |
| `rawDark` | int16_t | 0 | Raw ADC count in total darkness |
| `rawSun` | int16_t | 1667 | Raw ADC count in full direct sun |

**Calibration:** run `examples/00_ldr_calibration`, note the two values, then hardcode them:
```cpp
adc.enableLDR(3, 42, 1520);   // measured values for your specific circuit
```

Without calibration (default `rawDark=0, rawSun=1667`), `getSolarPct()` returns the raw ADC value as a percentage of the ADC full scale — a useful approximation if the hardware pot (R16) is centred correctly.

---

## Getters

### `getPressure()`
```cpp
float getPressure() const;
```
Returns pool pressure in bar, clamped to ≥ 0.0.

Returns `−1.0` if:
- `enablePressure()` was not called, or
- `calibratePressureZero()` has not yet run (pressure is not calibrated)

On a fresh install, pressure becomes valid after the first pump stop + 30 second settle. On subsequent boots, the stored EEPROM value is used immediately.

---

### `getCurrent()`
```cpp
float getCurrent() const;
```
Returns pump current in amps (true RMS). Returns `−1.0` until the first 32-sample cycle completes after `begin()`.

---

### `getAuxVoltage()`
```cpp
float getAuxVoltage() const;
```
Returns AUX channel voltage in volts (0.0–5.0). Returns `0.0` if `enableAux()` was not called.

---

### `getSolarPct()`
```cpp
float getSolarPct() const;
```
Returns solar irradiance as a percentage (0.0–100.0). Returns `−1.0` if:
- `enableLDR()` was not called, or
- `rawDark` and `rawSun` were passed with `rawSun <= rawDark`

---

### `getRawLDR()`
```cpp
int16_t getRawLDR() const;
```
Returns the raw ADS1015 count for the LDR channel. Use this in the calibration sketch (`examples/00_ldr_calibration`) to determine `rawDark` and `rawSun` values.

---

## Pressure calibration

### `calibratePressureZero()`
```cpp
void calibratePressureZero();
```
Captures the current ADS1015 reading as the zero-pressure reference point and saves it to EEPROM. Call only when the pump is off and the system has been at rest for a few seconds.

In normal operation you do not need to call this manually — `onPumpState(false)` triggers it automatically after 30 seconds.

---

### `onPumpState()`
```cpp
void onPumpState(bool on);
```
Informs APASENSE of pump on/off transitions. Connect to APAPUMP via `setPumpStateCallback()`:

```cpp
pump.setPumpStateCallback([](bool on) { adc.onPumpState(on); });
```

- `on = true`: cancels any pending settle timer (pump restarted)
- `on = false`: starts the 30-second settle timer; fires `calibratePressureZero()` when it expires

---

## PCF tank sensors

### `enableTankSensor()`
```cpp
void enableTankSensor(uint8_t bit, bool activeLow = true);
```
Enables a tank-empty sensor on PCF pin P0–P3.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `bit` | uint8_t | — | PCF input bit (0 = P0, 1 = P1, 2 = P2, 3 = P3) |
| `activeLow` | bool | true | `true` = pin LOW means tank is empty (optocoupler default) |

Default assignments on APA hardware:

| Bit | PCF pin | Chemical |
|-----|---------|----------|
| 0 | P0 | pH |
| 1 | P1 | CL (chlorine) |
| 2 | P2 | Flocculant |
| 3 | P3 | Algicide / AUX |

---

### `isTankEmpty()`
```cpp
bool isTankEmpty(uint8_t bit);
```
Returns `true` if the specified tank is empty. Performs a live PCF read each call.

Returns `false` if:
- `bit` is > 3
- `enableTankSensor(bit)` was not called
- `begin()` has not been called yet

**APADOSE bridge:**
```cpp
dose_ph.setTankEmptyCallback([]() { return adc.isTankEmpty(0); });
dose_cl.setTankEmptyCallback([]() { return adc.isTankEmpty(1); });
```

---

## PCF LEDs

### `setLed()`
```cpp
void setLed(uint8_t index, bool on);
```
Sets a status LED. Index 0–3 maps to PCF pins P4–P7 (active-low open-drain).

| Index | PCF pin |
|-------|---------|
| 0 | P4 |
| 1 | P5 |
| 2 | P6 |
| 3 | P7 |

Silently ignored if `index > 3` or `begin()` has not been called.

---

### `getLed()`
```cpp
bool getLed(uint8_t index) const;
```
Returns the commanded state of the LED (not a hardware readback). `true` = LED is on. Returns `false` if `index > 3`.

---

## Buzzer

### `enableBuzzer()`
```cpp
void enableBuzzer(uint8_t pin);
```
Configures the buzzer output pin. Call in `setup()`. The pin is set LOW immediately (buzzer off).

---

### `setBuzzer()`
```cpp
void setBuzzer(bool on);
```
Turns the buzzer on or off directly. Cancels any pending `beep()` timer.

---

### `beep()`
```cpp
void beep(uint16_t ms);
```
Starts a non-blocking timed beep. The buzzer turns off automatically after `ms` milliseconds, driven by `update()`. Passing `0` is a no-op.

---

## Constants

```cpp
#define            APASENSE_LIB_VERSION            "1.0.0"
constexpr uint16_t APASENSE_EEPROM_ADDR         = 582;    // start address in EEPROM
constexpr uint16_t APASENSE_EEPROM_MAGIC        = 0xA5C2;
constexpr uint8_t  APASENSE_EEPROM_VERSION      = 1;
constexpr uint8_t  APASENSE_RMS_SAMPLES         = 32;     // samples per RMS calculation
constexpr float    APASENSE_ADS_LSB_MV          = 3.0f;   // mV per LSB at ±6.144 V gain
constexpr uint16_t APASENSE_PRESSURE_SETTLE_MS  = 30000;  // ms after pump stop before re-zero
constexpr float    APASENSE_DEFAULT_MAX_BAR      = 6.9f;   // ≈100 PSI
constexpr float    APASENSE_DEFAULT_CURR_SENS    = 0.100f; // ACS712-20A: 100 mV/A
constexpr uint8_t  APASENSE_NO_PCF              = 0xFF;   // disable PCF in constructor
constexpr uint8_t  APASENSE_NO_BUZZER           = 0xFF;   // buzzer not configured (default)
```

---

## EEPROM layout

| Address | Size | Field |
|---------|------|-------|
| 582–583 | 2 B | Magic number `0xA5C2` |
| 584 | 1 B | Version (`1`) |
| 585–586 | 2 B | `pressureZero` (int16_t, raw ADC counts) |
| 587 | 1 B | Checksum (byte sum of preceding 5 bytes) |

**Total: 6 bytes.** Addresses 532–581 are reserved as an expansion gap for APAPUMP.

EEPROM is written only when `calibratePressureZero()` runs. On AVR the library uses `EEPROM.put()` which only writes bytes that have changed (no unnecessary wear). On ESP32/ESP8266 `EEPROM.commit()` is called automatically.

---

## Platform notes

| Platform | Note |
|----------|------|
| ESP32 / ESP8266 | `EEPROM.begin(600)` is called inside `begin()` — do not call it again in your sketch with a smaller value |
| ESP32 | `noInterrupts()` only disables interrupts on the calling core — no ISR-safety guarantees for multi-core access |
| AVR (Uno / Mega) | `snprintf` does not support `%f` — use `dtostrf()` for float formatting in your sketch |
| All platforms | Call `adc.update()` every `loop()`. Avoid `delay()` — it blocks ADC cycling and the beep/settle timers |

---

*APASENSE — APA Devices · [kecup@vazac.eu](mailto:kecup@vazac.eu)*
