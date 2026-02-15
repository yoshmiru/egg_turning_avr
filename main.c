#include "lcd.h"
#include "aht25.h" // AHT25センサー用
#include "i2c.h"   // I2C関数用
#include <stdio.h> // sprintf を使うため
#include <util/delay.h> // _delay_ms を使うため

void i2c_scan(void) {
    char buf[16];
    lcd_clear();
    lcd_putstr("I2C Scanning...");
    _delay_ms(1000);

    for (uint8_t addr = 1; addr < 127; addr++) {
        if (i2c_start()) {
            // 書き込みモードでアドレスを投げ、ACKが返るか確認
            if (i2c_write(addr << 1 | 0)) {
                // 返事があった！
                lcd_clear();
                sprintf(buf, "Found: 0x%02X", addr);
                lcd_putstr(buf);
                i2c_stop();
                _delay_ms(2000); // 確認のために2秒止める
            } else {
                i2c_stop();
            }
        }
        _delay_us(100); // 連続呼び出しによるセンサーのフリーズ防止
    }
    lcd_clear();
    lcd_putstr("Scan Finished");
}

// 温度表示関数
void display_temp(float temp) {
    char buffer[16];
    int whole = (int)temp;
    int fraction = (int)((temp - whole) * 10);
    sprintf(buffer, "Temp: %d.%d C   ", whole, fraction);
    lcd_putstr(buffer);
}

// 湿度表示関数
void display_humidity(float humidity) {
    char buffer[16];
    int whole = (int)humidity;
    int fraction = (int)((humidity - whole) * 10);
    sprintf(buffer, "Humi: %d.%d %%   ", whole, fraction);
    lcd_putstr(buffer);
}

int main(void) {
    lcd_init();
        lcd_set_cursor(0, 0);
        lcd_putstr("Initializing...");
        _delay_ms(1000); // 1秒間表示
    
        i2c_init(); // I2C初期化
        i2c_scan();
        if (!aht25_init()) { // AHT25センサー初期化
            lcd_clear();
            lcd_set_cursor(0, 0);
            lcd_putstr("AHT25 Init Fail!");
            while(1); // 初期化失敗で停止
        } else {
            lcd_debug_message("AHT25 Ready!");
            _delay_ms(1000); // 1秒間表示
        }
    
    float temperature = 0.0;
    float humidity = 0.0;
    uint8_t raw_sensor_data[7]; // 生の7バイトデータを格納
    
    while(1) {
      if (aht25_read_data(&temperature, &humidity, raw_sensor_data)) {
          // 温度を表示
          lcd_set_cursor(0, 0); // 次の表示のためにカーソルをセット
          display_temp(temperature);
          // 湿度を表示
          lcd_set_cursor(0, 1); // 次の表示のためにカーソルをセット
          display_humidity(humidity);
      } else {
          _delay_ms(1000); // 連続呼び出しによるセンサーのフリーズ防止
          // raw_sensor_dataの内容を16進数で表示
          char error_buf[16];
          lcd_set_cursor(0, 0);
          sprintf(error_buf, "%02X %02X %02X %02X", raw_sensor_data[0], raw_sensor_data[1], raw_sensor_data[2], raw_sensor_data[3]);
          lcd_putstr(error_buf);
          sprintf(error_buf, "%02X %02X %02X", raw_sensor_data[4], raw_sensor_data[5], raw_sensor_data[6]);
          lcd_set_cursor(0, 1);
          lcd_putstr(error_buf);
      }
      _delay_ms(2000); // メッセージを2秒間表示
    }
}
