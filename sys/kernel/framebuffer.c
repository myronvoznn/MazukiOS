/* SPDX-License-Identifier: GPL-2.0-only */
#include <framebuffer.h>

#define FONT_WIDTH 8
#define FONT_HEIGHT 8

static volatile uint8_t* framebuffer;
static uint32_t framebuffer_pitch;
static uint32_t framebuffer_width;
static uint32_t framebuffer_height;
static uint8_t framebuffer_bpp;
static uint8_t framebuffer_type;
static uint8_t red_position, green_position, blue_position;
static uint8_t red_mask, green_mask, blue_mask;

static const uint8_t font_letters[26][7] = {
    {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30}, {14,17,16,16,16,17,14},
    {30,17,17,17,17,17,30}, {31,16,16,30,16,16,31}, {31,16,16,30,16,16,16},
    {14,17,16,23,17,17,15}, {17,17,17,31,17,17,17}, {14,4,4,4,4,4,14},
    {1,1,1,1,17,17,14}, {17,18,20,24,20,18,17}, {16,16,16,16,16,16,31},
    {17,27,21,21,17,17,17}, {17,25,21,19,17,17,17}, {14,17,17,17,17,17,14},
    {30,17,17,30,16,16,16}, {14,17,17,17,21,18,13}, {30,17,17,30,20,18,17},
    {15,16,16,14,1,1,30}, {31,4,4,4,4,4,4}, {17,17,17,17,17,17,14},
    {17,17,17,17,17,10,4}, {17,17,17,21,21,21,10}, {17,17,10,4,10,17,17},
    {17,17,10,4,4,4,4}, {31,1,2,4,8,16,31}
};

static const uint8_t font_digits[10][7] = {
    {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14}, {14,17,1,2,4,8,31},
    {30,1,1,14,1,1,30}, {2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
    {14,16,16,30,17,17,14}, {31,1,2,4,8,8,8}, {14,17,17,14,17,17,14},
    {14,17,17,15,1,1,14}
};

static uint32_t color_value(uint8_t color) {
    static const uint8_t palette[16][3] = {
        {0,0,0}, {0,0,170}, {0,170,0}, {0,170,170}, {170,0,0}, {170,0,170},
        {170,85,0}, {170,170,170}, {85,85,85}, {85,85,255}, {85,255,85},
        {85,255,255}, {255,85,85}, {255,85,255}, {255,255,85}, {255,255,255}
    };
    uint8_t r = palette[color & 15][0], g = palette[color & 15][1], b = palette[color & 15][2];
    if (framebuffer_bpp == 15 || framebuffer_bpp == 16)
        return ((r >> (8 - red_mask)) << red_position) | ((g >> (8 - green_mask)) << green_position) | ((b >> (8 - blue_mask)) << blue_position);
    return ((uint32_t)r << red_position) | ((uint32_t)g << green_position) | ((uint32_t)b << blue_position);
}

static void put_pixel(uint32_t x, uint32_t y, uint32_t value) {
    volatile uint8_t* pixel = framebuffer + y * framebuffer_pitch + x * ((framebuffer_bpp + 7) / 8);
    if (framebuffer_bpp == 32) *(volatile uint32_t*)pixel = value;
    else if (framebuffer_bpp == 24) { pixel[0] = value; pixel[1] = value >> 8; pixel[2] = value >> 16; }
    else *(volatile uint16_t*)pixel = (uint16_t)value;
}

void framebuffer_init(const struct multiboot_tag_framebuffer* tag) {
    if (!tag || tag->framebuffer_type != 1 || tag->framebuffer_width < FONT_WIDTH || tag->framebuffer_height < FONT_HEIGHT) return;
    if (tag->framebuffer_bpp != 15 && tag->framebuffer_bpp != 16 && tag->framebuffer_bpp != 24 && tag->framebuffer_bpp != 32) return;
    framebuffer = (volatile uint8_t*)(uint32_t)tag->framebuffer_addr;
    framebuffer_pitch = tag->framebuffer_pitch; framebuffer_width = tag->framebuffer_width; framebuffer_height = tag->framebuffer_height;
    framebuffer_bpp = tag->framebuffer_bpp; framebuffer_type = tag->framebuffer_type;
    red_position = tag->red_field_position; red_mask = tag->red_mask_size;
    green_position = tag->green_field_position; green_mask = tag->green_mask_size;
    blue_position = tag->blue_field_position; blue_mask = tag->blue_mask_size;
}

int framebuffer_available(void) { return framebuffer != 0 && framebuffer_type == 1; }
uint32_t framebuffer_columns(void) { return framebuffer_width / FONT_WIDTH; }
uint32_t framebuffer_rows(void) { return framebuffer_height / FONT_HEIGHT; }

void framebuffer_clear(uint8_t color) {
    if (!framebuffer_available()) return;
    uint32_t value = color_value(color >> 4);
    for (uint32_t y = 0; y < framebuffer_height; y++) for (uint32_t x = 0; x < framebuffer_width; x++) put_pixel(x, y, value);
}

void framebuffer_scroll(uint8_t color) {
    if (!framebuffer_available()) return;
    uint32_t bytes = (framebuffer_bpp + 7) / 8;
    for (uint32_t y = FONT_HEIGHT; y < framebuffer_height; y++) for (uint32_t x = 0; x < framebuffer_width * bytes; x++) framebuffer[(y - FONT_HEIGHT) * framebuffer_pitch + x] = framebuffer[y * framebuffer_pitch + x];
    uint32_t value = color_value(color >> 4);
    for (uint32_t y = framebuffer_height - FONT_HEIGHT; y < framebuffer_height; y++) for (uint32_t x = 0; x < framebuffer_width; x++) put_pixel(x, y, value);
}

void framebuffer_draw_char(uint32_t column, uint32_t row, char c, uint8_t color) {
    if (!framebuffer_available() || column >= framebuffer_columns() || row >= framebuffer_rows()) return;
    uint8_t glyph[7] = {0,0,0,0,0,0,0};
    if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
    if (c >= 'A' && c <= 'Z') for (int i = 0; i < 7; i++) glyph[i] = font_letters[c - 'A'][i];
    else if (c >= '0' && c <= '9') for (int i = 0; i < 7; i++) glyph[i] = font_digits[c - '0'][i];
    else if (c == '-') glyph[3] = glyph[4] = 31;
    else if (c == '_') glyph[6] = 31;
    else if (c == '.') glyph[6] = 4;
    else if (c == ':') glyph[2] = glyph[5] = 4;
    else if (c == '/') { glyph[1] = 2; glyph[2] = 2; glyph[3] = 4; glyph[4] = 8; glyph[5] = 16; }
    uint32_t foreground = color_value(color & 15), background = color_value((color >> 4) & 15);
    for (uint32_t y = 0; y < FONT_HEIGHT; y++) for (uint32_t x = 0; x < FONT_WIDTH; x++)
        put_pixel(column * FONT_WIDTH + x, row * FONT_HEIGHT + y, y < 7 && x > 0 && x < 6 && (glyph[y] & (1 << (5 - x))) ? foreground : background);
}

void framebuffer_erase_char(uint32_t column, uint32_t row, uint8_t color) { framebuffer_draw_char(column, row, ' ', color); }