#include "AS5600.h"
#include "hardware/i2c.h"

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
