#ifndef AS5600_H
#define AS5600_H

#include "pico/stdlib.h"
#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>
#include <math.h>
#define SCL   17
#define SDA   16
#define I2C_PORT i2c0
#define AS5600_ADDR 0x36
#define Angle_read_addr 0x0F
int as5600_magnet_status(void); 
void init_AS5600();
int read_AS5600();
int read_AS5600_stable(void);

#endif