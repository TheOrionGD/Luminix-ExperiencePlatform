#include "json_io.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int db_save_to_file(Database* db, const char* filename) {
    if (!db || !filename) return ERROR;
    
    FILE* file = fopen(filename, "w");
    if (!file) return ERROR;
    
    fprintf(file, "{\n");
    fprintf(file, "  \"tables\": [\n");
    
    for (int t = 0; t < db->table_count; t++) {
        Table* table = &db->tables[t];
        
        fprintf(file, "    {\n");
        fprintf(file, "      \"name\": \"%s\",\n", table->name);
        fprintf(file, "      \"fields\": [\n");
        
        for (int f = 0; f < table->field_count; f++) {
            fprintf(file, "        {\"name\": \"%s\", \"type\": \"", table->field_names[f]);
            
            switch (table->field_types[f]) {
                case TYPE_INT: fprintf(file, "int"); break;
                case TYPE_STRING: fprintf(file, "string"); break;
                case TYPE_FLOAT: fprintf(file, "float"); break;
                case TYPE_BOOL: fprintf(file, "bool"); break;
            }
            
            if (f == table->field_count - 1) {
                fprintf(file, "\"}\n");
            } else {
                fprintf(file, "\"},\n");
            }
        }
        
        fprintf(file, "      ],\n");
        fprintf(file, "      \"records\": [\n");
        
        int record_count = 0;
        for (Record* curr = table->records; curr != NULL; curr = curr->next) {
            fprintf(file, "        {\n");
            fprintf(file, "          \"id\": %d,\n", curr->id);
            fprintf(file, "          \"values\": [\n");
            
            for (int f = 0; f < table->field_count; f++) {
                fprintf(file, "            ");
                
                switch (curr->fields[f].type) {
                    case TYPE_INT:
                        fprintf(file, "%d", curr->fields[f].value.int_value);
                        break;
                    case TYPE_STRING:
                        fprintf(file, "\"%s\"", curr->fields[f].value.string_value);
                        break;
                    case TYPE_FLOAT:
                        fprintf(file, "%.2f", curr->fields[f].value.float_value);
                        break;
                    case TYPE_BOOL:
                        fprintf(file, "%s", curr->fields[f].value.bool_value ? "true" : "false");
                        break;
                }
                
                if (f == table->field_count - 1) {
                    fprintf(file, "\n");
                } else {
                    fprintf(file, ",\n");
                }
            }
            
            fprintf(file, "          ]\n");
            
            record_count++;
            if (record_count < table->record_count) {
                fprintf(file, "        },\n");
            } else {
                fprintf(file, "        }\n");
            }
        }
        
        fprintf(file, "      ]\n");
        
        if (t == db->table_count - 1) {
            fprintf(file, "    }\n");
        } else {
            fprintf(file, "    },\n");
        }
    }
    
    fprintf(file, "  ]\n");
    fprintf(file, "}\n");
    
    fclose(file);
    return SUCCESS;
}

Database* db_load_from_file(const char* filename) {
    // Simplified loader - in production, you'd want to use cJSON
    // For now, we'll create a fresh database
    Database* db = db_create();
    if (!db) return NULL;
    
    // Check if file exists
    FILE* file = fopen(filename, "r");
    if (!file) {
        return db; // Return empty database if file doesn't exist
    }
    
    fclose(file);
    
    // TODO: Implement proper JSON parsing with cJSON
    // For now, we'll just return an empty database
    
    return db;
}