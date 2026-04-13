#include <stdio.h>
#include "font.h"
#include "ssd1306.h"
#include "pico/stdlib.h"
#include "hardware/i2c.h"


// I2C defines
// This example will use I2C0 on GPIO8 (SDA) and GPIO9 (SCL) running at 400KHz.
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define I2C_PORT i2c0
#define I2C_SDA 16
#define I2C_SCL 17
#define DIR_REG 0b00000001

#define ADDR  0b0100000
void drawChar(unsigned char x, unsigned char y,unsigned char c){
    if ((x < 0) || (x >= 128) || (y < 0) || (y >= 32)) {
        return;
    }
    for (int i = 0; i < 5; i++) {
        unsigned char line = ASCII[c-32][i];
        for (int j = 0; j<8;j++){
            if(line&0x1){
                ssd1306_drawPixel(x+i, y+j,1);
            }else{
                ssd1306_drawPixel(x+i, y+j,0);
            }
            line >>=1;
        }

    }
}
void drawMessage(unsigned char x, unsigned char y, char* message){
    if (x >= 128 || y >= 32) {
        return;
    }
    unsigned char cx = x;
    unsigned char cy = y;
    int i = 0;
    while (message[i] != '\0') {
        if (cx + 5 > 128) {
            cy = cy+8;
            cx = x;
            if (cy >= 32) {
                break;
            }
        }
        drawChar(cx, cy, message[i]);
        cx += 6; // 5 pixels for the char plus 1 pixel spacing
        i++;
    }
}

void I2C_Send_Data_init(void){
    uint8_t buf[2]; 
    buf[0] = 0x00;
    buf[1] = DIR_REG;
    i2c_write_blocking(i2c_default, ADDR, buf, 2, false);
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
    // For more examples of I2C use see https://github.com/raspberrypi/pico-examples/tree/master/i2c
    ssd1306_setup();
    ssd1306_clear();
    ssd1306_update();
    I2C_Send_Data_init();
    while (true) {
        ssd1306_drawPixel(10,20,1);
        ssd1306_update();
        sleep_ms(1000);
        ssd1306_drawPixel(10,20,0);
        ssd1306_update();
        sleep_ms(1000);
        int i = 15;
        char message[50]; 
        sprintf(message, "HEOFHEWFAHIWEAFHIWHEFIHejfhsjkldjfklsafhioweiohfa", i); 
        drawMessage(10, 10, message);
    }
}
