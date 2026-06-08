#include "AS5600.h"
#include "hardware/i2c.h"
int as5600_magnet_status(void) {
    uint8_t reg = 0x0B;
    uint8_t status;

    i2c_write_blocking(I2C_PORT, AS5600_ADDR, &reg, 1, true);
    i2c_read_blocking(I2C_PORT, AS5600_ADDR, &status, 1, false);

    if (!(status & (1 << 5))) return  0;  // no magnet
    if   (status & (1 << 4))  return -1;  // too far
    if   (status & (1 << 3)) return -2; // too close
    return 1; // good
}
void init_AS5600(){
    i2c_init(I2C_PORT, 100*1000);
    
    gpio_set_function(SDA, GPIO_FUNC_I2C);
    gpio_set_function(SCL, GPIO_FUNC_I2C);
    gpio_pull_up(SDA);
    gpio_pull_up(SCL);
}
int read_AS5600(){
    uint8_t reg = Angle_read_addr;
    uint8_t rx[2] = {0,0};
    i2c_write_blocking(I2C_PORT, AS5600_ADDR, &reg,1,true);
    int read_bytes = i2c_read_blocking(I2C_PORT, AS5600_ADDR, rx, 2, false);
    // Combine MSB and LSB, mask to 12 bits (0–4095)
    int raw = ((int)(rx[0] & 0x0F) << 8) | rx[1];

    // Convert to degrees (0–360)
    float degrees = (raw / 4096.0f) * 360.0f;

    return (int)degrees;
}
int read_AS5600_stable(void) {
    int sum = 0;
    const int samples = 8;

    for (int i = 0; i < samples; i++) {
        int val = read_AS5600();
        if (val < 0) return -1;  // bail on read error
        sum += val;
        sleep_us(100);
    }
    return sum / samples;
}