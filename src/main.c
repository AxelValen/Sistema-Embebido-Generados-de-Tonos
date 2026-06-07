#include <stdio.h>
#include <string.h>
#include <stdlib.h> 
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"

#define PWM_BUZZER_PIN 16
#define POTENTIOMEMTER_PIN 26
#define SDA_PIN 4
#define SCL_PIN 5
#define R1_PIN 6
#define R2_PIN 7 
#define R3_PIN 8
#define R4_PIN 9
#define C1_PIN 10
#define C2_PIN 11
#define C3_PIN 12
#define C4_PIN 13

const float conversion_factor = 3.3f / (1 << 12);
uint slice_num;
uint chan;
volatile char key_pending = '\0';
char freq_buffer[5] = {0};
uint32_t freq = 0;
int freq_index = 0;
int volumen = 0;
enum State {INACTIVE, ENTERING_FREQ};
enum State current_state = INACTIVE;


/*%-------------------------Codigo 4x4 matrix keypad---------------------------%*/
const int KEYPAD_ROWS[] = {R1_PIN, R2_PIN, R3_PIN, R4_PIN}; 
const int KEYPAD_COLS[] = {C1_PIN, C2_PIN, C3_PIN, C4_PIN};
char keypad[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};
void setupKeypad() {
    for (int row = 0; row < 4; row++) {
        gpio_init(KEYPAD_ROWS[row]);
        gpio_set_dir(KEYPAD_ROWS[row], GPIO_IN);
        gpio_pull_up(KEYPAD_ROWS[row]);
    }
    for (int col = 0; col < 4; col++) {
        gpio_init(KEYPAD_COLS[col]);
        gpio_set_dir(KEYPAD_COLS[col], GPIO_OUT);
        gpio_put(KEYPAD_COLS[col], 1);
    }
}
//------------------------- FIN Codigo 4x4 matrix keypad-------------------------//


//-------------------------------Codigo LCD----------------------------------//
const int LCD_CLEARDISPLAY = 0x01;
const int LCD_RETURNHOME = 0x02;
const int LCD_ENTRYMODESET = 0x04;
const int LCD_DISPLAYCONTROL = 0x08;
const int LCD_CURSORSHIFT = 0x10;
const int LCD_FUNCTIONSET = 0x20;
const int LCD_SETCGRAMADDR = 0x40;
const int LCD_SETDDRAMADDR = 0x80;

// flags for display entry mode
const int LCD_ENTRYSHIFTINCREMENT = 0x01;
const int LCD_ENTRYLEFT = 0x02;

// flags for display and cursor control
const int LCD_BLINKON = 0x01;
const int LCD_CURSORON = 0x02;
const int LCD_DISPLAYON = 0x04;

// flags for display and cursor shift
const int LCD_MOVERIGHT = 0x04;
const int LCD_DISPLAYMOVE = 0x08;

// flags for function set
const int LCD_5x10DOTS = 0x04;
const int LCD_2LINE = 0x08;
const int LCD_8BITMODE = 0x10;

// flag for backlight control
const int LCD_BACKLIGHT = 0x08;

const int LCD_ENABLE_BIT = 0x04;

// By default these LCD display drivers are on bus address 0x27
static int addr = 0x27;

// Modes for lcd_send_byte
#define LCD_CHARACTER  1
#define LCD_COMMAND    0

#define MAX_LINES      2
#define MAX_CHARS      16

/* Quick helper function for single byte transfers */
void i2c_write_byte(uint8_t val) {
#ifdef i2c_default
    i2c_write_blocking(i2c_default, addr, &val, 1, false);
#endif
}

void lcd_toggle_enable(uint8_t val) {
    // Toggle enable pin on LCD display
    // We cannot do this too quickly or things don't work
#define DELAY_US 600
    sleep_us(DELAY_US);
    i2c_write_byte(val | LCD_ENABLE_BIT);
    sleep_us(DELAY_US);
    i2c_write_byte(val & ~LCD_ENABLE_BIT);
    sleep_us(DELAY_US);
}

