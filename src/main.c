#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "database.h"
#include "json_io.h"
#include "utils.h"

void print_menu() {
    printf("\n");
    printf("╔══════════════════════════════════════════╗\n");
    printf("║            LUMINIX DATABASE CLI          ║\n");
    printf("╠══════════════════════════════════════════╣\n");
    printf("║  1. Create Table                         ║\n");
    printf("║  2. Insert Record                        ║\n");
    printf("║  3. Find Record by ID                    ║\n");
    printf("║  4. Update Record                        ║\n");
    printf("║  5. Delete Record                        ║\n");
    printf("║  6. List All Records                     ║\n");
    printf("║  7. Search Records                       ║\n");
    printf("║  8. Save Database to File                ║\n");
    printf("║  9. Load Database from File              ║\n");
    printf("║ 10. Execute SQL Query                    ║\n");
    printf("║  0. Exit                                 ║\n");
    printf("╚══════════════════════════════════════════╝\n");
    printf("\nEnter your choice: ");
}

void create_table_interactive(Database* db) {
    char table_name[MAX_TABLE_NAME];
    char field_name[MAX_FIELD_LEN];
    int field_count;
    
    printf("Enter table name: ");
    scanf("%s", table_name);
    getchar(); // Consume newline
    
    printf("Enter number of fields: ");
    scanf("%d", &field_count);
    getchar();
    
    if (field_count <= 0 || field_count > 20) {
        printf("Invalid field count.\n");
        return;
    }
    
    char** field_names = (char**)malloc(field_count * sizeof(char*));
    FieldType* field_types = (FieldType*)malloc(field_count * sizeof(FieldType));
    
    for (int i = 0; i < field_count; i++) {
        printf("Field %d name: ", i + 1);
        fgets(field_name, MAX_FIELD_LEN, stdin);
        trim_string(field_name);
        field_names[i] = strdup(field_name);
        
        printf("Field type (1=int, 2=string, 3=float, 4=bool): ");
        int type;
        scanf("%d", &type);
        getchar();
        
        switch(type) {
            case 1: field_types[i] = TYPE_INT; break;
            case 2: field_types[i] = TYPE_STRING; break;
            case 3: field_types[i] = TYPE_FLOAT; break;
            case 4: field_types[i] = TYPE_BOOL; break;
            default: field_types[i] = TYPE_STRING;
        }
    }
    
    int result = db_add_table(db, table_name, (const char**)field_names, field_types, field_count);
    
    if (result == SUCCESS) {
        printf("Table '%s' created successfully!\n", table_name);
    } else if (result == DUPLICATE_KEY) {
        printf("Table '%s' already exists.\n", table_name);
    } else {
        printf("Error creating table.\n");
    }
    
    for (int i = 0; i < field_count; i++) {
        free(field_names[i]);
    }
    free(field_names);
    free(field_types);
}

void insert_record_interactive(Database* db) {
    char table_name[MAX_TABLE_NAME];
    printf("Enter table name: ");
    scanf("%s", table_name);
    getchar();
    
    Table* table = db_get_table(db, table_name);
    if (!table) {
        printf("Table '%s' not found.\n", table_name);
        return;
    }
    
    printf("Inserting into table '%s'\n", table_name);
    Field* values = (Field*)malloc(table->field_count * sizeof(Field));
    
    for (int i = 0; i < table->field_count; i++) {
        printf("Enter value for '%s' (%s): ", 
               table->field_names[i],
               table->field_types[i] == TYPE_INT ? "int" :
               table->field_types[i] == TYPE_STRING ? "string" :
               table->field_types[i] == TYPE_FLOAT ? "float" : "bool");
        
        char input[MAX_FIELD_LEN];
        fgets(input, MAX_FIELD_LEN, stdin);
        trim_string(input);
        
        strcpy(values[i].name, table->field_names[i]);
        values[i].type = table->field_types[i];
        
        switch(table->field_types[i]) {
            case TYPE_INT:
                values[i].value.int_value = atoi(input);
                break;
            case TYPE_STRING:
                strcpy(values[i].value.string_value, input);
                break;
            case TYPE_FLOAT:
                values[i].value.float_value = atof(input);
                break;
            case TYPE_BOOL:
                values[i].value.bool_value = (strcasecmp(input, "true") == 0 || 
                                             strcasecmp(input, "1") == 0 ||
                                             strcasecmp(input, "yes") == 0);
                break;
        }
    }
    
    int result = db_insert_record(db, table_name, values);
    
    if (result == SUCCESS) {
        printf("Record inserted successfully!\n");
    } else if (result == DUPLICATE_KEY) {
        printf("Error: Duplicate ID\n");
    } else {
        printf("Error inserting record.\n");
    }
    
    free(values);
}

