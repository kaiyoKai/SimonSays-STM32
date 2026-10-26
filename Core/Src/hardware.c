#include "hardware.h"


/* INFO:
 * Ich mag Kommentare die Werte enthalten normalerweise nicht, weil sie schnell mit dem Code desyncen können.
 * Ein Type der die Zeiteinheit impliziert besitz ist meiner meinung nach der verständlichkeit her,wenn es nicht performance kritisch ist
 * oft zu bevorzugen ,aber für den scope der aufgabe belasse ich es bei Integer.
 * -----------------------------------------------------------------------------------------
 * Falls eine Kommentar im desync ist oder zu selbst erklärend oder gar nicht erklärend genug ist.
 * Es ist ein schmaler Grad oftmals, Ich bin für Feedback offen, schreibe leider normalerweise so gut wie keine Kommentare
 * Jedoch ist ausführlich zu kommentieren explzit teil der Aufgabenstellung.
 * ----------------------------------------------------------------------------------------------------------------------
*/


volatile GameState_t current_state = STATE_INIT;
volatile uint32_t state_timer = 0;
volatile uint32_t ms_ticks = 0;
volatile uint8_t sequence[9] = {0};
volatile uint32_t seq_len = 5;
volatile uint32_t input_index = 0;
volatile uint8_t last_pressed_button_correct = 0;
volatile uint32_t current_level = 1;



void led_on(void)
{
    GPIOB->ODR |= (1 << 8);  // PB8 auf HIGH
}

void led_off(void)
{
    GPIOB->ODR &= ~(1 << 8); // PB8 auf LOW
}

// Aus den Vorlesungsfolien übernommen (SysTick-Delay)
void systickDelayMs(int n)
{
    SysTick->LOAD = 16000;       // Zyklen pro ms bei HSI (16 MHz)
    SysTick->VAL = 0;
    SysTick->CTRL = 0x5;         // Enable Systick

    for (int i = 0; i < n; i++)
    {
        while ((SysTick->CTRL & 0x10000) == 0) {}
    }
    SysTick->CTRL = 0;
}

void disable_button_interrupts(void)
{
    EXTI->IMR1 &= ~(EXTI_IMR1_IM0 | EXTI_IMR1_IM2);
}

void enable_button_interrupts(void)
{
    EXTI->IMR1 |= EXTI_IMR1_IM0 | EXTI_IMR1_IM2;
}

void clear_button_flags(void)
{
    EXTI->PR1 = (1 << 0) | (1 << 2);
    NVIC_ClearPendingIRQ(EXTI0_IRQn);
    NVIC_ClearPendingIRQ(EXTI2_IRQn);
}

void init_hardware(void)
{
    // HSI (16 MHz) aktivieren
    RCC->CR |= RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY)) {}

    // Taktfreigabe für GPIOA und GPIOB
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN;

    // PB8 (LED) als Ausgang
    GPIOB->MODER &= ~(3 << (8 * 2));
    GPIOB->MODER |= (1 << (8 * 2));
    led_off();

    // PA0 und PA2 als Eingänge
    GPIOA->MODER &= ~(3 << (0 * 2));
    GPIOA->MODER &= ~(3 << (2 * 2));

    // Interne Pull-Ups aktivieren (schalten gegen GND)
    GPIOA->PUPDR &= ~(3 << (0 * 2));
    GPIOA->PUPDR |= (1 << (0 * 2));
    GPIOA->PUPDR &= ~(3 << (2 * 2));
    GPIOA->PUPDR |= (1 << (2 * 2));

    // EXTI für Taster konfigurieren (fallende Flanke)
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    SYSCFG->EXTICR[0] &= ~(SYSCFG_EXTICR1_EXTI0 | SYSCFG_EXTICR1_EXTI2);

    EXTI->FTSR1 |= EXTI_FTSR1_FT0 | EXTI_FTSR1_FT2;
    EXTI->RTSR1 &= ~(EXTI_RTSR1_RT0 | EXTI_RTSR1_RT2);

    clear_button_flags();
    enable_button_interrupts();

    // NVIC-Konfiguration für Taster (EXTI)
    NVIC_SetPriority(EXTI0_IRQn, 2);
    NVIC_SetPriority(EXTI2_IRQn, 2);
    NVIC_EnableIRQ(EXTI0_IRQn);
    NVIC_EnableIRQ(EXTI2_IRQn);

    // TIM6 konfigurieren (1 ms Zeitbasis)
    RCC->APB1ENR1 |= RCC_APB1ENR1_TIM6EN;
    TIM6->PSC = 15999;               // Vorteiler für 1 kHz Taktfrequenz
    TIM6->ARR = 0;
    TIM6->DIER |= TIM_DIER_UIE;

    NVIC_SetPriority(TIM6_DAC_IRQn, 1);
    NVIC_EnableIRQ(TIM6_DAC_IRQn);
    TIM6->CR1 |= TIM_CR1_CEN;

    // Basiert auf der Vorlesungsvorlage (ADC-Initialisierung an PA3)
    GPIOA->MODER |= (3 << (3 * 2)); // PA3 in Analogmodus versetzen

    RCC->AHB2ENR |= RCC_AHB2ENR_ADC12EN;
    ADC12_COMMON->CCR |= (1 << 17);

    ADC1->CR &= ~ADC_CR_DEEPPWD;
    ADC1->CR |= ADC_CR_ADVREGEN;

    systickDelayMs(1);

    ADC1->SQR1 = (4 << 6);          // Kanal 4 (PA3) als erste Wandlung

    ADC1->CR |= ADC_CR_ADEN;
    while (!(ADC1->ISR & ADC_ISR_ADRDY)) {}
}

// Basiert auf der Vorlesungsvorlage (RNG über ADC-Rauschen)
uint32_t get_random_number(void)
{
    uint32_t rand_val = 0;
    for (int i = 0; i < 32; i++)
    {
        ADC1->CR |= ADC_CR_ADSTART;
        while (!(ADC1->ISR & ADC_ISR_EOC)) {}
        uint32_t lsb = ADC1->DR & 0x01;
        rand_val |= (lsb << i);
    }
    return rand_val;
}

void TIM6_DAC_IRQHandler(void)
{
    TIM6->SR &= ~TIM_SR_UIF;

    ms_ticks++;
    if (state_timer > 0) {
        state_timer--;
    }
}

void EXTI0_IRQHandler(void)
{
    EXTI->PR1 = (1 << 0);
    NVIC_ClearPendingIRQ(EXTI0_IRQn);

    if (current_state == STATE_WAIT_INPUT)
    {
        disable_button_interrupts(); // Sofort sperren zum Entprellen

        if (sequence[input_index] == 1) {
            last_pressed_button_correct = 1;
        } else {
            last_pressed_button_correct = 0;
        }

        led_on();
        state_timer = 1000;          // 1.0s Feedback-Blinken
        current_state = STATE_PLAYING_INPUT;
    }
}

void EXTI2_IRQHandler(void)
{
    EXTI->PR1 = (1 << 2);
    NVIC_ClearPendingIRQ(EXTI2_IRQn);

    if (current_state == STATE_WAIT_INPUT)
    {
        disable_button_interrupts(); // Sofort sperren zum Entprellen

        if (sequence[input_index] == 0) {
            last_pressed_button_correct = 1;
        } else {
            last_pressed_button_correct = 0;
        }

        led_on();
        state_timer = 350;           // 0.35s Feedback-Blinken
        current_state = STATE_PLAYING_INPUT;
    }
}


