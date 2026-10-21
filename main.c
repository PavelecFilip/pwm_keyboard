#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "ssd1306.h"

// ─────────────────────────────────────────────────────────────────────────────
// Pinout
// ─────────────────────────────────────────────────────────────────────────────

// Klávesnice
#define ROW_COUNT  4
#define COL_COUNT  4
// R1→GPIO2, R2→GPIO3, R3→GPIO4, R4→GPIO5
// C1→GPIO6, C2→GPIO7, C3→GPIO8, C4→GPIO9
static const uint ROW_PINS[ROW_COUNT] = { 2, 3, 4, 5 };
static const uint COL_PINS[COL_COUNT] = { 6, 7, 8, 9 };

//        C1    C2    C3    C4
static const char KEY_MAP[ROW_COUNT][COL_COUNT] = {
    { '1', '2', '3', 'A' },  // R1
    { '4', '5', '6', 'B' },  // R2
    { '7', '8', '9', 'C' },  // R3
    { '*', '0', '#', 'D' },  // R4
};

// PWM výstup
#define PWM_OUT_PIN  15

// I2C / OLED
#define I2C_PORT    i2c0
#define I2C_SDA     16
#define I2C_SCL     17

// ─────────────────────────────────────────────────────────────────────────────
// Stav
// ─────────────────────────────────────────────────────────────────────────────
typedef enum { MODE_FREQ, MODE_DUTY } input_mode_t;

static uint     pwm_slice;
static uint     pwm_chan;
static bool     pwm_running  = false;
static uint32_t current_freq = 1000;  // Hz
static uint8_t  current_duty = 50;    // %

static input_mode_t mode = MODE_FREQ;

static char input_buf[8] = {0};
static int  input_len    = 0;

// ─────────────────────────────────────────────────────────────────────────────
// PWM helpers
// ─────────────────────────────────────────────────────────────────────────────
static void pwm_hw_init(void) {
    gpio_set_function(PWM_OUT_PIN, GPIO_FUNC_PWM);
    pwm_slice = pwm_gpio_to_slice_num(PWM_OUT_PIN);
    pwm_chan  = pwm_gpio_to_channel(PWM_OUT_PIN);
    pwm_set_enabled(pwm_slice, false);
}

// Aplikuje current_freq + current_duty na hardware
static void pwm_apply(void) {
    if (current_freq == 0) {
        pwm_set_enabled(pwm_slice, false);
        pwm_running = false;
        return;
    }

    // Najdi nejlepší kombinaci děličky a wrap pro požadovanou frekvenci.
    // Dělička může být float (1.0–255.9375), wrap max 65535.
    // Cíl: wrap = clk / (divider * freq), co nejblíže 65535 pro maximální rozlišení střidy.
    const uint32_t clk = 125000000;
    float best_div = 1.0f;
    uint32_t best_wrap = clk / current_freq;

    if (best_wrap > 65535) {
        // Celočíselná dělička nestačí — zkus float dělení
        float div_f = (float)clk / (65535.0f * (float)current_freq);
        if (div_f > 255.9375f) div_f = 255.9375f;
        best_div  = div_f;
        best_wrap = (uint32_t)((float)clk / (best_div * (float)current_freq));
    }
    if (best_wrap == 0) best_wrap = 1;
    if (best_wrap > 65535) best_wrap = 65535;

    uint32_t level = (best_wrap * current_duty) / 100;
    if (level > best_wrap) level = best_wrap;

    pwm_set_clkdiv(pwm_slice, best_div);
    pwm_set_wrap(pwm_slice, (uint16_t)(best_wrap - 1));
    pwm_set_chan_level(pwm_slice, pwm_chan, (uint16_t)level);
    pwm_set_enabled(pwm_slice, true);
    pwm_running = true;
}

