#include "hardware/adc.h"
#include "potentiometer.h"
#define POT_ADC_PIN  26   // GP26 = ADC0, or GP27/GP28 for ADC1/ADC2
#define POT_ADC_CH    0   // match to pin above

void pot_init(void) {
    adc_init();
    adc_gpio_init(POT_ADC_PIN);
    adc_select_input(POT_ADC_CH);
}

// Returns angle in degrees — calibrate MIN/MAX to your pot's actual range
int pot_read_angle(void) {


    uint16_t raw = adc_read();  // 0–4095
    float transformed = ((float)raw/4095.0)*360.0;
    return (int)transformed;
}