#include "aht25.h"
#include "i2c.h"
#include "lcd.h"
#include <util/delay.h>
#include <avr/io.h> // for _delay_ms

// メーカーサンプルに基づくCRC8計算 (多項式: 0x31, 初期値: 0xFF)
static uint8_t aht25_calculate_crc(uint8_t *pDat, uint8_t len) {
    uint8_t crc = 0xFF;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= pDat[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80) crc = (crc << 1) ^ 0x31;
            else crc <<= 1;
        }
    }
    return crc;
}

// AHT25を初期化する
bool aht25_init(void) {
    _delay_ms(150); // 電源投入後の安定待ち
    return true; 
}

// AHT25のステータスレジスタから、キャリブレーションが行われているか確認する
bool aht25_is_calibrated(void) {
    uint8_t status;
    if (!i2c_start()) { i2c_stop(); return false; }
    if (!i2c_write(AHT25_ADDRESS << 1 | 0x00)) { i2c_stop(); return false; }
    if (!i2c_write(0x71)) { i2c_stop(); return false; }
    i2c_stop();
    _delay_us(75);

    if (!i2c_start()) { i2c_stop(); return false; }
    if (!i2c_write(AHT25_ADDRESS << 1 | 0x01)) { i2c_stop(); return false; }
    status = i2c_read_ack();
    i2c_stop();

    return (status & AHT25_STATUS_CAL_MASK) != 0;
}

// 温度と湿度を読み取る
bool aht25_read_data(float *temperature, float *humidity, uint8_t *raw_data) {
    uint8_t buf[7];
    if (raw_data) {
        for (int i = 0; i < 7; i++) raw_data[i] = 0;
    }

    // 1. 測定開始
    if (!i2c_start()) return false;
    if (!i2c_write(AHT25_ADDRESS << 1 | 0)) { i2c_stop(); return false; }
    i2c_write(0xAC);
    i2c_write(0x33);
    i2c_write(0x00);
    i2c_stop();

    // 2. 待機 (現在の動作実績に基づき 500ms)
    _delay_ms(500); 

    // 3. 読み出しリトライ
    bool success = false;
    for (int i = 0; i < 50; i++) {
        if (i2c_start()) {
            if (i2c_write(AHT25_ADDRESS << 1 | 1)) {
                success = true;
                break;
            }
            i2c_stop();
        }
        _delay_ms(10);
    }
    if (!success) return false;

    // 4. データ受信 (7バイト)
    for (int i = 0; i < 6; i++) buf[i] = i2c_read_ack();
    buf[6] = i2c_read_nack();
    i2c_stop();

    if (raw_data) {
        for (int i = 0; i < 7; i++) raw_data[i] = buf[i];
    }

    // 5. CRCチェックの実行
    if (aht25_calculate_crc(buf, 6) != buf[6]) {
        return false; // CRC不一致
    }

    // 6. ステータス判定 (Busyフラグとキャリブレーションフラグ)
    if ((buf[0] & 0x88) != 0x08) {
        return false; 
    }

    // 7. データ結合と変換
    uint32_t hum_raw = ((uint32_t)buf[1] << 12) | ((uint32_t)buf[2] << 4) | (buf[3] >> 4);
    uint32_t tem_raw = (((uint32_t)buf[3] & 0x0F) << 16) | ((uint32_t)buf[4] << 8) | buf[5];

    *humidity = (float)hum_raw * 100.0 / 1048576.0;
    *temperature = (float)tem_raw * 200.0 / 1048576.0 - 50.0;

    return true;
}
