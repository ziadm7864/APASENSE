#pragma once
#include <Arduino.h>

// ── Version ──────────────────────────────────────────────────────────────────
#define APASENSE_LIB_VERSION "1.1.1"

// ── EEPROM ───────────────────────────────────────────────────────────────────
// APA EEPROM map: 520–531 APAPUMP | 532–581 APAPUMP reserve | 582–587 APASENSE
constexpr uint16_t APASENSE_EEPROM_ADDR    = 582;
constexpr uint16_t APASENSE_EEPROM_MAGIC   = 0xA5C2;
constexpr uint8_t  APASENSE_EEPROM_VERSION = 1;

// ── ADS1015 ──────────────────────────────────────────────────────────────────
constexpr uint8_t  APASENSE_RMS_SAMPLES       = 32;    // samples per RMS calculation
constexpr float    APASENSE_ADS_LSB_MV        = 3.0f;  // mV/LSB at ±6.144V gain, 12-bit
constexpr float    APASENSE_DEFAULT_MAX_BAR   = 6.9f;  // ≈100 PSI default pressure range
constexpr float    APASENSE_DEFAULT_CURR_SENS = 0.100f; // ACS712-20A: 100 mV/A

// ── Pressure ─────────────────────────────────────────────────────────────────
constexpr uint16_t APASENSE_PRESSURE_SETTLE_MS = 30000; // settle after pump stop → re-zero

// ── Mains voltage helpers ─────────────────────────────────────────────────────
constexpr float APASENSE_MAINS_EU = 230.0f; // European standard
constexpr float APASENSE_MAINS_US = 120.0f; // US / Canada standard

// ── Sentinels ────────────────────────────────────────────────────────────────
constexpr uint8_t APASENSE_NO_PCF    = 0xFF; // pcfAddr=0xFF → no PCF hardware
constexpr uint8_t APASENSE_NO_BUZZER = 0xFF; // buzzerPin=0xFF → buzzer not configured

// ── Buzzer alert severity ─────────────────────────────────────────────────────
enum BuzzerAlert : uint8_t {
    BUZZER_INFO    = 0, // single beep  (200ms on · 800ms off)
    BUZZER_WARNING = 1, // double beep  (200ms·150ms gap·200ms · 2s silence)
    BUZZER_ALARM   = 2  // triple beep  (150ms×3 · 100ms gaps · 1.5s silence)
};

// ─────────────────────────────────────────────────────────────────────────────

class ApaSense {
public:
    // ── Constructors ─────────────────────────────────────────────────────────
    ApaSense();                                         // ADS=0x4B, PCF=0x3B
    ApaSense(uint8_t adsAddr);                          // custom ADS, PCF=0x3B
    ApaSense(uint8_t adsAddr, uint8_t pcfAddr);         // both custom; pcfAddr=0xFF → no PCF

    // ── Core ─────────────────────────────────────────────────────────────────
    void begin();   // Wire.begin(), init ADS + PCF, auto-zero current
    void update();  // non-blocking: ADC cycle, RMS accum, buzzer sequencer, settle timer

    // ── ADS1015 channels — opt-in, all disabled by default ───────────────────
    void enablePressure(uint8_t channel = 0, float maxBar = APASENSE_DEFAULT_MAX_BAR);
    void enableCurrent(uint8_t channel = 1,
                       float sensitivity = APASENSE_DEFAULT_CURR_SENS,
                       float voltageV = 0.0f);          // voltageV > 0 enables getPower()
    void enableAux(uint8_t channel = 2);
    void enableLDR(uint8_t channel = 3, int16_t rawDark = 0, int16_t rawSun = 1667);
    // rawDark/rawSun: hardcode from calibration example; defaults give raw ADC ratio

    // ── Getters ───────────────────────────────────────────────────────────────
    float   getPressure()   const; // bar, ≥0.0f; -1.0f if not enabled/calibrated
    float   getCurrent()    const; // amps RMS; -1.0f if not enabled/calibrated
    float   getPower()      const; // apparent power VA = voltageV × I_rms; -1.0f if not configured
                                   // note: true watts = VA × power factor (≈0.85–0.95 for pumps)
    float   getAuxVoltage() const; // volts 0.0–5.0; 0.0f if not enabled
    float   getSolarPct()   const; // 0–100%; -1.0f if not enabled or rawDark==rawSun
    int16_t getRawLDR()     const; // raw ADC count — use in calibration sketch

    // ── Pressure calibration ──────────────────────────────────────────────────
    void calibratePressureZero(); // snapshot current reading as zero; pump must be off
    void onPumpState(bool on);    // APAPUMP bridge; triggers settle re-zero on false

