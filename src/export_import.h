/**
 * Export/Import Header File
 */

#ifndef EXPORT_IMPORT_H
#define EXPORT_IMPORT_H
#include "config.h"

int export_to_json(const char* db_path, const char* export_path);
int import_from_json(const char* json_path, const char* db_path);
int export_to_csv(const char* db_path, const char* export_path, const char* table);

#endif
