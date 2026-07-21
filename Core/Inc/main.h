/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.h
 * @brief          : Header for main.c file.
 * This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdbool.h>
#include <stdint.h>

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/// @brief Defines the four main operating modes of the system.
typedef enum {
  MODE_IDLE = 0,
  MODE_SELECT,
  MODE_RECEIVE,
  MODE_MANUAL
} SystemMode_t;

/// @brief Defines the steps for sending a single Morse code pulse.
typedef enum { IDLE, DEBOUNCE, SENDING, GAP } SubState_t;

/// @brief Tracks the current system mode and physical switch state.
typedef struct {
  SystemMode_t current_mode;
  GPIO_PinState prev_mode_sw_state;
} SystemState_t;

/// @brief Holds all variables needed for the text sending process.
typedef struct {
  char buffer[MAX_BUFFER];
  uint16_t index;
  uint32_t letter_idx;
  char current_char;

  bool ready_to_send;
  bool confirm_send;
  bool ready_to_reset;

  volatile const uint16_t *pattern_ptr;
  volatile size_t pattern_length;
  volatile bool is_running;
  volatile size_t msg_ptr;
  volatile uint16_t step;
} TransmitState_t;

/// @brief Holds all variables needed to receive and read light pulses.
typedef struct {
  uint32_t ldr_val;
  uint32_t threshold_idx;
  uint8_t unit_duration;

  char temp_pattern[8];
  uint16_t pattern_idx;
  uint32_t pulse_start;
  uint32_t gap_start;
  bool is_light_on;

  char buffer[MAX_BUFFER];
  uint16_t index;
  char found_char;

  uint16_t preset_idx;
  uint32_t press_start_time;
  bool button_was_pressed;
} ReceiveState_t;

/// @brief Links an alphabet letter to its Morse code timing pattern.
typedef struct {
  char character;
  const uint16_t *pattern_data;
  size_t length;
} MorseMapping_t;

extern SystemState_t sys;
extern ReceiveState_t rx;
extern TransmitState_t tx;

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */
extern const char *rx_morse_map[];
extern const MorseMapping_t morse_lookup_table[];
extern const size_t morse_lookup_length;
extern const uint8_t unit_presets[TOTAL_PRESETS];
/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/// @brief System limits and configuration sizes.
#define MAX_BUFFER 128
#define TOTAL_PRESETS 3

/* NOTE: Hardware pin definitions are grouped here for easy board mapping. */
#define DOT_PORT GPIOB
#define DOT_PIN_NUM 5U
#define DOT_PIN (1U << DOT_PIN_NUM)
#define DASH_PORT GPIOB
#define DASH_PIN_NUM 10U
#define DASH_PIN (1U << DASH_PIN_NUM)
#define BUZZER_PORT GPIOB
#define BUZZER_PIN_NUM 11U
#define BUZZER_PIN (1U << BUZZER_PIN_NUM)
#define MODE_SW_PORT GPIOA
#define MODE_SW_PIN_NUM 2U
#define MODE_SW_PIN (1U << MODE_SW_PIN_NUM)
#define ENC_SW_PORT GPIOB
#define ENC_SW_PIN_NUM 0U
#define ENC_SW_PIN (1U << ENC_SW_PIN_NUM)
#define LED1_PORT GPIOA
#define LED1_PIN_NUM 5U
#define LED1_PIN (1U << LED1_PIN_NUM)
#define LED2_TIM (&htim4)
#define LED2_CHANNEL TIM_CHANNEL_1
#define LED3_PORT GPIOB
#define LED3_PIN_NUM 12U
#define LED3_PIN (1U << LED3_PIN_NUM)

/// @brief Morse code timing constants measured in timer ticks (1 tick = 0.1
/// ms).
#define TIME_DOT 1300
#define TIME_DASH 3900
#define GAP_SYM 1300
#define GAP_CHAR 3900
#define GAP_WORD 9100

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
void TIM2_PeriodElapsedCallback(void);
uint32_t millis(void);
void delay_ms(uint32_t ms);
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LDR_Pin GPIO_PIN_1
#define LDR_GPIO_Port GPIOA
#define MODE_SW_Pin GPIO_PIN_2
#define MODE_SW_GPIO_Port GPIOA
#define MORSE_CAL_Pin GPIO_PIN_3
#define MORSE_CAL_GPIO_Port GPIOA
#define LED_Pin GPIO_PIN_5
#define LED_GPIO_Port GPIOA
#define ENC_A_Pin GPIO_PIN_6
#define ENC_A_GPIO_Port GPIOA
#define ENC_B_Pin GPIO_PIN_7
#define ENC_B_GPIO_Port GPIOA
#define ENC_SW_Pin GPIO_PIN_0
#define ENC_SW_GPIO_Port GPIOB
#define DASH_SW_Pin GPIO_PIN_10
#define DASH_SW_GPIO_Port GPIOB
#define BUZZER_Pin GPIO_PIN_11
#define BUZZER_GPIO_Port GPIOB
#define LEDB12_Pin GPIO_PIN_12
#define LEDB12_GPIO_Port GPIOB
#define DOT_SW_Pin GPIO_PIN_5
#define DOT_SW_GPIO_Port GPIOB
#define LEDB6_Pin GPIO_PIN_6
#define LEDB6_GPIO_Port GPIOB
#define OLED_SCL_Pin GPIO_PIN_8
#define OLED_SCL_GPIO_Port GPIOB
#define OLED_SDA_Pin GPIO_PIN_9
#define OLED_SDA_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
