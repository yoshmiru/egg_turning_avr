#ifndef AHT25_H
#define AHT25_H

#include <stdint.h>
#include <stdbool.h>

// AHT25 I2Cアドレス
#define AHT25_ADDRESS 0x38

// AHT25コマンド
#define AHT25_CMD_INITIALIZE 0xBE
#define AHT25_CMD_MEASURE    0xAC

// AHT25ステータスビット
#define AHT25_STATUS_BUSY_MASK 0x80
#define AHT25_STATUS_CAL_MASK  0x08

// 関数プロトタイプ
bool aht25_init(void);
bool aht25_read_data(float *temperature, float *humidity, uint8_t *raw_data);
bool aht25_is_calibrated(void);

#endif