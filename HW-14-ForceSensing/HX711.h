#ifndef HX711_H
#define HX711_H

#include "pico/stdlib.h"

#define SCK_Pin   15
#define DT_Pin    14
#define clock_time_us 1   // 1µs → well within 80Hz timing

void init_HX711();
int  read_HX711();

#endif