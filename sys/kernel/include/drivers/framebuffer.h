#pragma once

#include <stdint.h>

struct multiboot_tag_framebuffer {
    uint32_t type;
    uint32_t size;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
    uint16_t reserved;
    uint8_t red_field_position;
    uint8_t red_mask_size;
    uint8_t green_field_position;
    uint8_t green_mask_size;
    uint8_t blue_field_position;
    uint8_t blue_mask_size;
} __attribute__((packed));

void framebuffer_init(const struct multiboot_tag_framebuffer* tag);
int framebuffer_available(void);
uint32_t framebuffer_columns(void);
uint32_t framebuffer_rows(void);
void framebuffer_clear(uint8_t color);
void framebuffer_scroll(uint8_t color);
void framebuffer_draw_char(uint32_t column, uint32_t row, char c, uint8_t color);
void framebuffer_erase_char(uint32_t column, uint32_t row, uint8_t color);