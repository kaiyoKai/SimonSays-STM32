#include "hardware.h"

int main(void)
{
    SystemInit();
    init_hardware();
    
    while (1)
    {
        switch (current_state)
        {
            case STATE_INIT:
                systickDelayMs(10);
                current_state = STATE_START_LEVEL;
                break;
                
            case STATE_START_LEVEL:
                seq_len = current_level + 4; // Level 1 = 5 Bits, Level 5 = 9 Bits
                
                uint32_t rand_val = get_random_number();
                for (uint32_t i = 0; i < seq_len; i++) {
                    sequence[i] = (rand_val >> i) & 0x01;
                }
                
                input_index = 0;
                
                // Pause vor dem Abspielen der Sequenz
                state_timer = 1000;
                while (state_timer > 0) {}
                
                current_state = STATE_SHOW_SEQUENCE;
                break;
                
            case STATE_SHOW_SEQUENCE:
                disable_button_interrupts(); // Eingabe während der Anzeige sperren
                
                for (uint32_t i = 0; i < seq_len; i++) {
                    led_on();
                    if (sequence[i] == 1) {
                        state_timer = 1000;  // 1.0s für langes Signal
                    } else {
                        state_timer = 350;   // 0.35s für kurzes Signal
                    }
                    while (state_timer > 0) {}
                    
                    led_off();
                    state_timer = 200;       // Pause zwischen den Signalen
                    while (state_timer > 0) {}
                }
                
                // 1.5s Pause zum Einprägen vor der Eingabephase
                state_timer = 1500;
                while (state_timer > 0) {}
                
                input_index = 0;
                clear_button_flags();
                enable_button_interrupts();
                current_state = STATE_WAIT_INPUT;
                break;
                
            case STATE_WAIT_INPUT:
                // Energiesparmodus, Aufwachen bei Interrupt
                __WFI();
                break;
                
            case STATE_PLAYING_INPUT:
                while (state_timer > 0) {}
                led_off();
                
                // Kurze Pause zum Entprellen beim Loslassen
                state_timer = 50; 
                current_state = STATE_DEBOUNCE_RELEASE;
                break;
                
            case STATE_DEBOUNCE_RELEASE:
                while (state_timer > 0) {}
                
                if (last_pressed_button_correct) {
                    input_index++;
                    if (input_index >= seq_len) {
                        current_state = STATE_LEVEL_SUCCESS;
                    } else {
                        clear_button_flags();
                        enable_button_interrupts();
                        current_state = STATE_WAIT_INPUT;
                    }
                } else {
                    current_state = STATE_LEVEL_FAILURE;
                }
                break;
                
            case STATE_LEVEL_SUCCESS:
                disable_button_interrupts();
                
                // Erfolgs-Feedback: 2x lang blinken
                for (int i = 0; i < 2; i++) {
                    led_on();
                    state_timer = 1000;
                    while (state_timer > 0) {}
                    led_off();
                    state_timer = 200;
                    while (state_timer > 0) {}
                }
                
                if (current_level < 5) {
                    current_level++;
                }
                
                // 3s Pause vor dem nächsten Level
                state_timer = 3000;
                while (state_timer > 0) {}
                
                current_state = STATE_START_LEVEL;
                break;
                
            case STATE_LEVEL_FAILURE:
                disable_button_interrupts();
                
                // Fehler-Feedback: 3x kurz blinken
                for (int i = 0; i < 3; i++) {
                    led_on();
                    state_timer = 150;
                    while (state_timer > 0) {}
                    led_off();
                    state_timer = 150;
                    while (state_timer > 0) {}
                }
                
                current_level = 1;
                
                // 3s Pause vor dem Neustart
                state_timer = 3000;
                while (state_timer > 0) {}
                
                current_state = STATE_START_LEVEL;
                break;
        }
    }
}
