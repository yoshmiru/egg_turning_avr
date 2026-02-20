#include "lcd.h"
#include "aht25.h"
#include "i2c.h"
#include "servo.h"
#include <stdio.h>
#include <util/delay.h>

// 温度表示関数
void display_temp(float temp) {
    char buffer[16];
    int whole = (int)temp;
    int fraction = (int)((temp - whole) * 10);
    if (temp < 0 && fraction < 0) fraction = -fraction;
    sprintf(buffer, "Temp: %d.%d C   ", whole, fraction);
    lcd_set_cursor(0, 1);
    lcd_putstr(buffer);
}

// 湿度表示関数
void display_humidity(float humidity) {
    char buffer[16];
    int whole = (int)humidity;
    int fraction = (int)((humidity - whole) * 10);
    sprintf(buffer, "Humi: %d.%d %%   ", whole, fraction);
    lcd_set_cursor(0, 1);
    lcd_putstr(buffer);
}

int main(void) {
    lcd_init();
    i2c_init();
    servo_init();
    
    // ボタン PD6 を入力に設定し、プルアップ有効
    DDRD &= ~(1 << PD6);
    PORTD |= (1 << PD6);

    lcd_set_cursor(0, 0);
    lcd_putstr("Initializing...");
    _delay_ms(1000);

    if (!aht25_init()) {
        lcd_clear();
        lcd_set_cursor(0, 0);
        lcd_putstr("AHT25 Init Fail!");
        // 初期化失敗してもボタン操作は試せるように while(1) は避けるか、エラー表示のまま進む
    }

    float temperature = 0.0;
    float humidity = 0.0;
    uint8_t angle_state = 0; // 0: 0度, 1: 30度, 2: 60度
    uint16_t loop_cnt = 0;

    while(1) {
        // 1. ボタン入力チェック (約10msごとにチェック)
        if (!(PIND & (1 << PD6))) {
            _delay_ms(20); // チャタリング防止
            if (!(PIND & (1 << PD6))) {
                angle_state = (angle_state + 1) % 3;
                if (angle_state == 0) servo_set_angle(0);
                else if (angle_state == 1) servo_set_angle(30);
                else if (angle_state == 2) servo_set_angle(60);
                
                // ボタンが離されるまで待機
                while (!(PIND & (1 << PD6)));
                _delay_ms(20);
            }
        }

        // 2. 温湿度表示の更新 (約2秒ごとに切り替え)
        if (loop_cnt == 0) {
            lcd_set_cursor(0, 0);
            lcd_putstr("Hatchery System");
            if (aht25_read_data(&temperature, &humidity, NULL)) {
                display_temp(temperature);
            } else {
                lcd_set_cursor(0, 1);
                lcd_putstr("Sensor Error!   ");
            }
        } else if (loop_cnt == 200) { // 10ms * 200 = 2s
            if (aht25_read_data(&temperature, &humidity, NULL)) {
                display_humidity(humidity);
            }
        }

        _delay_ms(10);
        loop_cnt++;
        if (loop_cnt >= 400) loop_cnt = 0; // 4秒周期でループ
    }
}
