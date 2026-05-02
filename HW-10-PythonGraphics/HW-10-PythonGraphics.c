#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"

int main()
{
    
    stdio_init_all();
    gpio_init(10);
    adc_init();
    adc_gpio_init(26);
    adc_select_input(0);
    gpio_set_dir(10, GPIO_IN);
    
    while (true) {
        int button = gpio_get(10);
        uint16_t raw = adc_read();
        float pot = raw / 4095.0f;
        printf("(%i, %f)\n", button, pot);
        sleep_ms(10);
    }
}
