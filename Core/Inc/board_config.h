/**
 * board_config.h  ——  全车唯一的"引脚地图"
 * =====================================================================
 *  STM32F103C8T6（蓝板 / Blue Pill）
 *  以后换板子、改接线，只改这一个文件，其他 .c 一律不用动。
 *
 *  默认引脚分配（已经避开 SWD 的 PA13/PA14 和晶振的 PD0/PD1）：
 *  ------------------------------------------------------------------
 *   功能        外设            引脚      说明
 *  ------------------------------------------------------------------
 *   电机 PWM    TIM3_CH1        PA6       20 kHz
 *   电机方向1   GPIO 输出       PB0       接 TB6612 AIN1
 *   电机方向2   GPIO 输出       PB1       接 TB6612 AIN2
 *   电机使能    GPIO 输出       PB10      接 TB6612 STBY
 *   舵机 PWM    TIM4_CH1        PB6       50 Hz，1 us 分辨率
 *   调试串口    USART1          PA9/PA10  TX/RX → USB 转 TTL
 *   遥控串口    USART2          PA2/PA3   TX/RX → 以后接 ESP32
 *   电池检测    ADC1_IN1        PA1       分压后接这里
 *   板载 LED    GPIO 输出       PC13      低电平点亮
 *   （预留）编码器 TIM2_CH1/CH2 PA0/PA1   注意 PA1 和电池检测二选一
 *  ------------------------------------------------------------------
 */
#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include "main.h"   /* CubeMX 生成：HAL 和引脚宏 */
#include "tim.h"    /* 提供 htim3 / htim4  —— CubeMX 必须勾选 "per peripheral" 才会生成 */
#include "usart.h"  /* 提供 huart1 / huart2 */

/* ---------------- 电机（TB6612 / DRV8871 等 H 桥） ---------------- */
#define CAR_MOTOR_HTIM        htim3
#define CAR_MOTOR_CH          TIM_CHANNEL_1
#define CAR_MOTOR_FREQ_HZ     20000      /* 只做记录，实际由 CubeMX 决定 */

#define CAR_AIN1_PORT         GPIOB
#define CAR_AIN1_PIN          GPIO_PIN_0
#define CAR_AIN2_PORT         GPIOB
#define CAR_AIN2_PIN          GPIO_PIN_1
#define CAR_STBY_PORT         GPIOB
#define CAR_STBY_PIN          GPIO_PIN_10

#define CAR_MOTOR_INVERT      0          /* 方向反了就改成 1 */

/* ---------------- 转向舵机 ---------------- */
#define CAR_SERVO_HTIM        htim4
#define CAR_SERVO_CH          TIM_CHANNEL_1

/* 注意：这里的默认值故意收窄到 1100~1900 us。
 * 500~2500 us 是"理论极限"，直接照抄会把舵机打到底憋死、烧齿轮。
 * 等你按教程第 3 天标定出真实行程，再改这三个数。 */
#define CAR_SERVO_MIN_US      1100
#define CAR_SERVO_MID_US      1500
#define CAR_SERVO_MAX_US      1900
#define CAR_SERVO_INVERT      0          /* 左右反了就改成 1 */

/* ---------------- 串口 ---------------- */
#define CAR_DEBUG_HUART       huart1     /* 调试打印 + 接收命令/帧 */
#define CAR_RC_HUART          huart2     /* 以后接 ESP32 */

/* ---------------- 其他 ---------------- */
#define CAR_LED_PORT          GPIOC
#define CAR_LED_PIN           GPIO_PIN_13
#define CAR_LED_ON()          HAL_GPIO_WritePin(CAR_LED_PORT, CAR_LED_PIN, GPIO_PIN_RESET)
#define CAR_LED_OFF()         HAL_GPIO_WritePin(CAR_LED_PORT, CAR_LED_PIN, GPIO_PIN_SET)
#define CAR_LED_TOGGLE()      HAL_GPIO_TogglePin(CAR_LED_PORT, CAR_LED_PIN)

#endif /* BOARD_CONFIG_H */
