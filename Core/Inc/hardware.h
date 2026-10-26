#ifndef HARDWARE_H
#define HARDWARE_H

#ifdef DESKTOP_SIMULATION
#include <stdint.h>
#include <unistd.h>
#define __WFI() usleep(50000)
#define SystemInit() (void)0
#else
#include "stm32g431xx.h"
#endif

// Game States
typedef enum {
    STATE_INIT,
    STATE_START_LEVEL,
    STATE_SHOW_SEQUENCE,
    STATE_WAIT_INPUT,
    STATE_PLAYING_INPUT,
    STATE_DEBOUNCE_RELEASE,
    STATE_LEVEL_SUCCESS,
    STATE_LEVEL_FAILURE
} GameState_t;

extern volatile GameState_t current_state;
extern volatile uint32_t state_timer;
extern volatile uint32_t ms_ticks;
extern volatile uint8_t sequence[9];
extern volatile uint32_t seq_len;
extern volatile uint32_t input_index;
extern volatile uint8_t last_pressed_button_correct;
extern volatile uint32_t current_level;

void led_on(void);
void led_off(void);

void systickDelayMs(int n);

void disable_button_interrupts(void);
void enable_button_interrupts(void);

void clear_button_flags(void);

void init_hardware(void);

uint32_t get_random_number(void);

#endif
