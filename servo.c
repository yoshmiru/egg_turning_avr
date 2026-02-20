#include "servo.h"

// 1MHz / 8分周 = 125,000 Hz (1カウント = 8us)
// 20ms周期 = 2500カウント
#define SERVO_TOP 2499

// 角度からレジスタ値を計算
// 0度 (0.5ms) -> 500us / 8us = 62.5
// 180度 (2.5ms) -> 2500us / 8us = 312.5
#define PULSE_0_DEG   62
#define PULSE_180_DEG 312

void servo_init(void) {
    // PB1 (OC1A) を出力に設定
    DDRB |= (1 << PB1);

    // Timer1 設定: Fast PWM モード (14), ICR1をTOPとする
    // 非反転動作 (COM1A1=1, COM1A0=0)
    TCCR1A = (1 << COM1A1) | (1 << WGM11);
    TCCR1B = (1 << WGM13) | (1 << WGM12) | (1 << CS11); // 8分周

    ICR1 = SERVO_TOP;
    OCR1A = PULSE_0_DEG; // 初期位置 0度
}

void servo_set_angle(uint8_t angle) {
    if (angle > 180) angle = 180;
    
    // 角度をパルス幅（レジスタ値）に変換
    // OCR1A = 62 + (angle * (312 - 62) / 180)
    uint16_t pulse = PULSE_0_DEG + ((uint32_t)angle * (PULSE_180_DEG - PULSE_0_DEG) / 180);
    OCR1A = pulse;
}
