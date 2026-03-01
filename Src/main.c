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

// rotary encoder name-input variables
volatile char name_buffer[10] = {0}; // reserve and clear space
volatile int name_index = 0;
volatile bool confirm_send_flag = false;
volatile bool ready_to_send_flag = false;
volatile bool ready_to_reset_flag = false;

// morse transmission state
volatile size_t MSG_INDEX = 0; // Tracks the current character being processed
bool PREV_BUTTON_STATE = true;
volatile bool MORSE_RUNNING = false;
volatile int STEP_COUNTER = 0;

// prototype for lookup helper (defined later in file)
void handle_letter_selection(char input_char);
void status_feedback_handler(uint32_t timer);
void handle_morse_input(uint32_t timer, char letter);
void reset_after_commit();
void morse_commit();
bool lookup_and_load_pattern(char character);


// telling the compiler that this variable actually exist in another source file (.c)
extern const uint16_t PATTERN_SPACE[];

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// Define durations
// const uint16_t TIME_DOT = 1300;  // 130ms
// const uint16_t TIME_DASH = 3900; // 390ms
// const uint16_t GAP_SYM = 1300;   // Gap between parts of a letter (130ms)
// const uint16_t GAP_CHAR = 3900;  // Gap between letters (390ms)
// const uint16_t GAP_WORD = 9100;  // Gap between words (910ms)

#define TIME_DOT 1300 // 130ms
#define TIME_DASH 3900  // 390ms
#define GAP_SYM 1300  // Gap between parts of a letter (130ms)
#define GAP_CHAR 3900 // Gap between letters (390ms)
#define GAP_WORD 9100 // Gap between words (910ms)

typedef struct
{
  char character;
  const uint16_t *pattern_data;
  size_t length; // count the total morse code used in each alphabet by calculating in bytes, size_t is used for holding the result of sizeof()
} MorseMapping_t;

// [PATTERN DATA ARRAYS]
// Format: { Sound, Silence, Sound, Silence }
// MORSE PATTERNS A–Z
const uint16_t PATTERN_A[] = {TIME_DOT, GAP_SYM, TIME_DASH, GAP_CHAR};                           // .-
const uint16_t PATTERN_B[] = {TIME_DASH, GAP_SYM, TIME_DOT, GAP_SYM, TIME_DOT, GAP_SYM, TIME_DOT, GAP_CHAR};  // -...
const uint16_t PATTERN_C[] = {TIME_DASH, GAP_SYM, TIME_DOT, GAP_SYM, TIME_DASH, GAP_SYM, TIME_DOT, GAP_CHAR}; // -.-.
const uint16_t PATTERN_D[] = {TIME_DASH, GAP_SYM, TIME_DOT, GAP_SYM, TIME_DOT, GAP_CHAR};        // -..
const uint16_t PATTERN_E[] = {TIME_DOT, GAP_CHAR};                                               // .
const uint16_t PATTERN_F[] = {TIME_DOT, GAP_SYM, TIME_DOT, GAP_SYM, TIME_DASH, GAP_SYM, TIME_DOT, GAP_CHAR};   // ..-.
const uint16_t PATTERN_G[] = {TIME_DASH, GAP_SYM, TIME_DASH, GAP_SYM, TIME_DOT, GAP_CHAR};       // --.
const uint16_t PATTERN_H[] = {TIME_DOT, GAP_SYM, TIME_DOT, GAP_SYM, TIME_DOT, GAP_SYM, TIME_DOT, GAP_CHAR};    // ....
const uint16_t PATTERN_I[] = {TIME_DOT, GAP_SYM, TIME_DOT, GAP_CHAR};                            // ..
const uint16_t PATTERN_J[] = {TIME_DOT, GAP_SYM, TIME_DASH, GAP_SYM, TIME_DASH, GAP_SYM, TIME_DASH, GAP_CHAR}; // .---
const uint16_t PATTERN_K[] = {TIME_DASH, GAP_SYM, TIME_DOT, GAP_SYM, TIME_DASH, GAP_CHAR};       // -.-
const uint16_t PATTERN_L[] = {TIME_DOT, GAP_SYM, TIME_DASH, GAP_SYM, TIME_DOT, GAP_SYM, TIME_DOT, GAP_CHAR};   // .-..
const uint16_t PATTERN_M[] = {TIME_DASH, GAP_SYM, TIME_DASH, GAP_CHAR};                          // --
const uint16_t PATTERN_N[] = {TIME_DASH, GAP_SYM, TIME_DOT, GAP_CHAR};                           // -.
const uint16_t PATTERN_O[] = {TIME_DASH, GAP_SYM, TIME_DASH, GAP_SYM, TIME_DASH, GAP_CHAR};      // ---
const uint16_t PATTERN_P[] = {TIME_DOT, GAP_SYM, TIME_DASH, GAP_SYM, TIME_DASH, GAP_SYM, TIME_DOT, GAP_CHAR};  // .--.
const uint16_t PATTERN_Q[] = {TIME_DASH, GAP_SYM, TIME_DASH, GAP_SYM, TIME_DOT, GAP_SYM, TIME_DASH, GAP_CHAR}; // --.-
const uint16_t PATTERN_R[] = {TIME_DOT, GAP_SYM, TIME_DASH, GAP_SYM, TIME_DOT, GAP_CHAR};        // .-.
const uint16_t PATTERN_S[] = {TIME_DOT, GAP_SYM, TIME_DOT, GAP_SYM, TIME_DOT, GAP_CHAR};         // ...
const uint16_t PATTERN_T[] = {TIME_DASH, GAP_CHAR};                                              // -
const uint16_t PATTERN_U[] = {TIME_DOT, GAP_SYM, TIME_DOT, GAP_SYM, TIME_DASH, GAP_CHAR};        // ..-
const uint16_t PATTERN_V[] = {TIME_DOT, GAP_SYM, TIME_DOT, GAP_SYM, TIME_DOT, GAP_SYM, TIME_DASH, GAP_CHAR};   // ...-
const uint16_t PATTERN_W[] = {TIME_DOT, GAP_SYM, TIME_DASH, GAP_SYM, TIME_DASH, GAP_CHAR};       // .--
const uint16_t PATTERN_X[] = {TIME_DASH, GAP_SYM, TIME_DOT, GAP_SYM, TIME_DOT, GAP_SYM, TIME_DASH, GAP_CHAR};  // -..-
const uint16_t PATTERN_Y[] = {TIME_DASH, GAP_SYM, TIME_DOT, GAP_SYM, TIME_DASH, GAP_SYM, TIME_DASH, GAP_CHAR}; // -.--
const uint16_t PATTERN_Z[] = {TIME_DASH, GAP_SYM, TIME_DASH, GAP_SYM, TIME_DOT, GAP_SYM, TIME_DOT, GAP_CHAR};  // --..

