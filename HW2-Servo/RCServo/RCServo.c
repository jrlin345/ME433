#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#define LEDPin 16
void setServoAngle(int angle) {
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;

    uint16_t level = (.05+(angle/180.0)*.05)  * 60000; // Map angle to PWM level (0-60000)
    printf("Angle: %d, PWM Level: %d\n", angle, level);
    pwm_set_gpio_level(LEDPin, level);
}
int main()
{
    stdio_init_all();
    gpio_set_function(LEDPin, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(LEDPin);
    float div = 50;
    pwm_set_clkdiv(slice_num, div);
    uint16_t wrap = 60000;
    pwm_set_wrap(slice_num, wrap);
    pwm_set_enabled(slice_num, true);
    pwm_set_gpio_level(LEDPin, 0);
    

    while (true) {
        int i = 10;
        for (i = 10; i < 170; i += 10) {
            setServoAngle(i);
            sleep_ms(10);

        }
         for (i = 170; i > 10; i -= 10) {
            setServoAngle(i);
            sleep_ms(10);

        }
    }
}

