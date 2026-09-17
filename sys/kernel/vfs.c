/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Masix Kernel
 * Copyright (C) 2026, FelixProfi. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 */
#include <vfs.h>
#include <string.h>
#include <version.h>
#include <procfs.h>

#define MAX_FD 32
#define PIPE_COUNT 8
#define PIPE_SIZE 4096

// vfs_node_t* fd_table[32];
file_t fd_table[32];

extern int fs_create(const char* name);
extern int fs_read(int file_index, unsigned char* buffer, unsigned int size);
extern int fs_write(int file_index, const unsigned char* data, unsigned int size);
typedef struct { char name[32]; unsigned int size; int used; } FileEntry;
extern FileEntry file_table[];

static mountpoint_t *mount_list = NULL;

typedef struct {
    uint8_t data[PIPE_SIZE];
    uint32_t read_pos;
    uint32_t write_pos;
    uint32_t count;
    uint32_t readers;
    uint32_t writers;
    int used;
} pipe_t;

static pipe_t pipes[PIPE_COUNT];

static int32_t allocate_fd(file_type_t type, void *private_data) {
    for (int i = 3; i < MAX_FD; i++) {
        if (fd_table[i].type == FT_EMPTY) {
            fd_table[i].type = type;
            fd_table[i].offset = 0;
            fd_table[i].private_data = private_data;
            return i;
        }
    }
    return -24;
}

static int32_t pipe_read(pipe_t *pipe, uint8_t *buffer, uint32_t size) {
    uint32_t read = 0;
    while (read < size && pipe->count > 0) {
        buffer[read++] = pipe->data[pipe->read_pos];
        pipe->read_pos = (pipe->read_pos + 1) % PIPE_SIZE;
        pipe->count--;
    }
    return read;
}

static int32_t pipe_write(pipe_t *pipe, const uint8_t *buffer, uint32_t size) {
    if (pipe->readers == 0) return -32;
    uint32_t written = 0;
    while (written < size && pipe->count < PIPE_SIZE) {
        pipe->data[pipe->write_pos] = buffer[written++];
        pipe->write_pos = (pipe->write_pos + 1) % PIPE_SIZE;
        pipe->count++;
    }
    return written;
}

static int32_t ramfs_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)offset;
    return fs_read(node->internal_id, buffer, size);
}

static int32_t ramfs_write(vfs_node_t *node, uint32_t offset, uint32_t size, const uint8_t *buffer) {
    (void)offset;
    int32_t ret = fs_write(node->internal_id, buffer, size);
    node->size = file_table[node->internal_id].size;
    return ret;
}

void vfs_mount_core(const char *path, fsdriver_t *driver) {
    /* Монтаж корня (/) и аллокация памяти к нему */
    extern void* malloc(size_t size);
    mountpoint_t *mp = (mountpoint_t*)malloc(sizeof(mountpoint_t));
    if (!mp) return;

    strncpy(mp->path, path, 32);
    mp->driver = driver;
    mp->next = mount_list;
    mount_list = mp;
}

// void vfs_init(void) {
//     /* Инициализация VFS и монтирование procfs */
//     extern fsdriver_t procfs_driver;
//     vfs_mount_core("/proc", &procfs_driver);
// }
void vfs_init(void) {
    for (int i = 0; i < 32; i++) {
        fd_table[i].type = FT_EMPTY;
        fd_table[i].offset = 0;
        fd_table[i].private_data = NULL;
    }

    fd_table[0].type = FT_DEVICE; // stdin
    fd_table[1].type = FT_DEVICE; // stdout
    fd_table[2].type = FT_DEVICE; // stderr

    // монтирование procfs
    extern fsdriver_t procfs_driver;
    vfs_mount_core("/proc", &procfs_driver);
    extern fsdriver_t devtmpfs_driver;
    vfs_mount_core("/dev", &devtmpfs_driver);
}

