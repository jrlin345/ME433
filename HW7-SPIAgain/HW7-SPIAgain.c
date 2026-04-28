#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"

// SPI Defines
// We are going to use SPI 0, and allocate it to the following GPIO pins
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define SPI_PORT spi0
#define PIN_MISO 16
#define PIN_SCK  18
#define PIN_MOSI 19


#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#define PIN_CS 17
#include <math.h>
static inline void cs_select(uint cs_pin) {
    asm volatile("nop \n nop \n nop"); // FIXME
    gpio_put(cs_pin, 0);
    asm volatile("nop \n nop \n nop"); // FIXME
}

static inline void cs_deselect(uint cs_pin) {
    asm volatile("nop \n nop \n nop"); // FIXME
    gpio_put(cs_pin, 1);
    asm volatile("nop \n nop \n nop"); // FIXME
}

void writeDAC(float volt){

    if (volt <0.0f) volt = 0;
    if(volt>3.3) volt=3.3;
    uint16_t value = (uint16_t)((volt/3.3)*1023);
    uint16_t command = 0;
    command|=0b0111<<12;
    command|=value<<2;
    uint8_t data[2];
    data[0] = command>>8;
    data[1] = command &0xFF;
    cs_select(PIN_CS);
    spi_write_blocking(SPI_PORT, data,2);
    cs_deselect(PIN_CS);

}
int main()
{
    stdio_init_all();

    // SPI initialisation. This example will use SPI at 1MHz.
    spi_init(SPI_PORT, 1000*200);
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_CS,   GPIO_FUNC_SIO);
    gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    
    // Chip select is active-low, so we'll initialise it to a driven-high state
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, 1);
    // For more examples of SPI use see https://github.com/raspberrypi/pico-examples/tree/master/spi
    float t = 0;
    while (true) {
        
        t=t+.01;
        float voltage = (sinf(t)+1)/2*3.3;
        float voltage2 = fabsf(fmodf(t, 1.0f) - 0.5f) * 2.0f * 3.3f;

        writeDAC(voltage2);

        sleep_ms(10);
    }
}
