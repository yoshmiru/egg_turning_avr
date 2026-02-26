#ifndef SERVO_H
#define SERVO_H

#include <avr/io.h>

// サーボ個体差の調整 (1カウント = 8us)
#define SERVO_MIN_PULSE 62   
#define SERVO_MAX_PULSE 312  

void servo_init(void);
void servo_set_target_angle(uint8_t angle);
void servo_update(void); // 定期的に呼び出して角度を微更新する

#endif
