
/**
 * Storage Header File
 */

#ifndef STORAGE_H
#define STORAGE_H
#include "config.h"

#include <stddef.h>
#include <sys/types.h>

typedef struct {
    char* path;
    int fd;
    size_t size;
} Storage;

int storage_init(Storage* storage, const char* path);
int storage_read(Storage* storage, void* buffer, size_t size, off_t offset);
int storage_write(Storage* storage, const void* data, size_t size, off_t offset);
void storage_close(Storage* storage);

#endif
