#ifndef CAN_H
#define CAN_H

#include "pico/stdlib.h"

#define CAN_TX_PIN 10
#define CAN_RX_PIN 11

void can_init(void);
bool can_send_float(uint32_t id, float value);

#endif
