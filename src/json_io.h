#ifndef JSON_IO_H
#define JSON_IO_H

#include "database.h"

int db_save_to_file(Database* db, const char* filename);
Database* db_load_from_file(const char* filename);
char* record_to_json(Record* record, Table* table);
Record* json_to_record(const char* json_str, Table* table);

#endif