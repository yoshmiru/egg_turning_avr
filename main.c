#include "lcd.h"
#include "aht25.h"
#include "i2c.h"
#include "servo.h"
#include <stdio.h>
#include <util/delay.h>

// ==========================================
// --- 孵卵器システム設定 (オーバーシュート抑制版) ---
// ==========================================

// 温度制御 (PID)
#define TARGET_TEMP 37.5f         // 目標温度 (℃)
#define KP 80.0f                  // 比例: 250から大幅に下げて急加速を抑制
#define KI 0.1f                   // 積分: 微小なズレをゆっくり直す
#define KD 400.0f                 // 微分: ブレーキ性能を大幅に強化

// 転卵設定 (振り子式)
#define TURN_ANGLE_LEFT  0
#define TURN_ANGLE_RIGHT 60
#define TURN_INTERVAL_SEC 7200UL

// システム周期
#define TICK_MS 10
#define SSR_CYCLE_TICKS 100

// ==========================================
// --- ピン定義 ---
// ==========================================
#define SSR_PIN PD7
#define SSR_DDR DDRD
#define SSR_PORT PORTD
#define BUTTON_PIN PD6

// ==========================================
// --- 状態変数 ---
// ==========================================
static float integral = 0;
static float prev_error = 0;
static uint16_t heater_on_ticks = 0;

void update_display(float temp, float hum, uint32_t next_turn_sec, uint16_t heater_pct) {
    char buffer[17];
    int t_whole = (int)temp;
    int t_frac = (int)((temp - (float)t_whole) * 10.0f);
    if (temp < 0 && t_frac < 0) t_frac = -t_frac;

    sprintf(buffer, "%2d.%dC H:%3d%%    ", t_whole, t_frac, heater_pct);
    lcd_set_cursor(0, 0);
    lcd_putstr(buffer);

    int h_whole = (int)hum;
    uint16_t next_min = (uint16_t)(next_turn_sec / 60);
    sprintf(buffer, "%2d%% Next:%3dm   ", h_whole, next_min);
    lcd_set_cursor(0, 1);
    lcd_putstr(buffer);
}

float calculate_pid(float current_temp) {
    float error = TARGET_TEMP - current_temp;

    integral += error;
    if (integral > 100.0f) integral = 100.0f;
    if (integral < -100.0f) integral = -100.0f;

    float derivative = error - prev_error;
    prev_error = error;

    float output = (KP * error) + (KI * integral) + (KD * derivative);

    if (output > 100.0f) output = 100.0f;
    if (output < 0.0f) output = 0.0f;

    return output / 100.0f;
}

int main(void) {
    lcd_init();
    i2c_init();
    servo_init();

    SSR_DDR |= (1 << SSR_PIN);
    SSR_PORT &= ~(1 << SSR_PIN);
    DDRD &= ~(1 << BUTTON_PIN);
    PORTD |= (1 << BUTTON_PIN);

    lcd_set_cursor(0, 0);
    lcd_putstr("Hatchery System");
    lcd_set_cursor(0, 1);
    lcd_putstr("PID Optimized...");
    _delay_ms(1000);

    aht25_init();

    float temperature = 0.0;
    float humidity = 0.0;
    uint32_t ticks_since_turn = 0;
    uint16_t ssr_tick_counter = 0;
    uint16_t sensor_update_ticks = 0;
    uint8_t current_side = 0;
    uint16_t heater_pct_display = 0;

    servo_set_target_angle(TURN_ANGLE_LEFT);

    while(1) {
        servo_update();

        // 1. ヒーター制御 (1秒周期)
        if (heater_on_ticks > 0 && ssr_tick_counter < heater_on_ticks) {
            SSR_PORT |= (1 << SSR_PIN);
        } else {
            SSR_PORT &= ~(1 << SSR_PIN);
        }

        ssr_tick_counter++;
        if (ssr_tick_counter >= SSR_CYCLE_TICKS) {
            ssr_tick_counter = 0;
            // PID計算はセンサーデータがある場合のみ
            if (temperature > 5.0f && temperature < 60.0f) {
                float pid_out = calculate_pid(temperature);
                heater_on_ticks = (uint16_t)(pid_out * (float)SSR_CYCLE_TICKS);
                heater_pct_display = (uint16_t)(pid_out * 100.0f);
            } else {
                heater_on_ticks = 0;
                heater_pct_display = 0;
            }
        }

        // 2. ボタン入力
        if (!(PIND & (1 << BUTTON_PIN))) {
            _delay_ms(20);
            if (!(PIND & (1 << BUTTON_PIN))) {
                current_side = !current_side;
                servo_set_target_angle(current_side ? TURN_ANGLE_RIGHT : TURN_ANGLE_LEFT);
                ticks_since_turn = 0;
                while (!(PIND & (1 << BUTTON_PIN)));
                _delay_ms(20);
            }
        }

        // 3. 自動転卵タイマー
        if (ticks_since_turn >= (TURN_INTERVAL_SEC * 100)) {
            current_side = !current_side;
            servo_set_target_angle(current_side ? TURN_ANGLE_RIGHT : TURN_ANGLE_LEFT);
            ticks_since_turn = 0;
        }

        // 4. センサー読み取り更新周期を 0.5秒(50 ticks) に高速化
        if (sensor_update_ticks == 0) {
            if (aht25_read_data(&temperature, &humidity, NULL)) {
                uint32_t remaining_sec = TURN_INTERVAL_SEC - (ticks_since_turn / 100);
                update_display(temperature, humidity, remaining_sec, heater_pct_display);
            } else {
                lcd_set_cursor(0, 0);
                lcd_putstr("Sensor Error!   ");
                lcd_set_cursor(0, 1);
                lcd_putstr("Heater HALTED   ");
                temperature = -99.0f;
            }
        }

        _delay_ms(TICK_MS);
        ticks_since_turn++;
        sensor_update_ticks++;
        if (sensor_update_ticks >= 50) sensor_update_ticks = 0; // ここを50に変更
    }
}
