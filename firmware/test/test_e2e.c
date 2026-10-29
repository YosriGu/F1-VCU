#include <stdint.h>
#include <string.h>

#include "can_e2e.h"
#include "test_util.h"

static const can_e2e_cfg_t CLASS_A = { 0x5Au, 1u, 2u, 60u };
static const can_e2e_cfg_t CLASS_B = { 0x5Au, 2u, 2u, 100u };

/* Build a protected frame with the given counter (without advancing a TX counter). */
static void make_frame(uint8_t f[8], uint8_t data_id, uint8_t cnt)
{
    uint8_t c = cnt;
    memset(f, 0, 8u);
    f[2] = 0x11u;
    f[5] = 0xA5u;
    can_e2e_protect(data_id, f, &c);
}

static void tick(const can_e2e_cfg_t *cfg, can_e2e_rx_t *rx, unsigned int n)
{
    for (unsigned int i = 0; i < n; i++) {
        can_e2e_rx_tick_1ms(cfg, rx);
    }
}

static void test_tx_counter(void)
{
    uint8_t f[8] = { 0 };
    uint8_t cnt = 0u;
    for (unsigned int i = 0; i < 40u; i++) {
        can_e2e_protect(0x5Au, f, &cnt);
        CHECK((f[1] & 0x0Fu) == (i % 15u));        /* 0..14, wrap to 0, never 15 */
        CHECK(((f[1] & 0x0Fu) != 15u));
        CHECK(f[0] == can_e2e_frame_crc(0x5Au, f));
    }
    /* A corrupt counter variable is treated as 0. */
    cnt = 200u;
    can_e2e_protect(0x5Au, f, &cnt);
    CHECK((f[1] & 0x0Fu) == 0u);
    CHECK(cnt == 1u);
    /* Upper nibble of byte 1 is preserved and covered by the CRC. */
    f[1] = 0xA0u;
    cnt = 3u;
    can_e2e_protect(0x5Au, f, &cnt);
    CHECK(f[1] == 0xA3u);
    const uint8_t crc_a = f[0];
    f[1] = 0xB3u;
    CHECK(can_e2e_frame_crc(0x5Au, f) != crc_a);
}

static void test_valid_after_two_good(void)
{
    can_e2e_rx_t rx;
    uint8_t f[8];
    can_e2e_rx_init(&rx);
    CHECK(rx.state == CAN_E2E_INIT);
    CHECK(!can_e2e_rx_is_valid(&rx));

    make_frame(f, 0x5Au, 4u);
    CHECK(can_e2e_rx_frame(&CLASS_A, &rx, f, 8u) == CAN_E2E_FRAME_OK);
    CHECK(!can_e2e_rx_is_valid(&rx));              /* class A needs 2 good frames */
    make_frame(f, 0x5Au, 5u);
    CHECK(can_e2e_rx_frame(&CLASS_A, &rx, f, 8u) == CAN_E2E_FRAME_OK);
    CHECK(can_e2e_rx_is_valid(&rx));

    /* Full wrap of the counter stays valid: 14 -> 0. */
    uint8_t cnt = 6u;
    for (unsigned int i = 0; i < 40u; i++) {
        memset(f, 0, 8u);
        can_e2e_protect(0x5Au, f, &cnt);
        CHECK(can_e2e_rx_frame(&CLASS_A, &rx, f, 8u) == CAN_E2E_FRAME_OK);
        CHECK(can_e2e_rx_is_valid(&rx));
    }
}

static void bring_valid(can_e2e_rx_t *rx, const can_e2e_cfg_t *cfg, uint8_t first_cnt)
{
    uint8_t f[8];
    can_e2e_rx_init(rx);
    make_frame(f, cfg->data_id, first_cnt);
    (void)can_e2e_rx_frame(cfg, rx, f, 8u);
    make_frame(f, cfg->data_id, (uint8_t)((first_cnt + 1u) % 15u));
    (void)can_e2e_rx_frame(cfg, rx, f, 8u);
}

