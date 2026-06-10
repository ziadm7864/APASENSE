#include "APASENSE.h"
#include <Wire.h>
#include <EEPROM.h>
#include <math.h>

// ── EEPROM layout ─────────────────────────────────────────────────────────────
struct __attribute__((packed)) EepromData {
    uint16_t magic;
    uint8_t  version;
    int16_t  pressureZero;
    uint8_t  checksum;
};

// ── ADS1015 register addresses ────────────────────────────────────────────────
static constexpr uint8_t ADS_REG_CONVERT = 0x00;
static constexpr uint8_t ADS_REG_CONFIG  = 0x01;

// Config register base: OS=1(start), PGA=000(±6.144V), MODE=1(single-shot),
// DR=100(1600SPS), COMP_QUE=11(disabled)
// MUX bits 14-12 added per channel: (4+ch) << 12
static constexpr uint16_t ADS_CONFIG_BASE = 0x8183;
//  0x8000 OS  | 0x0000 PGA±6.144V | 0x0100 single-shot | 0x0080 1600SPS | 0x0003 comp-off

// ── Constructors ──────────────────────────────────────────────────────────────

ApaSense::ApaSense()
    : _adsAddr(0x4B), _pcfAddr(0x3B) { _init(); }

ApaSense::ApaSense(uint8_t adsAddr)
    : _adsAddr(adsAddr), _pcfAddr(0x3B) { _init(); }

ApaSense::ApaSense(uint8_t adsAddr, uint8_t pcfAddr)
    : _adsAddr(adsAddr), _pcfAddr(pcfAddr) { _init(); }

void ApaSense::_init() {
    _chBits             = 0xE4; // LDR=3, AUX=2, current=1, pressure=0 defaults
    _pressureZero       = 0;
    _currentZero        = 0;
    _ldrMin             = 0;
    _ldrMax             = 1667;
    _pressureMaxBar     = APASENSE_DEFAULT_MAX_BAR;
    _currentSensitivity = APASENSE_DEFAULT_CURR_SENS;
    _rawPressure        = 0;
    _rawAux             = 0;
    _rawLdr             = 0;
    _currentRms         = -1.0f;
    _currentSumSq       = 0;
    _currentSampleN     = 0;
    _pcfBState          = 0xFF;
    _tankBits           = 0x00;
    _buzzerPin          = APASENSE_NO_BUZZER;
    _beepEndMs          = 0;
    _settleStartMs      = 0;
    _adsPendingCh       = 0xFF;
    _adsCh              = 0;
    _adsConvMs16        = 0;
    memset(&_flags, 0, sizeof(_flags));
}

// ── Core ──────────────────────────────────────────────────────────────────────

void ApaSense::begin() {
    Wire.begin();

#if defined(ESP32) || defined(ESP8266)
    EEPROM.begin(600);
#endif

    _loadEEPROM();

    // Init PCF: 0xFF arms P0-P3 as inputs (pull-up) and keeps P4-P7 LEDs off
    if (_pcfAddr != APASENSE_NO_PCF) {
        _pcfBState = 0xFF;
        _writePCF();
        _flags.pcfReady = true;
    }

    _flags.adsReady = true;

    // Auto-zero current — 8 blocking samples (8 ms in setup, acceptable)
    if (_flags.currentEnabled) {
        int32_t sum = 0;
        for (uint8_t i = 0; i < 8; i++) {
            _startConversion(_getCurrentCh());
            delayMicroseconds(1000);
            sum += (int16_t)_readConversion();
        }
        _currentZero = (int16_t)(sum / 8);
    }
}

void ApaSense::update() {
    uint32_t now = millis();

    // Beep timer
    if (_beepEndMs && now >= _beepEndMs) {
        _beepEndMs = 0;
        if (_flags.buzzerEnabled) digitalWrite(_buzzerPin, LOW);
    }

    // Pressure settle → auto re-zero
    if (_flags.settlePending && now - _settleStartMs >= APASENSE_PRESSURE_SETTLE_MS) {
        calibratePressureZero();
    }

    if (!_flags.adsReady) return;

    // Read completed conversion
    if (_adsPendingCh != 0xFF && (uint16_t)now - _adsConvMs16 >= 1) {
        int16_t raw = _readConversion();
        _processADC(_adsPendingCh, raw);
        _adsPendingCh = 0xFF;
    }

    // Start next conversion (round-robin through enabled sensors)
    if (_adsPendingCh == 0xFF) {
        for (uint8_t i = 0; i < 4; i++) {
            uint8_t sensor = (_adsCh + i) & 0x03;
            uint8_t ch = 0;
            bool    en = false;
            switch (sensor) {
                case 0: en = _flags.pressureEnabled; ch = _getPressureCh(); break;
                case 1: en = _flags.currentEnabled;  ch = _getCurrentCh();  break;
                case 2: en = _flags.auxEnabled;       ch = _getAuxCh();      break;
                case 3: en = _flags.ldrEnabled;       ch = _getLdrCh();      break;
            }
            if (en) {
                _startConversion(ch);
                _adsPendingCh = sensor;
                _adsConvMs16  = (uint16_t)now;
                _adsCh        = (sensor + 1) & 0x03;
                break;
            }
        }
    }
}