static void pwm_stop(void) {
    pwm_set_enabled(pwm_slice, false);
    pwm_running = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Klávesnice
// ─────────────────────────────────────────────────────────────────────────────
static void keyboard_init(void) {
    for (int r = 0; r < ROW_COUNT; r++) {
        gpio_init(ROW_PINS[r]);
        gpio_set_dir(ROW_PINS[r], GPIO_OUT);
        gpio_put(ROW_PINS[r], 1);
    }
    for (int c = 0; c < COL_COUNT; c++) {
        gpio_init(COL_PINS[c]);
        gpio_set_dir(COL_PINS[c], GPIO_IN);
        gpio_pull_up(COL_PINS[c]);
    }
}

static char keyboard_scan(void) {
    for (int r = 0; r < ROW_COUNT; r++) {
        gpio_put(ROW_PINS[r], 0);
        sleep_us(20);
        for (int c = 0; c < COL_COUNT; c++) {
            if (!gpio_get(COL_PINS[c])) {
                sleep_ms(20);  // debounce
                if (!gpio_get(COL_PINS[c])) {
                    while (!gpio_get(COL_PINS[c])) sleep_ms(5);  // čekej na puštění
                    gpio_put(ROW_PINS[r], 1);
                    return KEY_MAP[r][c];
                }
            }
        }
        gpio_put(ROW_PINS[r], 1);
    }
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Displej
// ─────────────────────────────────────────────────────────────────────────────
static void display_update(void) {
    char buf[22];
    ssd1306_clear();

    // Řádek 1: frekvence
    if (mode == MODE_FREQ) {
        snprintf(buf, sizeof(buf), "FREQ>%6lu Hz", (unsigned long)current_freq);
    } else {
        snprintf(buf, sizeof(buf), "FREQ:%6lu Hz", (unsigned long)current_freq);
    }
    ssd1306_draw_string(0, 0, buf);

    // Řádek 2: duty cycle
    if (mode == MODE_DUTY) {
        snprintf(buf, sizeof(buf), "DUTY>%3d%%", current_duty);
    } else {
        snprintf(buf, sizeof(buf), "DUTY:%3d%%", current_duty);
    }
    ssd1306_draw_string(0, 9, buf);

    // Řádek 3: duty cycle bar (šířka 80px)
    ssd1306_draw_rect(0, 19, 82, 7, true);
    int fill = (current_duty * 80) / 100;
    if (fill > 0) ssd1306_fill_rect(1, 20, fill, 5, true);

    // Řádek 3 vpravo: stav PWM
    ssd1306_draw_string(86, 19, pwm_running ? "ON" : "OFF");

    // Řádek 4: input buffer
    if (input_len > 0) {
        snprintf(buf, sizeof(buf), "IN: %s_", input_buf);
        ssd1306_draw_string(0, 27, buf);
    }

    ssd1306_show(I2C_PORT);
}

// ─────────────────────────────────────────────────────────────────────────────
// Zpracování klávesy
// ─────────────────────────────────────────────────────────────────────────────
static void handle_key(char key) {

    if (key >= '0' && key <= '9') {
        if (input_len < 6) {
            input_buf[input_len++] = key;
            input_buf[input_len]   = '\0';
        }

    } else if (key == '#') {
        // Potvrzení hodnoty
        if (input_len > 0) {
            uint32_t val = (uint32_t)atoi(input_buf);
            if (mode == MODE_FREQ) {
                if (val > 0 && val <= 62500000) {
                    current_freq = val;
                    if (pwm_running) pwm_apply();
                    printf("FREQ: %lu Hz\n", (unsigned long)current_freq);
                }
            } else {
                if (val <= 100) {
                    current_duty = (uint8_t)val;
                    if (pwm_running) pwm_apply();
                    printf("DUTY: %lu %%\n", (unsigned long)val);
                }
            }
            input_len = 0;
            input_buf[0] = '\0';
        } else {
            // Prázdný buffer + # = spusť/zastav PWM
            if (pwm_running) {
                pwm_stop();
                printf("PWM zastaven.\n");
            } else {
                pwm_apply();
                printf("PWM spusten: %lu Hz, %d%%\n",
                       (unsigned long)current_freq, current_duty);
            }
        }

    } else if (key == '*') {
        // Smazání bufferu
        input_len = 0;
        input_buf[0] = '\0';

    } else if (key == 'A') {
        // Přepni na zadávání frekvence
        mode = MODE_FREQ;
        input_len = 0;
        input_buf[0] = '\0';
        printf("Rezim: FREKVENCE\n");

    } else if (key == 'B') {
        // Přepni na zadávání duty cycle
        mode = MODE_DUTY;
        input_len = 0;
        input_buf[0] = '\0';
        printf("Rezim: DUTY CYCLE\n");

    } else if (key == 'C') {
        // Předvolba: 1kHz / 50%
        current_freq = 1000;
        current_duty = 50;
        input_len = 0;
        input_buf[0] = '\0';
        pwm_apply();
        printf("Predvolba: 1000 Hz, 50%%\n");

    } else if (key == 'D') {
        // Stop
        input_len = 0;
        input_buf[0] = '\0';
        pwm_stop();
        printf("PWM zastaven (D).\n");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────
int main(void) {
    stdio_init_all();

    // Počkáme na stabilizaci napájení po připojení USB
    sleep_ms(250);

    // I2C + OLED
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Krátká pauza aby měl displej čas nastartovat napájecí obvody
    sleep_ms(100);
    ssd1306_init(I2C_PORT);

    // Klávesnice + PWM
    keyboard_init();
    pwm_hw_init();

    printf("\n=== PWM Generator ===\n");
    printf("A = rezim FREQ  B = rezim DUTY\n");
    printf("cisla + # = nastavit hodnotu\n");
    printf("prazdny # = spustit/zastavit\n");
    printf("* = smazat  C = predvolba 1kHz/50%%  D = stop\n\n");

    // Uvítací obrazovka
    ssd1306_clear();
    ssd1306_draw_string(10, 4,  "PWM GENERATOR");
    ssd1306_draw_string(16, 14, "A=FREQ B=DUTY");
    ssd1306_show(I2C_PORT);
    sleep_ms(2000);

    display_update();

    while (true) {
        char key = keyboard_scan();
        if (key != 0) {
            handle_key(key);
            display_update();
        }
        sleep_ms(10);
    }

    return 0;
}
