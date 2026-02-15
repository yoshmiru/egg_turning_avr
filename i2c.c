#include "i2c.h"
#include "lcd.h"
#include <util/delay.h>
#include <stdbool.h> // for bool type
#include <util/twi.h> // for TWI status codes (TW_START, etc.)

#ifndef F_CPU
#define F_CPU 1000000UL // Default to 1MHz if not defined by Makefile
#endif

#define SCL_CLOCK 50000UL // SCLクロック周波数 (50kHz)
#define I2C_TIMEOUT_LOOPS 3000 // I2C操作のタイムアウトループ回数

// TWINTフラグがセットされるまで待機するヘルパー関数
// 成功したら true, タイムアウトしたら false を返す
static bool i2c_wait_for_twint(void) {
    uint16_t timeout = 0;
    while (!(TWCR & (1 << TWINT))) {
        if (timeout++ > I2C_TIMEOUT_LOOPS) {
            return false; // タイムアウト
        }
        // timeout と I2C_TIMEOUT_LOOPSをデバッグする
        _delay_us(1); // 必要であれば短い遅延
    }
    return true; // TWINTセット
}

bool i2c_init(void) { // 戻り値をboolに変更
    // --- SDAがLowで固まっている場合の救出作戦 ---
    DDRC |= (1 << PC5);  // SCLを出力に
    DDRC &= ~(1 << PC4); // SDAを入力に
    PORTC |= (1 << PC4); // SDAをプルアップ

    for (uint8_t i = 0; i < 9; i++) {
        PORTC &= ~(1 << PC5); _delay_us(10);
        PORTC |= (1 << PC5);  _delay_us(10);
        // SDAがHighに戻ったら、センサーが解放してくれた証拠
        if (PINC & (1 << PC4)) break; 
    }
    // ------------------------------------------

    TWSR = 0x00;
    TWBR = ((F_CPU / SCL_CLOCK) - 16) / 2;
    TWCR = (1 << TWEN);
    return true;
}

bool i2c_start(void) {
    // 1. 強制的にリセットをかける（おまじない）
    TWCR = 0; 
    _delay_ms(10);
    
    // 2. Start信号発行
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    
    // 3. 待機（ここで止まらないようにタイムアウト付きを使う）
    if (!i2c_wait_for_twint()) {
        // タイムアウトした＝バスが物理的にロックしている
        return false; 
    }
    
    // 4. ステータス確認
    uint8_t status = TWSR & 0xF8;
    return (status == TW_START || status == TW_REP_START);
}

void i2c_stop(void) {
    // STOP発行
    TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
    
    uint16_t timeout = 0;
    // TWSTOが消えるのを待つが、最大1000回で諦める
    while ((TWCR & (1 << TWSTO)) && (timeout < 1000)) {
        timeout++;
        _delay_us(1);
    }
    
    // 次の通信のために少しだけバスを休ませる
    _delay_us(10);
}

bool i2c_write(uint8_t data) {
    _delay_us(5); // 書き込み前の短い遅延
    TWDR = data;
    TWCR = (1 << TWINT) | (1 << TWEN);
    if (!i2c_wait_for_twint()) return false;
    uint8_t status = TWSR & 0xF8;
    return (status == TW_MT_SLA_ACK || status == TW_MT_DATA_ACK || status == TW_MR_SLA_ACK);
}

uint8_t i2c_read_byte(bool ack) { // ack=trueならACK、ack=falseならNACK
    if (ack) {
        TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWEA);
    } else {
        TWCR = (1 << TWINT) | (1 << TWEN);
    }
    if (!i2c_wait_for_twint()) return 0xFF; // エラー値
    return TWDR;
}

// 互換性のため残す。新しいコードではi2c_read_byteを使う
uint8_t i2c_read_ack(void) {
    return i2c_read_byte(true);
}

uint8_t i2c_read_nack(void) {
    return i2c_read_byte(false);
}

void i2c_force_reset(void) {
    // I2Cピンを一度手動でHighにする
    DDRC &= ~((1 << PC4) | (1 << PC5)); // 入力
    PORTC |= (1 << PC4) | (1 << PC5);  // プルアップ有効
    _delay_ms(10);
}
