#include "can_e2e.h"

/* MSB-first CRC table for polynomial 0x1D (no init / xorout). Generated from the bitwise algorithm
 * and verified against it by test_crc for every input byte sequence in the test set. */
static const uint8_t k_crc_table[256] = {
    0x00u, 0x1Du, 0x3Au, 0x27u, 0x74u, 0x69u, 0x4Eu, 0x53u,
    0xE8u, 0xF5u, 0xD2u, 0xCFu, 0x9Cu, 0x81u, 0xA6u, 0xBBu,
    0xCDu, 0xD0u, 0xF7u, 0xEAu, 0xB9u, 0xA4u, 0x83u, 0x9Eu,
    0x25u, 0x38u, 0x1Fu, 0x02u, 0x51u, 0x4Cu, 0x6Bu, 0x76u,
    0x87u, 0x9Au, 0xBDu, 0xA0u, 0xF3u, 0xEEu, 0xC9u, 0xD4u,
    0x6Fu, 0x72u, 0x55u, 0x48u, 0x1Bu, 0x06u, 0x21u, 0x3Cu,
    0x4Au, 0x57u, 0x70u, 0x6Du, 0x3Eu, 0x23u, 0x04u, 0x19u,
    0xA2u, 0xBFu, 0x98u, 0x85u, 0xD6u, 0xCBu, 0xECu, 0xF1u,
    0x13u, 0x0Eu, 0x29u, 0x34u, 0x67u, 0x7Au, 0x5Du, 0x40u,
    0xFBu, 0xE6u, 0xC1u, 0xDCu, 0x8Fu, 0x92u, 0xB5u, 0xA8u,
    0xDEu, 0xC3u, 0xE4u, 0xF9u, 0xAAu, 0xB7u, 0x90u, 0x8Du,
    0x36u, 0x2Bu, 0x0Cu, 0x11u, 0x42u, 0x5Fu, 0x78u, 0x65u,
    0x94u, 0x89u, 0xAEu, 0xB3u, 0xE0u, 0xFDu, 0xDAu, 0xC7u,
    0x7Cu, 0x61u, 0x46u, 0x5Bu, 0x08u, 0x15u, 0x32u, 0x2Fu,
    0x59u, 0x44u, 0x63u, 0x7Eu, 0x2Du, 0x30u, 0x17u, 0x0Au,
    0xB1u, 0xACu, 0x8Bu, 0x96u, 0xC5u, 0xD8u, 0xFFu, 0xE2u,
    0x26u, 0x3Bu, 0x1Cu, 0x01u, 0x52u, 0x4Fu, 0x68u, 0x75u,
    0xCEu, 0xD3u, 0xF4u, 0xE9u, 0xBAu, 0xA7u, 0x80u, 0x9Du,
    0xEBu, 0xF6u, 0xD1u, 0xCCu, 0x9Fu, 0x82u, 0xA5u, 0xB8u,
    0x03u, 0x1Eu, 0x39u, 0x24u, 0x77u, 0x6Au, 0x4Du, 0x50u,
    0xA1u, 0xBCu, 0x9Bu, 0x86u, 0xD5u, 0xC8u, 0xEFu, 0xF2u,
    0x49u, 0x54u, 0x73u, 0x6Eu, 0x3Du, 0x20u, 0x07u, 0x1Au,
    0x6Cu, 0x71u, 0x56u, 0x4Bu, 0x18u, 0x05u, 0x22u, 0x3Fu,
    0x84u, 0x99u, 0xBEu, 0xA3u, 0xF0u, 0xEDu, 0xCAu, 0xD7u,
    0x35u, 0x28u, 0x0Fu, 0x12u, 0x41u, 0x5Cu, 0x7Bu, 0x66u,
    0xDDu, 0xC0u, 0xE7u, 0xFAu, 0xA9u, 0xB4u, 0x93u, 0x8Eu,
    0xF8u, 0xE5u, 0xC2u, 0xDFu, 0x8Cu, 0x91u, 0xB6u, 0xABu,
    0x10u, 0x0Du, 0x2Au, 0x37u, 0x64u, 0x79u, 0x5Eu, 0x43u,
    0xB2u, 0xAFu, 0x88u, 0x95u, 0xC6u, 0xDBu, 0xFCu, 0xE1u,
    0x5Au, 0x47u, 0x60u, 0x7Du, 0x2Eu, 0x33u, 0x14u, 0x09u,
    0x7Fu, 0x62u, 0x45u, 0x58u, 0x0Bu, 0x16u, 0x31u, 0x2Cu,
    0x97u, 0x8Au, 0xADu, 0xB0u, 0xE3u, 0xFEu, 0xD9u, 0xC4u,
};

