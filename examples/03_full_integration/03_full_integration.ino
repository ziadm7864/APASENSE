// APASENSE — Example 03: Full APA system integration (advanced)
//
// Shows the complete callback bridge wiring between APASENSE, APAPUMP,
// and APADOSE. This is the pattern used in a real APA pool controller.
//
// Libraries stay independent — this sketch is the bridge.
//
// What each bridge does:
//   APASENSE → APAPUMP    pressure and current readings fed as callbacks
//   APAPUMP  → APASENSE   pump state triggers pressure re-zero after stop
//   APAPUMP  → APASENSE   alarm state drives alarm LED and buzzer
//   APASENSE → APADOSE    tank-empty callbacks for each dosing pump
//
// LDR calibration: run examples/00_ldr_calibration first, then replace
// LDR_DARK and LDR_SUN with your measured values.
//
// Hardware:
//   ADS1015   at I2C 0x4B    (pressure AIN0, current AIN1, solar AIN3)
//   PCF8574AT at I2C 0x3B    (tank sensors P0-P3, LEDs P4-P7)
//   APAPUMP   PCF at I2C 0x3C (relay board)
//   Buzzer    on D5

#include <APASENSE.h>
#include <APAPUMP.h>
// Uncomment when using dosing pumps:
// #include <APADOSE.h>

// ── LDR calibration constants (from example 00) ──────────────────────────
// Replace with your measured values after running 00_ldr_calibration.
static constexpr int16_t LDR_DARK = 40;    // raw count in total darkness
static constexpr int16_t LDR_SUN  = 1500;  // raw count in full direct sun

// ── LED assignments (index 0-3 → PCF P4-P7) ─────────────────────────────
static constexpr uint8_t LED_ALARM  = 0;   // P4 — red:   pump alarm active
static constexpr uint8_t LED_SYSTEM = 1;   // P5 — green: system running
static constexpr uint8_t LED_PH     = 2;   // P6 — amber: pH tank empty
static constexpr uint8_t LED_CL     = 3;   // P7 — amber: CL tank empty

// ── Object instances ─────────────────────────────────────────────────────
ApaSense adc;
ApaPump  pump;
// ApaDose dose_ph(192);       // pH pump, EEPROM base 192
// ApaDose dose_cl(192 + 22);  // CL pump, EEPROM base 214

void setup() {
    Serial.begin(115200);

    // ── APASENSE — configure all channels BEFORE begin() ────────────────
    adc.enablePressure();                     // AIN0, 0–6.9 bar
    adc.enableCurrent();                      // AIN1, ACS712-20A
    adc.enableLDR(3, LDR_DARK, LDR_SUN);     // AIN3, solar irradiance

    adc.enableTankSensor(0);                  // P0 — pH tank
    adc.enableTankSensor(1);                  // P1 — CL tank
    adc.enableBuzzer(5);

    // begin() BEFORE pump.begin():
    //   - initialises Wire
    //   - auto-zeroes current sensor (pump must be off)
    //   - loads stored pressure zero-point from EEPROM
    adc.begin();

    // ── APASENSE → APAPUMP: sensor callbacks ────────────────────────────
    // getPressure() returns -1.0f until first zero-cal — APAPUMP handles this safely
    pump.enablePressure([]() { return adc.getPressure(); }, 4.0f);
    pump.setCurrentCallback([]() { return adc.getCurrent(); });

    // ── APAPUMP → APASENSE: pump state → pressure re-zero ───────────────
    // After pump stops, APASENSE waits 30 s then re-zeroes pressure.
    // This keeps the zero-point accurate across daily on/off cycles.
    pump.setPumpStateCallback([](bool on) { adc.onPumpState(on); });

    // ── APAPUMP → APASENSE: alarm → indicators ───────────────────────────
    pump.setPumpAlarmCallback([](PumpAlarm a) {
        bool alarm = (a != PUMP_ALARM_NONE);
        adc.setLed(LED_ALARM, alarm);
        if (alarm) adc.beep(1000);
    });

    // ── APAPUMP: schedule, solar, freeze, etc. ───────────────────────────
    // Add your schedule callback, temperature callbacks, etc. here.
    pump.begin(nullptr, nullptr, nullptr);

    // ── APADOSE: tank-empty bridges (uncomment when using) ───────────────
    // dose_ph.setTankEmptyCallback([]() { return adc.isTankEmpty(0); });
    // dose_cl.setTankEmptyCallback([]() { return adc.isTankEmpty(1); });
    // dose_ph.begin(...);
    // dose_cl.begin(...);

    // Signal ready
    adc.setLed(LED_SYSTEM, true);
    adc.beep(100);

    Serial.println(F("APA system started."));
}

void loop() {
    // All three update() calls must run every loop, in any order.
    adc.update();
    pump.update();
    // dose_ph.update();
    // dose_cl.update();

    // Reflect tank states on LEDs
    adc.setLed(LED_PH, adc.isTankEmpty(0));
    adc.setLed(LED_CL, adc.isTankEmpty(1));

    // Serial report every 5 seconds
    static uint32_t lastPrint = 0;
    if (millis() - lastPrint >= 5000) {
        lastPrint = millis();

        float p = adc.getPressure();
        float i = adc.getCurrent();
        float s = adc.getSolarPct();

        Serial.print(F("Pressure: "));
        if (p < 0) Serial.print(F("  --  "));
        else { Serial.print(p, 2); Serial.print(F(" bar ")); }

        Serial.print(F(" Current: "));
        if (i < 0) Serial.print(F("  --  "));
        else { Serial.print(i, 2); Serial.print(F(" A   ")); }

        Serial.print(F(" Solar: "));
        if (s < 0) Serial.print(F(" -- "));
        else { Serial.print(s, 0); Serial.print(F("%  ")); }

        Serial.print(F("  Pump: "));
        Serial.println(pump.isRunning() ? F("ON") : F("off"));
    }
}
