/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
#include "adc.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum {
  MODE_SELECT = 0,
  MODE_RECEIVE,
  MODE_MANUAL
} SystemMode_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim4;
TIM_HandleTypeDef htim2;

// mode selection
volatile SystemMode_t current_mode = MODE_SELECT;
GPIO_PinState prev_mode_sw_state = false;

// rotary encoder name-input variables
volatile char name_buffer[10] = {0}; // reserve and clear space
volatile int name_index = 0;
volatile bool confirm_send_flag = false;
volatile bool ready_to_send_flag = false;
volatile bool ready_to_reset_flag = false;

// morse transmission state
volatile size_t msg_index = 0; // Tracks the current character being processed
volatile bool morse_running = false;
volatile int step_counter = 0;



// for debugging
// volatile uint32_t ldr_val;
// volatile uint32_t threshold_index;

// telling the compiler that this variable actually exist in another source file (.c)
extern const uint16_t pattern_space[];

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

// prototype for lookup helper (defined later in file)
void handle_ldr_receive(uint32_t threshold);
void handle_letter_selection(char input_char);
void status_feedback_handler(uint32_t timer);
void handle_morse_input(uint32_t timer, char letter);
void reset_after_commit();
void morse_commit();
bool lookup_and_load_pattern(char character);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// Define durations
// const uint16_t time_dot = 1300;  // 130ms
// const uint16_t time_dash = 3900; // 390ms
// const uint16_t gap_sym = 1300;   // Gap between parts of a letter (130ms)
// const uint16_t gap_char = 3900;  // Gap between letters (390ms)
// const uint16_t gap_word = 9100;  // Gap between words (910ms)

#define time_dot 1300 // 130ms
#define time_dash 3900  // 390ms
#define gap_sym 1300  // Gap between parts of a letter (130ms)
#define gap_char 3900 // Gap between letters (390ms)
#define gap_word 9100 // Gap between words (910ms)

typedef struct
{
  char character;
  const uint16_t *pattern_data;
  size_t length; // count the total morse code used in each alphabet by calculating in bytes, size_t is used for holding the result of sizeof()
} MorseMapping_t;

// [PATTERN DATA ARRAYS]
// Format: { Sound, Silence, Sound, Silence }
// MORSE PATTERNS A–Z
const uint16_t pattern_a[] = {time_dot, gap_sym, time_dash, gap_char};                           // .-
const uint16_t pattern_b[] = {time_dash, gap_sym, time_dot, gap_sym, time_dot, gap_sym, time_dot, gap_char};  // -...
const uint16_t pattern_c[] = {time_dash, gap_sym, time_dot, gap_sym, time_dash, gap_sym, time_dot, gap_char}; // -.-.
const uint16_t pattern_d[] = {time_dash, gap_sym, time_dot, gap_sym, time_dot, gap_char};        // -..
const uint16_t pattern_e[] = {time_dot, gap_char};                                               // .
const uint16_t pattern_f[] = {time_dot, gap_sym, time_dot, gap_sym, time_dash, gap_sym, time_dot, gap_char};   // ..-.
const uint16_t pattern_g[] = {time_dash, gap_sym, time_dash, gap_sym, time_dot, gap_char};       // --.
const uint16_t pattern_h[] = {time_dot, gap_sym, time_dot, gap_sym, time_dot, gap_sym, time_dot, gap_char};    // ....
const uint16_t pattern_i[] = {time_dot, gap_sym, time_dot, gap_char};                            // ..
const uint16_t pattern_j[] = {time_dot, gap_sym, time_dash, gap_sym, time_dash, gap_sym, time_dash, gap_char}; // .---
const uint16_t pattern_k[] = {time_dash, gap_sym, time_dot, gap_sym, time_dash, gap_char};       // -.-
const uint16_t pattern_l[] = {time_dot, gap_sym, time_dash, gap_sym, time_dot, gap_sym, time_dot, gap_char};   // .-..
const uint16_t pattern_m[] = {time_dash, gap_sym, time_dash, gap_char};                          // --
const uint16_t pattern_n[] = {time_dash, gap_sym, time_dot, gap_char};                           // -.
const uint16_t pattern_o[] = {time_dash, gap_sym, time_dash, gap_sym, time_dash, gap_char};      // ---
const uint16_t pattern_p[] = {time_dot, gap_sym, time_dash, gap_sym, time_dash, gap_sym, time_dot, gap_char};  // .--.
const uint16_t pattern_q[] = {time_dash, gap_sym, time_dash, gap_sym, time_dot, gap_sym, time_dash, gap_char}; // --.-
const uint16_t pattern_r[] = {time_dot, gap_sym, time_dash, gap_sym, time_dot, gap_char};        // .-.
const uint16_t pattern_s[] = {time_dot, gap_sym, time_dot, gap_sym, time_dot, gap_char};         // ...
const uint16_t pattern_t[] = {time_dash, gap_char};                                              // -
const uint16_t pattern_u[] = {time_dot, gap_sym, time_dot, gap_sym, time_dash, gap_char};        // ..-
const uint16_t pattern_v[] = {time_dot, gap_sym, time_dot, gap_sym, time_dot, gap_sym, time_dash, gap_char};   // ...-
const uint16_t pattern_w[] = {time_dot, gap_sym, time_dash, gap_sym, time_dash, gap_char};       // .--
const uint16_t pattern_x[] = {time_dash, gap_sym, time_dot, gap_sym, time_dot, gap_sym, time_dash, gap_char};  // -..-
const uint16_t pattern_y[] = {time_dash, gap_sym, time_dot, gap_sym, time_dash, gap_sym, time_dash, gap_char}; // -.--
const uint16_t pattern_z[] = {time_dash, gap_sym, time_dash, gap_sym, time_dot, gap_sym, time_dot, gap_char};  // --..

