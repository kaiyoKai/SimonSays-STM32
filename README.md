# SimonSays-STM32

A bare-metal C implementation of the memory game *Simon Says* for the **STM32G431** microcontroller (ARM Cortex-M4), written for the *Technische Informatik* laboratory at **Hochschule Heilbronn**.

It also comes with a simple terminal fallback to play the game on Linux without the microcontroller.

(It sounded good in theory but after testing it out, it's way to difficult for me to actually play the Terminal version without cheating by usingthe debug mode,
but that might just be a skill issue lol).

---

## Overview

The codebase is split into the game logic (`main.c`) and a hardware abstraction header (`hardware.h`). Depending on the build target, the same state machine either drives real microcontroller pins or runs as a CLI application in the terminal:

* **STM32 Target:** Bare-metal firmware that manipulates memory-mapped registers directly (`stm32g431xx.h`) without using heavy vendor HAL libraries.
* **Desktop Target:** A native Linux binary compiled with `gcc`. It uses `termios` for raw keyboard input (so buttons react on keydown without pressing Enter) and `pthreads` to simulate hardware clock ticks.

---

## Technical Details

* **Direct Register Access:** LEDs are controlled via bit-shifts on `GPIOB->ODR`.
* **Hardware Timing:** Millisecond delays are generated directly via the ARM Cortex-M4 `SysTick` timer (`SysTick->LOAD = 16000` at 16 MHz).
* **Deterministic FSM:** 8-state finite state machine (`STATE_INIT` through sequence playback, input capture, debouncing, to win/loss states).
* **Desktop Simulation:** Pre-compiled as `./test_game` with ASCII art feedback for each state.

---

## Running the Terminal Simulator (Linux)

No microcontroller needed. You can run the existing binary or recompile it:

```bash
git clone https://github.com/kaiyoKai/SimonSays-STM32.git
cd SimonSays-STM32

# Run pre-compiled binary:
./test_game
```

If you want to build it from source just use this
```bash
make desktop
./test_game


```

Or do this if you are feeling a little adventurous
```bash
gcc -Wall -Wextra -pthread -DDESKTOP_SIMULATION \
-ICore/Inc Core/Src/main.c \
Desktop/desktop_hardware.c -o test_game



```



---

## Flashing the Hardware (STM32G431)

If you have an STM32G431 board and an ST-Link:

```bash
# Builds the ELF and flashes via OpenOCD:
make flash
```

---

## Project Structure

* `Core/Src/main.c` – Game engine and state machine logic.
* `Core/Src/hardware.c` – Bare-metal register implementation for STM32.
* `Core/Inc/hardware.h` – Platform switch (`#ifdef DESKTOP_SIMULATION`).
* `Desktop/desktop_hardware.c` – Linux simulator using `termios` and `pthreads`.
* `Desktop/assets/` – ASCII art assets for terminal playback.
* `Documentation.pdf` – Original lab documentation (circuit and hardware schematics).

---