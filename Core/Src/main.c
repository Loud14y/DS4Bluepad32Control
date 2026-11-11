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
#include "adc.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "board_config.h"
#include "rc_protocol.h"
#include "rc_cmd.h"
#include "car_control.h"
#include "dbg.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* 遥控数据从哪来？三选一，只改下面 RC_SOURCE 这一行 */
#define RC_SRC_CMD      1   /* 串口助手发文本命令，例如 T50 */
#define RC_SRC_FRAME    2   /* 串口助手发十六进制 12 字节帧 */
#define RC_SRC_ESP32    3   /* ESP32 转发 PS4 手柄 */

#define RC_SOURCE       RC_SRC_ESP32

#if (RC_SOURCE == RC_SRC_CMD)
  #define RC_TIMEOUT_MS   RC_TIMEOUT_CMD
#else
  #define RC_TIMEOUT_MS   RC_TIMEOUT_LINK
#endif
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static rc_input_t g_rc;
static uint8_t  g_rx1;
static uint8_t  g_rx2;
static uint32_t g_tick_10ms;
static uint32_t g_tick_1s;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1) {

#if (RC_SOURCE == RC_SRC_CMD)
    rc_cmd_feed(g_rx1);                    /* 文本命令 */
#else
    if (rc_parser_feed(g_rx1, &g_rc)) {    /* 二进制帧 */
      g_rc.last_rx_ms = HAL_GetTick();
    }
#endif
    HAL_UART_Receive_IT(&huart1, &g_rx1, 1);   /* 重新开中断 */

  } else if (huart->Instance == USART2) {

#if (RC_SOURCE != RC_SRC_CMD)
    if (rc_parser_feed(g_rx2, &g_rc)) {
      g_rc.last_rx_ms = HAL_GetTick();
    }
#endif
    HAL_UART_Receive_IT(&huart2, &g_rx2, 1);

  }
}

/** 串口出错回调：不做处理的话，出一次错就永远收不到数据了 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1) {
    __HAL_UART_CLEAR_OREFLAG(huart);
    HAL_UART_Receive_IT(&huart1, &g_rx1, 1);
  } else if (huart->Instance == USART2) {
    __HAL_UART_CLEAR_OREFLAG(huart);
    HAL_UART_Receive_IT(&huart2, &g_rx2, 1);
  }
}
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
  MX_ADC1_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  dbg_printf("\r\n===== 小车电控启动 =====\r\n");
#if (RC_SOURCE == RC_SRC_CMD)
  dbg_printf("遥控源: 串口命令 (发 ? 看帮助)\r\n");
  rc_cmd_init(&g_rc);
#elif (RC_SOURCE == RC_SRC_FRAME)
  dbg_printf("遥控源: 串口十六进制帧\r\n");
#else
  dbg_printf("遥控源: ESP32\r\n");
#endif

  rc_input_reset(&g_rc);

  car_init();                                   /* 舵机回中 + 使能 H 桥 */

  HAL_UART_Receive_IT(&huart1, &g_rx1, 1);      /* 打开两个串口的接收中断 */
  HAL_UART_Receive_IT(&huart2, &g_rx2, 1);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  g_tick_10ms = HAL_GetTick();
  g_tick_1s   = HAL_GetTick();

  while (1)
  {
    uint32_t now = HAL_GetTick();

    /* ---------- 10ms 控制周期 ---------- */
    if ((uint32_t)(now - g_tick_10ms) >= 10u) {
      g_tick_10ms += 10u;

      /* 1) 判断链路是否还活着 */
      rc_update_link(&g_rc, now, RC_TIMEOUT_MS);

      /* 2) 安全逻辑优先，任何一条成立就停车 */
      if (!g_rc.online) {
        car_stop();                        /* 失联保护 */
      } else if (g_rc.buttons & 0x0001u) {
        car_stop();                        /* □ 急停 */
      } else if (g_rc.l2 > 128u) {
        car_stop();                        /* L2 刹车 */
      } else {
        /* 3) 正常行驶：ly = 油门，rx = 转向 */
        car_command(g_rc.ly, g_rc.rx);
      }

      /* 4) 输出到电机（带斜坡） */
      car_task_10ms();
    }

    /* ---------- 1 秒心跳 ---------- */
    if ((uint32_t)(now - g_tick_1s) >= 1000u) {
      g_tick_1s = now;
      CAR_LED_TOGGLE();
      dbg_printf("[HB] on=%d thr=%d steer=%d l2=%d btn=%04X\r\n",
                 g_rc.online, car_get_throttle(), g_rc.rx, g_rc.l2, (unsigned)g_rc.buttons);
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
