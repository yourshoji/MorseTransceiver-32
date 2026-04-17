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
#include "i2c.h"
#include "tim.h"
#include "gpio.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"

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

const uint16_t unit_presets[TOTAL_PRESETS] = {130, 150, 200};

SystemState_t sys =
{
  .current_mode = MODE_IDLE,
  .prev_mode_sw_state = GPIO_PIN_SET
};

TransmitState_t tx = 
{
  .buffer          = {0},
  .index           = 0,
  .letter_idx      = 0, 
  .current_char    = 'A',

  .ready_to_send   = false,
  .confirm_send    = false,
  .ready_to_reset  = false,

  .pattern_ptr     = NULL,
  .pattern_length  = 0,
  .is_running      = false,
  .msg_ptr         = 0,
  .step            = 0
};

ReceiveState_t rx = 
{ 
  .ldr_val            = 0,
  .threshold_idx      = 0,
  .unit_duration      = 130, // default

  .temp_pattern       = {0},
  .pattern_idx        = 0,
  .pulse_start        = 0,
  .gap_start          = 0,
  .is_light_on        = false,

  .buffer             = {0},
  .index              = 0,
  .found_char         = '\0',

  .preset_idx         = 0,
  .press_start_time   = 0,
  .button_was_pressed = false
};

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

