#include "C5_BF3901_Camera.h"

#include "bf3901.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "esp_cam_ctlr.h"
#include "esp_cam_ctlr_spi.h"
#include "esp_cam_sensor.h"
#include "esp_cam_sensor_types.h"
#include "esp_heap_caps.h"
extern "C" {
#include "esp_sccb_i2c.h"
}

namespace {
constexpr gpio_num_t CAM_XCLK = GPIO_NUM_0;
constexpr gpio_num_t CAM_SDA = GPIO_NUM_4;
constexpr gpio_num_t CAM_SCL = GPIO_NUM_5;
constexpr gpio_num_t CAM_SCLK = GPIO_NUM_6;
constexpr gpio_num_t CAM_DATA = GPIO_NUM_7;
constexpr gpio_num_t CAM_CS = GPIO_NUM_10;

esp_err_t start_xclk() {
  ledc_timer_config_t timer = {};
  timer.speed_mode = LEDC_LOW_SPEED_MODE;
  timer.duty_resolution = LEDC_TIMER_1_BIT;
  timer.timer_num = LEDC_TIMER_0;
  timer.freq_hz = 24000000;
  timer.clk_cfg = LEDC_AUTO_CLK;
  esp_err_t err = ledc_timer_config(&timer);
  if (err != ESP_OK) return err;

  ledc_channel_config_t channel = {};
  channel.gpio_num = CAM_XCLK;
  channel.speed_mode = LEDC_LOW_SPEED_MODE;
  channel.channel = LEDC_CHANNEL_0;
  channel.intr_type = LEDC_INTR_DISABLE;
  channel.timer_sel = LEDC_TIMER_0;
  channel.duty = 1;
  channel.hpoint = 0;
  err = ledc_channel_config(&channel);
  if (err == ESP_OK) {
    Serial.printf("Camera XCLK: %lu Hz\n", (unsigned long)ledc_get_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0));
  }
  return err;
}
} // namespace

void C5BF3901Camera::releaseSensorBus() {
  if (sccb_) {
    esp_sccb_del_i2c_io(static_cast<esp_sccb_io_handle_t>(sccb_));
    sccb_ = nullptr;
  }
  if (i2c_bus_) {
    i2c_del_master_bus(static_cast<i2c_master_bus_handle_t>(i2c_bus_));
    i2c_bus_ = nullptr;
  }
}

esp_err_t C5BF3901Camera::beginSensor() {
  setStep("XCLK");
  esp_err_t err = start_xclk();
  if (err != ESP_OK) return err;
  delay(20);

  setStep("camera SCCB bus");
  i2c_master_bus_config_t bus_cfg = {};
  bus_cfg.i2c_port = I2C_NUM_0;
  bus_cfg.sda_io_num = CAM_SDA;
  bus_cfg.scl_io_num = CAM_SCL;
  bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
  bus_cfg.glitch_ignore_cnt = 7;
  bus_cfg.flags.enable_internal_pullup = true;
  i2c_master_bus_handle_t bus = nullptr;
  err = i2c_new_master_bus(&bus_cfg, &bus);
  if (err != ESP_OK) return err;
  i2c_bus_ = bus;

  Serial.print("Camera SCCB scan:");
  bool found = false;
  for (uint8_t address = 3; address < 0x78; ++address) {
    if (i2c_master_probe(bus, address, 20) == ESP_OK) {
      Serial.printf(" 0x%02X", address);
      found = true;
    }
  }
  Serial.println(found ? "" : " no device");

  sccb_i2c_config_t sccb_cfg = {};
  sccb_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  // Keep the address convention used by esp_cam_sensor 2.1.0 and the
  // working C5 ESP-IDF factory project.
  sccb_cfg.device_address = BF3901_SCCB_ADDR;
  sccb_cfg.scl_speed_hz = 100000;
  sccb_cfg.addr_bits_width = 8;
  sccb_cfg.val_bits_width = 8;
  esp_sccb_io_handle_t sccb = nullptr;
  err = sccb_new_i2c_io(bus, &sccb_cfg, &sccb);
  if (err != ESP_OK) {
    releaseSensorBus();
    return err;
  }
  sccb_ = sccb;

  setStep("BF3901 detect");
  esp_cam_sensor_config_t sensor_cfg = {};
  sensor_cfg.sccb_handle = sccb;
  sensor_cfg.reset_pin = GPIO_NUM_NC;
  sensor_cfg.pwdn_pin = GPIO_NUM_NC;
  sensor_cfg.xclk_pin = CAM_XCLK;
  sensor_cfg.xclk_freq_hz = 24000000;
  sensor_cfg.sensor_port = ESP_CAM_SENSOR_SPI;
  esp_cam_sensor_device_t *sensor = bf3901_detect(&sensor_cfg);
  if (!sensor) {
    releaseSensorBus();
    return ESP_ERR_NOT_FOUND;
  }
  sensor_ = sensor;

  setStep("BF3901 RGB565 format");
  err = esp_cam_sensor_set_format(sensor, nullptr);
  if (err != ESP_OK) {
    releaseSensorBus();
    return err;
  }

  esp_cam_sensor_format_t format = {};
  err = esp_cam_sensor_get_format(sensor, &format);
  if (err != ESP_OK) {
    releaseSensorBus();
    return err;
  }
  if (format.width != WIDTH || format.height != HEIGHT ||
      format.format != ESP_CAM_SENSOR_PIXFORMAT_RGB565_LE ||
      !format.spi_info.frame_info) {
    releaseSensorBus();
    return ESP_ERR_NOT_SUPPORTED;
  }
  raw_bytes_ = format.spi_info.frame_info->frame_size;

  setStep("BF3901 stream");
  int stream_on = 1;
  err = esp_cam_sensor_ioctl(sensor, ESP_CAM_SENSOR_IOC_S_STREAM, &stream_on);
  if (err != ESP_OK) {
    releaseSensorBus();
    return err;
  }

  // No further sensor-register accesses are needed.  Release I2C0 so the
  // board I/O expander and FT6336 can use their normal GPIO27/26 bus again.
  releaseSensorBus();
  setStep("sensor ready");
  return ESP_OK;
}

