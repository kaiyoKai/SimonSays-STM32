#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <termios.h>
#include <dirent.h>
#include <string.h>
#include "hardware.h"

volatile GameState_t current_state = STATE_INIT;
volatile uint32_t state_timer = 0;
volatile uint32_t ms_ticks = 0;
volatile uint32_t seq_len = 5;
volatile uint8_t sequence[9] = {0};
volatile uint32_t input_index = 0; volatile uint8_t last_pressed_button_correct = 0;
volatile uint32_t current_level = 1;

static volatile int prompt_printed = 0;
static volatile uint32_t playback_index = 0;

static volatile uint32_t debug_mode = 0;
static char active_ascii_file[256] = "";
static volatile uint32_t led_on_tick = 0;
static volatile uint32_t last_led_duration = 0;

static void print_active_ascii_art(void)
{
    if (active_ascii_file[0] == '\0') {
        return;
    }

    char full_path[512];
    snprintf(full_path, sizeof(full_path), "./Desktop/assets/%s", active_ascii_file);

    FILE *f = fopen(full_path, "r");
    if (f)
    {
        char line[256];
        while (fgets(line, sizeof(line), f))
        {
            printf("%s", line);
        }
        fclose(f);
        printf("\n");
    }
    else
    {
        printf("[Fehler: ASCII-Bild '%s' wurde nicht gefunden]\n\n", active_ascii_file);
    }
}

static void handle_ascii_art_picker(void)
{
    DIR *d;
    struct dirent *dir;
    d = opendir("./Desktop/assets");
    char file_list[10][256];
    int file_count = 0;

    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
            if (dir->d_name[0] == '.') continue;
            int len = strlen(dir->d_name);
            if (len > 4 && strcmp(dir->d_name + len - 4, ".txt") == 0)
            {
                snprintf(file_list[file_count], sizeof(file_list[file_count]), "%s", dir->d_name);
                file_count++;
                if (file_count >= 10) break;
            }
        }
        closedir(d);
    }

    printf("\033[H\033[2J");
    printf("==================================================\n");
    printf("  📂 ASCII-ART AUSWAHL\n");
    printf("==================================================\n\n");
    printf("  [0] Deaktivieren\n");
    for (int i = 0; i < file_count; i++)
    {
        printf("  [%d] %s\n", i + 1, file_list[i]);
    }
    printf("\nAuswahl (0-%d): ", file_count);
    fflush(stdout);

    char choice = getchar();
    while (getchar() != '\n');

    if (choice == '0')
    {
        active_ascii_file[0] = '\0';
    }
    else if (choice >= '1' && choice < '1' + file_count)
    {
        int idx = choice - '1';
        snprintf(active_ascii_file, sizeof(active_ascii_file), "%s", file_list[idx]);
    }

    prompt_printed = 0;
}

static void draw_screen_with_led(const char* led_str)
{
    printf("\033[H\033[2J");
    printf("==================================================\n");
    printf("      STM32G431 SPIEL - DESKTOP-EMULATOR\n");
    printf("==================================================\n\n");

    print_active_ascii_art();

    switch (current_state)
    {
        case STATE_INIT:
            printf("Initialisierung... Bitte warten.\n");
            break;
        case STATE_START_LEVEL:
            printf("▶️  Level %d wird gestartet...\n", current_level);
            break;
        case STATE_SHOW_SEQUENCE:
            printf("Sequenz wird abgespielt! \n");
            break;
        case STATE_PLAYING_INPUT:
            printf("👉 Eingabe läuft (Schritt %d von %d)\n", input_index + 1, seq_len);
            break;
        case STATE_DEBOUNCE_RELEASE:
            printf("⌛ Taste bitte loslassen .\n");
            break;
        case STATE_LEVEL_SUCCESS:
            printf("🎉 Richtig! Level erfolgreich bestanden.\n");
            break;
        case STATE_LEVEL_FAILURE:
            printf("Falsche Eingabe. Zurück zu Level 1. \n");
            break;
        default:
            printf("Spiel läuft...\n");
            break;
    }

    printf("\n  LED-Status:\n");
    printf("  =====================\n");
    printf("        %s\n", led_str);
    printf("  =====================\n\n");

    printf("--------------------------------------------------\n");
    printf("🐞 Debug (ausgespochen 'The Bug', weil Bug heißt Käfer auf Englisch): Letzte LED-Einschaltdauer: %u ms\n", last_led_duration);
    if (debug_mode)
    {
        printf("   [DEBUG-MODUS AKTIV]\n");
        printf("   - Soll-Sequenz: ");
        for (uint32_t i = 0; i < seq_len; i++) {
            printf("%d ", sequence[i]);
        }
        printf("\n");
        printf("   - Playback-Index: %d | User-Index: %d\n", playback_index, input_index);
    }
    else
    {
        printf("   ('d' + Enter für Debug-Infos | 'a' + Enter für ASCII-Art)\n");
    }
    printf("--------------------------------------------------\n\n");
    fflush(stdout);
}

