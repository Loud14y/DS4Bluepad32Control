/**
 * car_control.c  ——  电机 + 舵机执行层
 * =====================================================================
 * 依赖：board_config.h 里定义的引脚和定时器
 * 前提：
 *   1) 电机 PWM 定时器已配成 20 kHz（PSC/ARR 见教程第 2 天）
 *   2) 舵机 PWM 定时器计数器频率必须是 1 MHz、ARR = 19999
 *      （这样 CCR 的数值直接就是"微秒数"，不用换算）
 */
#include "board_config.h"
#include "car_control.h"

/* ---------------- 可调参数：先保守，跑顺了再放开 ---------------- */
#define THROTTLE_LIMIT      60   /* 全局限速 60%，新手不要一上来就 100 */
#define RAMP_STEP            3   /* 每 10ms 油门最多变化 3%，即 300%/秒 */
#define STEER_DEADZONE       4   /* 转向摇杆 ±4 以内当作回中 */

/* ---------------- 内部状态 ---------------- */
static int16_t s_target  = 0;   /* 目标油门  -100..100 */
static int16_t s_current = 0;   /* 当前油门（带斜坡） */
static int16_t s_steer   = 0;   /* 当前转向 */
static int16_t s_last_steer = -32768;  /* 用来避免重复写舵机 */

/* =====================================================================
 * 电机输出
 * ===================================================================== */
static void motor_apply(int16_t pct)
{
    uint32_t period = (uint32_t)__HAL_TIM_GET_AUTORELOAD(&CAR_MOTOR_HTIM) + 1u;
    uint32_t ccr;
    int16_t  duty;

    if (pct >  100) pct =  100;
    if (pct < -100) pct = -100;

#if CAR_MOTOR_INVERT
    pct = -pct;
#endif

    /* 方向：TB6612 的 AIN1/AIN2 一高一低才转，两个都低是"短路刹车" */
    if (pct > 0) {
        HAL_GPIO_WritePin(CAR_AIN1_PORT, CAR_AIN1_PIN, GPIO_PIN_SET);
        HAL_GPIO_WritePin(CAR_AIN2_PORT, CAR_AIN2_PIN, GPIO_PIN_RESET);
    } else if (pct < 0) {
        HAL_GPIO_WritePin(CAR_AIN1_PORT, CAR_AIN1_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(CAR_AIN2_PORT, CAR_AIN2_PIN, GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(CAR_AIN1_PORT, CAR_AIN1_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(CAR_AIN2_PORT, CAR_AIN2_PIN, GPIO_PIN_RESET);
    }

    duty = (pct >= 0) ? pct : (int16_t)(-pct);
    ccr  = period * (uint32_t)duty / 100u;
    __HAL_TIM_SET_COMPARE(&CAR_MOTOR_HTIM, CAR_MOTOR_CH, ccr);
}

/* =====================================================================
 * 舵机输出（CCR 直接等于微秒数，因为定时器是 1 MHz 计数）
 * ===================================================================== */
static void servo_apply(int16_t steer)
{
    int32_t us;

    if (steer >  100) steer =  100;
    if (steer < -100) steer = -100;

#if CAR_SERVO_INVERT
    steer = -steer;
#endif

    if (steer >= 0) {
        us = CAR_SERVO_MID_US +
             ((int32_t)steer * (CAR_SERVO_MAX_US - CAR_SERVO_MID_US) / 100);
    } else {
        us = CAR_SERVO_MID_US +
             ((int32_t)steer * (CAR_SERVO_MID_US - CAR_SERVO_MIN_US) / 100);
    }

    __HAL_TIM_SET_COMPARE(&CAR_SERVO_HTIM, CAR_SERVO_CH, (uint32_t)us);
}

/* =====================================================================
 * 对外接口
 * ===================================================================== */
void car_init(void)
{
    /* 1) 先把 H 桥关掉，防止上电瞬间乱动 */
    HAL_GPIO_WritePin(CAR_STBY_PORT, CAR_STBY_PIN, GPIO_PIN_RESET);

    /* 2) 占空比清零 */
    s_target = 0;
    s_current = 0;
    s_steer = 0;

    /* 3) 启动 PWM */
    HAL_TIM_PWM_Start(&CAR_MOTOR_HTIM, CAR_MOTOR_CH);
    HAL_TIM_PWM_Start(&CAR_SERVO_HTIM, CAR_SERVO_CH);

    motor_apply(0);

    /* 4) 舵机先回中，等它到位 */
    servo_apply(0);
    HAL_Delay(300);

    /* 5) 使能 H 桥 */
    HAL_GPIO_WritePin(CAR_STBY_PORT, CAR_STBY_PIN, GPIO_PIN_SET);
}

void car_command(int16_t throttle, int16_t steer)
{
    int16_t t = throttle;

    if (t > THROTTLE_LIMIT)  t = THROTTLE_LIMIT;
    if (t < -THROTTLE_LIMIT) t = -THROTTLE_LIMIT;
    s_target = t;

    /* 转向死区，避免摇杆没回正就一直在纠偏 */
    if (steer > -STEER_DEADZONE && steer < STEER_DEADZONE) steer = 0;
    if (steer >  100) steer =  100;
    if (steer < -100) steer = -100;

    if (steer != s_last_steer) {
        s_steer = steer;
        s_last_steer = steer;
        servo_apply(steer);
    }
}

void car_task_10ms(void)
{
    /* 油门斜坡：避免从 -60 直接跳到 +60 把电机齿轮打坏 */
    if (s_current < s_target) {
        s_current += RAMP_STEP;
        if (s_current > s_target) s_current = s_target;
    } else if (s_current > s_target) {
        s_current -= RAMP_STEP;
        if (s_current < s_target) s_current = s_target;
    }
    motor_apply(s_current);
}

void car_stop(void)
{
    s_target  = 0;
    s_current = 0;
    s_steer   = 0;
    s_last_steer = 0;
    motor_apply(0);
    servo_apply(0);
}

int16_t car_get_throttle(void)
{
    return s_current;
}
