#ifndef POTENTIOMENTER_H
#define POTENTIOMETER_H
#include "pico/stdlib.h"
#define POT_ADC_PIN  26   // GP26 = ADC0, or GP27/GP28 for ADC1/ADC2
#define POT_ADC_CH    0   // match to pin above
void pot_init(void);
int pot_read_angle(void);

#endif