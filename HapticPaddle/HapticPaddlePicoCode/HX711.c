#include "HX711.h"

void init_HX711() {
    gpio_init(SCK_Pin);
    gpio_set_dir(SCK_Pin, GPIO_OUT);
    gpio_init(DT_Pin);
    gpio_set_dir(DT_Pin, GPIO_IN);
    gpio_put(SCK_Pin, 0);
}

int read_HX711() {
    while (gpio_get(DT_Pin)) {
        sleep_ms(1);
    }

    unsigned int raw = 0;
    for (int i = 0; i < 24; i++) {
        gpio_put(SCK_Pin, 1);
        sleep_us(clock_time_us);
        raw = (raw << 1) | (gpio_get(DT_Pin) ? 1 : 0);  // read on rising edge
        gpio_put(SCK_Pin, 0);
        sleep_us(clock_time_us);
    }
    gpio_put(SCK_Pin, 1);
    sleep_us(clock_time_us);
    gpio_put(SCK_Pin, 0);
    sleep_us(clock_time_us);


    if (raw & 0x800000) {
        raw |= 0xFF000000;
    }
    return (int)raw;
}