    // ── PCF tank sensors — bit 0–3 maps to PCF P0–P3 ─────────────────────────
    void enableTankSensor(uint8_t bit, bool activeLow = true);
    bool isTankEmpty(uint8_t bit); // live PCF read; false if bit not enabled

    // ── PCF LEDs — index 0–3 maps to PCF P4–P7 ───────────────────────────────
    void setLed(uint8_t index, bool on); // on=true → PCF LOW (active-low)
    bool getLed(uint8_t index) const;    // returns commanded state

    // ── Buzzer ────────────────────────────────────────────────────────────────
    void enableBuzzer(uint8_t pin);
    void setBuzzer(bool on);                            // direct on/off; cancels any pattern
    void beep(uint16_t ms);                             // non-blocking single beep; 0 = no-op
    void alert(BuzzerAlert severity, bool repeat = false); // play rhythm pattern
    void stopAlert();                                   // stop repeating pattern

private:
    // ── I2C addresses ─────────────────────────────────────────────────────────
    uint8_t _adsAddr;
    uint8_t _pcfAddr;

    // ── Channel packing: bits 7-6=LDR, 5-4=AUX, 3-2=current, 1-0=pressure ───
    uint8_t _chBits;

    // ── Calibration values ────────────────────────────────────────────────────
    int16_t _pressureZero;  // auto: begin() + onPumpState(false); persisted to EEPROM
    int16_t _currentZero;   // auto: begin(); SRAM only, re-measured every boot
    int16_t _ldrMin;        // user: enableLDR(ch, rawDark, rawSun)
    int16_t _ldrMax;

    // ── Channel parameters ────────────────────────────────────────────────────
    float _pressureMaxBar;
    float _currentSensitivity;
    float _mainsVoltage;    // 0.0f = not configured; >0 enables getPower()

    // ── Cached ADC readings (raw counts, converted in getter) ─────────────────
    int16_t _rawPressure;
    int16_t _rawAux;
    int16_t _rawLdr;

    // ── Current RMS ───────────────────────────────────────────────────────────
    float    _currentRms;
    uint32_t _currentSumSq;
    uint8_t  _currentSampleN;

    // ── PCF state ─────────────────────────────────────────────────────────────
    uint8_t _pcfBState; // bits 4–7 = LED commanded states (write state)
    uint8_t _tankBits;  // bits 0–3=enabled, bits 4–7=polarity

    // ── Buzzer ────────────────────────────────────────────────────────────────
    uint8_t  _buzzerPin;
    uint8_t  _buzzerPattern;    // active pattern index (BuzzerAlert), 0xFF = none
    uint8_t  _buzzerStep;       // current step within pattern, 0xFF = idle/simple-beep mode
    uint32_t _buzzerStepEndMs;  // end time for current step (on or off phase)

    // ── Pressure settle ───────────────────────────────────────────────────────
    uint32_t _settleStartMs;

    // ── ADC conversion sequencing ─────────────────────────────────────────────
    uint8_t  _adsPendingCh;  // 0xFF = idle; 0-3 = sensor index with conversion in flight
    uint8_t  _adsCh;         // next sensor index to sample (0=pressure,1=current,2=aux,3=ldr)
    uint16_t _adsConvMs16;   // low 16 bits of millis() when conversion was started

    // ── Flags (12 bits = 2 bytes) ─────────────────────────────────────────────
    struct {
        bool pressureEnabled    : 1;
        bool currentEnabled     : 1;
        bool auxEnabled         : 1;
        bool ldrEnabled         : 1;
        bool pressureCalibrated : 1;
        bool currentCalibrated  : 1;
        bool buzzerEnabled      : 1;
        bool settlePending      : 1;
        bool adsReady           : 1;
        bool pcfReady           : 1;
        bool buzzerRepeat       : 1; // alert() repeat flag
        bool buzzerOnPhase      : 1; // true = currently in ON phase of a pattern step
    } _flags;

    // ── Private helpers ───────────────────────────────────────────────────────
    void     _init();
    uint8_t  _getPressureCh() const { return (_chBits >> 0) & 0x03; }
    uint8_t  _getCurrentCh()  const { return (_chBits >> 2) & 0x03; }
    uint8_t  _getAuxCh()      const { return (_chBits >> 4) & 0x03; }
    uint8_t  _getLdrCh()      const { return (_chBits >> 6) & 0x03; }
    void     _writeRegister(uint8_t reg, uint16_t value);
    uint16_t _readRegister(uint8_t reg);
    void     _startConversion(uint8_t channel);
    int16_t  _readConversion();
    void     _processADC(uint8_t sensor, int16_t raw);
    void     _writePCF();
    void     _loadEEPROM();
    void     _saveEEPROM();
    void     _advanceBuzzerPattern(uint32_t now); // drives pattern step sequencer
};