// ── ADS1015 channel enable ────────────────────────────────────────────────────

void ApaSense::enablePressure(uint8_t channel, float maxBar) {
    _chBits = (_chBits & ~0x03) | (channel & 0x03);
    _pressureMaxBar = maxBar;
    _flags.pressureEnabled = true;
}

void ApaSense::enableCurrent(uint8_t channel, float sensitivity) {
    _chBits = (_chBits & ~0x0C) | ((channel & 0x03) << 2);
    _currentSensitivity = (sensitivity > 0.0f) ? sensitivity : APASENSE_DEFAULT_CURR_SENS;
    _flags.currentEnabled = true;
}

void ApaSense::enableAux(uint8_t channel) {
    _chBits = (_chBits & ~0x30) | ((channel & 0x03) << 4);
    _flags.auxEnabled = true;
}

void ApaSense::enableLDR(uint8_t channel, int16_t rawDark, int16_t rawSun) {
    _chBits = (_chBits & ~0xC0) | ((channel & 0x03) << 6);
    _ldrMin = rawDark;
    _ldrMax = rawSun;
    _flags.ldrEnabled = true;
}

// ── Getters ───────────────────────────────────────────────────────────────────

float ApaSense::getPressure() const {
    if (!_flags.pressureEnabled || !_flags.pressureCalibrated) return -1.0f;
    // 4 V span (0.5–4.5 V ratiometric) = 4000 mV / 3 mV/LSB = 1333.33 counts full scale
    float bar = (float)(_rawPressure - _pressureZero) * _pressureMaxBar / 1333.33f;
    return (bar < 0.0f) ? 0.0f : bar;
}

float ApaSense::getCurrent() const {
    if (!_flags.currentEnabled || !_flags.currentCalibrated) return -1.0f;
    return _currentRms;
}

float ApaSense::getAuxVoltage() const {
    if (!_flags.auxEnabled) return 0.0f;
    return (float)_rawAux * APASENSE_ADS_LSB_MV / 1000.0f;
}

float ApaSense::getSolarPct() const {
    if (!_flags.ldrEnabled)    return -1.0f;
    if (_ldrMax <= _ldrMin)    return -1.0f;
    float pct = (float)(_rawLdr - _ldrMin) / (float)(_ldrMax - _ldrMin) * 100.0f;
    if (pct < 0.0f)   return 0.0f;
    if (pct > 100.0f) return 100.0f;
    return pct;
}

int16_t ApaSense::getRawLDR() const {
    return _rawLdr;
}

// ── Pressure calibration ──────────────────────────────────────────────────────

void ApaSense::calibratePressureZero() {
    if (!_flags.pressureEnabled || !_flags.adsReady) return;
    _pressureZero = _rawPressure;
    _flags.pressureCalibrated = true;
    _flags.settlePending = false;
    _saveEEPROM();
}

void ApaSense::onPumpState(bool on) {
    if (on) {
        _flags.settlePending = false;
    } else {
        _settleStartMs = millis();
        _flags.settlePending = true;
    }
}

// ── PCF tank sensors ──────────────────────────────────────────────────────────

void ApaSense::enableTankSensor(uint8_t bit, bool activeLow) {
    if (bit > 3) return;
    _tankBits |=  (1 << bit);
    if (activeLow) _tankBits |=  (uint8_t)(1 << (bit + 4));
    else           _tankBits &= ~(uint8_t)(1 << (bit + 4));
}

bool ApaSense::isTankEmpty(uint8_t bit) {
    if (bit > 3)                                     return false;
    if (!(_tankBits & (1 << bit)))                   return false; // not enabled
    if (_pcfAddr == APASENSE_NO_PCF || !_flags.pcfReady) return false;

    Wire.requestFrom(_pcfAddr, (uint8_t)1);
    if (!Wire.available()) return false;
    uint8_t state = Wire.read();

    bool physLow  = !(state & (1 << bit));
    bool activeLow = (_tankBits & (uint8_t)(1 << (bit + 4))) != 0;
    return activeLow ? physLow : !physLow;
}

// ── PCF LEDs ─────────────────────────────────────────────────────────────────

