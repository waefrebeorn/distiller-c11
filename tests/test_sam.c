/* test_sam.c — unit tests for the SAM module (protocol logic). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hw/sam/sam.h"

static int failures = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__); \
        failures++; \
    } \
} while (0)

int main(void)
{
    /* CRC-8/MAXIM known vector: "123456789" -> 0xA1 */
    const char *v = "123456789";
    uint8_t crc = sam_crc8((const uint8_t *)v, 9);
    CHECK(crc == 0xA1, "crc8 '123456789' == 0xA1");

    /* empty -> 0 */
    CHECK(sam_crc8((const uint8_t *)"", 0) == 0, "crc8 empty == 0");

    /* single byte 0x01 -> 0x5E (CRC-8/MAXIM reflected) */
    CHECK(sam_crc8((const uint8_t *)"\x01", 1) == 0x5E, "crc8 0x01 == 0x5E");

    /* button enum truth (v1 protocol) */
    CHECK(SAM_BTN_UP == 1 && SAM_BTN_DOWN == 2 &&
          SAM_BTN_SELECT == 4 && SAM_BTN_SHUTDOWN == 8,
          "button bitmask values match v1 protocol");

    /* NULL-safety on open */
    CHECK(sam_open(NULL, 9600, NULL) == NULL, "null open fails");
    CHECK(sam_crc8(NULL, 0) == 0, "null crc8 safe");

    if (failures == 0) {
        printf("test_sam: ALL PASS\n");
        return 0;
    }
    printf("test_sam: %d FAILURES\n", failures);
    return 1;
}
