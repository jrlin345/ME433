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
#define CAN_ID 0x150
#define Kp 200
#define Kd 0
// from 314 to 350
int main()
{
    stdio_init_all();

    init_AS5600();
    init_HX711();
    pot_init();
    can_init();
    tare_HX711(100);
    // For more examples of I2C use see https://github.com/raspberrypi/pico-examples/tree/master/i2c
    const uint32_t PERIOD_US = 1000;  // 1000Hz
    uint32_t next_time = time_us_32();
    float desired_force[150];

    for (int i = 0; i < 150; i++) {
        if (i > 130) {
            // Wall: ramp up as you approach 149
            float penetration = (i - 130) / 19.0f;     // 0.0 to 1.0
            desired_force[i] = penetration * 150.0f;   // 0 to 150
        }
        else {
            // Free space
            desired_force[i] = 0.0f;
        }
    }   

    float desired_current = 200.0f;  // replace with your value
    float derrorx =0;
    float filtered_force = 0;
    while (true) {
        int pos = pot_read_angle()-200;
        if (pos<0){
            pos = 0;
        }
        if (pos>150){
            pos = 150;;
        }
        
        filtered_force = 0.85f * filtered_force + 0.15f * convert_raw_N(read_HX711());
        float error = desired_force[pos]-filtered_force;
        float derror = error - derrorx;
        derrorx = error;
        float u = Kp * error + Kd *derror;
        //adding different gains for the wall vs the free space to increase transparency
        if (pos > 130) {
            float Kp_wall = 200.0f;
            float Kd_wall = 5.0f;
            u = Kp_wall * error + Kd_wall * derror;
        } else {
            float Kp_free = 300;
            float Kd_free = 50.0f;
            u = Kp_free * error + Kd_free * derror;
        }
        float desired_current = u/10;
        if (desired_current>2400){
            desired_current = 2400;
        }
        bool acked = can_send_float(CAN_ID, desired_current);
        
        //if (!acked) {
        //    printf("CAN no ACK\n");
        //}
    
        next_time += PERIOD_US;
        uint32_t now = time_us_32();
        if ((int32_t)(next_time - now) > 0) {
            sleep_us(next_time - now);
        }
        printf("%d, %f\n", pos, desired_force[pos]);
        
    }
   

}
