#pragma once

#include <Arduino.h>
#include "esp_err.h"

class C5BF3901Camera {
public:
  static constexpr uint16_t WIDTH = 240;
  static constexpr uint16_t HEIGHT = 320;
  static constexpr size_t RGB565_BYTES = WIDTH * HEIGHT * 2;

  // Call before the LCD is initialized.  This configures the sensor over
  // SCCB and then releases the temporary camera I2C bus.
  esp_err_t beginSensor();

  // Call after the LCD SPI bus is initialized.  The C5 camera driver is a
  // special SPI slave implementation designed to coexist with that bus.
  esp_err_t beginCapture();

  // Receives and decodes one complete RGB565 frame into dst.
  esp_err_t capture(uint8_t *dst, size_t dstSize, uint32_t timeoutMs = 1000);

  const char *lastStep() const { return last_step_; }
  size_t rawFrameBytes() const { return raw_bytes_; }

private:
  void setStep(const char *step) { last_step_ = step; }
  void releaseSensorBus();
  const char *last_step_ = "not started";
  void *sensor_ = nullptr;
  void *controller_ = nullptr;
  void *i2c_bus_ = nullptr;
  void *sccb_ = nullptr;
  uint8_t *raw_frame_ = nullptr;
  size_t raw_bytes_ = 0;
};