uint8_t can_crc8_j1850_bitwise(const uint8_t *data, size_t len)
{
    uint8_t crc = 0xFFu;
    for (size_t i = 0u; i < len; i++) {
        crc ^= data[i];
        for (uint8_t b = 0u; b < 8u; b++) {
            if ((crc & 0x80u) != 0u) {
                crc = (uint8_t)((uint8_t)(crc << 1) ^ 0x1Du);
            } else {
                crc = (uint8_t)(crc << 1);
            }
        }
    }
    return (uint8_t)(crc ^ 0xFFu);
}

uint8_t can_crc8_j1850(const uint8_t *data, size_t len)
{
    uint8_t crc = 0xFFu;
    for (size_t i = 0u; i < len; i++) {
        crc = k_crc_table[(uint8_t)(crc ^ data[i])];
    }
    return (uint8_t)(crc ^ 0xFFu);
}

uint8_t can_e2e_frame_crc(uint8_t data_id, const uint8_t frame[CAN_E2E_DLC])
{
    uint8_t crc = k_crc_table[(uint8_t)(0xFFu ^ data_id)];
    for (size_t i = 1u; i < CAN_E2E_DLC; i++) {
        crc = k_crc_table[(uint8_t)(crc ^ frame[i])];
    }
    return (uint8_t)(crc ^ 0xFFu);
}

void can_e2e_protect(uint8_t data_id, uint8_t frame[CAN_E2E_DLC], uint8_t *counter)
{
    uint8_t cnt = *counter;
    if (cnt > CAN_E2E_CNT_MAX) {
        cnt = 0u;
    }
    frame[1] = (uint8_t)((frame[1] & 0xF0u) | cnt);
    frame[0] = can_e2e_frame_crc(data_id, frame);
    *counter = (cnt >= CAN_E2E_CNT_MAX) ? 0u : (uint8_t)(cnt + 1u);
}

void can_e2e_rx_init(can_e2e_rx_t *rx)
{
    rx->state      = CAN_E2E_INIT;
    rx->age_ms     = 0u;
    rx->last_cnt   = 0u;
    rx->good_count = 0u;
    rx->have_last  = false;
}

static void rx_mark_bad(can_e2e_rx_t *rx)
{
    rx->state      = CAN_E2E_INVALID;
    rx->good_count = 0u;
}

can_e2e_frame_result_t can_e2e_rx_frame(const can_e2e_cfg_t *cfg, can_e2e_rx_t *rx,
                                        const uint8_t *frame, uint8_t dlc)
{
    rx->age_ms = 0u;   /* a frame arrived; a bad one must show INVALID, not TIMEOUT */

    if (dlc != CAN_E2E_DLC) {
        rx_mark_bad(rx);
        rx->have_last = false;
        return CAN_E2E_FRAME_BAD_DLC;
    }
    if (frame[0] != can_e2e_frame_crc(cfg->data_id, frame)) {
        rx_mark_bad(rx);
        rx->have_last = false;   /* counter cannot be trusted */
        return CAN_E2E_FRAME_BAD_CRC;
    }

    const uint8_t cnt = (uint8_t)(frame[1] & 0x0Fu);
    if (cnt > CAN_E2E_CNT_MAX) {
        rx_mark_bad(rx);
        rx->have_last = false;
        return CAN_E2E_FRAME_BAD_COUNTER;
    }

    if (rx->have_last) {
        const uint8_t delta = (uint8_t)((cnt + (CAN_E2E_CNT_MAX + 1u) - rx->last_cnt) % (CAN_E2E_CNT_MAX + 1u));
        if ((delta == 0u) || (delta > cfg->max_delta)) {
            rx_mark_bad(rx);
            rx->last_cnt  = cnt;   /* resynchronise on a frame whose CRC was good */
            rx->have_last = true;
            return CAN_E2E_FRAME_BAD_COUNTER;
        }
    }

    rx->last_cnt  = cnt;
    rx->have_last = true;
    if (rx->good_count < 255u) {
        rx->good_count++;
    }
    if (rx->good_count >= cfg->good_to_valid) {
        rx->state = CAN_E2E_VALID;
    }
    return CAN_E2E_FRAME_OK;
}

void can_e2e_rx_tick_1ms(const can_e2e_cfg_t *cfg, can_e2e_rx_t *rx)
{
    if (rx->age_ms < 0xFFFFu) {
        rx->age_ms++;
    }
    if (rx->age_ms >= cfg->timeout_ms) {
        /* Idempotent on purpose: a partial run of good frames must not survive a timeout. */
        rx->state      = CAN_E2E_TIMEOUT;
        rx->good_count = 0u;
        rx->have_last  = false;
    }
}

bool can_e2e_rx_is_valid(const can_e2e_rx_t *rx)
{
    return rx->state == CAN_E2E_VALID;
}
