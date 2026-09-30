/*****************************************************************************
 * | File         :   io_extension.c
 * | Author       :   Waveshare team
 * | Function     :   IO_EXTENSION GPIO control via I2C interface
 * | Info         :
 * |                 I2C driver code for controlling GPIO pins using IO_EXTENSION chip.
 * ----------------
 * | This version :   V1.0
 * | Date         :   2024-11-27
 * | Info         :   Basic version, includes functions to read and write 
 * |                 GPIO pins using I2C communication with IO_EXTENSION.
 *
 ******************************************************************************/
#include "io_extension.h"  // Include IO_EXTENSION driver header for GPIO functions

#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
static const char *TAG = "io exio";

io_extension_obj_t IO_EXTENSION;  // Define the global IO_EXTENSION object
static i2c_master_bus_handle_t  s_i2c_bus  = NULL;
static i2c_master_dev_handle_t  s_i2c_dev  = NULL;

/**
 * @brief Initialize the IO_EXTENSION device.
 * 
 * This function configures the slave addresses for different registers of the
 * IO_EXTENSION chip via I2C, and sets the control flags for input/output modes.
 */
esp_err_t IO_EXTENSION_Init(i2c_port_num_t port_num)
{

    ESP_RETURN_ON_ERROR(i2c_master_get_bus_handle(port_num, &s_i2c_bus), TAG, "get bus failed");
    const i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = IO_EXTENSION_ADDR,
        .scl_speed_hz    = 100000,   // 根据需要修改
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(s_i2c_bus, &dev_cfg, &s_i2c_dev), TAG, "add dev failed");

    // Load the initial levels before enabling only EXIO8-15 as outputs.
    // EXIO0-7 remain inputs throughout this test.
    esp_err_t ret = IO_EXTENSION_IO_Mode(0x0000);
    if (ret == ESP_OK) {
        const uint8_t data[] = {IO_EXTENSION_IO_OUTPUT_ADDR, 0x00, 0x00};
        ret = i2c_master_transmit(s_i2c_dev, data, sizeof(data), 1000);
    }
    if (ret == ESP_OK) {
        ret = IO_EXTENSION_IO_Mode(0xFF00);
    }
    if (ret != ESP_OK) {
        i2c_master_bus_rm_device(s_i2c_dev);
        s_i2c_dev = NULL;
        ESP_RETURN_ON_ERROR(ret, TAG, "configure EXIO failed");
    }
    IO_EXTENSION.Last_io_value = 0x0000;

    return ESP_OK;
}

/**
 * @brief 写 1 字节寄存器
 * 等价于:
 *   start + [addr+W] + Cmd + value + stop
 */
static void DEV_I2C_Write_Byte(uint8_t Cmd, uint8_t value)
{
    uint8_t buf[2] = { Cmd, value };

    esp_err_t ret = i2c_master_transmit(s_i2c_dev, buf, sizeof(buf), -1);
    if (ret != ESP_OK) {
        printf("The I2C transmission fails. - I2C Write, err=0x%x\r\n", ret);
    }
}

/**
 * @brief 读 16bit 寄存器
 * 等价于:
 *   start + [addr+W] + Cmd + repeated start + [addr+R] + data0 + data1 + stop
 * 返回: (data1 << 8) | data0
 */
static uint16_t DEV_I2C_Read_Word(uint8_t Cmd)
{
    uint8_t cmd = Cmd;
    uint8_t data[2] = {0};

    esp_err_t ret = i2c_master_transmit_receive(
                        s_i2c_dev,
                        &cmd, 1,        // 写 1 字节命令
                        data, 2,        // 读 2 字节
                        -1);
    if (ret != ESP_OK) {
        printf("I2C Read Word failed, err=0x%x\r\n", ret);
        return 0;
    }

    return (uint16_t)((data[1] << 8) | data[0]);
}

/**
 * @brief 连续写 N 字节
 * 等价于:
 *   start + [addr+W] + pdata[0..len-1] + stop
 * 注意: pdata 内部应包含寄存器地址 + 数据，和你原来的函数一致
 */
static void DEV_I2C_Write_Nbyte(uint8_t *pdata, uint8_t len)
{
    esp_err_t ret = i2c_master_transmit(s_i2c_dev, pdata, len, -1);
    if (ret != ESP_OK) {
        printf("I2C Write Nbyte failed! err=0x%x\r\n", ret);
    }
}

/**
 * @brief 先写寄存器地址，再读 N 字节
 * 等价于:
 *   start + [addr+W] + Cmd + repeated start + [addr+R] + pdata[0..len-1] + stop
 */
static void DEV_I2C_Read_Nbyte(uint8_t Cmd, uint8_t *pdata, uint8_t len)
{
    uint8_t cmd = Cmd;

    esp_err_t ret = i2c_master_transmit_receive(
                        s_i2c_dev,
                        &cmd, 1,        // 写 1 字节寄存器地址
                        pdata, len,     // 读 len 字节
                        -1);
    if (ret != ESP_OK) {
        printf("I2C Read Nbyte failed! err=0x%x\r\n", ret);
    }
}