// SPACE
const uint16_t PATTERN_SPACE[] = {GAP_WORD};

// LOOKUP TABLE
const MorseMapping_t MORSE_LOOKUP_TABLE[] = {
    {'A', PATTERN_A, sizeof(PATTERN_A)/sizeof(uint16_t)},
    {'B', PATTERN_B, sizeof(PATTERN_B)/sizeof(uint16_t)},
    {'C', PATTERN_C, sizeof(PATTERN_C)/sizeof(uint16_t)},
    {'D', PATTERN_D, sizeof(PATTERN_D)/sizeof(uint16_t)},
    {'E', PATTERN_E, sizeof(PATTERN_E)/sizeof(uint16_t)},
    {'F', PATTERN_F, sizeof(PATTERN_F)/sizeof(uint16_t)},
    {'G', PATTERN_G, sizeof(PATTERN_G)/sizeof(uint16_t)},
    {'H', PATTERN_H, sizeof(PATTERN_H)/sizeof(uint16_t)},
    {'I', PATTERN_I, sizeof(PATTERN_I)/sizeof(uint16_t)},
    {'J', PATTERN_J, sizeof(PATTERN_J)/sizeof(uint16_t)},
    {'K', PATTERN_K, sizeof(PATTERN_K)/sizeof(uint16_t)},
    {'L', PATTERN_L, sizeof(PATTERN_L)/sizeof(uint16_t)},
    {'M', PATTERN_M, sizeof(PATTERN_M)/sizeof(uint16_t)},
    {'N', PATTERN_N, sizeof(PATTERN_N)/sizeof(uint16_t)},
    {'O', PATTERN_O, sizeof(PATTERN_O)/sizeof(uint16_t)},
    {'P', PATTERN_P, sizeof(PATTERN_P)/sizeof(uint16_t)},
    {'Q', PATTERN_Q, sizeof(PATTERN_Q)/sizeof(uint16_t)},
    {'R', PATTERN_R, sizeof(PATTERN_R)/sizeof(uint16_t)},
    {'S', PATTERN_S, sizeof(PATTERN_S)/sizeof(uint16_t)},
    {'T', PATTERN_T, sizeof(PATTERN_T)/sizeof(uint16_t)},
    {'U', PATTERN_U, sizeof(PATTERN_U)/sizeof(uint16_t)},
    {'V', PATTERN_V, sizeof(PATTERN_V)/sizeof(uint16_t)},
    {'W', PATTERN_W, sizeof(PATTERN_W)/sizeof(uint16_t)},
    {'X', PATTERN_X, sizeof(PATTERN_X)/sizeof(uint16_t)},
    {'Y', PATTERN_Y, sizeof(PATTERN_Y)/sizeof(uint16_t)},
    {'Z', PATTERN_Z, sizeof(PATTERN_Z)/sizeof(uint16_t)},

    {' ', PATTERN_SPACE, sizeof(PATTERN_SPACE)/sizeof(uint16_t)}
};