void led_on(void)
{
    led_on_tick = ms_ticks;
    draw_screen_with_led("🟢 AN");
}

void led_off(void)
{
    last_led_duration = ms_ticks - led_on_tick;
    draw_screen_with_led("🔴 AUS");
}

void systickDelayMs(int n)
{
    usleep(n * 1000);
}

void disable_button_interrupts(void) {}
void enable_button_interrupts(void) {}
void clear_button_flags(void) {}

uint32_t get_random_number(void)
{
    return rand();
}

void* tim6_simulation_thread(void* arg)
{
    (void)arg;
    GameState_t last_state = STATE_INIT;

    while (1)
    {
        usleep(1000);
        ms_ticks++;
        if (state_timer > 0) {
            state_timer--;
        }

        if (current_state != last_state)
        {
            if (current_state == STATE_SHOW_SEQUENCE)
            {
                printf("\033[H\033[2J");
                printf("📺 Sequenz wird abgespielt...\n\n");
                fflush(stdout);
            }
            else if (current_state == STATE_LEVEL_SUCCESS)
            {
                printf("\n==========================================\n");
                printf("🎉 Richtig! Level bestanden.\n");
                printf("   LED blinkt 2x lang als Bestätigung.\n");
                printf("==========================================\n");
                fflush(stdout);
            }
            else if (current_state == STATE_LEVEL_FAILURE)
            {
                printf("\n==========================================\n");
                printf("💥 Falsche Eingabe.\n");
                printf("   LED blinkt 3x schnell als Fehler-Feedback.\n");
                printf("   Spiel wird auf Level 1 zurückgesetzt.\n");
                printf("==========================================\n");
                fflush(stdout);
            }
            last_state = current_state;
        }
    }
    return NULL;
}

void* input_simulation_thread(void* arg)
{
    (void)arg;
    while (1)
    {
        if (current_state == STATE_WAIT_INPUT)
        {
            if (!prompt_printed)
            {
                tcflush(0, TCIFLUSH);

                printf("\033[H\033[2J");
                printf("==================================================\n");
                printf("      STM32G431 SPIEL - DESKTOP-EMULATOR\n");
                printf("==================================================\n\n");

                print_active_ascii_art();

                printf("Du bist dran! Gib die Sequenz ein (Eingabe: %d/%d)\n", input_index, seq_len);
                printf("  [LED-Status: 🔴 AUS]\n\n");

                printf("--------------------------------------------------\n");
                printf("🐞 Debug: Letzte LED-Einschaltdauer: %u ms\n", last_led_duration);
                if (debug_mode)
                {
                    printf("   [DEBUG-MODUS AKTIV]\n");
                    printf("   - Soll-Sequenz: ");
                    for (uint32_t i = 0; i < seq_len; i++) {
                        printf("%d ", sequence[i]);
                    }
                    printf("\n");
                    printf("   - Playback-Index: %d | User-Index: %d\n", playback_index, input_index);
                }
                else
                {
                    printf("   ('d' + Enter für Debug-Infos | 'a' + Enter für ASCII-Art)\n");
                }
                printf("--------------------------------------------------\n\n");

                printf("Steuerung:\n");
                printf("  's' + Enter für kurz (Pin A2 / Bit 0)\n");
                printf("  'l' + Enter für lang (Pin A0 / Bit 1)\n");
                printf("Eingabe: ");
                fflush(stdout);
                prompt_printed = 1;
            }

            char input_char = getchar();
            // Eingabepuffer leeren (Newline verwerfen)
            while (getchar() != '\n');

            if (current_state == STATE_WAIT_INPUT)
            {
                if (input_char == 'd')
                {
                    debug_mode = !debug_mode;
                    prompt_printed = 0; // Bildschirm neu zeichnen
                    continue;
                }

                if (input_char == 'a')
                {
                    handle_ascii_art_picker();
                    continue;
                }

                if (input_char == 's')
                {
                    disable_button_interrupts();
                    if (sequence[input_index] == 0) {
                        last_pressed_button_correct = 1;
                    } else {
                        last_pressed_button_correct = 0;
                    }

                    led_on();
                    state_timer = 350; // 0,35s blinken
                    current_state = STATE_PLAYING_INPUT;
                }
                else if (input_char == 'l')
                {
                    disable_button_interrupts();
                    if (sequence[input_index] == 1) {
                        last_pressed_button_correct = 1;
                    } else {
                        last_pressed_button_correct = 0;
                    }

                    led_on();
                    state_timer = 1000;
                    current_state = STATE_PLAYING_INPUT;
                }
            }
        }
        else
        {
            prompt_printed = 0;
            usleep(10000);
        }
    }
    return NULL;
}

void init_hardware(void)
{
    pthread_t timer_tid, input_tid;
    pthread_create(&timer_tid, NULL, tim6_simulation_thread, NULL);
    pthread_create(&input_tid, NULL, input_simulation_thread, NULL);
}

