/* Checks that the generated code (can/gen) and the hand-written E2E layer agree. */
#include <stdint.h>
#include <string.h>

#include "can_e2e.h"
#include "can_msg_cfg.h"
#include "test_util.h"
#include "veh.h"
#include "veh_supervision.h"

static const can_msg_cfg_t *find(uint32_t id)
{
    for (unsigned int i = 0; i < VEH_MSG_COUNT; i++) {
        if (veh_msg_cfg[i].frame_id == id) {
            return &veh_msg_cfg[i];
        }
    }
    return NULL;
}

static void test_table(void)
{
    const can_msg_cfg_t *lim = find(VEH_AMS_LIMITS_FRAME_ID);
    CHECK(lim != NULL);
    if (lim != NULL) {
        CHECK(lim->period_ms == 20u);
        CHECK(lim->rx_timeout_ms == 60u);          /* 3 x period (spec 5.3) */
        CHECK(lim->safety_class == 'A');
        CHECK(lim->data_id == 0x5Au);
        CHECK(!lim->vcu_tx);
    }
    const can_msg_cfg_t *st = find(VEH_VCU_STATE_FRAME_ID);
    CHECK(st != NULL);
    if (st != NULL) {
        CHECK(st->vcu_tx);
        CHECK(st->period_ms == 10u);
        CHECK(st->data_id == 0xA7u);
    }
    CHECK(find(VEH_DASH_IN_FRAME_ID) != NULL);
    CHECK(find(VEH_DASH_IN_FRAME_ID)->rx_timeout_ms == 250u);
    CHECK(find(VEH_DASH_IN_FRAME_ID)->dlc == VEH_DASH_IN_LENGTH);

    /* Table is sorted by id with unique ids; class A/B always carry the E2E header. */
    for (unsigned int i = 0; i < VEH_MSG_COUNT; i++) {
        if (i > 0u) {
            CHECK(veh_msg_cfg[i].frame_id > veh_msg_cfg[i - 1u].frame_id);
        }
        if ((veh_msg_cfg[i].safety_class == 'A') || (veh_msg_cfg[i].safety_class == 'B')) {
            CHECK(veh_msg_cfg[i].data_id != 0u);
        }
        if (veh_msg_cfg[i].data_id != 0u) {
            CHECK(veh_msg_cfg[i].dlc == 8u);
            CHECK(veh_msg_cfg[i].data_id != (uint8_t)(veh_msg_cfg[i].frame_id & 0xFFu));
        }
    }
}

static void test_encode_rounds(void)
{
    /* 1.3f is stored as 1.29999995; plain truncation of value/0.1f would give 12. The generator is run
     * with use_round, so the raw value must be 13 (148 of 1001 values in 0..100.0 step 0.1 break otherwise). */
    CHECK(veh_vcu_state_vcu_torque_req_pct_encode(1.3f) == 13u);
    CHECK(veh_vcu_state_vcu_torque_req_pct_encode(2.1f) == 21u);
    CHECK(veh_vcu_state_vcu_torque_req_pct_encode(100.0f) == 1000u);
    CHECK(veh_vcu_state_vcu_torque_req_pct_encode(0.0f) == 0u);
}

static void test_e2e_with_generated_pack(void)
{
    const can_msg_cfg_t *cfg = find(VEH_VCU_STATE_FRAME_ID);
    CHECK(cfg != NULL);
    if (cfg == NULL) {
        return;
    }
    const can_e2e_cfg_t rxcfg = { cfg->data_id, 1u, 2u, 100u };
    can_e2e_rx_t rx;
    can_e2e_rx_init(&rx);

    uint8_t tx_cnt = 0u;
    for (unsigned int n = 0; n < 40u; n++) {
        struct veh_vcu_state_t s;
        CHECK(veh_vcu_state_init(&s) == 0);
        s.vcu_state_machine  = 6u;   /* DRIVE */
        s.vcu_fault_class    = 0u;
        s.vcu_torque_req_pct = veh_vcu_state_vcu_torque_req_pct_encode(37.5f);
        s.vcu_torque_lim_pct = veh_vcu_state_vcu_torque_lim_pct_encode(80.0f);
        s.vcu_r2_d           = 1u;
        s.vcu_ts_active      = 1u;
        s.vcu_sdc_sense      = 0x0Fu;

        uint8_t frame[8];
        CHECK(veh_vcu_state_pack(frame, &s, sizeof frame) == 8);
        can_e2e_protect(cfg->data_id, frame, &tx_cnt);       /* writes byte 0 and byte 1[3:0] */

        /* Generated unpack sees exactly what the E2E layer wrote. */
        struct veh_vcu_state_t r;
        CHECK(veh_vcu_state_unpack(&r, frame, sizeof frame) == 0);
        CHECK(r.vcu_state_crc == frame[0]);
        CHECK(r.vcu_state_cnt == (n % 15u));
        CHECK(r.vcu_state_machine == 6u);
        CHECK(r.vcu_torque_req_pct == 375u);
        CHECK(r.vcu_torque_lim_pct == 800u);
        CHECK(r.vcu_r2_d == 1u && r.vcu_ts_active == 1u && r.vcu_sdc_sense == 0x0Fu);

        /* Packing did not disturb the bits the E2E layer owns, and the receiver accepts the frame. */
        CHECK(can_e2e_rx_frame(&rxcfg, &rx, frame, 8u) == CAN_E2E_FRAME_OK);
        CHECK((n < 1u) || can_e2e_rx_is_valid(&rx));
    }
}

int main(void)
{
    test_table();
    test_encode_rounds();
    test_e2e_with_generated_pack();
    TEST_EXIT();
}