static void test_bad_crc(void)
{
    can_e2e_rx_t rx;
    uint8_t f[8];
    bring_valid(&rx, &CLASS_A, 0u);
    CHECK(can_e2e_rx_is_valid(&rx));

    make_frame(f, 0x5Au, 2u);
    f[3] ^= 0x01u;                                  /* corrupt payload, CRC no longer matches */
    CHECK(can_e2e_rx_frame(&CLASS_A, &rx, f, 8u) == CAN_E2E_FRAME_BAD_CRC);
    CHECK(rx.state == CAN_E2E_INVALID);             /* one bad frame invalidates class A */
    CHECK(!can_e2e_rx_is_valid(&rx));

    /* Recovery needs two consecutive good frames; the first is accepted without a delta check. */
    make_frame(f, 0x5Au, 9u);
    CHECK(can_e2e_rx_frame(&CLASS_A, &rx, f, 8u) == CAN_E2E_FRAME_OK);
    CHECK(!can_e2e_rx_is_valid(&rx));
    make_frame(f, 0x5Au, 10u);
    CHECK(can_e2e_rx_frame(&CLASS_A, &rx, f, 8u) == CAN_E2E_FRAME_OK);
    CHECK(can_e2e_rx_is_valid(&rx));
}

static void test_wrong_data_id(void)
{
    can_e2e_rx_t rx;
    uint8_t f[8];
    bring_valid(&rx, &CLASS_A, 0u);
    make_frame(f, 0x5Bu, 2u);                       /* masquerade: valid frame of another message */
    CHECK(can_e2e_rx_frame(&CLASS_A, &rx, f, 8u) == CAN_E2E_FRAME_BAD_CRC);
    CHECK(!can_e2e_rx_is_valid(&rx));
}

static void test_counter_faults(void)
{
    can_e2e_rx_t rx;
    uint8_t f[8];

    /* Stuck counter (same frame twice). */
    bring_valid(&rx, &CLASS_A, 0u);                 /* last counter = 1 */
    make_frame(f, 0x5Au, 1u);
    CHECK(can_e2e_rx_frame(&CLASS_A, &rx, f, 8u) == CAN_E2E_FRAME_BAD_COUNTER);
    CHECK(rx.state == CAN_E2E_INVALID);

    /* Skipped counter: class A rejects delta 2, class B accepts it. */
    bring_valid(&rx, &CLASS_A, 0u);
    make_frame(f, 0x5Au, 3u);
    CHECK(can_e2e_rx_frame(&CLASS_A, &rx, f, 8u) == CAN_E2E_FRAME_BAD_COUNTER);
    CHECK(!can_e2e_rx_is_valid(&rx));
    bring_valid(&rx, &CLASS_B, 0u);
    make_frame(f, 0x5Au, 3u);
    CHECK(can_e2e_rx_frame(&CLASS_B, &rx, f, 8u) == CAN_E2E_FRAME_OK);
    CHECK(can_e2e_rx_is_valid(&rx));
    make_frame(f, 0x5Au, 6u);                       /* delta 3 > max_delta 2 */
    CHECK(can_e2e_rx_frame(&CLASS_B, &rx, f, 8u) == CAN_E2E_FRAME_BAD_COUNTER);

    /* Backwards counter. */
    bring_valid(&rx, &CLASS_A, 5u);                 /* last = 6 */
    make_frame(f, 0x5Au, 5u);
    CHECK(can_e2e_rx_frame(&CLASS_A, &rx, f, 8u) == CAN_E2E_FRAME_BAD_COUNTER);

    /* Counter value 15 is never valid, even with a correct CRC. */
    bring_valid(&rx, &CLASS_A, 0u);
    memset(f, 0, 8u);
    f[1] = 0x0Fu;
    f[0] = can_e2e_frame_crc(0x5Au, f);
    CHECK(can_e2e_rx_frame(&CLASS_A, &rx, f, 8u) == CAN_E2E_FRAME_BAD_COUNTER);
    CHECK(!can_e2e_rx_is_valid(&rx));

    /* Resynchronisation after a counter fault: the next consecutive pair becomes valid again. */
    bring_valid(&rx, &CLASS_A, 0u);                 /* last = 1 */
    make_frame(f, 0x5Au, 7u);
    (void)can_e2e_rx_frame(&CLASS_A, &rx, f, 8u);   /* bad counter, resync to 7 */
    make_frame(f, 0x5Au, 8u);
    CHECK(can_e2e_rx_frame(&CLASS_A, &rx, f, 8u) == CAN_E2E_FRAME_OK);
    CHECK(!can_e2e_rx_is_valid(&rx));               /* one good frame is not enough */
    make_frame(f, 0x5Au, 9u);
    CHECK(can_e2e_rx_frame(&CLASS_A, &rx, f, 8u) == CAN_E2E_FRAME_OK);
    CHECK(can_e2e_rx_is_valid(&rx));
}

