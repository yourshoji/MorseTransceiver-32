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

// cd C:/Users/Shoji/Documents/STM32PIO/myMorseTransceiver

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
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

// Define pins
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
#define LED2_PORT       &htim4
#define LED2_PIN        TIM_CHANNEL_1
#define LED3_PORT       GPIOB
#define LED3_PIN        GPIO_PIN_12

// Define durations
#define time_dot  1300 // 130ms
#define time_dash 3900  // 390ms
#define gap_sym   1300  // Gap between parts of a letter (130ms)
#define gap_char  3900 // Gap between letters (390ms)
#define gap_word  9100 // Gap between words (910ms)

// Define buffer capacity
#define MAX_BUFFER 128

// mode selection
volatile SystemMode_t current_mode = MODE_IDLE;
GPIO_PinState prev_mode_sw_state = false;

// rotary encoder name-input variables
volatile char select_buffer[MAX_BUFFER] = {0}; // reserve and clear space
volatile int select_idx = 0;
volatile bool confirm_send_flag = false;
volatile bool ready_to_send_flag = false;
volatile bool ready_to_reset_flag = false;

// morse transmission state
volatile size_t msg_index = 0; // Tracks the current character being processed
volatile bool morse_running = false;
volatile int step_counter = 0;

// receiver mode variables
char temp_pattern[8] = {0};      // Stores dots/dashes for the current letter
int pattern_idx = 0;           // Current position in temp_pattern
uint32_t pulse_start = 0;        // When the light turned ON
uint32_t gap_start = 0;          // When the light turned OFF
bool light_is_on = false;        // Flag to track LDR state
// receive mode buffer
volatile char receive_buffer[MAX_BUFFER] = {0};
// unit duration
volatile uint8_t unit_duration = 130; // 130 as default
volatile int receive_idx = 0;
// tuner
const uint16_t unit_presets[] = {130, 150, 200};
const int total_presets = 3;
volatile int preset_idx = 0;
// tuner and reset function
static uint32_t press_start_time = 0;
static bool button_was_pressed = false;

// for debugging
volatile uint32_t letter_index;
volatile char current_letter;
volatile char found;
volatile uint32_t ldr_val;
volatile uint32_t threshold_index;

// telling the compiler that this variable actually exist in another source file (.c)
extern const uint16_t pattern_space[];

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

// prototype for lookup helper (defined later in file)
void update_buffer_ui(int idx, volatile char* buffer);
void unit_duration_receive_ui(int unit);
void index_roll_select(int idx);
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