void ApaSense::setLed(uint8_t index, bool on) {
    if (index > 3 || !_flags.pcfReady) return;
    uint8_t pcfBit = index + 4; // index 0-3 → PCF P4-P7
    uint8_t prev = _pcfBState;
    if (on) _pcfBState &= ~(uint8_t)(1 << pcfBit); // LOW = LED on
    else    _pcfBState |=  (uint8_t)(1 << pcfBit); // HIGH = LED off
    if (_pcfBState != prev) _writePCF();            // only write on state change
}

bool ApaSense::getLed(uint8_t index) const {
    if (index > 3) return false;
    return !(_pcfBState & (1 << (index + 4))); // LOW = on → invert
}

// ── Buzzer ────────────────────────────────────────────────────────────────────

void ApaSense::enableBuzzer(uint8_t pin) {
    if (pin == APASENSE_NO_BUZZER) return;
    _buzzerPin = pin;
    _flags.buzzerEnabled = true;
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
}

void ApaSense::setBuzzer(bool on) {
    if (!_flags.buzzerEnabled) return;
    _beepEndMs = 0;
    digitalWrite(_buzzerPin, on ? HIGH : LOW);
}

void ApaSense::beep(uint16_t ms) {
    if (!_flags.buzzerEnabled || ms == 0) return;
    _beepEndMs = millis() + ms;
    digitalWrite(_buzzerPin, HIGH);
}

// ── Private: I2C helpers ──────────────────────────────────────────────────────

void ApaSense::_writeRegister(uint8_t reg, uint16_t value) {
    Wire.beginTransmission(_adsAddr);
    Wire.write(reg);
    Wire.write((uint8_t)(value >> 8));
    Wire.write((uint8_t)(value & 0xFF));
    Wire.endTransmission();
}

uint16_t ApaSense::_readRegister(uint8_t reg) {
    Wire.beginTransmission(_adsAddr);
    Wire.write(reg);
    Wire.endTransmission();
    if (Wire.requestFrom(_adsAddr, (uint8_t)2) != 2) return 0;
    uint16_t val = ((uint16_t)Wire.read() << 8) | (uint8_t)Wire.read();
    return val;
}

void ApaSense::_startConversion(uint8_t channel) {
    // MUX: single-ended AIN0-3 → bits 14-12 = 4,5,6,7 for channels 0-3
    uint16_t config = ADS_CONFIG_BASE | ((uint16_t)(4 + channel) << 12);
    _writeRegister(ADS_REG_CONFIG, config);
}

int16_t ApaSense::_readConversion() {
    // Result is MSB-aligned 12-bit signed in a 16-bit register; shift right 4
    return (int16_t)_readRegister(ADS_REG_CONVERT) >> 4;
}

void ApaSense::_processADC(uint8_t sensor, int16_t raw) {
    switch (sensor) {
        case 0:
            _rawPressure = raw;
            break;
        case 1: {
            int16_t  centered = raw - _currentZero;
            _currentSumSq += (uint32_t)((int32_t)centered * centered);
            _currentSampleN++;
            if (_currentSampleN >= APASENSE_RMS_SAMPLES) {
                // I_rms = sqrt(sumSq/N) × (Vref/2048) / sensitivity
                _currentRms = sqrtf((float)_currentSumSq / (float)APASENSE_RMS_SAMPLES)
                              * (6.144f / 2048.0f) / _currentSensitivity;
                _currentSumSq   = 0;
                _currentSampleN = 0;
                _flags.currentCalibrated = true;
            }
            break;
        }
        case 2:
            _rawAux = raw;
            break;
        case 3:
            _rawLdr = raw;
            break;
    }
}

void ApaSense::_writePCF() {
    if (_pcfAddr == APASENSE_NO_PCF) return;
    Wire.beginTransmission(_pcfAddr);
    Wire.write(_pcfBState);
    Wire.endTransmission();
}

// ── Private: EEPROM ───────────────────────────────────────────────────────────

void ApaSense::_loadEEPROM() {
    EepromData d;
    EEPROM.get(APASENSE_EEPROM_ADDR, d);

    uint8_t chk = 0;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&d);
    for (uint8_t i = 0; i < sizeof(d) - 1; i++) chk += p[i];

    if (d.magic == APASENSE_EEPROM_MAGIC &&
        d.version == APASENSE_EEPROM_VERSION &&
        d.checksum == chk) {
        _pressureZero = d.pressureZero;
        _flags.pressureCalibrated = true;
    }
}

void ApaSense::_saveEEPROM() {
    EepromData d;
    d.magic        = APASENSE_EEPROM_MAGIC;
    d.version      = APASENSE_EEPROM_VERSION;
    d.pressureZero = _pressureZero;

    uint8_t chk = 0;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&d);
    for (uint8_t i = 0; i < sizeof(d) - 1; i++) chk += p[i];
    d.checksum = chk;

    EEPROM.put(APASENSE_EEPROM_ADDR, d);

#if defined(ESP32) || defined(ESP8266)
    EEPROM.commit();
#endif
}
