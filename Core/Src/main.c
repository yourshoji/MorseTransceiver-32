/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : MCT-32 Morse Code Transceiver — Main program start and mode
 * loop.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include "tim.h"
#include <adc.h>
#include <gpio.h>

/* USER CODE BEGIN Includes */
#include <ctype.h>
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* USER CODE BEGIN PV */
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim4;
TIM_HandleTypeDef htim2;

/// @brief Holds dot lengths in milliseconds that can change while the program
/// runs.
const uint8_t unit_presets[TOTAL_PRESETS] = {130, 150, 200};

/// @brief Tracks the current system mode and the mode switch state.
SystemState_t sys = {.current_mode = MODE_IDLE,
                     .prev_mode_sw_state = GPIO_PIN_SET};

/// @brief Stores the text to send, encoder positions, and sending status.
TransmitState_t tx = {.buffer = {0},
                      .index = 0,
                      .letter_idx = 0,
                      .current_char = 'A',
                      .ready_to_send = false,
                      .confirm_send = false,
                      .ready_to_reset = false,
                      .pattern_ptr = NULL,
                      .pattern_length = 0,
                      .is_running = false,
                      .msg_ptr = 0,
                      .step = 0};

/// @brief Stores ADC values, pulse timings, and the decoded Morse text.
ReceiveState_t rx = {.ldr_val = 0,
                     .threshold_idx = 0,
                     .unit_duration = 130,
                     .temp_pattern = {0},
                     .pattern_idx = 0,
                     .pulse_start = 0,
                     .gap_start = 0,
                     .is_light_on = false,
                     .buffer = {0},
                     .index = 0,
                     .found_char = '\0',
                     .preset_idx = 0,
                     .press_start_time = 0,
                     .button_was_pressed = false};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

/* USER CODE BEGIN PFP */
void update_buffer_ui(uint8_t idx, char *buffer);
void idle_ui(void);
void paddle_feedback_manual_ui(void);
void unit_duration_receive_ui(uint8_t unit);
void idx_roll_receive_ui(uint8_t idx);
void letter_roll_select_ui(char letter);
void play_intro_ui(void);
void refresh_n_setup_ui(SystemMode_t mode, char *buffer);
void handle_manual_mode(void);
bool handle_transmit(uint16_t pulse_duration);
void handle_ldr_receive(uint32_t threshold, uint32_t current_pwm_level);
void reset_and_tune_handler(void);
void reset_receive_buffer(void);
void unit_duration_tuner(void);
void handle_letter_selection(char input_char);
void status_feedback_handler(uint32_t timer);
void handle_morse_input(uint32_t timer, char letter);
void reset_after_commit(void);
void morse_commit(void);
bool lookup_and_load_pattern(char character);
/* USER CODE END PFP */

