/* gui.c — the e-ink app shell (C11). */

#include "gui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../compose/compose.h"
#include "../../dither/dither.h"
#include "../../pack/pack.h"

struct gui_shell {
    size_t width;
    size_t height;
    gui_menu_item *items;
    size_t count;
    size_t selected;
    uint8_t last_button;
};

gui_shell *gui_shell_create(size_t width, size_t height)
{
    if (width == 0 || height == 0) {
        return NULL;
    }
    gui_shell *g = calloc(1, sizeof(*g));
    if (g == NULL) {
        return NULL;
    }
    g->width = width;
    g->height = height;
    return g;
}

void gui_shell_free(gui_shell *g)
{
    if (g == NULL) {
        return;
    }
    free(g->items);
    free(g);
}

int gui_shell_set_menu(gui_shell *g, const gui_menu_item *items,
                       size_t count)
{
    if (g == NULL || (items == NULL && count > 0)) {
        return -1;
    }
    gui_menu_item *ni = NULL;
    if (count > 0) {
        ni = malloc(count * sizeof(gui_menu_item));
        if (ni == NULL) {
            return -1;
        }
        memcpy(ni, items, count * sizeof(gui_menu_item));
    }
    free(g->items);
    g->items = ni;
    g->count = count;
    if (g->selected >= count) {
        g->selected = 0;
    }
    return 0;
}

void gui_shell_navigate(gui_shell *g, int dir)
{
    if (g == NULL || g->count == 0) {
        return;
    }
    if (dir < 0) {
        g->selected = (g->selected == 0) ? g->count - 1 : g->selected - 1;
    } else {
        g->selected = (g->selected + 1) % g->count;
    }
}

size_t gui_shell_selected(const gui_shell *g)
{
    return g ? g->selected : 0;
}

gui_key gui_shell_handle_button(gui_shell *g, uint8_t state)
{
    if (g == NULL) {
        return GUI_KEY_NONE;
    }
    uint8_t pressed = (uint8_t)(state & ~g->last_button & 0x0F);
    g->last_button = (uint8_t)(state & 0x0F);
    if (pressed & GUI_KEY_UP) {
        gui_shell_navigate(g, -1);
        return GUI_KEY_UP;
    }
    if (pressed & GUI_KEY_DOWN) {
        gui_shell_navigate(g, 1);
        return GUI_KEY_DOWN;
    }
    if (pressed & GUI_KEY_SELECT) {
        return GUI_KEY_SELECT;
    }
    if (pressed & GUI_KEY_SHUTDOWN) {
        return GUI_KEY_SHUTDOWN;
    }
    return GUI_KEY_NONE;
}

int gui_shell_render(gui_shell *g, uint8_t *packed_out)
{
    if (g == NULL || packed_out == NULL || g->count == 0) {
        return -1;
    }
    canvas *c = canvas_create(g->width, g->height);
    if (c == NULL) {
        return -1;
    }
    /* Title */
    canvas_draw_text(c, "DISTILLER", 8, 8, 2);
    /* Menu items: selected = inverted block */
    const size_t y0 = 40;
    const size_t line_h = 22;
    for (size_t i = 0; i < g->count; i++) {
        size_t y = y0 + i * line_h;
        if (i == g->selected) {
            canvas_fill_rect(c, 4, y - 2, g->width - 8, line_h - 4);
        }
        char label[56];
        snprintf(label, sizeof(label), " %s", g->items[i].label);
        canvas_draw_text(c, label, 10, y, 2);
    }
    uint8_t *data = canvas_data(c);
    /* invert the selected row pixels: canvas_fill_rect made them
     * black; text on it is black too — for a selected row, flip
     * the whole band so text reads white on black. */
    if (g->selected < g->count) {
        size_t y = y0 + g->selected * line_h;
        for (size_t yy = y - 2; yy < y - 2 + line_h - 4 && yy < g->height; yy++) {
            for (size_t xx = 4; xx < g->width - 4; xx++) {
                data[yy * g->width + xx] = (uint8_t)(255 - data[yy * g->width + xx]);
            }
        }
    }
    /* dither + pack */
    dither_ctx *d = dither_create(g->width, g->height);
    if (d == NULL) {
        canvas_free(c);
        return -1;
    }
    uint8_t *raw = malloc(g->width * g->height);
    if (raw == NULL) {
        dither_free(d);
        canvas_free(c);
        return -1;
    }
    dither_bayer8(d, data, raw);
    pack_ctx *p = pack_create(g->width, g->height);
    if (p == NULL) {
        free(raw);
        dither_free(d);
        canvas_free(c);
        return -1;
    }
    pack_bits(p, raw, packed_out);
    pack_free(p);
    free(raw);
    dither_free(d);
    canvas_free(c);
    return 0;
}
