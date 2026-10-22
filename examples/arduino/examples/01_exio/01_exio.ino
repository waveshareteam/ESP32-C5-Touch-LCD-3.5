#include <Arduino.h>
#include <Wire.h>

#include "io_extension.h"

// ESP32-C5-Touch-LCD-3.5: SCL = GPIO26, SDA = GPIO27.
static constexpr uint8_t I2C_SDA = 27;
static constexpr uint8_t I2C_SCL = 26;
static constexpr uint32_t I2C_FREQ = 400000;

static constexpr uint8_t FIRST_OUTPUT_PIN = IO_EXTENSION_IO_8;
static constexpr uint8_t LAST_OUTPUT_PIN = IO_EXTENSION_IO_15;

static bool outputLevel = false;
static bool ioReady = false;

static bool i2cDevicePresent(uint8_t address)
{
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

static bool setOutputRange(uint8_t firstPin, uint8_t lastPin, bool level)
{
  for (uint8_t pin = firstPin; pin <= lastPin; pin++) {
    if (!IO_EXTENSION_Output(pin, level)) {
      return false;
    }
  }
  return true;
}

void setup()
{
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("IO extension test");

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(I2C_FREQ);

  if (!i2cDevicePresent(IO_EXTENSION_ADDR)) {
    Serial.printf("IO extension not found at 0x%02X. Check SDA/SCL and power.\r\n", IO_EXTENSION_ADDR);
    return;
  }

  if (!IO_EXTENSION_Init(Wire)) {
    Serial.println("IO extension init failed.");
    return;
  }

  // CH32V006: 1 = output. EXIO0-7 stay inputs, as in the IDF EXIO test.
  if (!IO_EXTENSION_IO_Mode(0xFF00)) {
    Serial.println("IO extension direction setup failed.");
    return;
  }
  ioReady = true;

  Serial.println("EXIO8~EXIO15 will toggle every second.");
}

void loop()
{
  if (!ioReady) {
    delay(1000);
    return;
  }

  if (!setOutputRange(FIRST_OUTPUT_PIN, LAST_OUTPUT_PIN, outputLevel)) {
    Serial.println("IO extension write failed; test stopped.");
    ioReady = false;
    return;
  }

  Serial.printf("EXIO8~EXIO15 output: %s\r\n", outputLevel ? "HIGH" : "LOW");
  outputLevel = !outputLevel;
  delay(1000);
}
