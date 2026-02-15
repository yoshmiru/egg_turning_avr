#ifndef I2C_H
#define I2C_H
#include <avr/io.h>
#include <stdbool.h>

// I2Cステータスのヘルパーマクロ
#define TW_STATUS   (TWSR & 0xF8)

bool i2c_init(void);
bool i2c_start(void);
void i2c_stop(void);
void i2c_force_reset(void);
bool i2c_write(uint8_t data);
uint8_t i2c_read_byte(bool ack);
uint8_t i2c_read_ack(void);
uint8_t i2c_read_nack(void);

#endif
