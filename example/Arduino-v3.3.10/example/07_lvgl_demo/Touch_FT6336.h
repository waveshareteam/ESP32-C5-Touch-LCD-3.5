#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "Board_IO.h"
#define FT6336_ADDR 0x38
#define FT6336_INT_PIN 3
#define FT6336_MAX_POINTS 2
bool FT6336_Init(void);
bool FT6336_Read(uint16_t *x,uint16_t *y,uint8_t *count,uint8_t max_points);
