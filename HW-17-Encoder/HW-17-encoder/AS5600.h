#ifndef AS5600_H
#define AS5600_H

#include "pico/stdlib.h"
#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>
#include <math.h>
#define SCL   1
#define SDA   0
#define I2C_PORT i2c0
#define AS5600_ADDR 0x36
#define Angle_read_addr 0x0F

int read_AS5600();

#endif