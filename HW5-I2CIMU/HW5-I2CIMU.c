#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "ssd1306.h"
#include "IMU.h"
// I2C defines
// This example will use I2C0 on GPIO8 (SDA) and GPIO9 (SCL) running at 400KHz.
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define ABS(x) ((x) < 0 ? -(x) : (x))
#define I2C_PORT i2c0
#define I2C_SDA 16
#define I2C_SCL 17

#define CX  64   // screen centre x
#define CY  16   // screen centre y (128x32 display)
#define SCALE 13 // max arm pixels — keeps line inside 16px half-height

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
        printf("[%i %i]", 3*(int8_t)ax, -3*(int8_t)ay);
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
        
        // gravity vector
        draw_line(CX, CY, ex, ey);

        ssd1306_update();
    }
}
