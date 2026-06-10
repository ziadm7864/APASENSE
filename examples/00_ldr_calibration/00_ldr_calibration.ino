// APASENSE — Example 00: LDR calibration helper
//
// Run this sketch once to measure the raw ADC counts for your specific
// LDR circuit. Use the printed values in your main sketch:
//
//   adc.enableLDR(3, <dark_value>, <sun_value>);
//
// The values depend on your LDR model, the R16 pot setting on the APA
// sensing board, and your lighting conditions. Once measured, they are
// stable — you do not need to repeat this calibration unless you
// adjust the pot or replace the LDR.
//
// Steps:
//   1. Upload this sketch and open Serial Monitor at 115200 baud.
//   2. Cover the LDR completely (total darkness). Note the printed value.
//   3. Expose the LDR to full direct sunlight. Note the printed value.
//   4. Copy both values into your main sketch as shown below.
//
// Hardware: ADS1015 at default address 0x4B, LDR on AIN3.

#include <APASENSE.h>

ApaSense adc;

void setup() {
    Serial.begin(115200);
    while (!Serial) {}  // wait for USB-CDC on Leonardo / Due

    // Enable only LDR — no other channels needed for calibration
    adc.enableLDR();    // AIN3, default range (will be overridden after calibration)
    adc.begin();

    Serial.println(F("===================================="));
    Serial.println(F("  APASENSE — LDR Calibration Tool"));
    Serial.println(F("===================================="));
    Serial.println();
    Serial.println(F("Watch the raw ADC count below."));
    Serial.println(F("  1. Cover LDR completely  → note the DARK value"));
    Serial.println(F("  2. Expose to full sun    → note the SUN value"));
    Serial.println();
    Serial.println(F("Then use them in your sketch:"));
    Serial.println(F("  adc.enableLDR(3, <dark>, <sun>);"));
    Serial.println();
    Serial.println(F("--- Raw LDR count (updates every 500 ms) ---"));
}

void loop() {
    adc.update();

    static uint32_t lastPrint = 0;
    if (millis() - lastPrint >= 500) {
        lastPrint = millis();
        Serial.print(F("Raw count: "));
        Serial.println(adc.getRawLDR());
    }
}
