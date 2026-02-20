#include "lcd.h"
#include "aht25.h"
#include "i2c.h"
#include "servo.h"
#include <stdio.h>
#include <util/delay.h>

#define TURN_INTERVAL_SEC 7200UL // 2時間 (7200秒)
#define TICK_MS 10               // ループ周期
#define ANGLE_A 0                // 端の角度A
#define ANGLE_B 60               // 端の角度B

// 温度・湿度表示
void update_display(float temp, float hum, uint32_t next_turn_sec) {
    char buffer[17];
    
    // 1行目: 温度と転卵カウントダウン (例: 37.5C T:119m)
    int t_whole = (int)temp;
    int t_frac = (int)((temp - t_whole) * 10);
    if (temp < 0 && t_frac < 0) t_frac = -t_frac;
    
    uint16_t next_turn_min = next_turn_sec / 60;
    sprintf(buffer, "%d.%dC  Next:%3dm", t_whole, t_frac, next_turn_min);
    lcd_set_cursor(0, 0);
    lcd_putstr(buffer);

    // 2行目: 湿度
    int h_whole = (int)hum;
    int h_frac = (int)((hum - h_whole) * 10);
    sprintf(buffer, "Humidity: %d.%d%% ", h_whole, h_frac);
    lcd_set_cursor(0, 1);
    lcd_putstr(buffer);
}

int main(void) {
    lcd_init();
    i2c_init();
    servo_init();
    
    DDRD &= ~(1 << PD6); // ボタン PD6
    PORTD |= (1 << PD6);

    lcd_set_cursor(0, 0);
    lcd_putstr("Hatchery System");
    lcd_set_cursor(0, 1);
    lcd_putstr("Initializing...");
    _delay_ms(1000);

    aht25_init();

    float temperature = 0.0;
    float humidity = 0.0;
    
    uint32_t ticks_since_turn = 0;
    uint8_t current_side = 0; // 0: ANGLE_A, 1: ANGLE_B
    uint16_t sensor_update_ticks = 0;

    // 初回動作
    servo_set_angle(ANGLE_A);

    while(1) {
        // 1. ボタン入力チェック (手動転卵)
        if (!(PIND & (1 << PD6))) {
            _delay_ms(20);
            if (!(PIND & (1 << PD6))) {
                current_side = !current_side;
                servo_set_angle(current_side ? ANGLE_B : ANGLE_A);
                ticks_since_turn = 0; // タイマーリセット
                
                while (!(PIND & (1 << PD6))); // 離すまで待機
                _delay_ms(20);
            }
        }

        // 2. 自動転卵タイマーチェック
        // TICK_MS(10ms) * ticks = 秒
        if (ticks_since_turn >= (TURN_INTERVAL_SEC * 100)) {
            current_side = !current_side;
            servo_set_angle(current_side ? ANGLE_B : ANGLE_A);
            ticks_since_turn = 0;
        }

        // 3. センサー読み取りと表示更新 (2秒ごと)
        if (sensor_update_ticks == 0) {
            aht25_read_data(&temperature, &humidity, NULL);
            uint32_t remaining_sec = TURN_INTERVAL_SEC - (ticks_since_turn / 100);
            update_display(temperature, humidity, remaining_sec);
        }

        _delay_ms(TICK_MS);
        ticks_since_turn++;
        sensor_update_ticks++;
        if (sensor_update_ticks >= 200) sensor_update_ticks = 0;
    }
}
