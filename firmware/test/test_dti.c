#include <math.h>
#include <stdint.h>
#include <string.h>

#include "can_dti.h"
#include "test_util.h"

static const can_dti_cfg_t CFG = { 0x16u, 400.0f, 200.0f, false };

static int32_t be32(const uint8_t b[4])
{
    return (int32_t)(((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | (uint32_t)b[3]);
}

static void test_ids(void)
{
    CHECK(can_dti_make_id(0x01u, 0x16u) == 0x00000116u);
    CHECK(can_dti_make_id(0x02u, 0x16u) == 0x00000216u);
    CHECK(can_dti_make_id(0x0Eu, 0x16u) == 0x00000E16u);
    CHECK(can_dti_make_id(0x01u, 0xFFu) == 0x000001FFu);
}

static void test_encoding(void)
{
    uint8_t d[4];

    /* 51 A -> 51000 = 0x0000C738 (same example value as the VESC CAN documentation). */
    CHECK(can_dti_encode_set_current(&CFG, 51.0f, d));
    CHECK(d[0] == 0x00u && d[1] == 0x00u && d[2] == 0xC7u && d[3] == 0x38u);

    CHECK(can_dti_encode_set_current(&CFG, -51.0f, d));
    CHECK(be32(d) == -51000);
    CHECK(d[0] == 0xFFu && d[1] == 0xFFu && d[2] == 0x38u && d[3] == 0xC8u);

    CHECK(can_dti_encode_set_current(&CFG, 0.0f, d));
    CHECK(be32(d) == 0);

    /* Rounding half away from zero at 1 mA resolution. */
    CHECK(can_dti_encode_set_current(&CFG, 0.0014f, d));
    CHECK(be32(d) == 1);
    CHECK(can_dti_encode_set_current(&CFG, -0.0014f, d));
    CHECK(be32(d) == -1);

    /* Clamping to the configured limits. */
    CHECK(can_dti_encode_set_current(&CFG, 1.0e6f, d));
    CHECK(be32(d) == 400000);
    CHECK(can_dti_encode_set_current(&CFG, -1.0e6f, d));
    CHECK(be32(d) == -200000);

    /* Rejection: NaN, +/-infinity. Output untouched is not promised, so only the return value is checked. */
    CHECK(!can_dti_encode_set_current(&CFG, NAN, d));
    CHECK(!can_dti_encode_set_current(&CFG, INFINITY, d));
    CHECK(!can_dti_encode_set_current(&CFG, -INFINITY, d));
    CHECK(!can_dti_encode_set_brake_current(&CFG, NAN, d));

    /* Brake current: magnitude only, always positive, clamped to the regen limit. */
    CHECK(can_dti_encode_set_brake_current(&CFG, -80.0f, d));
    CHECK(be32(d) == 80000);
    CHECK(can_dti_encode_set_brake_current(&CFG, 80.0f, d));
    CHECK(be32(d) == 80000);
    CHECK(can_dti_encode_set_brake_current(&CFG, 5000.0f, d));
    CHECK(be32(d) == 200000);
}

static void test_config_validation(void)
{
    uint8_t d[4];
    can_dti_cfg_t c = CFG;
    CHECK(can_dti_cfg_valid(&c));

    c = CFG; c.node = 0x00u;           CHECK(!can_dti_cfg_valid(&c));
    c = CFG; c.node = 0xFFu;           CHECK(!can_dti_cfg_valid(&c));   /* broadcast is forbidden */
    c = CFG; c.i_drive_max_a = 0.0f;   CHECK(!can_dti_cfg_valid(&c));
    c = CFG; c.i_drive_max_a = -1.0f;  CHECK(!can_dti_cfg_valid(&c));
    c = CFG; c.i_regen_max_a = 0.0f;   CHECK(!can_dti_cfg_valid(&c));
    c = CFG; c.i_drive_max_a = NAN;    CHECK(!can_dti_cfg_valid(&c));
    c = CFG; c.i_regen_max_a = INFINITY; CHECK(!can_dti_cfg_valid(&c));
    c = CFG; c.i_drive_max_a = CAN_DTI_ABS_MAX_A + 1.0f; CHECK(!can_dti_cfg_valid(&c));

    /* An invalid configuration encodes nothing and allows nothing. */
    c = CFG; c.node = 0xFFu;
    CHECK(!can_dti_encode_set_current(&c, 10.0f, d));
    CHECK(!can_dti_tx_allowed(&c, 0x000001FFu, true));
}

/* Reference model for the whitelist, written from the rule text (extended frame, 16-bit id space,
 * our node in the low byte, packet 0x01/0x02, plus 0x0C..0x0F only when enabled). */
static int expected_allowed(const can_dti_cfg_t *c, uint32_t id, int ext)
{
    if (!ext || ((id >> 16) != 0u) || ((id & 0xFFu) != c->node)) {
        return 0;
    }
    const uint32_t cmd = (id >> 8) & 0xFFu;
    if ((cmd == 0x01u) || (cmd == 0x02u)) {
        return 1;
    }
    return (c->allow_limit_cmds && (cmd >= 0x0Cu) && (cmd <= 0x0Fu)) ? 1 : 0;
}

static void test_whitelist(void)
{
    can_dti_cfg_t c = CFG;

    /* Exhaustive over every (cmd, node) pair for both settings of allow_limit_cmds. */
    for (int allow = 0; allow < 2; allow++) {
        c.allow_limit_cmds = (allow != 0);
        unsigned int allowed = 0;
        for (uint32_t cmd = 0; cmd < 256u; cmd++) {
            for (uint32_t node = 0; node < 256u; node++) {
                const uint32_t id = (cmd << 8) | node;
                const int want = expected_allowed(&c, id, 1);
                const int got = can_dti_tx_allowed(&c, id, true) ? 1 : 0;
                CHECK(got == want);
                allowed += (unsigned int)got;
                CHECK(!can_dti_tx_allowed(&c, id, false));   /* standard frames never pass */
            }
        }
        CHECK(allowed == (c.allow_limit_cmds ? 6u : 2u));    /* 0x01, 0x02 (+0x0C..0x0F) for our node only */
    }

    /* Node 0xFF broadcast and node 0x00 never pass, whatever the configured node is. */
    c = CFG;
    CHECK(!can_dti_tx_allowed(&c, 0x000001FFu, true));
    CHECK(!can_dti_tx_allowed(&c, 0x00000100u, true));

    /* Random fuzz over the whole 32-bit space, including bits above the 29-bit ID. */
    unsigned int seed = 0xCAFEF00Du;
    for (int allow = 0; allow < 2; allow++) {
        c.allow_limit_cmds = (allow != 0);
        for (int i = 0; i < 1000000; i++) {
            const uint32_t id = xorshift32(&seed);
            const int ext = (int)(xorshift32(&seed) & 1u);
            CHECK(((can_dti_tx_allowed(&c, id, ext != 0)) ? 1 : 0) == expected_allowed(&c, id, ext));
        }
    }

    /* IDs just outside the allowed set. */
    c = CFG;
    CHECK(can_dti_tx_allowed(&c, 0x00000116u, true));
    CHECK(can_dti_tx_allowed(&c, 0x00000216u, true));
    CHECK(!can_dti_tx_allowed(&c, 0x00000016u, true));    /* duty */
    CHECK(!can_dti_tx_allowed(&c, 0x00000316u, true));    /* ERPM */
    CHECK(!can_dti_tx_allowed(&c, 0x00000416u, true));    /* position */
    CHECK(!can_dti_tx_allowed(&c, 0x00000A16u, true));    /* relative current */
    CHECK(!can_dti_tx_allowed(&c, 0x00000C16u, true));    /* max AC current: off by default */
    CHECK(!can_dti_tx_allowed(&c, 0x80000116u, true));    /* stray high bit */
    CHECK(!can_dti_tx_allowed(&c, 0x01000116u, true));
    CHECK(!can_dti_tx_allowed(&c, 0x00010116u, true));
    c.allow_limit_cmds = true;
    CHECK(can_dti_tx_allowed(&c, 0x00000C16u, true));
    CHECK(can_dti_tx_allowed(&c, 0x00000F16u, true));
    CHECK(!can_dti_tx_allowed(&c, 0x00001016u, true));
    CHECK(!can_dti_tx_allowed(&c, 0x00000B16u, true));
}

int main(void)
{
    test_ids();
    test_encoding();
    test_config_validation();
    test_whitelist();
    TEST_EXIT();
}
