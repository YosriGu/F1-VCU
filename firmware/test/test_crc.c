#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "can_e2e.h"
#include "test_util.h"

#ifndef E2E_VECTORS_CSV
#error "E2E_VECTORS_CSV must point to can/tests/vectors/e2e_vectors.csv"
#endif

static int hex_to_bytes(const char *hex, uint8_t *out, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        unsigned int v;
        if (sscanf(hex + 2 * i, "%2x", &v) != 1) {
            return 0;
        }
        out[i] = (uint8_t)v;
    }
    return 1;
}

int main(void)
{
    /* 1. Standard check value for CRC-8/SAE-J1850. */
    const uint8_t check[] = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };
    CHECK(can_crc8_j1850_bitwise(check, sizeof check) == 0x4Bu);
    CHECK(can_crc8_j1850(check, sizeof check) == 0x4Bu);

    /* 2. Table version equals the bitwise oracle for many random buffers of every length 0..32. */
    unsigned int seed = 0x12345678u;
    for (int iter = 0; iter < 20000; iter++) {
        uint8_t buf[32];
        const size_t len = (size_t)(xorshift32(&seed) % 33u);
        for (size_t i = 0; i < len; i++) {
            buf[i] = (uint8_t)xorshift32(&seed);
        }
        CHECK(can_crc8_j1850(buf, len) == can_crc8_j1850_bitwise(buf, len));
    }
    /* Every single-byte input, to cover the whole table. */
    for (unsigned int b = 0; b < 256u; b++) {
        const uint8_t one = (uint8_t)b;
        CHECK(can_crc8_j1850(&one, 1u) == can_crc8_j1850_bitwise(&one, 1u));
    }

    /* 3. Frame CRC definition: crc8({DATA_ID, byte1..7}). */
    for (int iter = 0; iter < 5000; iter++) {
        uint8_t f[8];
        uint8_t tmp[8];
        for (size_t i = 0; i < 8u; i++) {
            f[i] = (uint8_t)xorshift32(&seed);
        }
        const uint8_t did = (uint8_t)xorshift32(&seed);
        tmp[0] = did;
        memcpy(&tmp[1], &f[1], 7u);
        CHECK(can_e2e_frame_crc(did, f) == can_crc8_j1850_bitwise(tmp, 8u));
    }

    /* 4. Cross-language vectors shared with the Python tests. */
    FILE *fp = fopen(E2E_VECTORS_CSV, "r");
    CHECK(fp != NULL);
    int vectors = 0;
    if (fp != NULL) {
        char line[128];
        while (fgets(line, (int)sizeof line, fp) != NULL) {
            unsigned int did;
            unsigned int crc;
            char hex[17];
            if (line[0] == '#') {
                continue;
            }
            if (sscanf(line, "%2x,%16[0-9a-fA-F],%2x", &did, hex, &crc) != 3) {
                continue;
            }
            uint8_t frame[8];
            CHECK(hex_to_bytes(hex, frame, 8u) == 1);
            CHECK(can_e2e_frame_crc((uint8_t)did, frame) == (uint8_t)crc);
            CHECK(frame[0] == (uint8_t)crc);

            /* protect() from the unprotected frame reproduces the vector. */
            uint8_t rebuilt[8];
            memcpy(rebuilt, frame, 8u);
            rebuilt[0] = 0u;
            uint8_t cnt = (uint8_t)(frame[1] & 0x0Fu);
            can_e2e_protect((uint8_t)did, rebuilt, &cnt);
            CHECK(memcmp(rebuilt, frame, 8u) == 0);
            vectors++;
        }
        fclose(fp);
    }
    CHECK(vectors >= 6);

    TEST_EXIT();
}