esp_err_t C5BF3901Camera::beginCapture() {
  if (!sensor_ || raw_bytes_ == 0) return ESP_ERR_INVALID_STATE;
  auto *sensor = static_cast<esp_cam_sensor_device_t *>(sensor_);
  esp_cam_sensor_format_t format = {};
  esp_err_t err = esp_cam_sensor_get_format(sensor, &format);
  if (err != ESP_OK) return err;

  setStep("SPI camera controller");
  esp_cam_ctlr_spi_config_t cfg = {};
  cfg.intf = ESP_CAM_CTLR_SPI_CAM_INTF_SPI;
  cfg.io_mode = ESP_CAM_CTLR_SPI_CAM_IO_MODE_1BIT;
  cfg.spi_port = SPI2_HOST;
  cfg.spi_cs_pin = CAM_CS;
  cfg.spi_sclk_pin = CAM_SCLK;
  cfg.spi_data0_io_pin = CAM_DATA;
  cfg.spi_data1_io_pin = GPIO_NUM_NC;
  cfg.spi_data2_io_pin = GPIO_NUM_NC;
  cfg.spi_data3_io_pin = GPIO_NUM_NC;
  cfg.reset_pin = GPIO_NUM_NC;
  cfg.pwdn_pin = GPIO_NUM_NC;
  cfg.h_res = WIDTH;
  cfg.v_res = HEIGHT;
  cfg.input_data_color_type = CAM_CTLR_COLOR_RGB565;
  cfg.frame_info = format.spi_info.frame_info;
  cfg.frame_buffer_count = 1;
  cfg.bk_buffer_dis = 1;
  cfg.auto_decode_dis = 1;
  esp_cam_ctlr_handle_t ctlr = nullptr;
  err = esp_cam_new_spi_ctlr(&cfg, &ctlr);
  if (err != ESP_OK) return err;
  controller_ = ctlr;

  raw_frame_ = static_cast<uint8_t *>(
      esp_cam_ctlr_alloc_buffer(ctlr, raw_bytes_, MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA));
  if (!raw_frame_) return ESP_ERR_NO_MEM;

  err = esp_cam_ctlr_enable(ctlr);
  if (err != ESP_OK) return err;
  err = esp_cam_ctlr_start(ctlr);
  if (err != ESP_OK) return err;
  setStep("capture ready");
  return ESP_OK;
}

esp_err_t C5BF3901Camera::capture(uint8_t *dst, size_t dstSize, uint32_t timeoutMs) {
  if (!controller_ || !raw_frame_ || !dst || dstSize < RGB565_BYTES) {
    return ESP_ERR_INVALID_ARG;
  }
  auto ctlr = static_cast<esp_cam_ctlr_handle_t>(controller_);
  esp_cam_ctlr_trans_t trans = {};
  trans.buffer = raw_frame_;
  trans.buflen = raw_bytes_;
  esp_err_t err = esp_cam_ctlr_receive(ctlr, &trans, timeoutMs);
  if (err != ESP_OK) return err;

  uint32_t decoded = 0;
  err = esp_cam_spi_decode_frame(ctlr, raw_frame_, trans.received_size,
                                 dst, dstSize, &decoded);
  if (err != ESP_OK) return err;
  return decoded == RGB565_BYTES ? ESP_OK : ESP_ERR_INVALID_SIZE;
}