// prototype for lookup helper (defined later in file)
void update_buffer_ui(int idx, char* buffer);
void idle_ui();
void paddle_feedback_manual_ui();
void unit_duration_receive_ui(int unit);
void idx_roll_receive_ui(int idx);
void letter_roll_select_ui(char letter);
void play_intro_ui();
void refresh_n_setup_ui(SystemMode_t mode, char* buffer);
void handle_manual_mode();
bool handle_transmit(int pulse_duration);
void handle_ldr_receive(uint32_t threshold, uint32_t current_pwm_level);
void reset_and_tune_handler();
void reset_receive_buffer();
void unit_duration_tuner();
void handle_letter_selection(char input_char);
void status_feedback_handler(uint32_t timer);
void handle_morse_input(uint32_t timer, char letter);
void reset_after_commit();
void morse_commit();
bool lookup_and_load_pattern(char character);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */
  ssd1306_Init();
  // play intro
  play_intro_ui();

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
  HAL_TIM_PWM_Start(LED2_PORT, LED2_PIN);

  // adc calibration
  HAL_ADCEx_Calibration_Start(&hadc1);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    // rotary encoder + switch handling
    uint32_t raw_cnt = __HAL_TIM_GET_COUNTER(&htim3);

    // encoder usage: letter scroller
    tx.letter_idx = (raw_cnt / 4) % 27; // wrap it up as a safety
    uint32_t pwm_val = (tx.letter_idx * 1000) / 25;
    // conditional for blank space
    if (tx.letter_idx < 26) 
    {
      tx.current_char = 'A' + tx.letter_idx; 
    }
    else 
    {
      tx.current_char = ' ';
    }

    volatile GPIO_PinState mode_sw_state = HAL_GPIO_ReadPin(MODE_SW_PORT, MODE_SW_PIN);

    // mode changer
    if (mode_sw_state == GPIO_PIN_RESET && sys.prev_mode_sw_state == GPIO_PIN_SET)
    {
        // clean up before switching
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LED3_PORT, LED3_PIN, GPIO_PIN_RESET);
        tx.is_running = false; // kill any active interrupt during transmission
        
        sys.current_mode = (sys.current_mode + 1) % 4;

        refresh_n_setup_ui(sys.current_mode, (char*)tx.buffer);
        
        HAL_Delay(50);
    }
    sys.prev_mode_sw_state = mode_sw_state;

    switch (sys.current_mode) {
      case MODE_IDLE:
      {
        idle_ui();
        break;
      }
      case MODE_SELECT:
      {
        // ASCII debugging via LED (yellow)
        handle_letter_selection(tx.current_char);
        update_buffer_ui(tx.index, tx.buffer);
        letter_roll_select_ui(tx.current_char);
        break;
      }
      case MODE_RECEIVE:
      {
        // encoder usage: LDR threshold
        rx.threshold_idx = tx.letter_idx * 150;
        handle_ldr_receive(rx.threshold_idx, pwm_val);
        update_buffer_ui(rx.index, rx.buffer);
        idx_roll_receive_ui(rx.threshold_idx);
        unit_duration_receive_ui(rx.unit_duration);
        break;
      }
      case MODE_MANUAL:
      {
        handle_manual_mode();
        paddle_feedback_manual_ui();
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

// // telling the compiler that this variable actually exist in another source file (.c)
// extern const uint16_t pattern_space[];

void update_buffer_ui(int idx, char* buffer)
{
  static int prev_idx = -1;

  if (idx != prev_idx)
  {
    // clear the old letters
  	ssd1306_SetCursor(2, 38);
  	ssd1306_WriteString("                                ", Font_7x10, White);
  	ssd1306_SetCursor(2, 50);
  	ssd1306_WriteString("                                ", Font_7x10, White);

    char line1[20] = {0};
    char line2[20] = {0};
    
    strncpy(line1, buffer, 18);
    ssd1306_SetCursor(2, 38);
    ssd1306_WriteString(line1, Font_7x10, White);

    if (strlen(buffer) > 18)
    {
      strncpy(line2, buffer + 18, 18);
      ssd1306_SetCursor(2, 50);
      ssd1306_WriteString(line2, Font_7x10, White);
    }

    ssd1306_UpdateScreen();

    // update the tracker
    prev_idx = idx;
  }
}

void idle_ui()
{ 
  static uint32_t prev_min = 0;
  uint32_t current_min = HAL_GetTick() / 60000;

  // shows current uptime in minutes
  if (current_min > prev_min)
  {
    char time_msg[20];
    snprintf(time_msg, sizeof(time_msg), "Uptime: %lu min(s)", current_min); // %lu = unsigned long

    ssd1306_SetCursor(2, 50);
    ssd1306_WriteString("                    ", Font_7x10, White); // clear
    ssd1306_SetCursor(2, 50);
    ssd1306_WriteString(time_msg, Font_7x10, White);
  
    ssd1306_UpdateScreen();

    prev_min = current_min;
  }

}

void paddle_feedback_manual_ui()
{
  // DOT
  if (HAL_GPIO_ReadPin(DOT_PORT, DOT_PIN) == GPIO_PIN_SET)
  {
    ssd1306_FillRectangle(30, 30, 40, 40, White);
  }
  else
  {
    ssd1306_FillRectangle(30, 30, 40, 40, Black);
    ssd1306_DrawRectangle(30, 30, 40, 40, White);
  }

  // DASH
  if (HAL_GPIO_ReadPin(DASH_PORT, DASH_PIN) == GPIO_PIN_SET)
  {
    ssd1306_FillRectangle(87, 30, 97, 40, White);
  }
  else
  {
    ssd1306_FillRectangle(87, 30, 97, 40, Black);
    ssd1306_DrawRectangle(87, 30, 97, 40, White);
  }
  ssd1306_UpdateScreen();
}

void unit_duration_receive_ui(int unit)
{
  static int prev_unit = -1;

  if (unit != prev_unit)
  {
    // clear the old letter
    ssd1306_SetCursor(30, 20);
    ssd1306_WriteString("     ", Font_7x10, White);
    
    char str[20] = {0};
    snprintf(str, sizeof(str), "%dms", unit);
    
    ssd1306_SetCursor(30, 20);
    ssd1306_WriteString(str, Font_7x10, White);

    ssd1306_UpdateScreen();

    // update the tracker
    prev_unit = unit;
  }
}

void idx_roll_receive_ui(int idx)
{
  static int prev_idx = -1; // use static so it lives even after the function finishes

  if (idx != prev_idx)
  {
    // clear the old letter
    ssd1306_SetCursor(70, 20);
    ssd1306_WriteString("   ", Font_7x10, White);
    
    char str[5] = {0};
    snprintf(str, sizeof(str), "%d", idx);
    
    ssd1306_SetCursor(70, 20);
    ssd1306_WriteString(str, Font_7x10, White);

    ssd1306_UpdateScreen();

    // update the tracker
    prev_idx = idx;
  }
}

void letter_roll_select_ui(char letter)
{
  static char prev_letter = '\0'; // use static so it lives even after the function finishes

  if (letter != prev_letter)
  {
    // clear the old letter
    ssd1306_SetCursor(105, 20);
    ssd1306_WriteString(" ", Font_11x18, White);
    
    ssd1306_SetCursor(105, 20);
    // put the character into a string, since its Write"String"
    // lock it up so it doesnt keep reading beyond the letter
    char str[2] = {letter, '\0'};
    ssd1306_WriteString(str, Font_11x18, White);

    ssd1306_UpdateScreen();

    // update the tracker
    prev_letter = letter;
  }
}

void play_intro_ui() 
{
    ssd1306_Fill(Black);
    
    // title
    ssd1306_SetCursor(30, 10);
    ssd1306_WriteString("MCT-32", Font_11x18, White);
    
    // version
    ssd1306_SetCursor(85, 30);
    ssd1306_WriteString("v1.0.0", Font_6x8, White); 

    // growing line anim
    for(uint8_t i = 0; i < 128; i += 8) {
        ssd1306_Line(0, 45, i, 45, White);
        ssd1306_UpdateScreen();
        HAL_Delay(50); // control the speed of the "loading"
    }

    ssd1306_SetCursor(20, 50);
    ssd1306_WriteString("SYSTEM READY", Font_7x10, White);
    ssd1306_UpdateScreen();
    
    HAL_Delay(1500); // pause

    // clear
    ssd1306_SetCursor(15, 50);
    ssd1306_WriteString("            ", Font_7x10, White);

    ssd1306_SetCursor(15, 50);
    ssd1306_WriteString("PRESS TO START", Font_7x10, White);

    ssd1306_UpdateScreen();
}

void refresh_n_setup_ui(SystemMode_t mode, char* buffer)
{
  // clear
  ssd1306_Fill(Black);

  // header line
  ssd1306_Line(0, 12, 127, 12, White);

  ssd1306_SetCursor(2, 0);
  if (mode == MODE_IDLE) 
  {
    ssd1306_WriteString("MODE: IDLE", Font_7x10, White);
    ssd1306_SetCursor(2, 20);
    ssd1306_WriteString("Slow down,", Font_7x10, White);
    ssd1306_SetCursor(2, 30);
    ssd1306_WriteString("let's take a break", Font_7x10, White);
  }
  if (mode == MODE_SELECT) 
  {
    ssd1306_WriteString("MODE: SELECT", Font_7x10, White);
    ssd1306_SetCursor(2, 20);
    ssd1306_WriteString("Text:", Font_7x10, White);
  }
  if (mode == MODE_RECEIVE) ssd1306_WriteString("MODE: RECEIVE", Font_7x10, White);
  if (mode == MODE_MANUAL) 
  {
    ssd1306_WriteString("MODE: MANUAL", Font_7x10, White);
  
    ssd1306_SetCursor(25, 20);
    ssd1306_WriteString("DOT", Font_7x10, White);
    
    ssd1306_SetCursor(80, 20);
    ssd1306_WriteString("DASH", Font_7x10, White);
  }
  ssd1306_UpdateScreen();
}

void handle_manual_mode()
{
  // check the param value to see its changes
  bool isBusy = handle_transmit(0);

  if (!isBusy)
  {
    if (HAL_GPIO_ReadPin(DOT_PORT, DOT_PIN) == GPIO_PIN_SET)
    {
      handle_transmit(130);
    }
    else if (HAL_GPIO_ReadPin(DASH_PORT, DASH_PIN) == GPIO_PIN_SET)
    {
      handle_transmit(390);
    }
  }
}

bool handle_transmit(int pulse_duration)
{
  static SubState_t current_state = IDLE;
  static uint32_t start_time = 0;
  static int current_duration = 0;

  uint32_t now = HAL_GetTick();

  switch(current_state)
  {
    case IDLE:
      if (pulse_duration > 0)
      {
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LED3_PORT, LED3_PIN, GPIO_PIN_RESET);
        current_duration = pulse_duration;
        start_time = now;
        current_state = DEBOUNCE;
      }
      return false; // not busy

    case DEBOUNCE:
      if (now - start_time >= 10) // 10ms for confirmation
      {
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);
        HAL_GPIO_WritePin(LED3_PORT, LED3_PIN, GPIO_PIN_SET);
        start_time = now;
        current_state = SENDING;
      }
      return true; // busy. lock the process, no new command allowed. (works bc its bool not void)
      
    case SENDING:
      if (now - start_time >= current_duration)
      {
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LED3_PORT, LED3_PIN, GPIO_PIN_RESET);
        start_time = now;
        current_state = GAP;
      }
      return true;

    case GAP:
      if (now - start_time >= 130)
      {
        current_state = IDLE;
      }
      return true;

    // in case of State Corruption
    default:
      current_state = IDLE;
      return false;
  }
}