/// @brief Main program loop.
/// @retval int Standard C return code.
int main(void) {
  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  MX_ADC1_Init();
  MX_I2C1_Init();

  /* USER CODE BEGIN 2 */
  ssd1306_Init();
  play_intro_ui();

  /* NOTE: We set up TIM2 here instead of using the auto-generated function.
   * The timer speed depends on user settings (unit_presets).
   * This setup gives a 10 kHz interrupt rate at 72 MHz. */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 7199;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 1299;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  HAL_TIM_Base_Init(&htim2);
  HAL_TIM_Base_Start_IT(&htim2);

  HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);
  HAL_TIM_PWM_Start(LED2_PORT, LED2_PIN);

  HAL_ADCEx_Calibration_Start(&hadc1);
  /* USER CODE END 2 */

  while (1) {
    /* Turns the raw encoder count into a 0-26 alphabet number.
     * Dividing by 4 handles the encoder's physical clicks. */
    uint32_t raw_cnt = __HAL_TIM_GET_COUNTER(&htim3);
    tx.letter_idx = (raw_cnt / 4) % 27;

    uint32_t pwm_val = (tx.letter_idx * 1000) / 25;

    tx.current_char = (tx.letter_idx < 26) ? ('A' + tx.letter_idx) : ' ';

    GPIO_PinState mode_sw_state = HAL_GPIO_ReadPin(MODE_SW_PORT, MODE_SW_PIN);

    /* Changes the system mode when the switch is pressed.
     * WARNING: Turn off outputs (buzzer, LED) before changing modes.
     * Otherwise, they might stay on in the new mode. */
    if (mode_sw_state == GPIO_PIN_RESET &&
        sys.prev_mode_sw_state == GPIO_PIN_SET) {

      HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
      HAL_GPIO_WritePin(LED3_PORT, LED3_PIN, GPIO_PIN_RESET);
      tx.is_running = false;
      sys.current_mode = (sys.current_mode + 1) % 4;
      refresh_n_setup_ui(sys.current_mode, (char *)tx.buffer);

      HAL_Delay(50);
    }

    sys.prev_mode_sw_state = mode_sw_state;

    switch (sys.current_mode) {
    case MODE_IDLE:
      idle_ui();
      break;

    case MODE_SELECT:
      handle_letter_selection(tx.current_char);
      update_buffer_ui(tx.index, tx.buffer);
      letter_roll_select_ui(tx.current_char);
      break;

    case MODE_RECEIVE:
      /* Turns the encoder number into a sensor threshold and LED brightness. */
      rx.threshold_idx = tx.letter_idx * 150;
      handle_ldr_receive(rx.threshold_idx, pwm_val);
      update_buffer_ui(rx.index, rx.buffer);
      idx_roll_receive_ui(rx.threshold_idx);
      unit_duration_receive_ui(rx.unit_duration);
      break;

    case MODE_MANUAL:
      handle_manual_mode();
      paddle_feedback_manual_ui();
      break;

    default:
      sys.current_mode = MODE_IDLE;
      idle_ui();
      break;
    }
  }
}

/* NOTE: Auto-generated ST clock code is kept hidden here. Use CubeMX to view
 * it. */
void SystemClock_Config(void) { /* ... (ST generated code unchanged) ... */ }

/* USER CODE BEGIN 4 */

/// @brief Updates the OLED screen only when the text length changes.
/// @param idx Current number of characters in the text.
/// @param buffer The text string to show.
void update_buffer_ui(uint8_t idx, char *buffer) {
  static int prev_idx = -1;

  if (idx == prev_idx) {
    return;
  }

  /* Clears both text rows before writing new text. */
  ssd1306_SetCursor(2, 38);
  ssd1306_WriteString("                                ", Font_7x10, White);
  ssd1306_SetCursor(2, 50);
  ssd1306_WriteString("                                ", Font_7x10, White);

  char line1[20] = {0};
  char line2[20] = {0};

  strncpy(line1, buffer, 18);
  ssd1306_SetCursor(2, 38);
  ssd1306_WriteString(line1, Font_7x10, White);

  /* Moves text to the second row if it is longer than 18 characters. */
  if (strlen(buffer) > 18) {
    strncpy(line2, buffer + 18, 18);
    ssd1306_SetCursor(2, 50);
    ssd1306_WriteString(line2, Font_7x10, White);
  }

  ssd1306_UpdateScreen();

  prev_idx = idx;
}

/// @brief Shows how many minutes the system has been running.
void idle_ui(void) {
  static uint32_t prev_min = 0;
  uint32_t current_min = HAL_GetTick() / 60000;

  if (current_min <= prev_min) {
    return;
  }

  char time_msg[20];
  snprintf(time_msg, sizeof(time_msg), "Uptime: %lu min(s)", current_min);

  ssd1306_SetCursor(2, 50);
  ssd1306_WriteString("                    ", Font_7x10, White);
  ssd1306_SetCursor(2, 50);
  ssd1306_WriteString(time_msg, Font_7x10, White);

  ssd1306_UpdateScreen();

  prev_min = current_min;
}

