/**
 * rc_protocol.h  ——  遥控帧协议 + 解析器（纯 C，不依赖 HAL）
 * =====================================================================
 *  ESP32 和"串口助手手动发的十六进制"用的都是同一个 12 字节帧，
 *  所以 STM32 这一侧只写一次。
 *
 *  帧格式（12 字节）：
 *    [0]  0xAA        帧头1
 *    [1]  0x55        帧头2
 *    [2]  seq         0~255 循环
 *    [3]  lx   int8   -100~100   左摇杆 X
 *    [4]  ly   int8   -100~100   左摇杆 Y（正 = 前进）
 *    [5]  rx   int8   -100~100   右摇杆 X（正 = 右转）
 *    [6]  ry   int8   -100~100   右摇杆 Y
 *    [7]  btn_lo      bit0 □  bit1 ×  bit2 ○  bit3 △  bit4 L1  bit5 R1  bit6 L3  bit7 R3
 *    [8]  btn_hi      bit0 SHARE  bit1 OPTIONS  bit2 PS  bit3 触摸板
 *    [9]  l2   uint8  0~255       左扳机（刹车）
 *    [10] r2   uint8  0~255       右扳机（油门倍率）
 *    [11] crc8        对 [2]~[10] 共 9 字节算 CRC-8（多项式 0x07，初值 0x00）
 */
#ifndef RC_PROTOCOL_H
#define RC_PROTOCOL_H

#include <stdint.h>

#define RC_FRAME_LEN      12
#define RC_HEAD0          0xAAu
#define RC_HEAD1          0x55u

#define RC_TIMEOUT_LINK   200u    /* 正常遥控链路：200ms 没帧就判定失联 */
#define RC_TIMEOUT_CMD    3000u   /* 串口命令模式：3 秒没新命令就停车 */

/** 上层控制代码只认这个结构体，不关心数据是手柄、键盘还是串口发的 */
typedef struct {
    int8_t   lx, ly, rx, ry;   /* -100 ~ +100 */
    uint8_t  l2, r2;           /* 0 ~ 255 */
    uint16_t buttons;          /* 位定义见文件头 */
    uint32_t last_rx_ms;       /* 最后一次收到有效数据的时刻 */
    uint8_t  online;           /* 1 = 链路正常 */
} rc_input_t;

/** CRC-8，多项式 0x07，初值 0x00 */
uint8_t rc_crc8(const uint8_t *data, uint16_t len);

/**
 * 每收到一个串口字节就调用一次。
 * 收满 12 字节且 CRC 正确 → 返回 1，数据写进 *out；否则返回 0。
 *
 * 注意：成功时请在调用处补一句 out->last_rx_ms = HAL_GetTick();
 * （解析器不依赖 HAL，所以拿不到时间）
 */
uint8_t rc_parser_feed(uint8_t byte, rc_input_t *out);

/** 主循环里定期调用，维护 in->online */
void rc_update_link(rc_input_t *in, uint32_t now_ms, uint32_t timeout_ms);

/** 上电时清零，保证一开始是"中立 + 离线" */
void rc_input_reset(rc_input_t *in);

#endif /* RC_PROTOCOL_H */
