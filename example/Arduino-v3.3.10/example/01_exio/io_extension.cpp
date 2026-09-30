#include "io_extension.h"

struct IoExtensionState {
  TwoWire *wire;
  uint16_t lastIoValue;
};

static IoExtensionState ioExtension = {
  &Wire,
  0x00,
};

static bool writeBytes(const uint8_t *data, size_t len)
{
  if (ioExtension.wire == nullptr || data == nullptr || len == 0) {
    return false;
  }

  ioExtension.wire->beginTransmission(IO_EXTENSION_ADDR);
  ioExtension.wire->write(data, len);
  return ioExtension.wire->endTransmission() == 0;
}

static bool readBytes(uint8_t reg, uint8_t *data, size_t len)
{
  if (ioExtension.wire == nullptr || data == nullptr || len == 0) {
    return false;
  }

  ioExtension.wire->beginTransmission(IO_EXTENSION_ADDR);
  ioExtension.wire->write(reg);
  if (ioExtension.wire->endTransmission(false) != 0) {
    return false;
  }

  if (ioExtension.wire->requestFrom(IO_EXTENSION_ADDR, static_cast<uint8_t>(len)) != len) {
    return false;
  }

  for (size_t i = 0; i < len; i++) {
    data[i] = ioExtension.wire->read();
  }

  return true;
}

static bool writeWordRegister(uint8_t reg, uint16_t value)
{
  // The 16-pin firmware expects EXIO0-7 first, then EXIO8-15.
  const uint8_t data[3] = {
    reg,
    static_cast<uint8_t>(value & 0xFF),
    static_cast<uint8_t>(value >> 8),
  };

  return writeBytes(data, sizeof(data));
}

static bool readByteRegister(uint8_t reg, uint8_t *value)
{
  uint8_t data = 0;

  if (value == nullptr || !readBytes(reg, &data, sizeof(data))) {
    return false;
  }

  *value = data;
  return true;
}

bool IO_EXTENSION_Init(TwoWire &wire)
{
  ioExtension.wire = &wire;
  ioExtension.lastIoValue = 0x00;

  if (!IO_EXTENSION_IO_Mode(0x0000)) {
    return false;
  }

  return writeWordRegister(IO_EXTENSION_IO_OUTPUT_ADDR, ioExtension.lastIoValue);
}

bool IO_EXTENSION_IO_Mode(uint16_t pin_mask)
{
  return writeWordRegister(IO_EXTENSION_Mode, pin_mask);
}

bool IO_EXTENSION_Output(uint8_t pin, uint8_t value)
{
  if (pin > IO_EXTENSION_IO_15) {
    return false;
  }

  uint16_t nextValue = ioExtension.lastIoValue;
  if (value) {
    nextValue |= (1U << pin);
  } else {
    nextValue &= ~(1U << pin);
  }

  if (!writeWordRegister(IO_EXTENSION_IO_OUTPUT_ADDR, nextValue)) {
    return false;
  }
  ioExtension.lastIoValue = nextValue;
  return true;
}

uint8_t IO_EXTENSION_Input(uint8_t pin)
{
  uint8_t data[2] = {0, 0};

  if (pin > IO_EXTENSION_IO_15 || !readBytes(IO_EXTENSION_IO_INPUT_ADDR, data, sizeof(data))) {
    return 0;
  }

  const uint16_t value = static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);
  return (value & (1U << pin)) ? 1 : 0;
}

bool IO_EXTENSION_Pwm_Output(uint8_t value)
{
  if (value > 97) {
    value = 97;
  }

  const uint8_t data[2] = {
    IO_EXTENSION_PWM_ADDR,
    static_cast<uint8_t>(value * 255.0f / 100.0f),
  };

  return writeBytes(data, sizeof(data));
}

uint16_t IO_EXTENSION_Adc_Input()
{
  uint16_t value = 0;
  uint8_t data[2] = {0, 0};
  if (readBytes(IO_EXTENSION_ADC_ADDR, data, sizeof(data))) {
    value = static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);
  }
  return value;
}

uint8_t IO_EXTENSION_RTC_INT_READ()
{
  uint8_t value = 0;
  readByteRegister(IO_EXTENSION_RTC_INT_ADDR, &value);
  return value;
}
