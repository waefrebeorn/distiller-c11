/* test_gui.c — unit tests for the e-ink app shell. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui/gui/gui.h"
#include "eink/eink.h"

static int failures = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__); \
        failures++; \
    } \
} while (0)

int main(void)
{
    gui_shell *g = gui_shell_create(240, 416);
    CHECK(g != NULL, "create");
    CHECK(gui_shell_create(0, 0) == NULL, "zero size rejected");

    /* empty menu */
    CHECK(gui_shell_render(g, NULL) == -1, "render null out rejected");
    uint8_t packed[12480];
    CHECK(gui_shell_render(g, packed) == -1, "render empty menu rejected");

    /* menu with items */
    gui_menu_item items[3] = {
        { .label = "Clock", .app_id = "clock" },
        { .label = "Weather", .app_id = "weather" },
        { .label = "AI Chat", .app_id = "ai-chat" },
    };
    CHECK(gui_shell_set_menu(g, items, 3) == 0, "set menu");
    CHECK(gui_shell_selected(g) == 0, "starts at 0");
    CHECK(gui_shell_set_menu(NULL, items, 3) == -1, "null shell rejected");
    CHECK(gui_shell_set_menu(g, NULL, 3) == -1, "null items rejected");

    /* render produces a packed frame */
    memset(packed, 0xFF, sizeof(packed));
    CHECK(gui_shell_render(g, packed) == 0, "render ok");
    /* a real menu has content: not all 0xFF */
    int has_content = 0;
    for (size_t i = 0; i < sizeof(packed); i++) {
        if (packed[i] != 0xFF) { has_content = 1; break; }
    }
    CHECK(has_content, "menu pixels present");

    /* navigation */
    gui_shell_navigate(g, 1);
    CHECK(gui_shell_selected(g) == 1, "down to 1");
    gui_shell_navigate(g, 1);
    CHECK(gui_shell_selected(g) == 2, "down to 2");
    gui_shell_navigate(g, 1);
    CHECK(gui_shell_selected(g) == 0, "wraps to 0");
    gui_shell_navigate(g, -1);
    CHECK(gui_shell_selected(g) == 2, "up wraps to 2");

    /* buttons: edge-triggered */
    CHECK(gui_shell_handle_button(g, 0) == GUI_KEY_NONE, "no button");
    CHECK(gui_shell_handle_button(g, GUI_KEY_DOWN) == GUI_KEY_DOWN, "down press");
    CHECK(gui_shell_selected(g) == 0, "down navigated to 0");
    CHECK(gui_shell_handle_button(g, GUI_KEY_DOWN) == GUI_KEY_NONE,
          "held button not re-triggered");
    CHECK(gui_shell_handle_button(g, GUI_KEY_SELECT) == GUI_KEY_SELECT, "select");
    CHECK(gui_shell_handle_button(g, GUI_KEY_SHUTDOWN) == GUI_KEY_SHUTDOWN, "shutdown");

    /* full pipeline: shell render -> eink frame (visual parity) */
    eink_frame *e = eink_create(240, 416);
    CHECK(e != NULL, "eink create");
    uint8_t frame_out[12480];
    /* build a gray frame from the packed one (unpack: bit=1 -> 255) */
    uint8_t gray[240 * 416];
    for (size_t row = 0; row < 416; row++) {
        for (size_t col = 0; col < 240; col++) {
            size_t byte = row * 30 + col / 8;
            int bit = (packed[byte] >> (7 - (col % 8))) & 1;
            gray[row * 240 + col] = bit ? 255 : 0;
        }
    }
    CHECK(eink_render_gray(e, gray, frame_out) == 12480, "pipeline render");
    eink_free(e);

    gui_shell_free(g);
    CHECK(gui_shell_selected(NULL) == 0, "null selected safe");

    if (failures == 0) {
        printf("test_gui: ALL PASS\n");
        return 0;
    }
    printf("test_gui: %d FAILURES\n", failures);
    return 1;
}