void find_record_interactive(Database* db) {
    char table_name[MAX_TABLE_NAME];
    int id;
    
    printf("Enter table name: ");
    scanf("%s", table_name);
    printf("Enter record ID: ");
    scanf("%d", &id);
    getchar();
    
    Record* record = db_find_record(db, table_name, id);
    Table* table = db_get_table(db, table_name);
    
    if (record && table) {
        printf("\nRecord found:\n");
        print_record(record, table);
    } else {
        printf("Record not found.\n");
    }
}

void execute_sql_query(Database* db) {
    printf("Enter SQL query (or 'back' to return):\n> ");
    
    char query[MAX_QUERY_LEN];
    fgets(query, MAX_QUERY_LEN, stdin);
    trim_string(query);
    
    if (strcasecmp(query, "back") == 0) {
        return;
    }
    
    // Simple SQL parser for demo
    char* query_lower = strdup(query);
    for (int i = 0; query_lower[i]; i++) {
        query_lower[i] = tolower(query_lower[i]);
    }
    
    if (strstr(query_lower, "insert into")) {
        // Parse INSERT query
        printf("Parsing INSERT query...\n");
        // TODO: Implement full SQL parser
    } else if (strstr(query_lower, "select")) {
        printf("Parsing SELECT query...\n");
        // TODO: Implement SELECT parsing
    } else if (strstr(query_lower, "create table")) {
        printf("Parsing CREATE TABLE query...\n");
        // TODO: Implement CREATE TABLE parsing
    } else {
        printf("Unsupported query type.\n");
    }
    
    free(query_lower);
}

int main() {
    // ASCII Logo
    printf("\n");
    printf("  _                 _       _      \n");
    printf(" | |               (_)     (_)     \n");
    printf(" | |_   _ _ __ ___  _ _ __  ___  __\n");
    printf(" | | | | | '_ ` _ \\| | '_ \\| \\ \\/ /\n");
    printf(" | | |_| | | | | | | | | | | |>  < \n");
    printf(" |_|\\__,_|_| |_| |_|_|_| |_|_/_/\\_\\\n");
    printf("\n");
    printf("Enterprise-Grade In-Memory JSON Database\n");
    printf("========================================\n\n");
    
    Database* db = db_create();
    
    // Create sample table
    const char* sample_fields[] = {"id", "name", "department", "salary"};
    FieldType sample_types[] = {TYPE_INT, TYPE_STRING, TYPE_STRING, TYPE_FLOAT};
    db_add_table(db, "employees", sample_fields, sample_types, 4);
    
    int choice;
    do {
        print_menu();
        scanf("%d", &choice);
        getchar(); // Consume newline
        
        switch(choice) {
            case 1:
                create_table_interactive(db);
                break;
            case 2:
                insert_record_interactive(db);
                break;
            case 3:
                find_record_interactive(db);
                break;
            case 4:
                // Update record
                printf("Update functionality to be implemented.\n");
                break;
            case 5:
                // Delete record
                printf("Delete functionality to be implemented.\n");
                break;
            case 6:
                // List all records
                {
                    char table_name[MAX_TABLE_NAME];
                    printf("Enter table name: ");
                    scanf("%s", table_name);
                    getchar();
                    
                    Table* table = db_get_table(db, table_name);
                    if (table) {
                        print_table(table);
                    } else {
                        printf("Table not found.\n");
                    }
                }
                break;
            case 7:
                // Search records
                printf("Search functionality to be implemented.\n");
                break;
            case 8:
                // Save database
                if (db_save_to_file(db, DATABASE_FILE) == SUCCESS) {
                    printf("Database saved to %s\n", DATABASE_FILE);
                } else {
                    printf("Error saving database.\n");
                }
                break;
            case 9:
                // Load database
                printf("Load functionality to be implemented.\n");
                break;
            case 10:
                execute_sql_query(db);
                break;
            case 0:
                printf("Exiting...\n");
                break;
            default:
                printf("Invalid choice. Please try again.\n");
        }
    } while (choice != 0);
    
    // Save before exit
    db_save_to_file(db, DATABASE_FILE);
    
    // Cleanup
    db_free(db);
    
    return 0;
}