void handle_ldr_receive(uint32_t threshold, uint32_t current_pwm_level) {

    reset_and_tune_handler();

    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
        rx.ldr_val = HAL_ADC_GetValue(&hadc1);
        uint32_t now = HAL_GetTick();

        // LIGHT IS ON
        if (rx.ldr_val > threshold) 
        {
            if (!rx.is_light_on) 
            { 
                rx.pulse_start = now; // mark the start of a pulse
                rx.is_light_on = true;
            }
            __HAL_TIM_SET_COMPARE(LED2_PORT, LED2_PIN, 1000); // threshold Feedback
            HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);
            HAL_GPIO_WritePin(LED3_PORT, LED3_PIN, GPIO_PIN_SET);
            rx.gap_start = now; // reset the "Gap" timer because light is present
        } 
        // LIGHT IS OFF
        else 
        { 
            if (rx.is_light_on) 
            { 
                uint32_t duration = now - rx.pulse_start;

                if (duration > 30 && duration < (rx.unit_duration * 2)) 
                rx.temp_pattern[rx.pattern_idx++] = '.';
                else if (duration >= (rx.unit_duration * 2)) 
                rx.temp_pattern[rx.pattern_idx++] = '-';
                
                rx.temp_pattern[rx.pattern_idx] = '\0'; // keep it a valid string
                rx.is_light_on = false;
                rx.gap_start = now; // start timing the silence
            }
            __HAL_TIM_SET_COMPARE(LED2_PORT, LED2_PIN, current_pwm_level);
            HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(LED3_PORT, LED3_PIN, GPIO_PIN_RESET);

            // GAP DETECTION (end of a letter)
            if (rx.pattern_idx > 0 && (now - rx.gap_start > (rx.unit_duration * 3))) 
            {
                for (int i = 0; i < 26; i++) // loop through all alphabets (eng)
                {
                    if (strcmp(rx.temp_pattern, rx_morse_map[i]) == 0) { // 0 means "equal"

                        rx.found_char = 'A' + i;
  
                        if (rx.index < (MAX_BUFFER - 1))
                        { // deny stack overflow
                          rx.buffer[rx.index++] = rx.found_char; // post-increment
                          rx.buffer[rx.index] = '\0'; // after being incremented, make it stay string by making it null
                        }
                        break;
                    }
                }
                rx.pattern_idx = 0; // clear for next letter
                rx.temp_pattern[0] = '\0';
            }
            // WORD GAP DETECTION
            if (now - rx.gap_start > (rx.unit_duration * 7)) 
            {
              if (rx.index > 0 && 
                  rx.buffer[rx.index - 1] != ' ' && // if the previous slot is not already a space  
                  rx.index < (MAX_BUFFER - 1))  // if not buffer overflow
              {
                  rx.buffer[rx.index++] = ' ';
                  rx.buffer[rx.index] = '\0';
              }
            }
        }
    }
    HAL_ADC_Stop(&hadc1);
}

