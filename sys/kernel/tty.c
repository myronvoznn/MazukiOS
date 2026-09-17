/* SPDX-License-Identifier: GPL-2.0-only */
#include <stdint.h>
#include <framebuffer.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

extern uint16_t cursor_x;
extern uint16_t cursor_y;
extern void vga_raw_putc(char c, uint8_t color);
extern void vga_clear(uint8_t color);
extern void vga_scroll(uint8_t color);

static uint32_t tty_x;
static uint32_t tty_y;
static uint8_t tty_color = 0x0F;
static int ansi_state;
static int ansi_arg1;
static int ansi_arg2;
static int* ansi_current_arg;
static const uint8_t ansi_to_vga[] = {0,4,2,6,1,5,3,7,8,12,10,14,9,13,11};

static uint32_t tty_width(void) { return framebuffer_available() ? framebuffer_columns() : VGA_WIDTH; }
static uint32_t tty_height(void) { return framebuffer_available() ? framebuffer_rows() : VGA_HEIGHT; }
static void tty_sync(void) { cursor_x = tty_x; cursor_y = tty_y; }
static void tty_scroll(void) { if (framebuffer_available()) framebuffer_scroll(tty_color); else vga_scroll(tty_color); tty_y = tty_height() - 1; }

static void tty_newline(void) {
    tty_x = 0;
    tty_y++;
    if (tty_y >= tty_height()) tty_scroll();
    tty_sync();
}

static int parse_ansi(char c) {
    if (ansi_state == 0) {
        if (c != '\033') return 0;
        ansi_state = 1;
        ansi_arg1 = 0;
        ansi_arg2 = 0;
        ansi_current_arg = &ansi_arg1;
        return 1;
    }
    if (ansi_state == 1) {
        ansi_state = c == '[' ? 2 : 0;
        return 1;
    }
    if (ansi_state == 2) {
        if (c >= '0' && c <= '9') {
            *ansi_current_arg = *ansi_current_arg * 10 + c - '0';
            return 1;
        }
        if (c == ';') {
            ansi_current_arg = &ansi_arg2;
            return 1;
        }
        if (c == 'J' && (ansi_arg1 == 0 || ansi_arg1 == 2)) {
            if (framebuffer_available()) framebuffer_clear(tty_color);
            else vga_clear(tty_color);
            tty_x = 0;
            tty_y = 0;
        } else if (c == 'H' || c == 'f') {
            tty_x = 0;
            tty_y = 0;
        } else if (c == 'm') {
            if (ansi_arg1 == 0) tty_color = 0x0F;
            else if (ansi_arg1 >= 30 && ansi_arg1 <= 37) tty_color = (tty_color & 0xF0) | ansi_to_vga[ansi_arg1 - 30];
            else if (ansi_arg1 >= 90 && ansi_arg1 <= 97) tty_color = (tty_color & 0xF0) | ansi_to_vga[ansi_arg1 - 90];
            if (ansi_arg2 >= 30 && ansi_arg2 <= 37) tty_color = (tty_color & 0xF0) | ansi_to_vga[ansi_arg2 - 30];
        }
        ansi_state = 0;
        tty_sync();
        return 1;
    }
    ansi_state = 0;
    return 1;
}

void tty_backspace(void) {
    if (tty_x > 0) tty_x--;
    else if (tty_y > 0) {
        tty_y--;
        tty_x = tty_width() - 1;
    }
    if (framebuffer_available()) framebuffer_erase_char(tty_x, tty_y, tty_color);
    else {
        tty_sync();
        vga_raw_putc(' ', tty_color);
        cursor_x = tty_x;
        cursor_y = tty_y;
    }
    tty_sync();
}

void tty_write_char(char c) {
    if (parse_ansi(c)) return;
    if (c == '\n') {
        tty_newline();
        return;
    }
    if (c == '\r') {
        tty_x = 0;
        tty_sync();
        return;
    }
    if (c == '\t') {
        do tty_write_char(' '); while (tty_x && (tty_x & 7));
        return;
    }
    if (c == '\b') {
        tty_backspace();
        return;
    }
    if (framebuffer_available()) framebuffer_draw_char(tty_x, tty_y, c, tty_color);
    else {
        tty_sync();
        vga_raw_putc(c, tty_color);
    }
    tty_x++;
    if (tty_x >= tty_width()) tty_newline();
    else tty_sync();
}