// The display is sent a byte as two separate nibble transfers
void lcd_send_byte(uint8_t val, int mode) {
    uint8_t high = mode | (val & 0xF0) | LCD_BACKLIGHT;
    uint8_t low = mode | ((val << 4) & 0xF0) | LCD_BACKLIGHT;

    i2c_write_byte(high);
    lcd_toggle_enable(high);
    i2c_write_byte(low);
    lcd_toggle_enable(low);
}

void lcd_clear(void) {
    lcd_send_byte(LCD_CLEARDISPLAY, LCD_COMMAND);
}

// go to location on LCD
void lcd_set_cursor(int line, int position) {
    int val = (line == 0) ? 0x80 + position : 0xC0 + position;
    lcd_send_byte(val, LCD_COMMAND);
}

static inline void lcd_char(char val) {
    lcd_send_byte(val, LCD_CHARACTER);
}

void lcd_string(const char *s) {
    while (*s) {
        lcd_char(*s++);
    }
}

void lcd_init() {
    lcd_send_byte(0x03, LCD_COMMAND);
    lcd_send_byte(0x03, LCD_COMMAND);
    lcd_send_byte(0x03, LCD_COMMAND);
    lcd_send_byte(0x02, LCD_COMMAND);

    lcd_send_byte(LCD_ENTRYMODESET | LCD_ENTRYLEFT, LCD_COMMAND);
    lcd_send_byte(LCD_FUNCTIONSET | LCD_2LINE, LCD_COMMAND);
    lcd_send_byte(LCD_DISPLAYCONTROL | LCD_DISPLAYON, LCD_COMMAND);
    lcd_clear();
}

void lcd_display_formatted(const char *format, ...) {
    char buffer[MAX_CHARS * MAX_LINES + 1]; // Buffer para el texto formateado
    char *lines[MAX_LINES] = {0};          // Punteros a cada línea
    int line_count = 0;
    
    // Procesar los argumentos variables
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    // Dividir el texto en líneas (separadas por \n)
    char *token = strtok(buffer, "\n");
    while (token != NULL && line_count < MAX_LINES) {
        lines[line_count++] = token;
        token = strtok(NULL, "\n");
    }
    
    // Mostrar cada línea en el LCD
    lcd_clear();
    for (int i = 0; i < line_count; i++) {
        lcd_set_cursor(i, 0);
        
        lcd_string(lines[i]);
    }
}
//--------------------------FIN Codigo LCD----------------------------------//

uint32_t pwm_set_freq_duty (uint slice_num, uint chan, uint32_t f, int d){
    uint32_t clock = SYS_CLK_HZ;
    uint32_t divider16 = clock/f/4096 + (clock % (f*4096) != 0 );

    if (divider16/16 == 0){
        divider16 = 16;
    }

    uint32_t wrap = clock * 16 / divider16 / f-1;

    pwm_set_clkdiv_int_frac(slice_num, divider16/16, divider16 & 0xF);
    pwm_set_wrap(slice_num, wrap);
    pwm_set_chan_level(slice_num, chan, wrap*d/100);
    
    return wrap;
}

void reset_freq_input() {
    freq_index = 0;
    memset(freq_buffer, 0, sizeof(freq_buffer));
    current_state = INACTIVE;
}

