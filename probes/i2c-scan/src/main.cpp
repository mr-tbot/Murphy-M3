// Murphy M3 I2C bus probe.
//
// Expected devices on the shared bus (SDA=13, SCL=12, 100 kHz), per the
// community reverse schematic (Murphy repo PR #2) and freeink-sdk BoardConfig:
//   0x10/0x11  ES8388 audio codec (power-gated by GPIO43, HIGH = on)
//   0x2e       CHSC6x-class touch (power-gated by GPIO45)
//   0x32       Epson RX8010SJ RTC
//   0x38       AHT30 temp/humidity
//
// The sketch scans with the power gates in every combination so we learn each
// device's gate dependency, then dumps a few identifying registers.

#include <Arduino.h>
#include <Wire.h>

constexpr int SDA_PIN = 13;
constexpr int SCL_PIN = 12;
constexpr int ES8388_POWER = 43;  // HIGH = codec LDO on (OEM keeps it high)
constexpr int TOUCH_POWER = 45;   // polarity unconfirmed — probe tries both

void scanBus(const char* label) {
  Serial.printf("--- scan [%s]: ", label);
  int found = 0;
  for (uint8_t a = 0x03; a <= 0x77; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.printf("0x%02x ", a);
      found++;
    }
  }
  Serial.printf("(%d found)\n", found);
}

void dumpReg(uint8_t addr, uint8_t reg, const char* name) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    Serial.printf("  %s @0x%02x reg 0x%02x: NACK\n", name, addr, reg);
    return;
  }
  if (Wire.requestFrom(addr, (uint8_t)1) == 1) {
    Serial.printf("  %s @0x%02x reg 0x%02x = 0x%02x\n", name, addr, reg, Wire.read());
  } else {
    Serial.printf("  %s @0x%02x reg 0x%02x: no data\n", name, addr, reg);
  }
}

void setup() {
  Serial.begin(115200);
  delay(3000);  // let USB CDC enumerate
  Serial.println("\n=== Murphy M3 I2C probe ===");

  pinMode(ES8388_POWER, OUTPUT);
  pinMode(TOUCH_POWER, OUTPUT);
  Wire.begin(SDA_PIN, SCL_PIN, 100000);
}

void loop() {
  digitalWrite(ES8388_POWER, LOW);
  digitalWrite(TOUCH_POWER, LOW);
  delay(150);
  scanBus("es8388=OFF touch=LOW");

  digitalWrite(ES8388_POWER, HIGH);
  delay(150);
  scanBus("es8388=ON  touch=LOW");

  digitalWrite(TOUCH_POWER, HIGH);
  delay(150);
  scanBus("es8388=ON  touch=HIGH");

  // Identify registers while everything is powered.
  dumpReg(0x10, 0x00, "ES8388 CHIPCTL1");
  dumpReg(0x32, 0x10, "RX8010 reg16");
  dumpReg(0x38, 0x71, "AHT30 status");
  dumpReg(0x2e, 0x00, "touch  reg0");

  Serial.println();
  delay(4000);
}
