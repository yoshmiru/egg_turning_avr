# 設定
MCU = atmega8
F_CPU = 1000000UL
BAUD = 19200
PROGRAMMER = avrisp
PORT = /dev/ttyUSB0

# ファイル名
TARGET = main
SRCS = main.c lcd.c i2c.c aht25.c

# コンパイルオプション
CC = avr-gcc
OBJCOPY = avr-objcopy
CFLAGS = -mmcu=$(MCU) -DF_CPU=$(F_CPU) -Os -Wall

all: $(TARGET).hex

$(TARGET).elf: $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

flash: $(TARGET).hex
	avrdude -c $(PROGRAMMER) -p $(MCU) -P $(PORT) -b $(BAUD) -U flash:w:$<:i

clean:
	rm -f *.elf *.hex
