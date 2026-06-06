#include "storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <io.h>
#define open _open
#define read _read
#define write _write
#define close _close
#define lseek _lseek
#else
#include <unistd.h>
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

int storage_init(Storage* storage, const char* path) {
    if (!storage || !path) return -1;
    storage->path = strdup(path);
    if (!storage->path) return -1;
    
#ifdef _WIN32
    storage->fd = open(path, O_RDWR | O_CREAT | O_BINARY, S_IREAD | S_IWRITE);
#else
    storage->fd = open(path, O_RDWR | O_CREAT, 0666);
#endif

    if (storage->fd < 0) {
        free(storage->path);
        storage->path = NULL;
        return -1;
    }
    
    struct stat st;
    if (fstat(storage->fd, &st) == 0) {
        storage->size = st.st_size;
    } else {
        storage->size = 0;
    }
    
    printf("Storage initialized at: %s, size: %lu bytes\n", path, (unsigned long)storage->size);
    return 0;
}

int storage_read(Storage* storage, void* buffer, size_t size, off_t offset) {
    if (!storage || storage->fd < 0 || !buffer) return -1;
    if (lseek(storage->fd, offset, SEEK_SET) == (off_t)-1) {
        return -1;
    }
    return read(storage->fd, buffer, size);
}

int storage_write(Storage* storage, const void* data, size_t size, off_t offset) {
    if (!storage || storage->fd < 0 || !data) return -1;
    if (lseek(storage->fd, offset, SEEK_SET) == (off_t)-1) {
        return -1;
    }
    int bytes_written = write(storage->fd, data, size);
    if (bytes_written > 0) {
        if (offset + bytes_written > (off_t)storage->size) {
            storage->size = offset + bytes_written;
        }
    }
    return bytes_written;
}

void storage_close(Storage* storage) {
    if (storage) {
        if (storage->fd >= 0) {
            close(storage->fd);
            storage->fd = -1;
        }
        if (storage->path) {
            free(storage->path);
            storage->path = NULL;
        }
        storage->size = 0;
    }
}