int32_t vfs_open(const char *path) {
    int fd = -1;
    for (int i = 3; i < MAX_FD; i++) {
        if (fd_table[i].type == FT_EMPTY) { fd = i; break; }
    }
    if (fd == -1) return -24; // -EMFILE

    extern void* malloc(size_t size);
    vfs_node_t* node = (vfs_node_t*)malloc(sizeof(vfs_node_t));
    if (!node) return -12; // -ENOMEM
    /* Склеиваем путь к файлу с точки монтирования...
     * Освобождаем используемую память после
     * strncmp гарантирует (около 52%) что все пройдет успешно
     */
    for (mountpoint_t *mp = mount_list; mp != NULL; mp = mp->next) {
        size_t len = strlen(mp->path);
        if (strncmp(path, mp->path, len) == 0) {
            if (path[len] == '/' || path[len] == '\0') {
                const char *subpath = path + len;
                if (*subpath == '/') subpath++;

                int32_t res = mp->driver->open(subpath, node);
                if (res == 0) {
                    fd_table[fd].type = FT_VFS_FILE;
                    fd_table[fd].offset = 0;
                    fd_table[fd].private_data = (void*)node;

                    return fd;
                }

                extern void free(void* ptr);
                free(node);
                return res;
            }
        }
    }

    int file_idx = -1;
    for (int i = 0; i < 64; i++) {
        if (file_table[i].used && strcmp(file_table[i].name, path) == 0) {
            file_idx = i;
            break;
        }
    }

    if (file_idx == -1) {
        file_idx = fs_create(path);
        if (file_idx == -1) {
            extern void free(void* ptr);
            free(node);
            return -28;
        } // -ENOSPC
    }

    strcpy(node->name, file_table[file_idx].name);
    node->flags = VFS_FILE;
    node->size = file_table[file_idx].size;
    node->internal_id = file_idx;
    node->read = ramfs_read;
    node->write = ramfs_write;

    fd_table[fd].type = FT_VFS_FILE;
    fd_table[fd].offset = 0;
    fd_table[fd].private_data = (void*)node;

    return fd;
}

int32_t vfs_read(int fd, void *buf, uint32_t count) {
    if (fd < 0 || fd >= MAX_FD || fd_table[fd].type == FT_EMPTY) return -9;
    if (fd_table[fd].type == FT_PIPE_READ) return pipe_read((pipe_t*)fd_table[fd].private_data, (uint8_t*)buf, count);
    if (fd_table[fd].type != FT_VFS_FILE) return -9;
    vfs_node_t *node = (vfs_node_t*)fd_table[fd].private_data;
    if (!node || !node->read) return -9;
    int32_t result = node->read(node, fd_table[fd].offset, count, (uint8_t*)buf);
    if (result > 0) fd_table[fd].offset += result;
    return result;
}

int32_t vfs_write(int fd, const void *buf, uint32_t count) {
    if (fd < 0 || fd >= MAX_FD || fd_table[fd].type == FT_EMPTY) return -9;
    if (fd_table[fd].type == FT_PIPE_WRITE) return pipe_write((pipe_t*)fd_table[fd].private_data, (const uint8_t*)buf, count);
    if (fd_table[fd].type != FT_VFS_FILE) return -9;
    vfs_node_t *node = (vfs_node_t*)fd_table[fd].private_data;
    if (!node || !node->write) return -9;
    int32_t result = node->write(node, fd_table[fd].offset, count, (const uint8_t*)buf);
    if (result > 0) fd_table[fd].offset += result;
    return result;
}

int32_t vfs_close(int fd) {
    if (fd < 0 || fd >= MAX_FD || fd_table[fd].type == FT_EMPTY) return -9;
    if (fd_table[fd].type == FT_PIPE_READ || fd_table[fd].type == FT_PIPE_WRITE) {
        pipe_t *pipe = (pipe_t*)fd_table[fd].private_data;
        if (fd_table[fd].type == FT_PIPE_READ && pipe->readers > 0) pipe->readers--;
        if (fd_table[fd].type == FT_PIPE_WRITE && pipe->writers > 0) pipe->writers--;
        if (pipe->readers == 0 && pipe->writers == 0) pipe->used = 0;
    } else {
        extern void free(void *ptr);
        if (fd_table[fd].private_data) free(fd_table[fd].private_data);
    }
    fd_table[fd].type = FT_EMPTY;
    fd_table[fd].offset = 0;
    fd_table[fd].private_data = NULL;
    return 0;
}

int32_t vfs_pipe(int32_t pipefd[2]) {
    pipe_t *pipe = NULL;
    for (int i = 0; i < PIPE_COUNT; i++) {
        if (!pipes[i].used) { pipe = &pipes[i]; break; }
    }
    if (!pipe) return -24;
    int32_t read_fd = -1;
    int32_t write_fd = -1;
    pipe->read_pos = 0;
    pipe->write_pos = 0;
    pipe->count = 0;
    pipe->readers = 1;
    pipe->writers = 1;
    pipe->used = 1;
    read_fd = allocate_fd(FT_PIPE_READ, pipe);
    if (read_fd < 0) { pipe->used = 0; return read_fd; }
    write_fd = allocate_fd(FT_PIPE_WRITE, pipe);
    if (write_fd < 0) { fd_table[read_fd].type = FT_EMPTY; pipe->used = 0; return write_fd; }
    pipefd[0] = read_fd;
    pipefd[1] = write_fd;
    return 0;
}
