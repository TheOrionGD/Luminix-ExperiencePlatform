#include "export_import.h"
#include "json_io.h"
#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int export_to_json(const char* db_path, const char* export_path) {
    JsonLoadOptions load_options = {0};
    Database* db = db_load_from_file(db_path, &load_options);
    if (!db) {
        printf("Failed to load database from %s\n", db_path);
        return -1;
    }
    
    JsonSaveOptions save_options = {0};
    save_options.serialize.pretty = true;
    ErrorCode result = db_save_to_file(db, export_path, &save_options);
    
    db_close(db);
    
    if (result == SUCCESS) {
        printf("Successfully exported to JSON: %s -> %s\n", db_path, export_path);
        return 0;
    } else {
        printf("Failed to export database to %s (Error %d)\n", export_path, result);
        return -1;
    }
}

int import_from_json(const char* json_path, const char* db_path) {
    JsonLoadOptions load_options = {0};
    Database* db = db_load_from_file(json_path, &load_options);
    if (!db) {
        printf("Failed to load JSON database from %s\n", json_path);
        return -1;
    }
    
    JsonSaveOptions save_options = {0};
    save_options.serialize.compress = true;
    ErrorCode result = db_save_to_file(db, db_path, &save_options);
    
    db_close(db);
    
    if (result == SUCCESS) {
        printf("Successfully imported from JSON: %s -> %s\n", json_path, db_path);
        return 0;
    } else {
        printf("Failed to save imported database to %s (Error %d)\n", db_path, result);
        return -1;
    }
}

int export_to_csv(const char* db_path, const char* export_path, const char* table_name) {
    JsonLoadOptions load_options = {0};
    Database* db = db_load_from_file(db_path, &load_options);
    if (!db) {
        printf("Failed to load database from %s\n", db_path);
        return -1;
    }
    
    Table* table = db_get_table(db, table_name);
    if (!table) {
        printf("Table '%s' not found in database\n", table_name);
        db_close(db);
        return -1;
    }
    
    FILE* file = fopen(export_path, "w");
    if (!file) {
        printf("Failed to open export file %s\n", export_path);
        db_close(db);
        return -1;
    }
    
    // Write CSV header
    fprintf(file, "id");
    for (int i = 0; i < table->field_count; i++) {
        fprintf(file, ",%s", table->field_names[i]);
    }
    fprintf(file, "\n");
    
    // Write records
    Record* record = table->records;
    while (record) {
        fprintf(file, "%d", record->id);
        for (int i = 0; i < table->field_count; i++) {
            fprintf(file, ",");
            switch (record->fields[i].type) {
                case TYPE_INT:
                    fprintf(file, "%d", record->fields[i].value.int_value);
                    break;
                case TYPE_STRING:
                    fprintf(file, "\"%s\"", record->fields[i].value.string_value);
                    break;
                case TYPE_FLOAT:
                    fprintf(file, "%.2f", record->fields[i].value.float_value);
                    break;
                case TYPE_DOUBLE:
                    fprintf(file, "%.4f", record->fields[i].value.double_value);
                    break;
                case TYPE_BOOL:
                    fprintf(file, "%s", record->fields[i].value.bool_value ? "true" : "false");
                    break;
                default:
                    break;
            }
        }
        fprintf(file, "\n");
        record = record->next;
    }
    
    fclose(file);
    db_close(db);
    printf("Successfully exported table '%s' to CSV: %s -> %s\n", table_name, db_path, export_path);
    return 0;
}
