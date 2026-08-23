# MCT-32: STM32 Morse Code Transceiver

A four-mode Morse code transceiver built on an STM32F103, written in both HAL and bare-metal, register-level C (different branches).

<p align="center">
  <!-- <img width="750" height="500" alt="Board" src="https://github.com/user-attachments/assets/7ec91233-3fce-4ac0-89f7-128f45f3a5ef" /> -->
  <div style="width: 300px; height: 200px; overflow: hidden;">
  <img src="https://github.com/user-attachments/assets/dfbb7c80-f490-468d-ac29-a76053d93a36" 
       alt="Demo" 
       style="transform: scale(1.25) translate(-20px, -30px); transform-origin: top left;" />
</div>
</p>

## What this is
A Morse code transceiver on an STM32F103, written in both implementations, HAL and bare-metal C. It sends Morse code via an interrupt-driven timer (TIM2) that toggles output on every Morse timing unit without blocking the main loop, receives by polling an LDR through the ADC and measuring pulse widths to decode dots and dashes in real time, and reads user input through a polled rotary encoder (TIM3 encoder mode) and two touch-sensor paddles for manual keying.

## System modes

The system is a single state machine (`SystemState_t`), switched between modes on a physical button press.

| Mode | Description |
| --- | --- |
| **IDLE** | standby; tracks system uptime on the OLED. |
| **SELECT** | a rotary encoder scrolls through the alphabet; a button commits characters to a message buffer. On send, TIM2 fires an interrupt on every Morse unit boundary, toggling buzzer/LED without blocking the main loop. |
| **RECEIVE** | an LDR is polled through ADC1; pulse and gap durations are measured against a tunable unit length to decode dots, dashes, letter breaks, and word breaks back into text. |
| **MANUAL** | two touch sensors act as a hand key, software-debounced, with dot/dash/gap timing enforced in a small state machine. |

## Hardware

* **MCU:** STM32F103 (Cortex-M3)
* **Display:** 0.96" SSD1306 OLED, I2C
* **Sensing:** LDR via ADC1
* **Input:** EC11 rotary encoder + button, separate mode-switch button, TTP223 touch sensors ×2
* **Output:** active buzzer, status LEDs

## Register-level subsystems

Everything below is configured directly against peripheral registers, no HAL init calls:

* **GPIO** - `CRL`/`CRH` for pin mode/speed/AF config, `IDR`/`BSRR` for reads/writes
* **SysTick** - custom millisecond tick counter, drives `millis()`/`delay_ms()`
* **I2C1** - manual peripheral reset, `CCR`/`TRISE` timing calculation for 100kHz standard mode
* **TIM2** - basic timer with update interrupt, drives non-blocking Morse transmission timing
* **TIM3** - encoder mode, reads the rotary encoder via `SMCR`/`CCMR1`/`CCER`
* **TIM4** - PWM output for LED brightness in RECEIVE mode
* **ADC1** - software-triggered single-channel conversion for the LDR

## Code layout

* `main.h` - pin mappings, register macros, system-state structs
* `main.c` - the state machine, register-level peripheral setup, ISR logic
* `stm32f1xx_it.c` - interrupt vector table entries, SysTick/TIM2 handlers
* `morse_data.c` - Morse timing tables and character lookup table, read-only flash data

---
<p align="center">
  <a href="https://youtu.be/C65Wo_ZKYWc" target="_blank">
    <img src="https://img.youtube.com/vi/C65Wo_ZKYWc/hqdefault.jpg" alt="Watch Demo" width="1000" />
  </a>
</p>

<p align="center">
  <img width="49%" height="1000" alt="Image" src="https://github.com/user-attachments/assets/c6c80486-1730-40d0-b8fe-f8a1d3e5ed3a" />
  <img width="49%" height="500" alt="Image" src="https://github.com/user-attachments/assets/375366f1-46bb-4212-9504-7f8e5aef7c53" />
</p>

<p align="center">
  <img src="https://github.com/user-attachments/assets/bc285bfc-8fdc-46b1-8c65-d5c4eaa7f954" alt="IDLE" height="300" width="49%">
  <img src="https://github.com/user-attachments/assets/e81b5cd9-4a0e-4e04-9213-df8d084afe3d" alt="SELECT" height="300" width="49%">
</p>

<p align="center">
  <img src="https://github.com/user-attachments/assets/d310d7a6-77c8-4343-9e0b-2aeeae3a32ec" alt="RECEIVE" height="300" width="49%">
  <img src="https://github.com/user-attachments/assets/e3f9c06e-4e49-4d10-b271-ebaf0b7c9de9" alt="MANUAL" height="300" width="49%">
</p>

<p align="center">
  <img src="https://github.com/user-attachments/assets/e2db9e68-d184-465c-a4bc-ac89b4183816" alt="CIRCUIT DIAGRAM" height="1000" width=100%/>
</p>

<!-- <p align="center">
  <img width="49%" height="500" alt="Image" src="https://github.com/user-attachments/assets/cb8b3848-a3a1-43cf-b985-b54f5a98a3e7" />
  <img width="49%" height="500" alt="Image" src="https://github.com/user-attachments/assets/f1d3dd8d-beb3-4772-b625-5b6da7c42562" />
  <img width="49%" height="500" alt="Image" src="https://github.com/user-attachments/assets/778b86bb-ceb0-4345-a081-fe75678c050b" />
</p> -->