/// @brief Draws filled or empty boxes to show if paddles are pressed.
void paddle_feedback_manual_ui(void) {
  if (HAL_GPIO_ReadPin(DOT_PORT, DOT_PIN) == GPIO_PIN_SET) {
    ssd1306_FillRectangle(30, 30, 40, 40, White);
  } else {
    ssd1306_FillRectangle(30, 30, 40, 40, Black);
    ssd1306_DrawRectangle(30, 30, 40, 40, White);
  }

  if (HAL_GPIO_ReadPin(DASH_PORT, DASH_PIN) == GPIO_PIN_SET) {
    ssd1306_FillRectangle(87, 30, 97, 40, White);
  } else {
    ssd1306_FillRectangle(87, 30, 97, 40, Black);
    ssd1306_DrawRectangle(87, 30, 97, 40, White);
  }

  ssd1306_UpdateScreen();
}

/// @brief Shows the current dot speed on the screen.
/// @param unit Dot length in milliseconds.
void unit_duration_receive_ui(uint8_t unit) {
  static int prev_unit = -1;

  if (unit == prev_unit) {
    return;
  }

  char str[20] = {0};
  snprintf(str, sizeof(str), "%dms", unit);

  ssd1306_SetCursor(30, 20);
  ssd1306_WriteString("     ", Font_7x10, White);
  ssd1306_SetCursor(30, 20);
  ssd1306_WriteString(str, Font_7x10, White);

  ssd1306_UpdateScreen();

  prev_unit = unit;
}

/// @brief Shows the current light sensor limit on the screen.
/// @param idx Sensor limit based on the encoder knob.
void idx_roll_receive_ui(uint8_t idx) {
  static int prev_idx = -1;

  if (idx == prev_idx) {
    return;
  }

  char str[5] = {0};
  snprintf(str, sizeof(str), "%d", idx);

  ssd1306_SetCursor(70, 20);
  ssd1306_WriteString("   ", Font_7x10, White);
  ssd1306_SetCursor(70, 20);
  ssd1306_WriteString(str, Font_7x10, White);

  ssd1306_UpdateScreen();

  prev_idx = idx;
}

/// @brief Shows the letter currently picked by the user.
/// @param letter The letter chosen by the encoder knob.
void letter_roll_select_ui(char letter) {
  static char prev_letter = '\0';

  if (letter == prev_letter) {
    return;
  }

  char str[2] = {letter, '\0'};
  ssd1306_SetCursor(105, 20);
  ssd1306_WriteString(" ", Font_11x18, White);
  ssd1306_SetCursor(105, 20);
  ssd1306_WriteString(str, Font_11x18, White);

  ssd1306_UpdateScreen();

  prev_letter = letter;
}

/// @brief Shows the startup screen and a loading bar. Blocks code while
/// running.
void play_intro_ui(void) {
  ssd1306_Fill(Black);

  ssd1306_SetCursor(30, 10);
  ssd1306_WriteString("MCT-32", Font_11x18, White);
  ssd1306_SetCursor(85, 30);
  ssd1306_WriteString("v1.0.0", Font_6x8, White);

  /* Draws a loading bar in 8-pixel steps. */
  for (uint8_t i = 0; i < 128; i += 8) {
    ssd1306_Line(0, 45, i, 45, White);
    ssd1306_UpdateScreen();
    HAL_Delay(50);
  }

  ssd1306_SetCursor(20, 50);
  ssd1306_WriteString("SYSTEM READY", Font_7x10, White);
  ssd1306_UpdateScreen();

  HAL_Delay(1500);

  ssd1306_SetCursor(15, 50);
  ssd1306_WriteString("              ", Font_7x10, White);
  ssd1306_SetCursor(15, 50);
  ssd1306_WriteString("PRESS TO START", Font_7x10, White);
  ssd1306_UpdateScreen();
}

