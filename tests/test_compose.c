/* test_compose.c — unit tests for the compose module. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compose/compose.h"

static int failures = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__); \
        failures++; \
    } \
} while (0)

int main(void)
{
    canvas *c = canvas_create(100, 50);
    CHECK(c != NULL, "create");
    CHECK(canvas_width(c) == 100 && canvas_height(c) == 50, "dims");

    /* initial: all white */
    uint8_t *d = canvas_data(c);
    int allwhite = 1;
    for (int i = 0; i < 100 * 50; i++) if (d[i] != 255) allwhite = 0;
    CHECK(allwhite, "starts white");

    /* fill_rect paints black */
    canvas_fill_rect(c, 10, 10, 5, 5);
    CHECK(d[10 * 100 + 10] == 0, "rect pixel black");
    CHECK(d[0] == 255, "outside rect white");

    /* draw text: 'I' at 0,0 scale 1 — the I glyph has a vertical
     * bar at bit 2 (x=2) rows 1..5? Actually I = {0,65,127,65,0}
     * rows 1-3. Check center column gets pixels. */
    canvas *t = canvas_create(20, 10);
    canvas_draw_text(t, "I", 0, 0, 1);
    uint8_t *td = canvas_data(t);
    /* 'I' glyph: row0=0, row1=65(bit6), row2=127, row3=65, row4=0.
     * bit6 at scale 1 -> x=6. Check (6,1) and (6,2) are black. */
    CHECK(td[1 * 20 + 6] == 0, "I row1 col6 black");
    CHECK(td[2 * 20 + 6] == 0, "I row2 col6 black");
    CHECK(td[0] == 255, "I row0 white");
    canvas_free(t);

    /* invert flips everything */
    canvas_invert(c);
    CHECK(d[10 * 100 + 10] == 255, "invert: black->white");
    CHECK(d[0] == 0, "invert: white->black");

    /* NULL safety */
    CHECK(canvas_draw_text(NULL, "x", 0, 0, 1) == -1, "null canvas");
    CHECK(canvas_draw_text(c, NULL, 0, 0, 1) == -1, "null text");
    CHECK(canvas_draw_text(c, "x", 0, 0, 0) == -1, "scale 0");

    canvas_free(c);
    if (failures == 0) {
        printf("test_compose: ALL PASS\n");
        return 0;
    }
    printf("test_compose: %d FAILURES\n", failures);
    return 1;
}
