/*
 * ESP32-C5-Touch-LCD-3.5 AXP2101 example
 *
 * Ported from the ESP32-S3 example and aligned with the official
 * ESP32-C5-Touch-LCD-3.5 ESP-IDF 08_AXP2101 example.
 *
 * This example intentionally does not change or disable any DCDC/LDO rail.
 */

#include <Arduino.h>
#include <Wire.h>

#define XPOWERS_CHIP_AXP2101
#include <XPowersLib.h>

static constexpr uint8_t PMU_I2C_SDA = 27;
static constexpr uint8_t PMU_I2C_SCL = 26;
static constexpr uint32_t PMU_I2C_FREQ_HZ = 400000;
static constexpr uint32_t STATUS_INTERVAL_MS = 5000;

static XPowersPMU power;
static uint32_t last_status_ms = 0;

static void print_power_rails()
{
  Serial.println("DCDC ======================================================================");
  Serial.printf("DC1     %s  %u mV\n", power.isEnableDC1() ? "ON " : "OFF", power.getDC1Voltage());
  Serial.printf("DC2     %s  %u mV\n", power.isEnableDC2() ? "ON " : "OFF", power.getDC2Voltage());
  Serial.printf("DC3     %s  %u mV\n", power.isEnableDC3() ? "ON " : "OFF", power.getDC3Voltage());
  Serial.printf("DC4     %s  %u mV\n", power.isEnableDC4() ? "ON " : "OFF", power.getDC4Voltage());
  Serial.printf("DC5     %s  %u mV\n", power.isEnableDC5() ? "ON " : "OFF", power.getDC5Voltage());

  Serial.println("LDO ========================================================================");
  Serial.printf("ALDO1   %s  %u mV\n", power.isEnableALDO1() ? "ON " : "OFF", power.getALDO1Voltage());
  Serial.printf("ALDO2   %s  %u mV\n", power.isEnableALDO2() ? "ON " : "OFF", power.getALDO2Voltage());
  Serial.printf("ALDO3   %s  %u mV\n", power.isEnableALDO3() ? "ON " : "OFF", power.getALDO3Voltage());
  Serial.printf("ALDO4   %s  %u mV\n", power.isEnableALDO4() ? "ON " : "OFF", power.getALDO4Voltage());
  Serial.printf("BLDO1   %s  %u mV\n", power.isEnableBLDO1() ? "ON " : "OFF", power.getBLDO1Voltage());
  Serial.printf("BLDO2   %s  %u mV\n", power.isEnableBLDO2() ? "ON " : "OFF", power.getBLDO2Voltage());
  Serial.printf("CPUSLDO %s  %u mV\n", power.isEnableCPUSLDO() ? "ON " : "OFF", power.getCPUSLDOVoltage());
  Serial.printf("DLDO1   %s  %u mV\n", power.isEnableDLDO1() ? "ON " : "OFF", power.getDLDO1Voltage());
  Serial.printf("DLDO2   %s  %u mV\n", power.isEnableDLDO2() ? "ON " : "OFF", power.getDLDO2Voltage());
  Serial.println("=============================================================================");
}

static void print_pmu_status()
{
  Serial.printf("VBUS: %s, valid: %s, %u mV\n",
                power.isVbusIn() ? "connected" : "disconnected",
                power.isVbusGood() ? "yes" : "no",
                power.getVbusVoltage());
  Serial.printf("Battery: %s, charging: %s, discharging: %s, %u mV",
                power.isBatteryConnect() ? "connected" : "disconnected",
                power.isCharging() ? "yes" : "no",
                power.isDischarge() ? "yes" : "no",
                power.getBattVoltage());
  if (power.isBatteryConnect()) {
    Serial.printf(", %u%%", power.getBatteryPercent());
  }
  Serial.println();
  Serial.printf("System: %u mV, PMU temperature: %.2f C\n",
                power.getSystemVoltage(), power.getTemperature());
}

static void print_irq_events()
{
  power.getIrqStatus();

  if (power.isVbusInsertIrq()) Serial.println("Event: VBUS inserted");
  if (power.isVbusRemoveIrq()) Serial.println("Event: VBUS removed");
  if (power.isBatInsertIrq()) Serial.println("Event: battery inserted");
  if (power.isBatRemoveIrq()) Serial.println("Event: battery removed");
  if (power.isBatChargeStartIrq()) Serial.println("Event: charging started");
  if (power.isBatChargeDoneIrq()) Serial.println("Event: charging completed");
  if (power.isPekeyShortPressIrq()) Serial.println("Event: power key short press");
  if (power.isPekeyLongPressIrq()) Serial.println("Event: power key long press");

  power.clearIrqStatus();
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("ESP32-C5-Touch-LCD-3.5 AXP2101 example");

  if (!power.begin(Wire, AXP2101_SLAVE_ADDRESS, PMU_I2C_SDA, PMU_I2C_SCL)) {
    Serial.println("ERROR: AXP2101 was not found at I2C address 0x34");
    while (true) {
      delay(1000);
    }
  }
  Wire.setClock(PMU_I2C_FREQ_HZ);

  Serial.printf("AXP2101 detected, chip ID: 0x%02X\n", power.getChipID());

  // Board-safe settings taken from the C5 ESP-IDF AXP2101 example.
  power.disableTSPinMeasure();
  power.enableBattDetection();
  power.enableVbusVoltageMeasure();
  power.enableBattVoltageMeasure();
  power.enableSystemVoltageMeasure();
  power.enableTemperatureMeasure();

  power.setPrechargeCurr(XPOWERS_AXP2101_PRECHARGE_50MA);
  power.setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_400MA);
  power.setChargerTerminationCurr(XPOWERS_AXP2101_CHG_ITERM_25MA);
  power.setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V2);

  power.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
  power.clearIrqStatus();
  power.enableIRQ(
    XPOWERS_AXP2101_BAT_INSERT_IRQ |
    XPOWERS_AXP2101_BAT_REMOVE_IRQ |
    XPOWERS_AXP2101_VBUS_INSERT_IRQ |
    XPOWERS_AXP2101_VBUS_REMOVE_IRQ |
    XPOWERS_AXP2101_PKEY_SHORT_IRQ |
    XPOWERS_AXP2101_PKEY_LONG_IRQ |
    XPOWERS_AXP2101_BAT_CHG_DONE_IRQ |
    XPOWERS_AXP2101_BAT_CHG_START_IRQ);

  print_power_rails();
  print_pmu_status();
  last_status_ms = millis();
}

void loop()
{
  print_irq_events();

  const uint32_t now = millis();
  if (now - last_status_ms >= STATUS_INTERVAL_MS) {
    last_status_ms = now;
    print_pmu_status();
  }
  delay(100);
}
