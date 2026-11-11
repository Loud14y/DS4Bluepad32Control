/**
 * rc_cmd.c  ——  串口文本命令解析
 */
#include "main.h"
#include "rc_protocol.h"
#include "rc_cmd.h"
#include "dbg.h"

static rc_input_t *s_in    = 0;
static char        s_line[24];
static uint8_t     s_len   = 0;
static uint8_t     s_estop = 0;

#define BTN_SQUARE  0x0001u

/* ------------------------------------------------------------------ */
static void touch(void)
{
    if (s_in) {
        s_in->last_rx_ms = HAL_GetTick();
    }
}

static int clamp100(int v)
{
    if (v >  100) return  100;
    if (v < -100) return -100;
    return v;
}

/** 解析一个十进制整数，成功返回 1 */
static uint8_t parse_int(const char *s, int *out)
{
    int sign = 1;
    int v = 0;
    uint8_t any = 0;

    while (*s == ' ' || *s == '\t') s++;

    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') { s++; }

    while (*s >= '0' && *s <= '9') {
        v = v * 10 + (*s - '0');
        if (v > 100000) v = 100000;      /* 防止溢出 */
        any = 1;
        s++;
    }

    *out = sign * v;
    return any;
}

/* ------------------------------------------------------------------ */
static void print_help(void)
{
    dbg_printf("\r\n========== RC 命令 ==========\r\n");
    dbg_printf(" T<num>   油门, -100~+100    例: T50\r\n");
    dbg_printf(" S<num>   方向, -100~+100    例: S-20\r\n");
    dbg_printf(" B        刹车\r\n");
    dbg_printf(" Z        全部归零\r\n");
    dbg_printf(" E / R    急停 / 解除急停\r\n");
    dbg_printf(" ?        打印本帮助\r\n");
    dbg_printf("==============================\r\n");
}

static void cmd_execute(const char *s)
{
    char c;
    int  v;

    while (*s == ' ') s++;
    if (*s == '\0') return;

    c = *s++;
    while (*s == ' ') s++;

    switch (c) {
    case 'T': case 't':
        if (s_estop) { dbg_printf("[RC] 急停中, 先发 R\r\n"); break; }
        if (!parse_int(s, &v)) { dbg_printf("[RC] 格式错, 例: T50\r\n"); break; }
        s_in->ly = (int8_t)clamp100(v);
        dbg_printf("[RC] 油门 = %d\r\n", s_in->ly);
        touch();
        break;

    case 'S': case 's':
        if (s_estop) { dbg_printf("[RC] 急停中, 先发 R\r\n"); break; }
        if (!parse_int(s, &v)) { dbg_printf("[RC] 格式错, 例: S-20\r\n"); break; }
        s_in->rx = (int8_t)clamp100(v);
        dbg_printf("[RC] 方向 = %d\r\n", s_in->rx);
        touch();
        break;

    case 'B': case 'b':
        s_in->ly = 0;
        dbg_printf("[RC] 刹车\r\n");
        touch();
        break;

    case 'Z': case 'z':
        s_in->ly = 0;
        s_in->rx = 0;
        dbg_printf("[RC] 归零\r\n");
        touch();
        break;

    case 'E': case 'e':
        s_estop = 1;
        s_in->ly = 0;
        s_in->rx = 0;
        s_in->buttons |= BTN_SQUARE;
        dbg_printf("[RC] *** 急停 ***\r\n");
        touch();
        break;

    case 'R': case 'r':
        s_estop = 0;
        s_in->buttons &= (uint16_t)~BTN_SQUARE;
        dbg_printf("[RC] 解除急停\r\n");
        touch();
        break;

    case '?':
        print_help();
        break;

    default:
        dbg_printf("[RC] 未知命令 '%c', 发 ? 看帮助\r\n", c);
        break;
    }
}

/* ------------------------------------------------------------------ */
void rc_cmd_init(rc_input_t *in)
{
    s_in    = in;
    s_len   = 0;
    s_estop = 0;
    if (in) {
        in->ly = 0;
        in->rx = 0;
        in->buttons = 0;
    }
}

void rc_cmd_feed(uint8_t byte)
{
    if (!s_in) return;

    /* 收到行尾：执行 */
    if (byte == '\r' || byte == '\n') {
        if (s_len > 0) {
            s_line[s_len] = '\0';
            cmd_execute(s_line);
            s_len = 0;
        }
        return;
    }

    /* 退格 */
    if (byte == 0x08u || byte == 0x7Fu) {
        if (s_len > 0) s_len--;
        return;
    }

    /* 普通字符存起来 */
    if (s_len < (uint8_t)(sizeof(s_line) - 1u)) {
        s_line[s_len++] = (char)byte;
    } else {
        s_len = 0;              /* 太长了，直接丢掉这行 */
    }
}
