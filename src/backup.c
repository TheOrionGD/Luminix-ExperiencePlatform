#include "backup.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <io.h>
#else
#include <dirent.h>
#endif

static int copy_file(const char* src_path, const char* dest_path) {
    FILE* src = fopen(src_path, "rb");
    if (!src) return -1;
    FILE* dest = fopen(dest_path, "wb");
    if (!dest) {
        fclose(src);
        return -1;
    }
    char buffer[4096];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        fwrite(buffer, 1, bytes, dest);
    }
    fclose(src);
    fclose(dest);
    return 0;
}

int backup_create(const char* source_path, const char* backup_dir) {
    if (!source_path || !backup_dir) return -1;
    char backup_path[512];
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    
    snprintf(backup_path, sizeof(backup_path), 
             "%s/backup_%04d%02d%02d_%02d%02d%02d.db",
             backup_dir,
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec);
             
    int res = copy_file(source_path, backup_path);
    if (res == 0) {
        printf("Backup created: %s\n", backup_path);
        return 0;
    }
    printf("Failed to create backup\n");
    return -1;
}

int backup_restore(const char* backup_path, const char* target_path) {
    if (!backup_path || !target_path) return -1;
    int res = copy_file(backup_path, target_path);
    if (res == 0) {
        printf("Restored from: %s to: %s\n", backup_path, target_path);
        return 0;
    }
    printf("Failed to restore backup\n");
    return -1;
}

void backup_list(const char* backup_dir) {
    if (!backup_dir) return;
    printf("Listing backups in: %s\n", backup_dir);
#ifdef _WIN32
    char search_path[512];
    snprintf(search_path, sizeof(search_path), "%s/backup_*.db", backup_dir);
    struct _finddata_t file_info;
    intptr_t handle = _findfirst(search_path, &file_info);
    if (handle == -1) {
        printf("  No backups found.\n");
        return;
    }
    do {
        printf("  - %s (%ld bytes)\n", file_info.name, (long)file_info.size);
    } while (_findnext(handle, &file_info) == 0);
    _findclose(handle);
#else
    DIR* dir = opendir(backup_dir);
    if (!dir) {
        printf("  Cannot open directory.\n");
        return;
    }
    struct dirent* entry;
    int found = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "backup_", 7) == 0 && strstr(entry->d_name, ".db")) {
            printf("  - %s\n", entry->d_name);
            found = 1;
        }
    }
    if (!found) {
        printf("  No backups found.\n");
    }
    closedir(dir);
#endif
}
