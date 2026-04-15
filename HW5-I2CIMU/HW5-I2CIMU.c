#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "ssd1306.h"
#include <math.h>
// I2C defines
// This example will use I2C0 on GPIO8 (SDA) and GPIO9 (SCL) running at 400KHz.
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define ABS(x) ((x) < 0 ? -(x) : (x))
#define I2C_PORT i2c0
#define I2C_SDA 16
#define I2C_SCL 17
#define ADDR 0x68
// config registers
#define CONFIG 0x1A
#define GYRO_CONFIG 0x1B
#define ACCEL_CONFIG 0x1C
#define PWR_MGMT_1 0x6B
#define PWR_MGMT_2 0x6C
// sensor data registers:
#define ACCEL_XOUT_H 0x3B
#define ACCEL_XOUT_L 0x3C
#define ACCEL_YOUT_H 0x3D
#define ACCEL_YOUT_L 0x3E
#define ACCEL_ZOUT_H 0x3F
#define ACCEL_ZOUT_L 0x40
#define TEMP_OUT_H   0x41
#define TEMP_OUT_L   0x42
#define GYRO_XOUT_H  0x43
#define GYRO_XOUT_L  0x44
#define GYRO_YOUT_H  0x45
#define GYRO_YOUT_L  0x46
#define GYRO_ZOUT_H  0x47
#define GYRO_ZOUT_L  0x48
#define WHO_AM_I     0x75
#define CX  64   // screen centre x
#define CY  16   // screen centre y (128x32 display)
#define SCALE 13 // max arm pixels — keeps line inside 16px half-height
void init_IMU(){
    uint8_t buf0[2]; 
    buf0[0] = PWR_MGMT_1;
    buf0[1] = 0x00;
    i2c_write_blocking(i2c_default, ADDR, buf0, 2, false);
    uint8_t buf1[2]; 
    buf1[0] = GYRO_CONFIG;
    buf1[1] = 0x00;
    i2c_write_blocking(i2c_default, ADDR, buf1, 2, false);
    uint8_t buf2[2]; 
    buf2[0] = ACCEL_CONFIG;
    buf2[1] = 0x00;
    i2c_write_blocking(i2c_default, ADDR, buf2, 2, false);
}

int read_IMU(float *accel, float *gyro, float *temp){
    uint8_t buf[14]; 
    uint8_t reg;
    reg = ACCEL_XOUT_H; //GPIO register
    i2c_write_blocking(i2c_default, ADDR, &reg, 1, true);  
    i2c_read_blocking(i2c_default, ADDR, buf, 14, false);
    accel[0] = (int16_t)(buf[0]<<8|buf[1])*0.000061f;  // X
    accel[1] = (int16_t)(buf[2]<<8|buf[3])*0.000061f;  // Y
    accel[2] = (int16_t)(buf[4]  << 8 |buf[5])*0.000061f;  // Z
    *temp = (int16_t)(buf[6]<<8 |buf[7])/340+36.53f;
    gyro[0]  = (int16_t)(buf[8]  << 8 | buf[9])*0.007630f;  // X
    gyro[1]  = (int16_t)(buf[10] << 8 | buf[11])*0.007630f; // Y
    gyro[2]  = (int16_t)(buf[12] << 8 | buf[13])*0.007630f; // Z
    return 0;
}
void draw_line(int x0, int y0, int x1, int y1) {
    int dx =  ABS(x1-x0), sx = x0 < x1 ? 1 : -1;
    int dy = -ABS(y1-y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (true) {
        ssd1306_drawPixel(x0, y0, 1);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

int main()
{
    stdio_init_all();

    // I2C Initialisation. Using it at 400Khz.
    i2c_init(I2C_PORT, 400*1000);
    
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_setup();
    ssd1306_clear();
    ssd1306_update();
    float accel[3];
    float gyro[3];
    float temp;
    init_IMU();
    while (true) {
        read_IMU(accel, gyro, &temp);
        float ax = accel[0];
        float ay = accel[1];
        // clamp to ±1g so line stays on screen
        if (ax >  1.0f) ax =  1.0f;
        if (ax < -1.0f) ax = -1.0f;
        if (ay >  1.0f) ay =  1.0f;
        if (ay < -1.0f) ay = -1.0f;

        int ex = CX + (int)(ax * SCALE);
        int ey = CY + (int)(ay * SCALE);

        ssd1306_clear();

        // small crosshair at centre
        ssd1306_drawPixel(CX,   CY,   1);
        ssd1306_drawPixel(CX+1, CY,   1);
        ssd1306_drawPixel(CX-1, CY,   1);
        ssd1306_drawPixel(CX,   CY+1, 1);
        ssd1306_drawPixel(CX,   CY-1, 1);
        printf("%i, %i |", ex, ey);
        // gravity vector
        draw_line(CX, CY, ex, ey);

        ssd1306_update();
    }
}