void reset_and_tune_handler()
{
  bool is_pressed = (HAL_GPIO_ReadPin(ENC_SW_PORT, ENC_SW_PIN) == GPIO_PIN_RESET);
  
  // button just pushed down
  if (is_pressed && !rx.button_was_pressed) 
  {
    rx.press_start_time = HAL_GetTick();
    rx.button_was_pressed = true;
  }
  // button just released (happening while holding)
  else if (!is_pressed && rx.button_was_pressed)
  {
    uint32_t duration = HAL_GetTick() - rx.press_start_time;

    // reset buffer
    if (duration >= 1000 && duration < 3000) 
    {
      HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_SET);
      HAL_Delay(100);
      HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_RESET);

      reset_receive_buffer();
    }
    else if (duration > 50)
    {
      unit_duration_tuner();
    }
    rx.button_was_pressed = false;
    rx.press_start_time = 0;
  }
}

void reset_receive_buffer()
{
  rx.index = 0;
  rx.buffer[0] = '\0';
  // put status feedback here
}

void unit_duration_tuner() 
{
    rx.preset_idx = (rx.preset_idx + 1) % TOTAL_PRESETS;
    rx.unit_duration = unit_presets[rx.preset_idx];
    // put status feedback here
}

void handle_letter_selection(char input_char)
{
  // ENC_SW: add / delete / send sequence
  static uint32_t press_start = 0;
  bool is_pressed = (HAL_GPIO_ReadPin(ENC_SW_PORT, ENC_SW_PIN) == GPIO_PIN_RESET);
  
  if (is_pressed) 
    {
      if (press_start == 0) press_start = HAL_GetTick();
        uint32_t elapsed = HAL_GetTick() - press_start;
        status_feedback_handler(elapsed);
  }
  // button released
  else if (press_start != 0) 
  {
    uint32_t duration = HAL_GetTick() - press_start;
    handle_morse_input(duration, input_char);
    press_start = 0; // reset
    // ACTION*
    morse_commit();
    // RESET*
    reset_after_commit();
  }
  HAL_Delay(10);
}

