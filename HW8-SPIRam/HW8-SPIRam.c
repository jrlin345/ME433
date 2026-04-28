#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"

#define PIN_CS_DAC 17
#define PIN_CS_RAM 5
void spi_ram_init(){

}
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
void writeDAC(int channel, float volt){

    uint8_t data[2];
    data[0]= 0b1111011;
    data[1]= 0b1111100;
    data[0] = data[0] | ((channel& 0b1)<<7);
    uint16_t myV = volt/3.3*1023;
    data[0] = data[0] | (myV>>6)&0b0000111;
    data[1] = (myV<<2)&0xFF;
    cs_select(PIN_CS_DAC);
    spi_write_blocking(spi_default, data, 2); // where data is a uint8_t array with length len
    cs_deselect(PIN_CS_DAC);

}
int main()
{
    
    stdio_init_all();
    spi_init(spi_default, 1000 * 1000); // the baud, or bits per second
    gpio_set_function(PICO_DEFAULT_SPI_RX_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PICO_DEFAULT_SPI_TX_PIN, GPIO_FUNC_SPI);

    gpio_init(PICO_DEFAULT_SPI_CSN_PIN);
    gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 1);
    gpio_set_dir(PICO_DEFAULT_SPI_CSN_PIN, GPIO_OUT);
    while (true) {
        printf("Hello, world!\n");
        sleep_ms(1000);
    }
}
