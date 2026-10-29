/*
 * VCU CAN frame protection (VCU CAN spec, section 5).
 *
 * Layout of a protected classic-CAN frame (8 bytes):
 *   byte 0        CRC-8/SAE-J1850 (poly 0x1D, init 0xFF, xorout 0xFF) over { DATA_ID, byte1..byte7 }
 *   byte 1 [3:0]  rolling counter 0..14 (15 is never valid)
 *   byte 1 [7:4]  message-specific state bits (covered by the CRC)
 *   bytes 2..7    payload
 *
 * Pure C, no hardware access, no dynamic memory. Safe to call from the 1 ms task.
 */
#ifndef CAN_E2E_H
#define CAN_E2E_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CAN_E2E_DLC        8u
#define CAN_E2E_CNT_MAX    14u   /* counter runs 0..14 and wraps; 15 is invalid */

/* CRC-8/SAE-J1850. Check value for "123456789" is 0x4B. The bitwise version is the test oracle. */
uint8_t can_crc8_j1850_bitwise(const uint8_t *data, size_t len);
uint8_t can_crc8_j1850(const uint8_t *data, size_t len);

/* CRC over { data_id, frame[1..7] }. frame[0] is ignored. */
uint8_t can_e2e_frame_crc(uint8_t data_id, const uint8_t frame[CAN_E2E_DLC]);

/* TX: writes *counter into byte 1 bits 0..3, writes the CRC into byte 0, then advances *counter
 * (0..14, wrapping to 0). A counter value above 14 is treated as 0. */
void can_e2e_protect(uint8_t data_id, uint8_t frame[CAN_E2E_DLC], uint8_t *counter);

/* ---- RX supervision (spec 5.3) ---- */

typedef enum {
    CAN_E2E_INIT = 0,   /* boot: no frame seen yet, no timeout yet */
    CAN_E2E_VALID,
    CAN_E2E_INVALID,    /* last frame failed DLC/CRC/counter check */
    CAN_E2E_TIMEOUT     /* no frame within timeout_ms */
} can_e2e_state_t;

typedef enum {
    CAN_E2E_FRAME_OK = 0,
    CAN_E2E_FRAME_BAD_DLC,
    CAN_E2E_FRAME_BAD_CRC,
    CAN_E2E_FRAME_BAD_COUNTER
} can_e2e_frame_result_t;

typedef struct {
    uint8_t  data_id;         /* per-message constant from the DBC (DataId), 1..255 */
    uint8_t  max_delta;       /* accepted counter step: 1 for class A, 2 for class B/C */
    uint8_t  good_to_valid;   /* consecutive good frames needed for VALID (2 for class A) */
    uint16_t timeout_ms;      /* RxTimeoutMs; 0 is a configuration error and always reads TIMEOUT */
} can_e2e_cfg_t;

typedef struct {
    can_e2e_state_t state;
    uint16_t        age_ms;      /* ms since the last frame (any frame, good or bad) */
    uint8_t         last_cnt;
    uint8_t         good_count;
    bool            have_last;   /* last_cnt is trustworthy */
} can_e2e_rx_t;

void can_e2e_rx_init(can_e2e_rx_t *rx);

/* Call for every received frame of this message. */
can_e2e_frame_result_t can_e2e_rx_frame(const can_e2e_cfg_t *cfg, can_e2e_rx_t *rx,
                                        const uint8_t *frame, uint8_t dlc);

/* Call once per millisecond. */
void can_e2e_rx_tick_1ms(const can_e2e_cfg_t *cfg, can_e2e_rx_t *rx);

/* Only VALID counts as valid. INIT, INVALID and TIMEOUT all mean "signals invalid, use fallback". */
bool can_e2e_rx_is_valid(const can_e2e_rx_t *rx);

#endif /* CAN_E2E_H */
