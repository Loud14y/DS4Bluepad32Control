/**
 * dbg.h  ——  串口调试打印
 * =====================================================================
 * 为什么不用 printf？
 *   因为 Keil 的 printf 重定向要写一串 __use_no_semihosting / _sys_exit，
 *   新手极容易被它卡住；换成 vsnprintf + HAL_UART_Transmit 就完全没这个问题，
 *   Keil(AC5/AC6) 和 arm-none-eabi-gcc 都能直接用。
 */
#ifndef DBG_H
#define DBG_H

#include <stdint.h>

/** 通过 USART1 打印。支持 %d %u %x %s %c，用法和 printf 一样 */
void dbg_printf(const char *fmt, ...);

/** 直接发一段原始字节（调试协议时用） */
void dbg_write(const uint8_t *data, uint16_t len);

#endif /* DBG_H */
