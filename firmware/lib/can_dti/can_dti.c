#include "can_dti.h"

#include <math.h>

bool can_dti_cfg_valid(const can_dti_cfg_t *cfg)
{
    if ((cfg->node == 0x00u) || (cfg->node == 0xFFu)) {
        return false;
    }
    if (!isfinite(cfg->i_drive_max_a) || !isfinite(cfg->i_regen_max_a)) {
        return false;
    }
    if ((cfg->i_drive_max_a <= 0.0f) || (cfg->i_drive_max_a > CAN_DTI_ABS_MAX_A)) {
        return false;
    }
    if ((cfg->i_regen_max_a <= 0.0f) || (cfg->i_regen_max_a > CAN_DTI_ABS_MAX_A)) {
        return false;
    }
    return true;
}

uint32_t can_dti_make_id(uint8_t cmd, uint8_t node)
{
    return ((uint32_t)cmd << 8) | (uint32_t)node;
}

static void put_i32_be(uint8_t out[4], int32_t value)
{
    const uint32_t v = (uint32_t)value;
    out[0] = (uint8_t)(v >> 24);
    out[1] = (uint8_t)(v >> 16);
    out[2] = (uint8_t)(v >> 8);
    out[3] = (uint8_t)(v);
}

static int32_t amps_to_milli(float amps)
{
    const float scaled = amps * 1000.0f;
    return (int32_t)(scaled + ((scaled >= 0.0f) ? 0.5f : -0.5f));   /* round half away from zero */
}

bool can_dti_encode_set_current(const can_dti_cfg_t *cfg, float amps, uint8_t out[4])
{
    if (!can_dti_cfg_valid(cfg) || !isfinite(amps)) {
        return false;
    }
    if (amps > cfg->i_drive_max_a) {
        amps = cfg->i_drive_max_a;
    }
    if (amps < -cfg->i_regen_max_a) {
        amps = -cfg->i_regen_max_a;
    }
    put_i32_be(out, amps_to_milli(amps));
    return true;
}

bool can_dti_encode_set_brake_current(const can_dti_cfg_t *cfg, float amps, uint8_t out[4])
{
    if (!can_dti_cfg_valid(cfg) || !isfinite(amps)) {
        return false;
    }
    if (amps < 0.0f) {
        amps = -amps;
    }
    if (amps > cfg->i_regen_max_a) {
        amps = cfg->i_regen_max_a;
    }
    put_i32_be(out, amps_to_milli(amps));
    return true;
}

bool can_dti_tx_allowed(const can_dti_cfg_t *cfg, uint32_t id, bool extended)
{
    if (!extended || !can_dti_cfg_valid(cfg)) {
        return false;
    }
    if ((id & 0xFFu) != (uint32_t)cfg->node) {
        return false;
    }
    const uint32_t cmd = id >> 8;   /* any bit above bit 15 makes cmd > 0xFF and fails below */
    if ((cmd == CAN_DTI_CMD_SET_CURRENT) || (cmd == CAN_DTI_CMD_SET_BRAKE_CURRENT)) {
        return true;
    }
    if (cfg->allow_limit_cmds && (cmd >= CAN_DTI_CMD_MAX_AC_CURRENT) && (cmd <= CAN_DTI_CMD_MAX_DC_BRAKE)) {
        return true;
    }
    return false;
}
