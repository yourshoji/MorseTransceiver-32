# MCT-32: STM32 Morse Code Transceiver

A HAL-based embedded C project (No RTOS) that converts an STM32 microcontroller into a multi-mode Morse code transceiver.

<p align="center">
  <img src="https://github.com/user-attachments/assets/bfe344a3-8355-466d-ac3b-50dcfe67205c" alt="BOARD" height="500" width="750">
</p>

## Hardware Stack
* **MCU:** STM32F1 Series (STM32F103/STM32F1xx)
* **Display:** 0.96" SSD1306 OLED (I2C)
* **Inputs:**
    * LDR (Light Dependent Resistor) via ADC for Morse reception.
    * Rotary Encoder & Encoder Button (EC11) for UI navigation and threshold tuning.
    * Push-Button for mode switching.
    * Touch Sensors (TTP223) as dual paddles for sending dots and dashes.
* **Outputs:** Active Buzzer (SFM-20B), status LEDs.

## System Modes
1.  **MODE_IDLE:** Standby state and system uptime tracking.
2.  **MODE_SELECT:** Rotary encoder-driven text entry. Select characters A-Z and a gap (' ') to transmit them automatically via LED/Buzzer.
3.  **MODE_RECEIVE:** Uses the LDR to read incoming light flashes, dynamically tracking pulse/gap widths to decode standard Morse back into ASCII text. Features tunable unit durations (130ms, 150ms, 200ms) and ADC threshold calibration.
4.  **MODE_MANUAL:** Direct hardware keyer mode using the physical Dot/Dash paddles with software debouncing and precise gap timing enforcement.

## Software Architecture
* `main.h`: Hardware mappings, macros, and strict typedefs for system states (`SystemState_t`, `TransmitState_t`, `ReceiveState_t`).
* `main.c`: Core state machine, hardware polling, ADC conversion routines, and `HAL_TIM_PeriodElapsedCallback` for non-blocking Morse transmission.
* `morse_data.c`: Isolated, read-only flash memory data layer containing timing arrays and the master lookup table.

<p align="center">
  <img src="https://github.com/user-attachments/assets/bc285bfc-8fdc-46b1-8c65-d5c4eaa7f954" alt="IDLE" height="300" width="49%">
  <img src="https://github.com/user-attachments/assets/e81b5cd9-4a0e-4e04-9213-df8d084afe3d" alt="SELECT" height="300" width="49%">

</p>

<p align="center">
  <img src="https://github.com/user-attachments/assets/d310d7a6-77c8-4343-9e0b-2aeeae3a32ec" alt="RECEIVE" height="300" width="49%">
  <img src="https://github.com/user-attachments/assets/e3f9c06e-4e49-4d10-b271-ebaf0b7c9de9" alt="MANUAL" height="300" width="49%">
</p>

### [ ! ] System Showcase, Prototype, and PCB Design are coming soon.
