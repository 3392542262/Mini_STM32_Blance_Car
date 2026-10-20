/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include "dma.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "key.h"
#include "motor.h"
#include "oled.h"
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
/// 调试器可观察的测试数据（Test data readable from the debugger）.
volatile int32_t test_left_enc;
volatile int32_t test_right_enc;
volatile int32_t test_left_delta;
volatile int32_t test_right_delta;
volatile float left_speed;
volatile float right_speed;

/// 按键与 OLED 演示计数，调试器可直接观察（Key demo counters）.
volatile uint32_t test_key_press_count;
volatile uint32_t test_key_long_count;

/// OLED 刷新节拍（Display refresh tick）.
static uint32_t s_oled_refresh_tick;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

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
  MX_DMA_Init();
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */
  Motor_Init();
  Key_Init();
  OLED_Init();

  /// 开机画面（Boot screen）.
  OLED_Show_String(0, 0, "Mini Balance Car", 1);
  OLED_Show_String(1, 0, "F103 + SSD1306", 1);
  OLED_Show_String(2, 0, "KEY: PA5", 1);
  OLED_Show_String(3, 0, "press to count", 1);
  s_oled_refresh_tick = HAL_GetTick();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /// 按键轮询，必须每个循环都调用一次（Poll the key every iteration）.
    KeyEvent key_event = Key_Scan();
    uint32_t now = HAL_GetTick();

    if (key_event == KEY_EVENT_PRESS) {
      test_key_press_count++;
      OLED_Show_String(3, 0, "key pressed!  ", 1);
    } else if (key_event == KEY_EVENT_LONG_PRESS) {
      test_key_long_count++;
      OLED_Show_String(3, 0, "key long hold ", 1);
    } else if (key_event == KEY_EVENT_RELEASE) {
      OLED_Show_String(3, 0, "press to count", 1);
    }

    /// 每 100 ms 刷新一次动态数据（Refresh the live data every 100 ms）.
    if ((now - s_oled_refresh_tick) >= 100U) {
      s_oled_refresh_tick = now;

      /// 标签 6 个字符 = 48 像素列，数字从第 7 个字符的位置（56 列）开始画.
      OLED_Show_String(4, 0, "Press:", 1);
      OLED_Show_Int(4, 56, (int32_t)test_key_press_count, 6, 1);

      OLED_Show_String(5, 0, "Long :", 1);
      OLED_Show_Int(5, 56, (int32_t)test_key_long_count, 6, 1);

      OLED_Show_String(6, 0, "Up   :", 1);
      OLED_Show_Int(6, 56, (int32_t)(now / 1000U), 6, 1);

      OLED_Show_String(7, 0, Key_Is_Pressed() ? "KEY  DOWN" : "KEY  UP  ", 1);
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