static void test_dlc(void)
{
    can_e2e_rx_t rx;
    uint8_t f[8];
    bring_valid(&rx, &CLASS_A, 0u);
    make_frame(f, 0x5Au, 2u);
    CHECK(can_e2e_rx_frame(&CLASS_A, &rx, f, 7u) == CAN_E2E_FRAME_BAD_DLC);
    CHECK(!can_e2e_rx_is_valid(&rx));
    bring_valid(&rx, &CLASS_A, 0u);
    CHECK(can_e2e_rx_frame(&CLASS_A, &rx, f, 0u) == CAN_E2E_FRAME_BAD_DLC);
}

static void test_timeout(void)
{
    can_e2e_rx_t rx;
    uint8_t f[8];

    /* Boot: never received anything -> TIMEOUT after timeout_ms, not before. */
    can_e2e_rx_init(&rx);
    tick(&CLASS_A, &rx, 59u);
    CHECK(rx.state == CAN_E2E_INIT);
    tick(&CLASS_A, &rx, 1u);
    CHECK(rx.state == CAN_E2E_TIMEOUT);

    /* Valid stream: 59 ms of silence still valid, the 60th ms times out. */
    bring_valid(&rx, &CLASS_A, 0u);
    tick(&CLASS_A, &rx, 59u);
    CHECK(can_e2e_rx_is_valid(&rx));
    tick(&CLASS_A, &rx, 1u);
    CHECK(rx.state == CAN_E2E_TIMEOUT);
    CHECK(!can_e2e_rx_is_valid(&rx));

    /* A frame resets the age: no timeout while frames arrive every 20 ms. */
    bring_valid(&rx, &CLASS_A, 0u);
    uint8_t cnt = 2u;
    for (unsigned int i = 0; i < 100u; i++) {
        tick(&CLASS_A, &rx, 20u);
        memset(f, 0, 8u);
        can_e2e_protect(0x5Au, f, &cnt);
        (void)can_e2e_rx_frame(&CLASS_A, &rx, f, 8u);
        CHECK(can_e2e_rx_is_valid(&rx));
    }

    /* A bad frame shows INVALID (a frame arrived), and later silence turns it into TIMEOUT. */
    make_frame(f, 0x5Au, 3u);
    f[0] ^= 0xFFu;
    (void)can_e2e_rx_frame(&CLASS_A, &rx, f, 8u);
    tick(&CLASS_A, &rx, 10u);
    CHECK(rx.state == CAN_E2E_INVALID);
    tick(&CLASS_A, &rx, 50u);
    CHECK(rx.state == CAN_E2E_TIMEOUT);

    /* After TIMEOUT two consecutive good frames are needed. */
    make_frame(f, 0x5Au, 4u);
    (void)can_e2e_rx_frame(&CLASS_A, &rx, f, 8u);
    CHECK(!can_e2e_rx_is_valid(&rx));
    make_frame(f, 0x5Au, 5u);
    (void)can_e2e_rx_frame(&CLASS_A, &rx, f, 8u);
    CHECK(can_e2e_rx_is_valid(&rx));

    /* A partial run of good frames must not survive a second timeout. */
    can_e2e_rx_init(&rx);
    tick(&CLASS_A, &rx, 60u);                       /* TIMEOUT */
    make_frame(f, 0x5Au, 1u);
    (void)can_e2e_rx_frame(&CLASS_A, &rx, f, 8u);   /* good_count = 1 */
    tick(&CLASS_A, &rx, 60u);                       /* TIMEOUT again: run is discarded */
    make_frame(f, 0x5Au, 2u);
    (void)can_e2e_rx_frame(&CLASS_A, &rx, f, 8u);
    CHECK(!can_e2e_rx_is_valid(&rx));
    make_frame(f, 0x5Au, 3u);
    (void)can_e2e_rx_frame(&CLASS_A, &rx, f, 8u);
    CHECK(can_e2e_rx_is_valid(&rx));

    /* Configuration error: timeout 0 can never be valid. */
    const can_e2e_cfg_t bad = { 0x5Au, 1u, 1u, 0u };
    can_e2e_rx_init(&rx);
    make_frame(f, 0x5Au, 0u);
    (void)can_e2e_rx_frame(&bad, &rx, f, 8u);
    can_e2e_rx_tick_1ms(&bad, &rx);
    CHECK(!can_e2e_rx_is_valid(&rx));
}

int main(void)
{
    test_tx_counter();
    test_valid_after_two_good();
    test_bad_crc();
    test_wrong_data_id();
    test_counter_faults();
    test_dlc();
    test_timeout();
    TEST_EXIT();
}
