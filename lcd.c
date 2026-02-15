#include "lcd.h"

// ストロボ信号（Enable）を送る
static void lcd_pulse_enable(void) {
    LCD_PORT |= (1 << EN);
    _delay_ms(2); // Rustで失敗した要因と思われる短いパルスを避け、2ms確保
    LCD_PORT &= ~(1 << EN);
    _delay_ms(2);
}

// 4ビット分を送信する
static void lcd_send_4bit(uint8_t val) {
    // 一旦データピン(PD2-PD5)をクリア
    LCD_PORT &= ~((1 << D4) | (1 << D5) | (1 << D6) | (1 << D7));
    
    // 値をセット
    if (val & 0x01) LCD_PORT |= (1 << D4);
    if (val & 0x02) LCD_PORT |= (1 << D5);
    if (val & 0x04) LCD_PORT |= (1 << D6);
    if (val & 0x08) LCD_PORT |= (1 << D7);
    
    lcd_pulse_enable();
}

// 1バイト(8bit)を2回に分けて送信
static void lcd_send_byte(uint8_t val, uint8_t is_data) {
    if (is_data) {
        LCD_PORT |= (1 << RS);
    } else {
        LCD_PORT &= ~(1 << RS);
    }
    
    lcd_send_4bit(val >> 4);   // 上位4ビット
    lcd_send_4bit(val & 0x0F); // 下位4ビット
    _delay_us(100);            // 実行待ち
}

void lcd_init(void) {
    LCD_DDR |= (1 << RS) | (1 << EN) | (1 << D4) | (1 << D5) | (1 << D6) | (1 << D7);
    _delay_ms(100); // 電源立ち上がり待ち

    // 4ビットモード初期化の儀式
    LCD_PORT &= ~(1 << RS);
    lcd_send_4bit(0x03); _delay_ms(5);
    lcd_send_4bit(0x03); _delay_ms(5);
    lcd_send_4bit(0x03); _delay_ms(5);
    lcd_send_4bit(0x02); _delay_ms(5); // 4bitモード確定

    // 設定
    lcd_command(0x28); // 2行, 5x8ドット
    lcd_command(0x0C); // 表示ON, カーソルOFF
    lcd_command(0x06); // エントリモード: 増分
    lcd_clear();
}

void lcd_command(uint8_t cmd) { lcd_send_byte(cmd, 0); }
void lcd_data(uint8_t data) { lcd_send_byte(data, 1); }

void lcd_clear(void) {
    lcd_command(0x01);
    _delay_ms(2);
}

void lcd_putstr(const char *s) {
    while (*s) lcd_data(*s++);
}

void lcd_set_cursor(uint8_t col, uint8_t row) {
    uint8_t addr = (row == 0) ? 0x00 : 0x40;
    lcd_command(0x80 | (addr + col));
}

// デバッグ用: 0.5秒間渡されたテキストを表示する関数
void lcd_debug_message(const char *msg) {
    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_putstr(msg);
    _delay_ms(500);
}
