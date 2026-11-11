/**
 * car_control.h  ——  电机 + 舵机执行层
 * =====================================================================
 * 上层只调用这三个函数，不关心 PWM/GPIO 细节：
 *     car_init();                      // 上电调用一次
 *     car_command(油门, 转向);          // -100 ~ +100
 *     car_task_10ms();                 // 每 10ms 调用一次
 */
#ifndef CAR_CONTROL_H
#define CAR_CONTROL_H

#include <stdint.h>

/** 上电初始化：关输出、舵机回中、使能 H 桥。必须在 main() 里调用一次 */
void car_init(void);

/** 设定目标：throttle / steer 都是 -100 ~ +100 */
void car_command(int16_t throttle, int16_t steer);

/** 10ms 周期任务：油门斜坡 + 输出到硬件 */
void car_task_10ms(void);

/** 立即停车（不做斜坡），用于失联/急停 */
void car_stop(void);

/** 查询当前实际输出的油门百分比（调试用） */
int16_t car_get_throttle(void);

#endif /* CAR_CONTROL_H */