bool scanKeypadTimer(struct repeating_timer *t) {
    static int current_col = 0;
    static bool key_pressed = false;

    // Desactivar todas las columnas
    for (int c = 0; c < 4; c++) {
        gpio_put(KEYPAD_COLS[c], 1);
    }

    // Activar columna actual
    gpio_put(KEYPAD_COLS[current_col], 0);

    // Escanear filas
    for (int row = 0; row < 4; row++) {
        if (gpio_get(KEYPAD_ROWS[row]) == 0) {
            char k = keypad[row][current_col];

            if (!key_pressed && key_pending == '\0') {
                key_pending = k;
                key_pressed = true;
            }
        }
    }

    // Detectar si se soltaron todas las teclas
    bool all_released = true;
    for (int r = 0; r < 4; r++) {
        if (gpio_get(KEYPAD_ROWS[r]) == 0) {
            all_released = false;
            break;
        }
    }
    if (all_released) {
        key_pressed = false;
    }

    // Avanzar a la siguiente columna
    current_col = (current_col + 1) % 4;

    //============================= Control del tono ============================//
    volumen = (adc_read() * conversion_factor / 3.3) * 100;
    if (volumen > 90) volumen = 90;

    if (freq > 0) {
        pwm_set_freq_duty(slice_num, chan, freq, volumen);
    } else {
        pwm_set_chan_level(slice_num, chan, 0);
    }

    return true;
}

void keypad_interrupt_handler(){
    printf("KEYPAD INTERRUPT TRIGGERED \n");
    static absolute_time_t last_interrupt = 0;
    absolute_time_t now = get_absolute_time();
    
    if(absolute_time_diff_us(last_interrupt, now) > 10000) { //debounce
        last_interrupt = now;

        char k = key_pending;
        key_pending = '\0'; // Marca como procesada
        printf("Key: %c\n", k);

        switch(k){
          case '0'...'9':
              if (current_state == ENTERING_FREQ && freq_index <= 4) {
                  freq_buffer[freq_index++] = k;
                  printf("Frecuencia: %sHz\n", freq_buffer);
              } else if (current_state == INACTIVE) {
                  current_state = ENTERING_FREQ;
                  freq_buffer[freq_index++] = k;
                  printf("Frecuencia: %sHz\n", freq_buffer);
              }
              break;
              
          case '#':
              if (current_state == ENTERING_FREQ) {
                  freq_buffer[freq_index] = '\0';
                  freq = atoi(freq_buffer);
                  if (freq < 20) freq = 20;
                  if (freq > 20000) freq = 18000;
                  reset_freq_input();
              }
              break;
              
          case '*':
              reset_freq_input();
              freq_buffer[0]='0';
              freq = 0; // Silenciar
              printf("Silenciado \n");
              break;
        }
    }
}

int main() {
    stdio_init_all();
    setupKeypad();
  #if !defined(i2c_default) || !defined(PICO_DEFAULT_I2C_SDA_PIN) || !defined(PICO_DEFAULT_I2C_SCL_PIN)
      #warning i2c/lcd_1602_i2c example requires a board with I2C pins
  #else
      // This example will use I2C0 on the default SDA and SCL pins (4, 5 on a Pico)
      i2c_init(i2c_default, 100 * 1000);
      gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
      gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
      gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
      gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);
      // Make the I2C pins available to picotool
      bi_decl(bi_2pins_with_func(PICO_DEFAULT_I2C_SDA_PIN, PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C));

      lcd_init();

      adc_init();
      adc_gpio_init(POTENTIOMEMTER_PIN);
      adc_select_input(0);

      gpio_set_function(PWM_BUZZER_PIN, GPIO_FUNC_PWM);
      uint slice_num = pwm_gpio_to_slice_num(PWM_BUZZER_PIN);
      uint chan = pwm_gpio_to_channel(PWM_BUZZER_PIN);

      pwm_set_enabled(slice_num, true);

      struct repeating_timer timer;
      add_repeating_timer_ms(10, scanKeypadTimer, NULL, &timer);
      
      for (int row = 0; row < 4; row++) {
          gpio_set_irq_enabled_with_callback(KEYPAD_ROWS[row], GPIO_IRQ_EDGE_RISE, true, &keypad_interrupt_handler);
      }

      while (1) {
        if(current_state == ENTERING_FREQ){
          lcd_display_formatted("Freq: %s Hz\nVolumen: %d %%\n", freq_buffer, volumen);
        }
        else if(current_state == INACTIVE){
          lcd_display_formatted("Freq: %d Hz\nVolumen: %d %%\n", freq, volumen);
        }
        sleep_ms(100);
        tight_loop_contents();
      }
  #endif
}