// SPACE
const uint16_t pattern_space[] = {gap_word};

// LOOKUP TABLE
const MorseMapping_t morse_lookup_table[] = {
    {'A', pattern_a, sizeof(pattern_a)/sizeof(uint16_t)},
    {'B', pattern_b, sizeof(pattern_b)/sizeof(uint16_t)},
    {'C', pattern_c, sizeof(pattern_c)/sizeof(uint16_t)},
    {'D', pattern_d, sizeof(pattern_d)/sizeof(uint16_t)},
    {'E', pattern_e, sizeof(pattern_e)/sizeof(uint16_t)},
    {'F', pattern_f, sizeof(pattern_f)/sizeof(uint16_t)},
    {'G', pattern_g, sizeof(pattern_g)/sizeof(uint16_t)},
    {'H', pattern_h, sizeof(pattern_h)/sizeof(uint16_t)},
    {'I', pattern_i, sizeof(pattern_i)/sizeof(uint16_t)},
    {'J', pattern_j, sizeof(pattern_j)/sizeof(uint16_t)},
    {'K', pattern_k, sizeof(pattern_k)/sizeof(uint16_t)},
    {'L', pattern_l, sizeof(pattern_l)/sizeof(uint16_t)},
    {'M', pattern_m, sizeof(pattern_m)/sizeof(uint16_t)},
    {'N', pattern_n, sizeof(pattern_n)/sizeof(uint16_t)},
    {'O', pattern_o, sizeof(pattern_o)/sizeof(uint16_t)},
    {'P', pattern_p, sizeof(pattern_p)/sizeof(uint16_t)},
    {'Q', pattern_q, sizeof(pattern_q)/sizeof(uint16_t)},
    {'R', pattern_r, sizeof(pattern_r)/sizeof(uint16_t)},
    {'S', pattern_s, sizeof(pattern_s)/sizeof(uint16_t)},
    {'T', pattern_t, sizeof(pattern_t)/sizeof(uint16_t)},
    {'U', pattern_u, sizeof(pattern_u)/sizeof(uint16_t)},
    {'V', pattern_v, sizeof(pattern_v)/sizeof(uint16_t)},
    {'W', pattern_w, sizeof(pattern_w)/sizeof(uint16_t)},
    {'X', pattern_x, sizeof(pattern_x)/sizeof(uint16_t)},
    {'Y', pattern_y, sizeof(pattern_y)/sizeof(uint16_t)},
    {'Z', pattern_z, sizeof(pattern_z)/sizeof(uint16_t)},

    {' ', pattern_space, sizeof(pattern_space)/sizeof(uint16_t)}
};

