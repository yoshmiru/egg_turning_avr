#ifndef LCD_H
#define LCD_H

#include <avr/io.h>
#include <util/delay.h>

// ピン接続の定義 (PD0-PD5)
#define LCD_PORT PORTD
#define LCD_DDR  DDRD
#define RS       PD0
#define EN       PD1
#define D4       PD2
#define D5       PD3
#define D6       PD4
#define D7       PD5

// 公開関数
void lcd_init(void);
void lcd_command(uint8_t cmd);
void lcd_data(uint8_t data);
void lcd_putstr(const char *s);
void lcd_clear(void);
void lcd_set_cursor(uint8_t col, uint8_t row);
void lcd_debug_message(const char *msg);

#endif
