#include "Board_IO.h"

static TwoWire *io_wire = &Wire;
static uint8_t io_output = 0;
static bool io_ready = false;

static bool write_reg(uint8_t reg, const uint8_t *data, size_t len) {
  io_wire->beginTransmission(BOARD_IO_EXTENSION_ADDR);
  io_wire->write(reg);
  io_wire->write(data, len);
  return io_wire->endTransmission() == 0;
}
static bool write_u8(uint8_t reg, uint8_t value) {
  return write_reg(reg, &value, 1);
}
bool Board_IO_Init(TwoWire &wire) {
  io_wire = &wire;
  io_wire->begin(BOARD_I2C_SDA_PIN, BOARD_I2C_SCL_PIN);
  io_wire->setClock(BOARD_I2C_FREQ_HZ);
  // Match the working IDF CH32V003 driver exactly: the register bank is 8-bit,
  // and direction bit 1 means output on this device.
  io_ready = write_u8(BOARD_IO_EXTENSION_MODE_REG, 0xFF) &&
             write_u8(BOARD_IO_EXTENSION_OUTPUT_REG, 0x00) &&
             write_u8(BOARD_IO_EXTENSION_PWM_REG, 0x00) &&
             write_u8(BOARD_IO_EXTENSION_MODE_REG,
                      (1U << BOARD_TP_RST_EXIO) |
                      (1U << BOARD_LCD_RST_EXIO) |
                      (1U << BOARD_POWER_EXIO));
  if (!io_ready) return false;

  io_output = (1U << BOARD_TP_RST_EXIO) | (1U << BOARD_LCD_RST_EXIO);
  io_ready = write_u8(BOARD_IO_EXTENSION_OUTPUT_REG, io_output);
  delay(50);
  io_output = 0;
  io_ready = io_ready && write_u8(BOARD_IO_EXTENSION_OUTPUT_REG, io_output);
  delay(50);
  io_output = (1U << BOARD_TP_RST_EXIO) |
              (1U << BOARD_LCD_RST_EXIO) |
              (1U << BOARD_POWER_EXIO);
  io_ready = io_ready && write_u8(BOARD_IO_EXTENSION_OUTPUT_REG, io_output);
  return io_ready;
}
bool Board_IO_SetOutput(uint8_t pin, bool level) {
  if (pin > 15 || (!io_ready && !Board_IO_Init(*io_wire))) return false;
  if (level) io_output |= (1U << pin); else io_output &= ~(1U << pin);
  return write_u8(BOARD_IO_EXTENSION_OUTPUT_REG, io_output);
}
bool Board_SetBacklight(uint8_t percent) {
  if (!io_ready && !Board_IO_Init(*io_wire)) return false;
  if (percent > 100) percent = 100;
  uint8_t pwm = (uint8_t)(percent * 255U / 100U);
  return write_reg(BOARD_IO_EXTENSION_PWM_REG, &pwm, 1);
}
void Board_LCD_Reset(void) {
  Board_IO_SetOutput(BOARD_LCD_RST_EXIO, true); delay(10);
  Board_IO_SetOutput(BOARD_LCD_RST_EXIO, false); delay(20);
  Board_IO_SetOutput(BOARD_LCD_RST_EXIO, true); delay(120);
}
void Board_Touch_Reset(void) {
  Board_IO_SetOutput(BOARD_TP_RST_EXIO, true); delay(10);
  Board_IO_SetOutput(BOARD_TP_RST_EXIO, false); delay(20);
  Board_IO_SetOutput(BOARD_TP_RST_EXIO, true); delay(120);
}