// Total size of the lookup table
const size_t morse_lookup_length = sizeof(morse_lookup_table) / sizeof(morse_lookup_table[0]);

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  MX_ADC1_Init();
  /* USER CODE BEGIN 2 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 7199; // 72 MHz / 7199+1 == 10kHz (0.1 ms)
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP; // counter mode activated
  htim2.Init.Period = 1299; // 1300 (1299+1) * 0.1 ms == 130 ms
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  HAL_TIM_Base_Init(&htim2);
  HAL_TIM_Base_Start_IT(&htim2); // timer interrupt

  /* rotary encoder peripherals (encoder on TIM3, PWM on TIM4) */
  HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);
  HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);

  // adc calibration
  HAL_ADCEx_Calibration_Start(&hadc1);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    // ---- rotary encoder + switch handling ----
    uint32_t raw_cnt = __HAL_TIM_GET_COUNTER(&htim3);

    // encoder usage: letter scroller
    uint32_t letter_index = (raw_cnt / 4) % 26; // wrap it up as a safety
    uint32_t pwm_val = (letter_index * 1000) / 25;
    char current_letter = 'A' + letter_index;
    volatile GPIO_PinState mode_sw_state = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_2);

    // mode changer
    if (mode_sw_state == GPIO_PIN_RESET && prev_mode_sw_state == GPIO_PIN_SET)
    {
      current_mode = (current_mode + 1) % 3;
      HAL_Delay(50);
    }
    prev_mode_sw_state = mode_sw_state;

    switch (current_mode) {
      case MODE_SELECT:
      {

        // ASCII debugging via LED (yellow)
        __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, pwm_val); // we should be able to set this in numeric when the UI is done
        handle_letter_selection(current_letter);
        break;
      }
      case MODE_RECEIVE:
      {
        // encoder usage: LDR threshold
        uint32_t threshold_index = letter_index * 150;
        // brightness: level of threshold
        __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, pwm_val);
        handle_ldr_receive(threshold_index);
        break;
      }
      
      case MODE_MANUAL:
      {
        __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, 500);
        // handle_manual_tap;
        break;
      }
    }

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

// Pointers to the active pattern and its length (must be set by the lookup function)
volatile const uint16_t *current_pattern_ptr = NULL; // Points to the PATTERN w/o the need to reassign it
volatile size_t current_pattern_length = 0;

// telling the compiler that this variable actually exist in another source file (.c)
extern const uint16_t pattern_space[];

void handle_ldr_receive(uint32_t threshold)
{
  HAL_ADC_Start(&hadc1);
  
  if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
    uint32_t ldr_val = HAL_ADC_GetValue(&hadc1);

    if (ldr_val > threshold) 
    {
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, GPIO_PIN_SET);
    }
    else 
    {
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, GPIO_PIN_RESET);
    }
  }
  
  HAL_ADC_Stop(&hadc1);
}

void handle_letter_selection(char input_char)
{
  // ENC_SW: add / delete / send sequence
  if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0) == GPIO_PIN_RESET) 
    {
      uint32_t press_start = HAL_GetTick();

      while (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0) == GPIO_PIN_RESET)
      {
        uint32_t elapsed = HAL_GetTick() - press_start;
        status_feedback_handler(elapsed);
      }
      uint32_t duration = HAL_GetTick() - press_start;
      handle_morse_input(duration, input_char);
      
      HAL_Delay(50);
    }

    // RESET*
    reset_after_commit();
    // ACTION*
    morse_commit();

    HAL_Delay(10);
}

