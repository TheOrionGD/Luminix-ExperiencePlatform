#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void print_record(Record* record, Table* table) {
    if (!record || !table) return;
    
    printf("Record ID: %d\n", record->id);
    printf("Fields:\n");
    for (int i = 0; i < table->field_count; i++) {
        printf("  %s: ", table->field_names[i]);
        
        switch (record->fields[i].type) {
            case TYPE_INT:
                printf("%d", record->fields[i].value.int_value);
                break;
            case TYPE_STRING:
                printf("%s", record->fields[i].value.string_value);
                break;
            case TYPE_FLOAT:
                printf("%.2f", record->fields[i].value.float_value);
                break;
            case TYPE_BOOL:
                printf("%s", record->fields[i].value.bool_value ? "true" : "false");
                break;
        }
        printf("\n");
    }
    printf("\n");
}

void print_table(Table* table) {
    if (!table) return;
    
    printf("\n=== Table: %s ===\n", table->name);
    printf("Fields: ");
    for (int i = 0; i < table->field_count; i++) {
        printf("%s", table->field_names[i]);
        if (i < table->field_count - 1) printf(", ");
    }
    printf("\n");
    printf("Records: %d\n\n", table->record_count);
    
    int count = 0;
    for (Record* curr = table->records; curr != NULL; curr = curr->next) {
        printf("Record %d:\n", ++count);
        print_record(curr, table);
    }
    
    if (count == 0) {
        printf("No records found.\n");
    }
}

Field create_int_field(const char* name, int value) {
    Field field;
    strncpy(field.name, name, MAX_FIELD_LEN);
    field.name[MAX_FIELD_LEN - 1] = '\0';
    field.type = TYPE_INT;
    field.value.int_value = value;
    return field;
}

Field create_string_field(const char* name, const char* value) {
    Field field;
    strncpy(field.name, name, MAX_FIELD_LEN);
    field.name[MAX_FIELD_LEN - 1] = '\0';
    field.type = TYPE_STRING;
    strncpy(field.value.string_value, value, MAX_FIELD_LEN);
    field.value.string_value[MAX_FIELD_LEN - 1] = '\0';
    return field;
}

Field create_float_field(const char* name, float value) {
    Field field;
    strncpy(field.name, name, MAX_FIELD_LEN);
    field.name[MAX_FIELD_LEN - 1] = '\0';
    field.type = TYPE_FLOAT;
    field.value.float_value = value;
    return field;
}

Field create_bool_field(const char* name, bool value) {
    Field field;
    strncpy(field.name, name, MAX_FIELD_LEN);
    field.name[MAX_FIELD_LEN - 1] = '\0';
    field.type = TYPE_BOOL;
    field.value.bool_value = value;
    return field;
}

void trim_string(char* str) {
    if (!str) return;
    
    char* end;
    
    // Trim leading space
    while(isspace((unsigned char)*str)) str++;
    
    if(*str == 0) return;
    
    // Trim trailing space
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    
    // Write new null terminator
    *(end + 1) = 0;
}

char** split_string(const char* str, const char* delimiter, int* count) {
    if (!str || !delimiter || !count) return NULL;
    
    char* copy = strdup(str);
    if (!copy) return NULL;
    
    // Count tokens
    *count = 1;
    char* ptr = copy;
    while ((ptr = strstr(ptr, delimiter)) != NULL) {
        (*count)++;
        ptr += strlen(delimiter);
    }
    
    // Allocate array
    char** tokens = (char**)malloc(*count * sizeof(char*));
    if (!tokens) {
        free(copy);
        return NULL;
    }
    
    // Split
    int i = 0;
    char* token = strtok(copy, delimiter);
    while (token != NULL) {
        trim_string(token);
        tokens[i] = strdup(token);
        if (!tokens[i]) {
            // Cleanup on error
            for (int j = 0; j < i; j++) free(tokens[j]);
            free(tokens);
            free(copy);
            return NULL;
        }
        i++;
        token = strtok(NULL, delimiter);
    }
    
    free(copy);
    return tokens;
}