/// @brief Draws the basic screen layout for the current mode.
/// @param mode The current system mode.
/// @param buffer Text buffer used in modes that show words.
void refresh_n_setup_ui(SystemMode_t mode, char *buffer) {
  ssd1306_Fill(Black);
  ssd1306_Line(0, 12, 127, 12, White);
  ssd1306_SetCursor(2, 0);

  if (mode == MODE_IDLE) {
    ssd1306_WriteString("MODE: IDLE", Font_7x10, White);
    ssd1306_SetCursor(2, 20);
    ssd1306_WriteString("Slow down,", Font_7x10, White);
    ssd1306_SetCursor(2, 30);
    ssd1306_WriteString("let's take a break", Font_7x10, White);
  } else if (mode == MODE_SELECT) {
    ssd1306_WriteString("MODE: SELECT", Font_7x10, White);
    ssd1306_SetCursor(2, 20);
    ssd1306_WriteString("Text:", Font_7x10, White);
  } else if (mode == MODE_RECEIVE) {
    ssd1306_WriteString("MODE: RECEIVE", Font_7x10, White);
  } else if (mode == MODE_MANUAL) {
    ssd1306_WriteString("MODE: MANUAL", Font_7x10, White);
    ssd1306_SetCursor(25, 20);
    ssd1306_WriteString("DOT", Font_7x10, White);
    ssd1306_SetCursor(80, 20);
    ssd1306_WriteString("DASH", Font_7x10, White);
  }

  ssd1306_UpdateScreen();
}

/// @brief Checks the manual paddles and starts sending dots or dashes.
void handle_manual_mode(void) {
  bool isBusy = handle_transmit(0);

  if (!isBusy) {
    if (HAL_GPIO_ReadPin(DOT_PORT, DOT_PIN) == GPIO_PIN_SET) {
      handle_transmit(130);
    } else if (HAL_GPIO_ReadPin(DASH_PORT, DASH_PIN) == GPIO_PIN_SET) {
      handle_transmit(390);
    }
  }
}

/// @brief Sends Morse code pulses without freezing the program.
/// @param pulse_duration Length of the pulse in milliseconds. 0 means just
/// check status.
/// @retval true The sender is busy.
/// @retval false The sender is ready for a new pulse.
bool handle_transmit(uint16_t pulse_duration) {
  static SubState_t current_state = IDLE;
  static uint32_t start_time = 0;
  static uint16_t current_duration = 0;

  uint32_t now = HAL_GetTick();

  switch (current_state) {
  case IDLE:
    if (pulse_duration > 0) {
      HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
      HAL_GPIO_WritePin(LED3_PORT, LED3_PIN, GPIO_PIN_RESET);
      current_duration = pulse_duration;
      start_time = now;
      current_state = DEBOUNCE;
    }
    return false;

  case DEBOUNCE:
    /* NOTE: We wait 10 ms to ignore noisy signals from the touch sensor.
     * Change this if using a different physical button. */
    if (now - start_time >= 10) {
      HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);
      HAL_GPIO_WritePin(LED3_PORT, LED3_PIN, GPIO_PIN_SET);
      start_time = now;
      current_state = SENDING;
    }
    return true;

  case SENDING:
    if (now - start_time >= current_duration) {
      HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
      HAL_GPIO_WritePin(LED3_PORT, LED3_PIN, GPIO_PIN_RESET);
      start_time = now;
      current_state = GAP;
    }
    return true;

  case GAP:
    /* Forces a standard space between dots and dashes (130 ms). */
    if (now - start_time >= 130) {
      current_state = IDLE;
    }
    return true;

  default:
    current_state = IDLE;
    return false;
  }
}

