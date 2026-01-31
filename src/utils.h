#ifndef UTILS_H
#define UTILS_H

#include "database.h"

void print_record(Record* record, Table* table);
void print_table(Table* table);
Field create_int_field(const char* name, int value);
Field create_string_field(const char* name, const char* value);
Field create_float_field(const char* name, float value);
Field create_bool_field(const char* name, bool value);
int parse_sql_insert(const char* query, char* table_name, Field** values, int* field_count);
void trim_string(char* str);
char** split_string(const char* str, const char* delimiter, int* count);

#endif