void status_feedback_handler(uint32_t timer)
{
  if (timer < 500)
  {
    // add feedback
      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
      HAL_Delay(50);
      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
  }
  if (timer >= 500 && timer < 1500) 
  {
    // delete feedback
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
    HAL_Delay(100);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
    HAL_Delay(100);
  } 
  else if (timer >= 1500 && timer < 3000) 
  {
    // send feedback
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
    HAL_Delay(1500);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
  }
}

void handle_morse_input(uint32_t timer, char letter)
{
  // ADD & CONFIRM SEND
  if (timer < 500) 
    {
      if (ready_to_send_flag) 
      {
        confirm_send_flag = true;
        ready_to_reset_flag = true;
      } 
      else if (name_index < (int)(sizeof(name_buffer) - 1)) 
      {
        name_buffer[name_index++] = letter;
        name_buffer[name_index] = '\0';
      }
    } 
    // DELETE
    else if (timer < 1500) 
    {
      if (name_index > 0) 
      {
        name_index--;
        name_buffer[name_index] = '\0';
      }
    } 
    // CODE SEND
    else 
    {
      ready_to_send_flag = true;
    }
}

void reset_after_commit()
{
  if (ready_to_send_flag && ready_to_reset_flag && !confirm_send_flag) 
  {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
    HAL_Delay(100);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);

    ready_to_send_flag = false;
    ready_to_reset_flag = false;
    name_index = 0;
    name_buffer[0] = '\0';
  }
}

void morse_commit()
{
  if (confirm_send_flag && !morse_running && name_buffer[0] != '\0') 
  {
    msg_index = 0;
    if (lookup_and_load_pattern(name_buffer[msg_index])) 
    {
      morse_running = true;
      step_counter = 0; // make sure we start from beginning
    }
    /* consume send flag so it doesn't retrigger repeatedly */
    confirm_send_flag = false;
  }
}

bool lookup_and_load_pattern(char character)
{
  // Convert character to uppercase to match the lookup table
  char lookup_char = toupper((unsigned char)character);

  // Loop through the morse lookup table array
  for (size_t i = 0; i < morse_lookup_length; i++)
  {
    if (morse_lookup_table[i].character == lookup_char)
    {

      // Pointer to the matching PATTERN DATA (eg., pattern_a)
      current_pattern_ptr = morse_lookup_table[i].pattern_data; // Current Pattern
      current_pattern_length = morse_lookup_table[i].length;    // Current Pattern Length

      return true;
    }
  }
  // Character not supported
  return false;
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  // If we use pointer *htim to access the chosen htim's Instance and it's not TIM2, stop the function.
  if (htim->Instance != TIM2)
    return;

  // OUTPUT LOGIC, runs every tick if the flag is set
  if (morse_running == true)
  {
    // Set the next ARR (Counter) to reach only the required MORSE CODE unit (accurately)
    __HAL_TIM_SET_AUTORELOAD(&htim2, current_pattern_ptr[step_counter] - 1);

    // MORSE CODE is an alternating sequence of sound and silence (sound, silence, sound)
    if (step_counter % 2 == 0) // Checks if the current position is either EVEN or ODD
    {
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, GPIO_PIN_SET); // If EVEN
    }
    else
    {
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, GPIO_PIN_RESET); // If ODD
    }
    // Moving on to the next piece of MORSE CODE
    step_counter++;

    // Checks for character end, if it is ended, forward to the next
    if (step_counter >= current_pattern_length)
    {
      msg_index++; // Advance to the next character

      // If the character is "null" (none), terminate it.
      if (name_buffer[msg_index] == '\0')
      {
        morse_running = false;
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, GPIO_PIN_RESET);
        msg_index = 0; // set back to default
        return;
      }

      // Loads next character
      if (lookup_and_load_pattern(name_buffer[msg_index]))
      {
        step_counter = 0;
      }
      else
      {
        current_pattern_ptr = pattern_space;
        current_pattern_length = 1;
        step_counter = 0;
      }
    }

    // Detects if the entire MORSE CODE sequence is finished, sent.
  }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
