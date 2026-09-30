#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
#include "bsp/esp-bsp.h"
#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t chip_id;
    bool battery_connected;
    bool vbus_present;
    bool charging;
    int battery_percent;
    uint16_t battery_voltage_mv;
    uint16_t vbus_voltage_mv;
    uint16_t system_voltage_mv;
} axp2101_power_info_t;

void *bsp_axp2101_init(void);
bool bsp_axp2101_get_power_info(void *dev, axp2101_power_info_t *info);

#ifdef __cplusplus
}
#endif