// Durations for Receiver Mode
const char* morse_lookup[] = {
    ".-",    // A
    "-...",  // B
    "-.-.",  // C
    "-..",   // D
    ".",     // E
    "..-.",  // F
    "--.",   // G
    "....",  // H
    "..",    // I
    ".---",  // J
    "-.-",   // K
    ".-..",  // L
    "--",    // M
    "-.",    // N
    "---",   // O
    ".--.",  // P
    "--.-",  // Q
    ".-.",   // R
    "...",   // S
    "-",     // T
    "..-",   // U
    "...-",  // V
    ".--",   // W
    "-..-",  // X
    "-.--",  // Y
    "--.."   // Z
};

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
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */
  ssd1306_Init();
  // play intro
  play_intro_ui();
  ssd1306_UpdateScreen();


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
    // ---- rotary encoder + switch handling ----
    uint32_t raw_cnt = __HAL_TIM_GET_COUNTER(&htim3);

    // encoder usage: letter scroller
    letter_index = (raw_cnt / 4) % 27; // wrap it up as a safety
    uint32_t pwm_val = (letter_index * 1000) / 25;
    // conditional for blank space
    if (letter_index < 26) 
    {
      current_letter = 'A' + letter_index; 
    }
    else 
    {
      current_letter = ' ';
    }
    volatile GPIO_PinState mode_sw_state = HAL_GPIO_ReadPin(MODE_SW_PORT, MODE_SW_PIN);

    // mode changer
    if (mode_sw_state == GPIO_PIN_RESET && prev_mode_sw_state == GPIO_PIN_SET)
    {
        // clean up before switching
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LED3_PORT, LED3_PIN, GPIO_PIN_RESET);
        morse_running = false; // kill any active interrupt during transmission
        
        current_mode = (current_mode + 1) % 4;

        refresh_n_setup_ui(current_mode, (char*)select_buffer);
        
        HAL_Delay(50);
    }
    prev_mode_sw_state = mode_sw_state;

    switch (current_mode) {
      case MODE_IDLE:
      {
        // hey
        break;
      }
      case MODE_SELECT:
      {
        // ASCII debugging via LED (yellow)
        // __HAL_TIM_SET_COMPARE(LED2_PORT, LED2_PIN, pwm_val); // we should be able to set this in numeric when the UI is done
        handle_letter_selection(current_letter);
        update_buffer_ui(select_idx, select_buffer);
        letter_roll_select_ui(current_letter);
        break;
      }
      case MODE_RECEIVE:
      {
        // encoder usage: LDR threshold
        threshold_index = letter_index * 150;
        handle_ldr_receive(threshold_index, pwm_val);
        update_buffer_ui(receive_idx, receive_buffer);
        index_roll_receive_ui(threshold_index);
        unit_duration_receive_ui(unit_duration);
        break;
      }
      case MODE_MANUAL:
      {
        // __HAL_TIM_SET_COMPARE(LED2_PORT, LED2_PIN, 0);
        handle_manual_mode();
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

void update_buffer_ui(int idx, volatile char* buffer)
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

void unit_duration_receive_ui(int unit)
{
  static int prev_unit = -1;

  if (unit != prev_unit)
  {
    // clear the old letter
    ssd1306_SetCursor(30, 20);
    ssd1306_WriteString("     ", Font_7x10, White);
    
    char str[20] = {unit, '\0'};
    snprintf(str, sizeof(str), "%dms", unit);
    
    ssd1306_SetCursor(30, 20);
    ssd1306_WriteString(str, Font_7x10, White);

    ssd1306_UpdateScreen();

    // update the tracker
    prev_unit = unit;
  }
}

void index_roll_receive_ui(int idx)
{
  static int prev_idx = -1; // use static so it lives even after the function finishes

  if (idx != prev_idx)
  {
    // clear the old letter
    ssd1306_SetCursor(70, 20);
    ssd1306_WriteString("   ", Font_7x10, White);
    
    char str[5] = {idx, '\0'};
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
    
    // Title
    ssd1306_SetCursor(30, 15);
    ssd1306_WriteString("MCT-32", Font_11x18, White);
    
    // Animation: Growing line
    for(uint8_t i = 0; i < 128; i += 8) {
        ssd1306_Line(0, 45, i, 45, White);
        ssd1306_UpdateScreen();
        HAL_Delay(50); // control the speed of the "loading"
    }
    
    ssd1306_SetCursor(20, 50);
    ssd1306_WriteString("SYSTEM READY", Font_7x10, White);
    ssd1306_UpdateScreen();
    
    HAL_Delay(1500); // pause for 3s

    // clear
    ssd1306_SetCursor(15, 50);
    ssd1306_WriteString("            ", Font_7x10, White);

    ssd1306_SetCursor(15, 50);
    ssd1306_WriteString("PRESS TO START", Font_7x10, White);

    ssd1306_UpdateScreen();
}

void refresh_n_setup_ui(SystemMode_t mode, char* buffer)
{
  // Clear
  ssd1306_Fill(Black);

  // Header Line
  ssd1306_Line(0, 12, 127, 12, White);

  ssd1306_SetCursor(2, 0);
  if (mode == MODE_IDLE) ssd1306_WriteString("MODE: IDLE", Font_7x10, White);
  if (mode == MODE_SELECT) 
  {
    ssd1306_WriteString("MODE: SELECT", Font_7x10, White);
    ssd1306_SetCursor(2, 20);
    ssd1306_WriteString("Text:", Font_7x10, White);
  }
  if (mode == MODE_RECEIVE) ssd1306_WriteString("MODE: RECEIVE", Font_7x10, White);
  if (mode == MODE_MANUAL) ssd1306_WriteString("MODE: MANUAL", Font_7x10, White);
  
  // Push to OLED
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
        ldr_val = HAL_ADC_GetValue(&hadc1);
        uint32_t now = HAL_GetTick();

        // LIGHT IS ON
        if (ldr_val > threshold) 
        {
            if (!light_is_on) 
            { 
                pulse_start = now; // Mark the start of a pulse
                light_is_on = true;
            }
            __HAL_TIM_SET_COMPARE(LED2_PORT, LED2_PIN, 1000); // Threshold Feedback
            HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);
            HAL_GPIO_WritePin(LED3_PORT, LED3_PIN, GPIO_PIN_SET);
            gap_start = now; // Reset the "Gap" timer because light is present
        } 
        // LIGHT IS OFF
        else 
        { 
            if (light_is_on) 
            { 
                uint32_t duration = now - pulse_start;

                if (duration > 30 && duration < (unit_duration * 2)) 
                temp_pattern[pattern_idx++] = '.';
                else if (duration >= (unit_duration * 2)) 
                temp_pattern[pattern_idx++] = '-';
                
                temp_pattern[pattern_idx] = '\0'; // Keep it a valid string
                light_is_on = false;
                gap_start = now; // Start timing the silence
            }
            __HAL_TIM_SET_COMPARE(LED2_PORT, LED2_PIN, current_pwm_level);
            HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(LED3_PORT, LED3_PIN, GPIO_PIN_RESET);

            // GAP DETECTION (end of a letter)
            if (pattern_idx > 0 && (now - gap_start > (unit_duration * 3))) 
            {
                for (int i = 0; i < 26; i++) // loop through all alphabets (eng)
                {
                    if (strcmp(temp_pattern, morse_lookup[i]) == 0) {

                        found = 'A' + i;
  
                        if (receive_idx < (int)(sizeof(receive_buffer) - 1))
                        { // basically, receive_idx not exceeding 31
                          receive_buffer[receive_idx++] = found; // post-increment
                          receive_buffer[receive_idx] = '\0'; // after being incremented, make it stay string by making it null
                        }
                        break;
                    }
                }
                pattern_idx = 0; // Clear for next letter
                temp_pattern[0] = '\0';
            }
            // WORD GAP DETECTION
            if (now - gap_start > (unit_duration * 7)) 
            {
              if (receive_idx > 0 && 
                  receive_buffer[receive_idx - 1] != ' ' && // if the previous slot is not already a space  
                  receive_idx < (MAX_BUFFER - 1))  // if not buffer overflow
              {
                  receive_buffer[receive_idx++] = ' ';
                  receive_buffer[receive_idx] = '\0';
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
  if (is_pressed && !button_was_pressed) 
  {
    press_start_time = HAL_GetTick();
    button_was_pressed = true;
  }
  // button just released (happening while holding)
  else if (!is_pressed && button_was_pressed)
  {
    uint32_t duration = HAL_GetTick() - press_start_time;

    // reset buffer
    if (duration > 2000) 
    {
      reset_receive_buffer();
    }
    else if (duration > 50)
    {
      unit_duration_tuner();
    }

    button_was_pressed = false;
    press_start_time = 0;
  }
}

void reset_receive_buffer()
{
  receive_idx = 0;
  receive_buffer[0] = '\0';
  // put status feedback here
}

void unit_duration_tuner() 
{
    preset_idx = (preset_idx + 1) % total_presets;
    unit_duration = unit_presets[preset_idx];
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
    // __HAL_TIM_SET_COMPARE(LED2_PORT, LED2_PIN, 100); // Feedback

  }
  if (timer >= 500 && timer < 1500) 
  {
    // delete feedback
    HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_SET);
    HAL_Delay(100);
    HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_RESET);
    HAL_Delay(100);
    // __HAL_TIM_SET_COMPARE(LED2_PORT, LED2_PIN, 500); // Feedback
  
  } 
  else if (timer >= 1500 && timer < 3000) 
  {
    // send feedback
    HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_SET);
    HAL_Delay(1500);
    HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_RESET);
    // __HAL_TIM_SET_COMPARE(LED2_PORT, LED2_PIN, 1000); // Feedback
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
      else if (select_idx < (int)(sizeof(select_buffer) - 1)) 
      {
        select_buffer[select_idx++] = letter;
        select_buffer[select_idx] = '\0';
      }
    } 
    // DELETE
    else if (timer < 1500) 
    {
      if (select_idx > 0) 
      {
        select_idx--;
        select_buffer[select_idx] = '\0';
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
    HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_SET);
    HAL_Delay(100);
    HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_RESET);

    ready_to_send_flag = false;
    ready_to_reset_flag = false;
    select_idx = 0;
    select_buffer[0] = '\0';
  }
}