void status_feedback_handler(uint32_t timer)
{
  if (timer < 500)
  {
    // add feedback
      HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_SET);
      HAL_Delay(50);
      HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_RESET);

  }
  if (timer >= 500 && timer < 1500) 
  {
    // delete feedback
    HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_SET);
    HAL_Delay(100);
    HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_RESET);
    HAL_Delay(100);
  
  } 
  else if (timer >= 1500 && timer < 3000) 
  {
    // send feedback
    HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_SET);
    HAL_Delay(1500);
    HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_RESET);
  }
}

void handle_morse_input(uint32_t timer, char letter)
{
  // ADD & CONFIRM SEND
  if (timer < 500) 
    {
      if (tx.ready_to_send) 
      {
        tx.confirm_send = true;
        tx.ready_to_reset = true;
      } 
      else if (tx.index < (int)(sizeof(tx.buffer) - 1)) 
      {
        tx.buffer[tx.index++] = letter;
        tx.buffer[tx.index] = '\0';
      }
    } 
    // DELETE
    else if (timer < 1500) 
    {
      if (tx.index > 0) 
      {
        tx.index--;
        tx.buffer[tx.index] = '\0';
      }
    } 
    // CODE SEND
    else 
    {
      tx.ready_to_send = true;
    }
}

void reset_after_commit()
{
  if (tx.ready_to_send && tx.ready_to_reset && !tx.confirm_send) 
  {
    HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_SET);
    HAL_Delay(100);
    HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_RESET);

    // resetting
    tx.ready_to_send = false;
    tx.ready_to_reset = false;
    tx.index = 0;
    tx.buffer[0] = '\0';
  }
}

void morse_commit()
{
  if (tx.confirm_send && !tx.is_running && tx.buffer[0] != '\0') 
  {
    tx.msg_ptr = 0;
    if (lookup_and_load_pattern(tx.buffer[tx.msg_ptr])) 
    {
      tx.is_running = true;
      tx.step = 0; // make sure we start from beginning
    }
    //  consume send flag so it doesn't retrigger repeatedly
    tx.confirm_send = false;
  }
}

bool lookup_and_load_pattern(char character)
{
  // convert character to uppercase to match the lookup table
  char lookup_char = toupper((unsigned char)character);

  // loop through the morse lookup table array
  for (size_t i = 0; i < morse_lookup_length; i++)
  {
    if (morse_lookup_table[i].character == lookup_char)
    {
      // pointer to the matching PATTERN DATA (eg., pattern_a)
      tx.pattern_ptr = morse_lookup_table[i].pattern_data; // Current Pattern
      tx.pattern_length = morse_lookup_table[i].length;    // Current Pattern Length

      return true;
    }
  }
  // character not supported
  return false;
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  // if we use pointer *htim to access the chosen htim's Instance and it's not TIM2, stop the function.
  if (htim->Instance != TIM2) return;
  if (sys.current_mode != MODE_SELECT) return; // do not run in other modes

  // OUTPUT LOGIC, runs every tick if the flag is set
  if (tx.is_running == true)
  {
    // set the next ARR (counter) to reach only the required MORSE CODE unit
    __HAL_TIM_SET_AUTORELOAD(&htim2, tx.pattern_ptr[tx.step] - 1);

    if (tx.buffer[tx.msg_ptr] == ' ') 
    {
      HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
      HAL_GPIO_WritePin(LED3_PORT, LED3_PIN, GPIO_PIN_RESET);
    }
    else 
    {
      // MORSE CODE is an alternating sequence of sound and silence (ex: sound, silence, sound)
      if (tx.step % 2 == 0) // checks if the current position is either EVEN or ODD
      {
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET); // if EVEN
        HAL_GPIO_WritePin(LED3_PORT, LED3_PIN, GPIO_PIN_SET); // if EVEN
      }
      else
      {
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET); // if ODD
        HAL_GPIO_WritePin(LED3_PORT, LED3_PIN, GPIO_PIN_RESET); // if ODD
      }
    }
    
    // moving on to the next piece of MORSE CODE
    tx.step++;

    // checks for character end, if its ended, forward to the next
    if (tx.step >= tx.pattern_length)
    {
      tx.msg_ptr++; // advance to the next character

      // if the character is null, terminate it (aka. stopping)
      if (tx.buffer[tx.msg_ptr] == '\0')
      {
        tx.is_running = false;
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LED3_PORT, LED3_PIN, GPIO_PIN_RESET);
        tx.msg_ptr = 0; // set back to default
        return;
      }

      // loads next character
      if (lookup_and_load_pattern(tx.buffer[tx.msg_ptr]))
      {
        tx.step = 0;
      }
      else
      {
        // tx.pattern_ptr = pattern_space;
        // tx.pattern_length = 1;
        lookup_and_load_pattern(' ');
        tx.step = 0;
      }
    }
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
