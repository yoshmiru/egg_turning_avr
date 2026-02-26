#include "servo.h"

#define SERVO_TOP 2499

static float current_angle = 0;
static uint8_t target_angle = 0;

void servo_init(void) {
    DDRB |= (1 << PB1);
    TCCR1A = (1 << COM1A1) | (1 << WGM11);
    TCCR1B = (1 << WGM13) | (1 << WGM12) | (1 << CS11);
    ICR1 = SERVO_TOP;
    
    current_angle = 0;
    target_angle = 0;
    OCR1A = SERVO_MIN_PULSE;
}

void servo_set_target_angle(uint8_t angle) {
    if (angle > 180) angle = 180;
    target_angle = angle;
}

// 角度をパルス幅に変換する内部ヘルパー
static uint16_t angle_to_pulse(float angle) {
    return (uint16_t)(SERVO_MIN_PULSE + (angle * (SERVO_MAX_PULSE - SERVO_MIN_PULSE) / 180.0f));
}

void servo_update(void) {
    // 目標角度まで 0.1度ずつ近づける (10msごとに実行)
    // 60度移動するのに 60/0.1 = 600回 = 6秒かかる計算
    const float step = 0.1f;

    if (current_angle < (float)target_angle) {
        current_angle += step;
        if (current_angle > (float)target_angle) current_angle = (float)target_angle;
    } 
    else if (current_angle > (float)target_angle) {
        current_angle -= step;
        if (current_angle < (float)target_angle) current_angle = (float)target_angle;
    }

    OCR1A = angle_to_pulse(current_angle);
}