/**
 * @brief Set the IO mode for the specified pins.
 *
 * This function configures the direction of IO extension pins by writing a
 * 16-bit mode mask to the mode register.
 *
 * Each bit in the mask represents one pin:
 *   - bit = 0 : configure the pin as input
 *   - bit = 1 : configure the pin as output
 *
 * Bit mapping:
 *   bit0  -> IO0
 *   bit1  -> IO1
 *   ...
 *   bit15 -> IO15
 *
 * @param mode_mask A 16-bit value where each bit represents the mode of a pin
 *                  (0 = input, 1 = output).
 */

esp_err_t IO_EXTENSION_IO_Mode(uint16_t pin_mask)
{
    // IO_EXTENSION.Mode_value = pin_mask;

    uint8_t data[3];
    data[0] = IO_EXTENSION_Mode;                         // mode register addr
    data[1] = (uint8_t)(pin_mask & 0xFF);               // low byte IO0~7
    data[2] = (uint8_t)((pin_mask >> 8) & 0xFF);        // high byte IO8~15

    return i2c_master_transmit(s_i2c_dev, data, sizeof(data), 1000);
}

/**
 * @brief Set the value of the IO output pins on the IO_EXTENSION device.
 * 
 * This function writes a 16-bit value to the IO output register. The value
 * determines the high or low state of the pins.
 * 
 * @param pin The pin number to set (0-15).
 * @param value The value to set on the specified pin (0 = low, 1 = high).
 */
esp_err_t IO_EXTENSION_Output(uint8_t pin, uint8_t value)
{
    if (pin > 15) return ESP_ERR_INVALID_ARG;
    uint16_t next_value = IO_EXTENSION.Last_io_value;

    if (value)
        next_value |=  (1U << pin);   // Set high
    else
        next_value &= ~(1U << pin);   // Set low

    uint8_t data[3];
    data[0] = IO_EXTENSION_IO_OUTPUT_ADDR;                 // 输出寄存器地址
    data[1] = (uint8_t)(next_value & 0xFF);        // 低字节
    data[2] = (uint8_t)((next_value >> 8) & 0xFF); // 高字节

    ESP_RETURN_ON_ERROR(i2c_master_transmit(s_i2c_dev, data, sizeof(data), 1000), TAG, "write output failed");
    IO_EXTENSION.Last_io_value = next_value;
    return ESP_OK;
}

/**
 * @brief Read the value from the IO input pins on the IO_EXTENSION device.
 * 
 * This function reads the value of the IO input register and returns the state
 * of the specified pins.
 * 
 * @param pin The bit mask to specify which pin to read (e.g., 0x01 for the first pin).
 * @return The value of the specified pin(s) (0 = low, 1 = high).
 */
uint8_t IO_EXTENSION_Input(uint8_t pin) 
{
    if (pin > 15) return 0;  // 防止越界

    uint8_t buf[2] = {0};
    uint16_t value = 0;

    // Read 16-bit input register (2 bytes)
    DEV_I2C_Read_Nbyte(IO_EXTENSION_IO_INPUT_ADDR, buf, 2);

    // 低字节在前，高字节在后（如与你芯片手册不符请交换）
    value = (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);

    return (value & (1U << pin)) ? 1 : 0;
}


/**
 * @brief Set the PWM output value on the IO_EXTENSION device.
 * 
 * This function sets the PWM output value, which controls the duty cycle of the PWM signal.
 * The duty cycle is calculated based on the input value and the resolution (12 bits).
 * 
 * @param Value The input value to set the PWM duty cycle (0-100).
 */
void IO_EXTENSION_Pwm_Output(uint8_t Value)
{
    // Prevent the screen from completely turning off
    if (Value >= 97)
    {
        Value = 97;
    }

    uint8_t data[2] = {IO_EXTENSION_PWM_ADDR, Value}; // Prepare the data to write to the PWM register
    // Calculate the duty cycle based on the resolution (12 bits)
    data[1] = Value * (255 / 100.0);
    // Write the 8-bit value to the PWM output register
    DEV_I2C_Write_Nbyte(data, 2);
}

/**
 * @brief Read the ADC input value from the IO_EXTENSION device.
 * 
 * This function reads the ADC input value from the IO_EXTENSION device.
 * 
 * @return The ADC input value.
 */
uint16_t IO_EXTENSION_Adc_Input()
{
    // Read the ADC input value from the IO_EXTENSION device
    return DEV_I2C_Read_Word( IO_EXTENSION_ADC_ADDR);
}

uint8_t IO_EXTENSION_RTC_INT_READ()
{
    // Read the ADC input value from the IO_EXTENSION device
    return DEV_I2C_Read_Word(IO_EXTENSION_RTC_INT_ADDR);
}