void morse_commit()
{
  if (confirm_send_flag && !morse_running && select_buffer[0] != '\0') 
  {
    msg_index = 0;
    if (lookup_and_load_pattern(select_buffer[msg_index])) 
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
  if (htim->Instance != TIM2) return;
  if (current_mode != MODE_SELECT) return; // do not run in other modes

  // OUTPUT LOGIC, runs every tick if the flag is set
  if (morse_running == true)
  {
    // Set the next ARR (Counter) to reach only the required MORSE CODE unit (accurately)
    __HAL_TIM_SET_AUTORELOAD(&htim2, current_pattern_ptr[step_counter] - 1);

    if (select_buffer[msg_index] == ' ') 
    {
      HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
      HAL_GPIO_WritePin(LED3_PORT, LED3_PIN, GPIO_PIN_RESET);
    }
    else 
    {
      // MORSE CODE is an alternating sequence of sound and silence (sound, silence, sound)
      if (step_counter % 2 == 0) // Checks if the current position is either EVEN or ODD
      {
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET); // If EVEN
        HAL_GPIO_WritePin(LED3_PORT, LED3_PIN, GPIO_PIN_SET); // If EVEN
      }
      else
      {
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET); // If ODD
        HAL_GPIO_WritePin(LED3_PORT, LED3_PIN, GPIO_PIN_RESET); // If ODD
      }
    }
    
    // Moving on to the next piece of MORSE CODE
    step_counter++;

    // Checks for character end, if it is ended, forward to the next
    if (step_counter >= current_pattern_length)
    {
      msg_index++; // Advance to the next character

      // If the character is "null" (none), terminate it.
      if (select_buffer[msg_index] == '\0')
      {
        morse_running = false;
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LED3_PORT, LED3_PIN, GPIO_PIN_RESET);
        msg_index = 0; // set back to default
        return;
      }

      // Loads next character
      if (lookup_and_load_pattern(select_buffer[msg_index]))
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
