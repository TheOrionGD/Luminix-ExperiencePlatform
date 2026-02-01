
/**
 * Backup Header File
 */

#ifndef BACKUP_H
#define BACKUP_H

int backup_create(const char* source_path, const char* backup_dir);
int backup_restore(const char* backup_path, const char* target_path);
void backup_list(const char* backup_dir);

#endif
