/*
 * DTI HV-550 command encoding and the PT-CAN transmit whitelist (VCU CAN spec, section 6.1).
 *
 * ID format: 29-bit extended, (packet_id << 8) | node_id.                         (DTI CAN description V1.3)
 * Value format: int32, amps x 1000, MSB first in data[0..3].                      (scaling: V1.3; byte
 *               order: VESC lineage -- CONFIRM BY BENCH CAPTURE, test T-DTI-1/T-DTI-2)
 *
 * Only the commands the VCU is allowed to send are encodable here. Duty, ERPM, position and relative
 * current commands do not exist in this module on purpose.
 */
#ifndef CAN_DTI_H
#define CAN_DTI_H

#include <stdbool.h>
#include <stdint.h>

#define CAN_DTI_CMD_SET_CURRENT        0x01u
#define CAN_DTI_CMD_SET_BRAKE_CURRENT  0x02u
#define CAN_DTI_CMD_MAX_AC_CURRENT     0x0Cu   /* 0x0C..0x0F: only after test T-DTI-6, reduce-only */
#define CAN_DTI_CMD_MAX_DC_BRAKE       0x0Fu

#define CAN_DTI_ABS_MAX_A              1000.0f /* sanity bound for configuration, not a rule value */

typedef struct {
    uint8_t node;               /* 1..254. 0x00 and 0xFF (broadcast) are rejected */
    float   i_drive_max_a;      /* > 0: largest positive Set current */
    float   i_regen_max_a;      /* > 0: largest regen current magnitude */
    bool    allow_limit_cmds;   /* permit IDs 0x0C..0x0F (default false) */
} can_dti_cfg_t;

bool     can_dti_cfg_valid(const can_dti_cfg_t *cfg);
uint32_t can_dti_make_id(uint8_t cmd, uint8_t node);

/* Set current (0x01). Signed: positive = drive, negative = regen (T-DTI-5 decides the regen method).
 * Clamps to [-i_regen_max, +i_drive_max]. Rejects NaN, infinity and an invalid configuration. */
bool can_dti_encode_set_current(const can_dti_cfg_t *cfg, float amps, uint8_t out[4]);

/* Set brake current (0x02). The DTI ignores the sign, so the magnitude is always sent positive,
 * clamped to [0, i_regen_max]. */
bool can_dti_encode_set_brake_current(const can_dti_cfg_t *cfg, float amps, uint8_t out[4]);

/* Transmit whitelist for the PT-CAN driver: true only for extended frames addressed to cfg->node
 * with packet 0x01 or 0x02 (and 0x0C..0x0F if allow_limit_cmds). Everything else is refused. */
bool can_dti_tx_allowed(const can_dti_cfg_t *cfg, uint32_t id, bool extended);

#endif /* CAN_DTI_H */
