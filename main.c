#include "lcd.h"
#include "aht25.h"
#include "i2c.h"
#include "servo.h"
#include <stdio.h>
#include <util/delay.h>

// --- 設定定数 ---
#define TARGET_TEMP 37.5f         // 目標温度
#define TURN_INTERVAL_SEC 7200UL  // 転卵周期 (2時間)
#define TICK_MS 10                // ループ周期 (10ms)
#define SSR_CYCLE_TICKS 100       // SSRの1サイクル (10ms * 100 = 1s)

// --- PIDパラメータ (孵卵器の環境に合わせて調整が必要) ---
#define KP 250.0f  // 比例: 誤差に直接比例して出力を出す
#define KI 0.2f    // 積分: 微小な残留偏差を解消する
#define KD 150.0f  // 微分: 急激な温度変化（蓋を開けた時など）を抑制する

// --- ピン定義 ---
#define SSR_PIN PD7
#define SSR_DDR DDRD
#define SSR_PORT PORTD

// PID状態変数
static float integral = 0;
static float prev_error = 0;
static uint16_t heater_on_ticks = 0;

// 温度・湿度・状態の表示更新
void update_display(float temp, float hum, uint32_t next_turn_sec, uint16_t heater_pct) {
    char buffer[17];
    
    // 1行目: 温度とヒーター出力% (例: 37.5C H:100%)
    int t_whole = (int)temp;
    int t_frac = (int)((temp - (float)t_whole) * 10.0f);
    if (temp < 0 && t_frac < 0) t_frac = -t_frac;
    
    sprintf(buffer, "%2d.%dC H:%3d%%    ", t_whole, t_frac, heater_pct);
    lcd_set_cursor(0, 0);
    lcd_putstr(buffer);

    // 2行目: 湿度と転卵タイマー (例: 55% Next:119m)
    int h_whole = (int)hum;
    uint16_t next_min = (uint16_t)(next_turn_sec / 60);
    sprintf(buffer, "%2d%% Next:%3dm   ", h_whole, next_min);
    lcd_set_cursor(0, 1);
    lcd_putstr(buffer);
}

// PID計算関数 (0.0〜1.0 の範囲で出力を返す)
float calculate_pid(float current_temp) {
    float error = TARGET_TEMP - current_temp;
    
    // 比例項
    float p_term = KP * error;
    
    // 積分項 (ワインドアップ対策付き)
    integral += error;
    if (integral > 100.0f) integral = 100.0f;
    if (integral < -100.0f) integral = -100.0f;
    float i_term = KI * integral;
    
    // 微分項
    float d_term = KD * (error - prev_error);
    prev_error = error;
    
    float output = p_term + i_term + d_term;
    
    // 出力制限 (0% 〜 100%)
    if (output > 100.0f) output = 100.0f;
    if (output < 0.0f) output = 0.0f;
    
    return output / 100.0f;
}

int main(void) {
    lcd_init();
    i2c_init();
    servo_init();
    
    // SSRピン(PD7)を出力に設定
    SSR_DDR |= (1 << SSR_PIN);
    SSR_PORT &= ~(1 << SSR_PIN); // 初期はOFF

    // ボタン(PD6)設定: 入力 + プルアップ有効
    DDRD &= ~(1 << PD6);
    PORTD |= (1 << PD6);

    lcd_set_cursor(0, 0);
    lcd_putstr("Hatchery System");
    lcd_set_cursor(0, 1);
    lcd_putstr("Starting PID...");
    _delay_ms(1500);

    aht25_init();

    float temperature = 0.0;
    float humidity = 0.0;
    uint32_t ticks_since_turn = 0;
    uint16_t ssr_tick_counter = 0;
    uint16_t sensor_update_ticks = 0;
    uint8_t current_side = 0;
    uint16_t heater_pct_display = 0;

    // 初回サーボ位置
    servo_set_angle(0);

    while(1) {
        // 1. SSR タイム・プロポーショナル制御 (10ms周期でON/OFF判定)
        if (heater_on_ticks > 0 && ssr_tick_counter < heater_on_ticks) {
            SSR_PORT |= (1 << SSR_PIN); // ON
        } else {
            SSR_PORT &= ~(1 << SSR_PIN); // OFF
        }
        
        ssr_tick_counter++;
        // 1秒(100 ticks)ごとにPID計算を更新
        if (ssr_tick_counter >= SSR_CYCLE_TICKS) {
            ssr_tick_counter = 0;
            if (temperature > 5.0f && temperature < 60.0f) { // センサー値が妥当な範囲ならPID計算
                float pid_out = calculate_pid(temperature);
                heater_on_ticks = (uint16_t)(pid_out * (float)SSR_CYCLE_TICKS);
                heater_pct_display = (uint16_t)(pid_out * 100.0f);
            } else {
                // センサー異常時は安全のためヒーターを完全に切る
                heater_on_ticks = 0;
                heater_pct_display = 0;
            }
        }

        // 2. ボタン入力 (手動転卵)
        if (!(PIND & (1 << PD6))) {
            _delay_ms(20); // デバウンス
            if (!(PIND & (1 << PD6))) {
                current_side = !current_side;
                servo_set_angle(current_side ? 60 : 0);
                ticks_since_turn = 0; // 自動転卵タイマーをリセット
                while (!(PIND & (1 << PD6))); // 離すまで待機
                _delay_ms(20);
            }
        }

        // 3. 自動転卵タイマー (2時間経過チェック)
        if (ticks_since_turn >= (TURN_INTERVAL_SEC * 100)) {
            current_side = !current_side;
            servo_set_angle(current_side ? 60 : 0);
            ticks_since_turn = 0;
        }

        // 4. センサー読み取りと表示更新 (2秒ごと)
        if (sensor_update_ticks == 0) {
            if (aht25_read_data(&temperature, &humidity, NULL)) {
                uint32_t remaining_sec = TURN_INTERVAL_SEC - (ticks_since_turn / 100);
                update_display(temperature, humidity, remaining_sec, heater_pct_display);
            } else {
                lcd_set_cursor(0, 0);
                lcd_putstr("Sensor Error!   ");
                lcd_set_cursor(0, 1);
                lcd_putstr("Heater HALTED   ");
                temperature = -99.0f; // PID計算を止めるためのフラグ
            }
        }

        _delay_ms(TICK_MS); // 10ms待機
        ticks_since_turn++;
        sensor_update_ticks++;
        if (sensor_update_ticks >= 200) sensor_update_ticks = 0;
    }
}