// Total size of the lookup table
const size_t MORSE_LOOKUP_LENGTH = sizeof(MORSE_LOOKUP_TABLE) / sizeof(MORSE_LOOKUP_TABLE[0]);

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


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    // ---- rotary encoder + switch handling ----
    uint32_t raw_cnt = __HAL_TIM_GET_COUNTER(&htim3);
    uint32_t letter_index = raw_cnt / 4;
    uint32_t pwm_val = (letter_index * 1000) / 25;
    char current_letter = 'A' + letter_index;
    
    // Debugging via LED (yellow)
    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, pwm_val);

    // reserves for MODE_SELECT *DO NOT FORGET U 4 EYES*
    handle_letter_selection(current_letter);

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
}

/* USER CODE BEGIN 4 */

// Pointers to the active pattern and its length (must be set by the lookup function)
volatile const uint16_t *CURRENT_PATTERN_PTR = NULL; // Points to the PATTERN w/o the need to reassign it
volatile size_t CURRENT_PATTERN_LENGTH = 0;

// telling the compiler that this variable actually exist in another source file (.c)
extern const uint16_t PATTERN_SPACE[];

void handle_letter_selection(char input_char){
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
  if (confirm_send_flag && !MORSE_RUNNING && name_buffer[0] != '\0') 
  {
    MSG_INDEX = 0;
    if (lookup_and_load_pattern(name_buffer[MSG_INDEX])) 
    {
      MORSE_RUNNING = true;
      STEP_COUNTER = 0; // make sure we start from beginning
    }
    /* consume send flag so it doesn't retrigger repeatedly */
    confirm_send_flag = false;
  }
}

bool lookup_and_load_pattern(char character)
{
  // Convert character to uppercase to match the lookup table
  char lookup_char = toupper((unsigned char)character);

  // Loop through the MORSE LOOKUP TABLE array
  for (size_t i = 0; i < MORSE_LOOKUP_LENGTH; i++)
  {
    if (MORSE_LOOKUP_TABLE[i].character == lookup_char)
    {

      // Pointer to the matching PATTERN DATA (eg., PATTERN_A)
      CURRENT_PATTERN_PTR = MORSE_LOOKUP_TABLE[i].pattern_data; // Current Pattern
      CURRENT_PATTERN_LENGTH = MORSE_LOOKUP_TABLE[i].length;    // Current Pattern Length

      return true;
    }
  }
  // Character not supported
  return false;
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  // Read button
  GPIO_PinState BUTTON_STATE = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_2);

  // If we use pointer *htim to access the chosen htim's Instance and it's not TIM2, stop the function.
  if (htim->Instance != TIM2)
    return;

  // STEP_COUNTER is now a global variable; it must be reset on start.
  // (previously static local to retain value between calls)


  // EDGE DETECTION, checks if the sequence should start
  if (BUTTON_STATE == GPIO_PIN_RESET && PREV_BUTTON_STATE == GPIO_PIN_SET)
  {
    if (MORSE_RUNNING == false) // Activating MORSE and STEP
    {
      MSG_INDEX = 0;

      // use the dynamically entered name_buffer instead of fixed MSG
      if (lookup_and_load_pattern(name_buffer[MSG_INDEX])) 
      {
        MORSE_RUNNING = true;
        STEP_COUNTER = 0;
      }
    }
  }

  // OUTPUT LOGIC, runs every tick if the flag is set
  if (MORSE_RUNNING == true)
  {
    // Set the next ARR (Counter) to reach only the required MORSE CODE unit (accurately)
    __HAL_TIM_SET_AUTORELOAD(&htim2, CURRENT_PATTERN_PTR[STEP_COUNTER] - 1);

    // MORSE CODE is an alternating sequence of sound and silence (sound, silence, sound)
    if (STEP_COUNTER % 2 == 0) // Checks if the current position is either EVEN or ODD
    {
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, GPIO_PIN_SET); // If EVEN
    }
    else
    {
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, GPIO_PIN_RESET); // If ODD
    }
    // Moving on to the next piece of MORSE CODE
    STEP_COUNTER++;

    // Checks for character end, if it is ended, forward to the next
    if (STEP_COUNTER >= CURRENT_PATTERN_LENGTH)
    {
      MSG_INDEX++; // Advance to the next character

      // If the character is "null" (none), terminate it.
      if (name_buffer[MSG_INDEX] == '\0')
      {
        MORSE_RUNNING = false;
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, GPIO_PIN_RESET);
        MSG_INDEX = 0; // set back to default
        return;
      }

      // Loads next character
      if (lookup_and_load_pattern(name_buffer[MSG_INDEX]))
      {
        STEP_COUNTER = 0;
      }
      else
      {
        CURRENT_PATTERN_PTR = PATTERN_SPACE;
        CURRENT_PATTERN_LENGTH = 1;
        STEP_COUNTER = 0;
      }
    }

    // Detects if the entire MORSE CODE sequence is finished, sent.
  }

  PREV_BUTTON_STATE = BUTTON_STATE;
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