/// @brief Reads the light sensor and changes light flashes into text.
///
/// NOTE: Dots are shorter than 2 units. Dashes are longer.
/// New letters start after 3 units of silence. New words after 7.
///
/// @param threshold The sensor level that means the light is on.
/// @param current_pwm_level Normal LED brightness when quiet.
void handle_ldr_receive(uint32_t threshold, uint32_t current_pwm_level) {
  reset_and_tune_handler();

  HAL_ADC_Start(&hadc1);
  if (HAL_ADC_PollForConversion(&hadc1, 10) != HAL_OK) {
    HAL_ADC_Stop(&hadc1);
    return;
  }

  rx.ldr_val = HAL_ADC_GetValue(&hadc1);
  uint32_t now = HAL_GetTick();

  if (rx.ldr_val > threshold) {
    if (!rx.is_light_on) {
      rx.pulse_start = now;
      rx.is_light_on = true;
    }

    __HAL_TIM_SET_COMPARE(LED2_PORT, LED2_PIN, 1000);
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED3_PORT, LED3_PIN, GPIO_PIN_SET);

    rx.gap_start = now;

  } else {
    if (rx.is_light_on) {
      uint32_t duration = now - rx.pulse_start;

      if ((duration > 30) && (duration < (rx.unit_duration * 2))) {
        rx.temp_pattern[rx.pattern_idx++] = '.';
      } else if (duration >= (rx.unit_duration * 2)) {
        rx.temp_pattern[rx.pattern_idx++] = '-';
      }

      rx.temp_pattern[rx.pattern_idx] = '\0';
      rx.is_light_on = false;
      rx.gap_start = now;
    }

    __HAL_TIM_SET_COMPARE(LED2_PORT, LED2_PIN, current_pwm_level);
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED3_PORT, LED3_PIN, GPIO_PIN_RESET);

    /* Saves the letter if we wait long enough after a pulse. */
    if (rx.pattern_idx > 0 && (now - rx.gap_start > (rx.unit_duration * 3))) {
      for (int i = 0; i < 26; i++) {
        if (strcmp(rx.temp_pattern, rx_morse_map[i]) == 0) {
          rx.found_char = 'A' + i;

          if (rx.index < (MAX_BUFFER - 1)) {
            rx.buffer[rx.index++] = rx.found_char;
            rx.buffer[rx.index] = '\0';
          }

          break;
        }
      }

      rx.pattern_idx = 0;
      rx.temp_pattern[0] = '\0';
    }

    /* Adds a space if we wait even longer. */
    if (now - rx.gap_start > (rx.unit_duration * 7)) {
      if (rx.index > 0 && rx.buffer[rx.index - 1] != ' ' &&
          rx.index < (MAX_BUFFER - 1)) {
        rx.buffer[rx.index++] = ' ';
        rx.buffer[rx.index] = '\0';
      }
    }
  }

  HAL_ADC_Stop(&hadc1);
}

/// @brief Checks how long the button is held to clear text or change speed.
void reset_and_tune_handler(void) {
  bool is_pressed =
      (HAL_GPIO_ReadPin(ENC_SW_PORT, ENC_SW_PIN) == GPIO_PIN_RESET);

  if (is_pressed && !rx.button_was_pressed) {
    rx.press_start_time = HAL_GetTick();
    rx.button_was_pressed = true;

  } else if (!is_pressed && rx.button_was_pressed) {
    uint32_t duration = HAL_GetTick() - rx.press_start_time;

    if (duration >= 1000 && duration < 3000) {
      HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_SET);
      HAL_Delay(100);
      HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_RESET);
      reset_receive_buffer();
    } else if (duration > 50) {
      unit_duration_tuner();
    }

    rx.button_was_pressed = false;
    rx.press_start_time = 0;
  }
}

/// @brief Empties the received text.
void reset_receive_buffer(void) {
  rx.index = 0;
  rx.buffer[0] = '\0';
}

/// @brief Changes the dot speed to the next option, looping back to the start.
void unit_duration_tuner(void) {
  rx.preset_idx = (rx.preset_idx + 1) % TOTAL_PRESETS;
  rx.unit_duration = unit_presets[rx.preset_idx];
}

/// @brief Checks button presses to add, delete, or send text.
/// @param input_char The letter shown on the screen.
void handle_letter_selection(char input_char) {
  static uint32_t press_start = 0;
  bool is_pressed =
      (HAL_GPIO_ReadPin(ENC_SW_PORT, ENC_SW_PIN) == GPIO_PIN_RESET);

  if (is_pressed) {
    if (press_start == 0) {
      press_start = HAL_GetTick();
    }

    status_feedback_handler(HAL_GetTick() - press_start);

  } else if (press_start != 0) {
    uint32_t duration = HAL_GetTick() - press_start;
    handle_morse_input(duration, input_char);

    press_start = 0;

    morse_commit();
    reset_after_commit();
  }

  HAL_Delay(10);
}

/// @brief Blinks a light to show how long the button has been held.
/// @param timer Time held in milliseconds.
void status_feedback_handler(uint32_t timer) {
  if (timer < 500) {
    HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_SET);
    HAL_Delay(50);
    HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_RESET);

  } else if (timer < 1500) {
    HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_SET);
    HAL_Delay(100);
    HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_RESET);
    HAL_Delay(100);

  } else if (timer < 3000) {
    HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_SET);
    HAL_Delay(1500);
    HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_RESET);
  }
}

