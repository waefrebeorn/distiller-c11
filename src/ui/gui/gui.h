#ifndef GUI_H
#define GUI_H

/* gui.h — the e-ink app shell (C11, opaque API).
 *
 * NOT a GUI redesign — the same visual output the v1 menu has,
 * rendered through the fast C11 pipeline. The shell binds the
 * modules: registry (which apps) -> compose (draw) -> dither ->
 * pack -> einkdrv (display). App logic stays in the registry's
 * "builtin" apps; the shell handles the render loop.
 */

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gui_shell gui_shell;

typedef enum gui_key {
    GUI_KEY_NONE = 0,
    GUI_KEY_UP = 1,
    GUI_KEY_DOWN = 2,
    GUI_KEY_SELECT = 4,
    GUI_KEY_SHUTDOWN = 8,
} gui_key;

typedef struct gui_menu_item {
    char label[48];
    char app_id[64];
    int selected;
} gui_menu_item;

/* Create the shell for a 240x416 display. */
gui_shell *gui_shell_create(size_t width, size_t height);

void gui_shell_free(gui_shell *g);

/* Set the menu items (copied). Returns 0 on success. */
int gui_shell_set_menu(gui_shell *g, const gui_menu_item *items,
                       size_t count);

/* Move the selection; 0 = up, 1 = down. */
void gui_shell_navigate(gui_shell *g, int dir);

/* Index of the selected item. */
size_t gui_shell_selected(const gui_shell *g);

/* Render the menu to a packed frame (EPD_FRAME_BYTES for 240x416).
 * The visual style matches the v1 menu: black text on white,
 * selected item inverted. */
int gui_shell_render(gui_shell *g, uint8_t *packed_out);

/* Feed a button state bitmask; returns the key that was newly
 * pressed (or GUI_KEY_NONE). */
gui_key gui_shell_handle_button(gui_shell *g, uint8_t state);

#ifdef __cplusplus
}
#endif

#endif /* GUI_H */
