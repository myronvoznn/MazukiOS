/* SPDX-License-Identifier: GPL-2.0-only */
#include <vfs.h>
#include <string.h>

enum {
    DEV_NULL,
    DEV_ZERO,
    DEV_TTY,
    DEV_CONSOLE
};

static int32_t dev_null_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset; (void)size; (void)buffer;
    return 0;
}

static int32_t dev_null_write(vfs_node_t *node, uint32_t offset, uint32_t size, const uint8_t *buffer) {
    (void)node; (void)offset; (void)buffer;
    return size;
}

static int32_t dev_zero_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset;
    for (uint32_t i = 0; i < size; i++) buffer[i] = 0;
    return size;
}

static int32_t dev_tty_write(vfs_node_t *node, uint32_t offset, uint32_t size, const uint8_t *buffer) {
    (void)node; (void)offset;
    extern void tty_write_char(char c);
    for (uint32_t i = 0; i < size; i++) tty_write_char((char)buffer[i]);
    return size;
}

int32_t devtmpfs_open(const char *subpath, vfs_node_t *node) {
    node->flags = VFS_FILE;
    node->size = 0;
    node->read = dev_null_read;
    node->write = dev_null_write;
    if (strcmp(subpath, "null") == 0) { strcpy(node->name, "null"); node->internal_id = DEV_NULL; return 0; }
    if (strcmp(subpath, "zero") == 0) { strcpy(node->name, "zero"); node->internal_id = DEV_ZERO; node->read = dev_zero_read; return 0; }
    if (strcmp(subpath, "tty") == 0) { strcpy(node->name, "tty"); node->internal_id = DEV_TTY; node->write = dev_tty_write; return 0; }
    if (strcmp(subpath, "console") == 0) { strcpy(node->name, "console"); node->internal_id = DEV_CONSOLE; node->write = dev_tty_write; return 0; }
    return -2;
}

fsdriver_t devtmpfs_driver = {
    .open = devtmpfs_open
};