/// @brief Uses the button press time to add, delete, or mark text to send.
/// @param timer Total time the button was held.
/// @param letter The letter to add if it was a quick press.
void handle_morse_input(uint32_t timer, char letter) {
  if (timer < 500) {
    if (tx.ready_to_send) {
      tx.confirm_send = true;
      tx.ready_to_reset = true;
    } else if (tx.index < (int)(sizeof(tx.buffer) - 1)) {
      tx.buffer[tx.index++] = letter;
      tx.buffer[tx.index] = '\0';
    }
  } else if (timer < 1500) {
    if (tx.index > 0) {
      tx.index--;
      tx.buffer[tx.index] = '\0';
    }
  } else {
    tx.ready_to_send = true;
  }
}

/// @brief Clears the text if the user readies a message but cancels before
/// sending.
void reset_after_commit(void) {
  if (tx.ready_to_send && tx.ready_to_reset && !tx.confirm_send) {
    HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_SET);
    HAL_Delay(100);
    HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_RESET);

    tx.ready_to_send = false;
    tx.ready_to_reset = false;
    tx.index = 0;
    tx.buffer[0] = '\0';
  }
}

/// @brief Starts sending the message if the user confirmed it.
void morse_commit(void) {
  if (tx.confirm_send && !tx.is_running && tx.buffer[0] != '\0') {
    tx.msg_ptr = 0;

    if (lookup_and_load_pattern(tx.buffer[tx.msg_ptr])) {
      tx.is_running = true;
      tx.step = 0;
    }

    tx.confirm_send = false;
  }
}

/// @brief Finds a letter in the Morse dictionary and gets its pattern.
/// @param character The letter to look up.
/// @retval true Found the letter.
/// @retval false Letter is not in the dictionary.
bool lookup_and_load_pattern(char character) {
  char lookup_char = toupper((unsigned char)character);

  for (size_t i = 0; i < morse_lookup_length; i++) {
    if (morse_lookup_table[i].character == lookup_char) {
      tx.pattern_ptr = morse_lookup_table[i].pattern_data;
      tx.pattern_length = morse_lookup_table[i].length;
      return true;
    }
  }

  return false;
}

/// @brief Moves the Morse sending sequence forward on every timer tick.
///
/// WARNING: The pattern pointer must not be empty if the system is running.
/// The program will crash if it is.
///
/// @param htim The timer causing the interrupt.
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
  if (htim->Instance != TIM2) {
    return;
  }
  if (sys.current_mode != MODE_SELECT) {
    return;
  }
  if (!tx.is_running) {
    return;
  }

  __HAL_TIM_SET_AUTORELOAD(&htim2, tx.pattern_ptr[tx.step] - 1);

  if (tx.buffer[tx.msg_ptr] == ' ') {
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED3_PORT, LED3_PIN, GPIO_PIN_RESET);
  } else {
    if (tx.step % 2 == 0) {
      HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);
      HAL_GPIO_WritePin(LED3_PORT, LED3_PIN, GPIO_PIN_SET);
    } else {
      HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
      HAL_GPIO_WritePin(LED3_PORT, LED3_PIN, GPIO_PIN_RESET);
    }
  }

  tx.step++;

  if (tx.step >= tx.pattern_length) {
    tx.msg_ptr++;

    /* Stops the sender when it reaches the end of the text. */
    if (tx.buffer[tx.msg_ptr] == '\0') {
      tx.is_running = false;
      HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
      HAL_GPIO_WritePin(LED3_PORT, LED3_PIN, GPIO_PIN_RESET);
      tx.msg_ptr = 0;
      return;
    }

    /* Gets the next character, or uses a space if it is missing. */
    if (lookup_and_load_pattern(tx.buffer[tx.msg_ptr])) {
      tx.step = 0;
    } else {
      lookup_and_load_pattern(' ');
      tx.step = 0;
    }
  }
}

/// @brief Freezes the program if a major error occurs.
void Error_Handler(void) {
  __disable_irq();
  while (1) {
  }
}
/* USER CODE END 4 */
