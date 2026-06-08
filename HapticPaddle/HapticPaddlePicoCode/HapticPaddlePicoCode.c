#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "AS5600.h"
#include "HX711.h"
#include "can.h"
#include "potentiometer.h"
// I2C defines
// This example will use I2C0 on GPIO8 (SDA) and GPIO9 (SCL) running at 400KHz.
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define I2C_PORT i2c0
#define I2C_SDA 16
#define I2C_SCL 17



int main()
{
    stdio_init_all();

    init_AS5600();
    init_HX711();
    pot_init();
    // For more examples of I2C use see https://github.com/raspberrypi/pico-examples/tree/master/i2c
    
    while (true) {
        
        int pos2 = pot_read_angle();
        int force = read_HX711();
        printf("%d, %d\n", pos2, force);
        sleep_ms(100);
    }
}
