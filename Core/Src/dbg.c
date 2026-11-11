/**
 * dbg.c  ——  串口调试打印实现
 */
#include <stdarg.h>
#include <stdio.h>

#include "main.h"
#include "board_config.h"
#include "dbg.h"

static char s_buf[128];

void dbg_printf(const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(s_buf, sizeof(s_buf), fmt, ap);
    va_end(ap);

    if (n > 0) {
        if (n > (int)sizeof(s_buf) - 1) {
            n = (int)sizeof(s_buf) - 1;
        }
        HAL_UART_Transmit(&CAR_DEBUG_HUART, (uint8_t *)s_buf, (uint16_t)n, 100);
    }
}

void dbg_write(const uint8_t *data, uint16_t len)
{
    HAL_UART_Transmit(&CAR_DEBUG_HUART, (uint8_t *)data, len, 100);
}
