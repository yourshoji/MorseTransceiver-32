/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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
/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */
#define MAX_BUFFER      128
#define TOTAL_PRESETS   3

// define pins
#define DOT_PORT        GPIOB
#define DOT_PIN         GPIO_PIN_5
#define DASH_PORT       GPIOB
#define DASH_PIN        GPIO_PIN_10
#define BUZZER_PORT     GPIOB
#define BUZZER_PIN      GPIO_PIN_11
#define MODE_SW_PORT    GPIOA
#define MODE_SW_PIN     GPIO_PIN_2
#define ENC_SW_PORT     GPIOB
#define ENC_SW_PIN      GPIO_PIN_0
#define LED1_PORT       GPIOA
#define LED1_PIN        GPIO_PIN_5
#define LED2_PORT       (&htim4)
#define LED2_PIN        TIM_CHANNEL_1
#define LED3_PORT       GPIOB
#define LED3_PIN        GPIO_PIN_12

// define durations
#define TIME_DOT        1300  // 130ms
#define TIME_DASH       3900  // 390ms
#define GAP_SYM         1300  // gap between parts of a letter (130ms)
#define GAP_CHAR        3900  // gap between letters (390ms)
#define GAP_WORD        9100  // gap between words (910ms)
/* USER CODE END EM */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */
typedef enum 
{
  MODE_IDLE = 0,
  MODE_SELECT,
  MODE_RECEIVE,
  MODE_MANUAL
} SystemMode_t;

typedef enum 
{
  IDLE,
  DEBOUNCE,
  SENDING,
  GAP
} SubState_t;

typedef struct
{
  // mode selection
  SystemMode_t current_mode;
  GPIO_PinState prev_mode_sw_state;
} SystemState_t;

typedef struct
{
  // rotary encoder name-input variables
  char      buffer[MAX_BUFFER];           // reserve and clear space
  uint16_t  index;
  uint32_t  letter_idx;                   // current position in alphabets (0-26)
  char      current_char;                 // current alphabet selected
  // control flags
  bool      ready_to_send;
  bool      confirm_send;
  bool      ready_to_reset;
  // morse transmission state
  // pointers to the active pattern and its length (must be set by the lookup function)
  volatile const uint16_t *pattern_ptr;   // points to the PATTERN w/o the need to reassign it
  volatile size_t pattern_length;
  volatile bool     is_running;
  volatile size_t   msg_ptr;              // tracks the current character being processed
  volatile uint16_t step;
  
} TransmitState_t;

typedef struct
{
  // hardware readings & settings
  uint32_t  ldr_val;              // tracks ldr value
  uint32_t  threshold_idx;        // current position in thresholds (letter_idx * 150)
  uint8_t   unit_duration;        // current timing unit
  // morse logic
  char      temp_pattern[8];      // stores dots/dashes for the current letter
  uint16_t  pattern_idx;          // current position in temp_pattern
  uint32_t  pulse_start;          // timestamp: light on
  uint32_t  gap_start;            // timestamp: light off
  bool      is_light_on;          // logic flag for LDR state
  // results
  char      buffer[MAX_BUFFER];   // buffer for storing found letters
  uint16_t  index;                  // current position in receive_buffer
  char      found_char;           // alphabet found
  // tuner & reset logic
  uint16_t  preset_idx;           // current position in unit_presets
  uint32_t  press_start_time;
  bool      button_was_pressed;
} ReceiveState_t;

typedef struct
{
  char character;
  const uint16_t *pattern_data;
  size_t length; // count the total morse code used in each alphabet by calculating in bytes, size_t is used for holding the result of sizeof()
} MorseMapping_t;

extern SystemState_t sys;
extern ReceiveState_t rx;
extern TransmitState_t tx;
/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */
extern const char* rx_morse_map[];
extern const MorseMapping_t morse_lookup_table[];
extern const size_t morse_lookup_length;
extern const uint16_t unit_presets[TOTAL_PRESETS];
/* USER CODE END EC */


/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

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
