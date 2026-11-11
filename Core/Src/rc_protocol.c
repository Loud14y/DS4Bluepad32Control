/**
 * rc_protocol.c  ——  12 字节帧解析状态机
 * 纯 C，不依赖 HAL / 标准库，可以拿到 PC 上单独编译测试。
 */
#include "rc_protocol.h"

static uint8_t s_buf[RC_FRAME_LEN];
static uint8_t s_idx = 0;

/* =====================================================================
 * CRC-8：多项式 0x07，初值 0x00
 * 逐位算法，比查表慢一点，但 12 字节 × 100Hz 对 72MHz 的 M3 毫无压力
 * ===================================================================== */
uint8_t rc_crc8(const uint8_t *data, uint16_t len)
{
    uint8_t crc = 0x00;
    uint8_t i;

    while (len--) {
        crc ^= *data++;
        for (i = 0; i < 8; i++) {
            if (crc & 0x80u) {
                crc = (uint8_t)((crc << 1) ^ 0x07u);
            } else {
                crc = (uint8_t)(crc << 1);
            }
        }
    }
    return crc;
}

/* =====================================================================
 * 状态机：逐字节喂进来
 * ===================================================================== */
uint8_t rc_parser_feed(uint8_t byte, rc_input_t *out)
{
    /* ---------- s_idx == 0：等第一个帧头 ---------- */
    if (s_idx == 0u) {
        if (byte == RC_HEAD0) {
            s_buf[0] = byte;
            s_idx = 1u;
        }
        return 0;
    }

    /* ---------- s_idx == 1：等第二个帧头 ---------- */
    if (s_idx == 1u) {
        if (byte == RC_HEAD1) {
            s_buf[1] = byte;
            s_idx = 2u;
        } else if (byte != RC_HEAD0) {
            s_idx = 0u;              /* 帧头不对，重新找 */
        }
        /* byte == RC_HEAD0 时什么都不做：s_buf[0]=0xAA 继续等 0x55 */
        return 0;
    }

    /* ---------- s_idx >= 2：正在收数据 ---------- *
     * 关键：如果 "上一个字节 + 当前字节" 又构成了 AA 55，
     * 说明前面那一帧是半截的（丢了字节），必须丢弃重来，
     * 否则下一帧的帧头会被当成数据吞掉，连带丢两帧。            */
    if (s_buf[s_idx - 1u] == RC_HEAD0 && byte == RC_HEAD1) {
        s_buf[0] = RC_HEAD0;
        s_buf[1] = RC_HEAD1;
        s_idx = 2u;
        return 0;
    }

    s_buf[s_idx++] = byte;

    /* ---------- 还没收满，继续等 ---------- */
    if (s_idx < RC_FRAME_LEN) {
        return 0;
    }

    /* ---------- 收满 12 字节，立刻复位（不管对错） ---------- */
    s_idx = 0u;

    /* ---------- CRC 校验 ---------- */
    if (rc_crc8(&s_buf[2], 9) != s_buf[11]) {
        return 0;                    /* 帧损坏，丢弃 */
    }

    /* ---------- 拆包 ---------- */
    out->lx      = (int8_t)s_buf[3];
    out->ly      = (int8_t)s_buf[4];
    out->rx      = (int8_t)s_buf[5];
    out->ry      = (int8_t)s_buf[6];
    out->buttons = (uint16_t)s_buf[7] | ((uint16_t)s_buf[8] << 8);
    out->l2      = s_buf[9];
    out->r2      = s_buf[10];
    return 1;
}

/* =====================================================================
 * 链路超时判断
 * ===================================================================== */
void rc_update_link(rc_input_t *in, uint32_t now_ms, uint32_t timeout_ms)
{
    if (in->last_rx_ms == 0u) {
        in->online = 0u;
        return;
    }
    in->online = ((uint32_t)(now_ms - in->last_rx_ms) <= timeout_ms) ? 1u : 0u;
}

void rc_input_reset(rc_input_t *in)
{
    uint8_t *p = (uint8_t *)in;
    uint16_t i;
    for (i = 0; i < sizeof(rc_input_t); i++) {
        p[i] = 0;
    }
    in->online = 0u;
}
