#include <stdio.h>
#include "pico/stdlib.h"
#include "HX711.h"

#define MAX_SAMPLES 2000

int main() {
    stdio_init_all();
    sleep_ms(2000);  // give USB serial time to connect
    init_HX711();

    while (true) {
        int num = 0;
        printf("send a number!");
       
        scanf("%d", &num);
        
        int   v[MAX_SAMPLES];
        uint64_t t[MAX_SAMPLES];
        float alpha = .1;
        float iir   = (float)read_HX711();  // prime the filter

        for (int i = 0; i < num; i++) {
            int raw = read_HX711();
            iir = alpha * (float)raw + (1.0f - alpha) * iir;
            v[i] = (int)iir;
            t[i] = to_ms_since_boot(get_absolute_time());
        }

        // Send all data back
        for (int i = 0; i < num; i++) {
            printf("%d %llu %d\n", i, t[i], v[i]);
        }
        printf("DONE\n");
    }
}