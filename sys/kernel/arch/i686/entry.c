/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Masix Kernel
 * Copyright (C) 2026, FelixProfi. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 */
#include <stdint.h>

#define MULTIBOOT2_MAGIC 0xE85250D6
#define MULTIBOOT2_HEADER_LEN 48

__attribute__((section(".multiboot2"), used, aligned(8)))
const uint32_t multiboot2_header[] = {
    MULTIBOOT2_MAGIC,
    0,
    MULTIBOOT2_HEADER_LEN,
    -(MULTIBOOT2_MAGIC + 0 + MULTIBOOT2_HEADER_LEN),

    /* Тег заголовка буфера кадров */
    5, 20,
    0, 0, 0,

    /* framebuffer tag is 20 bytes,
     *      so add 4 bytes to reach 8-byte alignment */
    0,

    /* Конец тега */
    0, 8
};

extern void kernel_main(uint32_t magic, uint32_t addr);

__attribute__((naked))
void _start(void)
{
    __asm__ __volatile__(
        "pushl %ebx \n\t"
        "pushl %eax \n\t"

        "call kernel_main \n\t"

        "1: \n\t"
        "hlt \n\t"
        "jmp 1b \n\t"
    );
}
