#ifndef SSD1306_H
#define SSD1306_H

#include <stdint.h>
#include <stdbool.h>
#include "hardware/i2c.h"

// Rozměry displeje
#define SSD1306_WIDTH  128
#define SSD1306_HEIGHT 32

// I2C adresa (obvykle 0x3C)
#define SSD1306_I2C_ADDR 0x3C

// SSD1306 příkazy
#define SSD1306_SET_CONTRAST        0x81
#define SSD1306_DISPLAY_ALL_ON_RESUME 0xA4
#define SSD1306_NORMAL_DISPLAY      0xA6
#define SSD1306_DISPLAY_OFF         0xAE
#define SSD1306_DISPLAY_ON          0xAF
#define SSD1306_SET_DISPLAY_OFFSET  0xD3
#define SSD1306_SET_COM_PINS        0xDA
#define SSD1306_SET_VCOM_DETECT     0xDB
#define SSD1306_SET_DISPLAY_CLOCK   0xD5
#define SSD1306_SET_PRECHARGE       0xD9
#define SSD1306_SET_MULTIPLEX       0xA8
#define SSD1306_SET_LOW_COLUMN      0x00
#define SSD1306_SET_HIGH_COLUMN     0x10
#define SSD1306_SET_START_LINE      0x40
#define SSD1306_MEMORY_MODE         0x20
#define SSD1306_COLUMN_ADDR         0x21
#define SSD1306_PAGE_ADDR           0x22
#define SSD1306_COM_SCAN_DEC        0xC8
#define SSD1306_SEG_REMAP           0xA1
#define SSD1306_CHARGE_PUMP         0x8D

// Framebuffer: 128 * 32 / 8 = 512 bytů
#define SSD1306_BUFFER_SIZE (SSD1306_WIDTH * SSD1306_HEIGHT / 8)

// Inicializace displeje
void ssd1306_init(i2c_inst_t *i2c);

// Vymaže framebuffer (vše černé)
void ssd1306_clear(void);

// Odešle framebuffer na displej
void ssd1306_show(i2c_inst_t *i2c);

// Nakreslí pixel na souřadnicích x, y (hodnota: true = svítí, false = nesvítí)
void ssd1306_draw_pixel(int x, int y, bool on);

// Vypíše text (základní 5x7 font) na pozici x, y
void ssd1306_draw_string(int x, int y, const char *str);

// Nakreslí prázdný obdélník
void ssd1306_draw_rect(int x, int y, int w, int h, bool on);

// Nakreslí vyplněný obdélník
void ssd1306_fill_rect(int x, int y, int w, int h, bool on);

#endif // SSD1306_H
