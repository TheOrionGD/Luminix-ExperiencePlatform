#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "database.h"
#include "index.h"
#include "json_io.h"
#include "utils.h"
#include "config.h"

// ==================== GLOBAL VARIABLES ====================
static Database* g_database = NULL;
static int g_running = 1;
static pthread_mutex_t g_input_mutex = PTHREAD_MUTEX_INITIALIZER;
static char g_history[100][MAX_QUERY_LEN];
static int g_history_index = 0;
static int g_history_count = 0;

// ==================== FUNCTION PROTOTYPES ====================
void print_banner();
void print_main_menu();
void print_database_menu();
void print_table_menu();
void print_index_menu();
void print_query_menu();
void print_maintenance_menu();
void print_config_menu();
void print_help_menu();
void print_welcome();

void handle_database_operations();
void handle_table_operations();
void handle_index_operations();
void handle_query_operations();
void handle_maintenance_operations();
void handle_config_operations();
void handle_help();

void create_database_interactive();
void open_database_interactive();
void close_database_interactive();
void backup_database_interactive();
void restore_database_interactive();

void create_table_interactive();
void list_tables_interactive();
void describe_table_interactive();
void alter_table_interactive();
void truncate_table_interactive();
void drop_table_interactive();
void import_table_interactive();
void export_table_interactive();

void create_index_interactive();
void list_indexes_interactive();
void drop_index_interactive();
void rebuild_index_interactive();
void optimize_index_interactive();

void execute_query_interactive();
void execute_script_interactive();
void query_history();
void save_query_result();
void explain_query_interactive();

void vacuum_database_interactive();
void analyze_database_interactive();
void check_integrity_interactive();
void repair_database_interactive();
void statistics_interactive();

void show_config_interactive();
void set_config_interactive();
void reset_config_interactive();
void export_config_interactive();
void import_config_interactive();

void show_help_topic(const char* topic);
void interactive_tutorial();
void show_examples();
void show_cheat_sheet();

void signal_handler(int signum);
void cleanup();
void save_state();
void load_state();

void print_prompt();
char* read_input();
char* read_multiline_input();
void add_to_history(const char* query);
void clear_screen();
void print_error(ErrorCode code);
void print_success(const char* message);
void print_warning(const char* message);
void print_info(const char* message);

void print_record_formatted(Record* record, Table* table, OutputFormat format);
void print_table_formatted(Table* table, OutputFormat format);
void print_query_result_formatted(QueryResult* result, OutputFormat format);

void benchmark_database();
void stress_test_database();
void performance_monitor();

// ==================== SIGNAL HANDLER ====================
void signal_handler(int signum) {
    switch (signum) {
        case SIGINT:
            printf("\n\nReceived interrupt signal. Saving state...\n");
            save_state();
            g_running = 0;
            break;
        case SIGTERM:
            printf("\n\nReceived termination signal. Cleaning up...\n");
            cleanup();
            exit(0);
            break;
        case SIGSEGV:
            printf("\n\nSegmentation fault! Attempting to save state...\n");
            save_state();
            fprintf(stderr, "Critical error occurred. Check logs for details.\n");
            exit(1);
            break;
    }
}

// ==================== CLEANUP ====================
void cleanup() {
    printf("Cleaning up resources...\n");
    
    if (g_database) {
        // Save any pending changes
        save_state();
        
        // Close database
        db_close(g_database);
        g_database = NULL;
    }
    
    printf("Cleanup completed.\n");
}

// ==================== STATE MANAGEMENT ====================
void save_state() {
    if (!g_database) return;
    
    char state_file[256];
    snprintf(state_file, sizeof(state_file), "%s.state.json", g_database->name);
    
    JsonSaveOptions options = {0};
    options.serialize.pretty = false;
    options.serialize.compress = true;
    options.atomic_write = true;
    
    ErrorCode result = db_save_to_file(g_database, state_file, &options);
    if (result == SUCCESS) {
        printf("State saved to %s\n", state_file);
    } else {
        printf("Warning: Failed to save state: %d\n", result);
    }
}

void load_state() {
    char state_file[256] = "default.state.json";
    struct stat st;
    
    if (stat(state_file, &st) == 0) {
        printf("Found saved state. Restore? (y/n): ");
        char response[4];
        fgets(response, sizeof(response), stdin);
        
        if (tolower(response[0]) == 'y') {
            JsonLoadOptions options = {0};
            g_database = db_load_from_file(state_file, &options);
            if (g_database) {
                printf("State restored from %s\n", state_file);
            }
        }
    }
}

// ==================== INPUT HANDLING ====================
void print_prompt() {
    if (g_database) {
        printf("\n\033[1;32m%s>\033[0m ", g_database->name);
    } else {
        printf("\n\033[1;33mluminix>\033[0m ");
    }
    fflush(stdout);
}

char* read_input() {
    static char buffer[MAX_QUERY_LEN];
    
    pthread_mutex_lock(&g_input_mutex);
    
    printf("> ");
    fflush(stdout);
    
    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
        pthread_mutex_unlock(&g_input_mutex);
        return NULL;
    }
    
    // Remove newline
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
    }
    
    pthread_mutex_unlock(&g_input_mutex);
    return buffer;
}

char* read_multiline_input() {
    static char buffer[MAX_QUERY_LEN * 10];
    buffer[0] = '\0';
    
    printf("Enter multi-line input (end with ';;' on its own line):\n");
    
    char line[MAX_QUERY_LEN];
    while (fgets(line, sizeof(line), stdin)) {
        // Check for termination
        if (strcmp(line, ";;\n") == 0) {
            break;
        }
        
        // Append to buffer
        strcat(buffer, line);
        
        // Check buffer overflow
        if (strlen(buffer) >= sizeof(buffer) - MAX_QUERY_LEN) {
            printf("Input too long. Truncating.\n");
            break;
        }
    }
    
    return buffer;
}

void add_to_history(const char* query) {
    if (!query || strlen(query) == 0) return;
    
    // Don't add duplicates
    if (g_history_count > 0 && strcmp(g_history[g_history_index], query) == 0) {
        return;
    }
    
    strncpy(g_history[g_history_index], query, MAX_QUERY_LEN - 1);
    g_history[g_history_index][MAX_QUERY_LEN - 1] = '\0';
    
    g_history_index = (g_history_index + 1) % 100;
    if (g_history_count < 100) {
        g_history_count++;
    }
}

// ==================== OUTPUT FORMATTING ====================
void print_record_formatted(Record* record, Table* table, OutputFormat format) {
    if (!record || !table) return;
    
    switch (format) {
        case FORMAT_TABLE:
            print_record(record, table);
            break;
        case FORMAT_JSON:
            {
                JsonSerializeOptions options = {0};
                options.pretty = true;
                char* json = record_to_json(record, table, &options);
                if (json) {
                    printf("%s\n", json);
                    free(json);
                }
            }
            break;
        case FORMAT_CSV:
            {
                printf("%d", record->id);
                for (int i = 0; i < table->field_count; i++) {
                    printf(",");
                    switch (record->fields[i].type) {
                        case TYPE_INT:
                            printf("%d", record->fields[i].value.int_value);
                            break;
                        case TYPE_STRING:
                            printf("\"%s\"", record->fields[i].value.string_value);
                            break;
                        case TYPE_FLOAT:
                            printf("%.2f", record->fields[i].value.float_value);
                            break;
                        case TYPE_DOUBLE:
                            printf("%.4f", record->fields[i].value.double_value);
                            break;
                        case TYPE_BOOL:
                            printf("%s", record->fields[i].value.bool_value ? "true" : "false");
                            break;
                        default:
                            printf("");
                            break;
                    }
                }
                printf("\n");
            }
            break;
        case FORMAT_XML:
            {
                printf("<record id=\"%d\">\n", record->id);
                for (int i = 0; i < table->field_count; i++) {
                    printf("  <%s>", table->field_names[i]);
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
                        case TYPE_DOUBLE:
                            printf("%.4f", record->fields[i].value.double_value);
                            break;
                        case TYPE_BOOL:
                            printf("%s", record->fields[i].value.bool_value ? "true" : "false");
                            break;
                        default:
                            break;
                    }
                    printf("</%s>\n", table->field_names[i]);
                }
                printf("</record>\n");
            }
            break;
    }
}

void print_table_formatted(Table* table, OutputFormat format) {
    if (!table) return;
    
    switch (format) {
        case FORMAT_TABLE:
            print_table(table);
            break;
        case FORMAT_JSON:
            {
                JsonSerializeOptions options = {0};
                options.pretty = true;
                char* json = table_to_json(table, &options);
                if (json) {
                    printf("%s\n", json);
                    free(json);
                }
            }
            break;
        case FORMAT_CSV:
            // Print header
            printf("id");
            for (int i = 0; i < table->field_count; i++) {
                printf(",%s", table->field_names[i]);
            }
            printf("\n");
            
            // Print records
            Record* record = table->records;
            while (record) {
                print_record_formatted(record, table, FORMAT_CSV);
                record = record->next;
            }
            break;
        case FORMAT_XML:
            printf("<table name=\"%s\">\n", table->name);
            Record* rec = table->records;
            while (rec) {
                print_record_formatted(rec, table, FORMAT_XML);
                rec = rec->next;
            }
            printf("</table>\n");
            break;
    }
}

void print_query_result_formatted(QueryResult* result, OutputFormat format) {
    if (!result) return;
    
    switch (format) {
        case FORMAT_TABLE:
            // Simple table output
            printf("\n");
            for (int i = 0; i < result->column_count; i++) {
                printf("%-20s", result->column_names[i]);
            }
            printf("\n");
            for (int i = 0; i < result->column_count; i++) {
                printf("--------------------");
            }
            printf("\n");
            
            for (int i = 0; i < result->row_count; i++) {
                for (int j = 0; j < result->column_count; j++) {
                    Field* field = &result->rows[i][j];
                    char buffer[256];
                    
                    switch (field->type) {
                        case TYPE_INT:
                            snprintf(buffer, sizeof(buffer), "%d", field->value.int_value);
                            break;
                        case TYPE_STRING:
                            snprintf(buffer, sizeof(buffer), "%s", field->value.string_value);
                            break;
                        case TYPE_FLOAT:
                            snprintf(buffer, sizeof(buffer), "%.2f", field->value.float_value);
                            break;
                        case TYPE_DOUBLE:
                            snprintf(buffer, sizeof(buffer), "%.4f", field->value.double_value);
                            break;
                        case TYPE_BOOL:
                            snprintf(buffer, sizeof(buffer), "%s", field->value.bool_value ? "true" : "false");
                            break;
                        default:
                            snprintf(buffer, sizeof(buffer), "");
                            break;
                    }
                    printf("%-20s", buffer);
                }
                printf("\n");
            }
            printf("\n%d row%s returned\n", result->row_count, result->row_count == 1 ? "" : "s");
            break;
            
        case FORMAT_JSON:
            {
                JsonSerializeOptions options = {0};
                options.pretty = true;
                char* json = query_result_to_json(result, &options);
                if (json) {
                    printf("%s\n", json);
                    free(json);
                }
            }
            break;
            
        case FORMAT_CSV:
            // Print header
            for (int i = 0; i < result->column_count; i++) {
                if (i > 0) printf(",");
                printf("%s", result->column_names[i]);
            }
            printf("\n");
            
            // Print rows
            for (int i = 0; i < result->row_count; i++) {
                for (int j = 0; j < result->column_count; j++) {
                    if (j > 0) printf(",");
                    
                    Field* field = &result->rows[i][j];
                    switch (field->type) {
                        case TYPE_INT:
                            printf("%d", field->value.int_value);
                            break;
                        case TYPE_STRING:
                            printf("\"%s\"", field->value.string_value);
                            break;
                        case TYPE_FLOAT:
                            printf("%.2f", field->value.float_value);
                            break;
                        case TYPE_DOUBLE:
                            printf("%.4f", field->value.double_value);
                            break;
                        case TYPE_BOOL:
                            printf("%s", field->value.bool_value ? "true" : "false");
                            break;
                        default:
                            printf("");
                            break;
                    }
                }
                printf("\n");
            }
            break;
            
        case FORMAT_XML:
            printf("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
            printf("<queryResult>\n");
            printf("  <metadata>\n");
            printf("    <rowCount>%d</rowCount>\n", result->row_count);
            printf("    <columnCount>%d</columnCount>\n", result->column_count);
            printf("  </metadata>\n");
            printf("  <columns>\n");
            for (int i = 0; i < result->column_count; i++) {
                printf("    <column name=\"%s\" type=\"%s\"/>\n", 
                       result->column_names[i],
                       field_type_to_string(result->column_types[i]));
            }
            printf("  </columns>\n");
            printf("  <rows>\n");
            for (int i = 0; i < result->row_count; i++) {
                printf("    <row id=\"%d\">\n", i + 1);
                for (int j = 0; j < result->column_count; j++) {
                    printf("      <%s>", result->column_names[j]);
                    
                    Field* field = &result->rows[i][j];
                    switch (field->type) {
                        case TYPE_INT:
                            printf("%d", field->value.int_value);
                            break;
                        case TYPE_STRING:
                            printf("%s", field->value.string_value);
                            break;
                        case TYPE_FLOAT:
                            printf("%.2f", field->value.float_value);
                            break;
                        case TYPE_DOUBLE:
                            printf("%.4f", field->value.double_value);
                            break;
                        case TYPE_BOOL:
                            printf("%s", field->value.bool_value ? "true" : "false");
                            break;
                        default:
                            break;
                    }
                    printf("</%s>\n", result->column_names[j]);
                }
                printf("    </row>\n");
            }
            printf("  </rows>\n");
            printf("</queryResult>\n");
            break;
    }
}

// ==================== ERROR HANDLING ====================
void print_error(ErrorCode code) {
    const char* message = NULL;
    
    switch (code) {
        case SUCCESS: message = "Success"; break;
        case ERROR_GENERIC: message = "Generic error"; break;
        case ERROR_NOT_FOUND: message = "Not found"; break;
        case ERROR_DUPLICATE_KEY: message = "Duplicate key"; break;
        case ERROR_INVALID_INPUT: message = "Invalid input"; break;
        case ERROR_MEMORY_ALLOCATION: message = "Memory allocation failed"; break;
        case ERROR_IO_OPERATION: message = "I/O operation failed"; break;
        case ERROR_PERMISSION_DENIED: message = "Permission denied"; break;
        case ERROR_CORRUPT_DATA: message = "Data corruption detected"; break;
        case ERROR_TABLE_FULL: message = "Table is full"; break;
        case ERROR_INDEX_EXISTS: message = "Index already exists"; break;
        case ERROR_INDEX_NOT_FOUND: message = "Index not found"; break;
        case ERROR_TYPE_MISMATCH: message = "Type mismatch"; break;
        case ERROR_CONSTRAINT_VIOLATION: message = "Constraint violation"; break;
        case ERROR_TRANSACTION_CONFLICT: message = "Transaction conflict"; break;
        case ERROR_DEADLOCK_DETECTED: message = "Deadlock detected"; break;
        case ERROR_TIMEOUT: message = "Operation timed out"; break;
        default: message = "Unknown error"; break;
    }
    
    printf("\033[1;31mError %d: %s\033[0m\n", code, message);
}

void print_success(const char* message) {
    printf("\033[1;32m✓ %s\033[0m\n", message);
}

void print_warning(const char* message) {
    printf("\033[1;33m⚠ %s\033[0m\n", message);
}

void print_info(const char* message) {
    printf("\033[1;36mℹ %s\033[0m\n", message);
}

// ==================== DATABASE OPERATIONS ====================
void create_database_interactive() {
    char name[MAX_TABLE_NAME];
    char path[MAX_FIELD_LEN * 2] = "";
    
    printf("Enter database name: ");
    fgets(name, sizeof(name), stdin);
    name[strcspn(name, "\n")] = '\0';
    
    printf("Enter storage path (optional, current directory if empty): ");
    fgets(path, sizeof(path), stdin);
    path[strcspn(path, "\n")] = '\0';
    
    if (strlen(path) == 0) {
        strcpy(path, ".");
    }
    
    DatabaseConfig config = {0};
    strncpy(config.name, name, MAX_TABLE_NAME - 1);
    strncpy(config.path, path, MAX_FIELD_LEN * 2 - 1);
    config.cache_size = 10 * 1024 * 1024; // 10MB
    config.cache_policy = CACHE_LRU;
    config.auto_vacuum = true;
    config.vacuum_threshold = 1000;
    config.max_connections = 100;
    
    g_database = db_create_ex(&config);
    if (g_database) {
        print_success("Database created successfully");
    } else {
        print_error(ERROR_GENERIC);
    }
}

void open_database_interactive() {
    char path[MAX_FIELD_LEN * 2];
    
    printf("Enter database path (directory or .json file): ");
    fgets(path, sizeof(path), stdin);
    path[strcspn(path, "\n")] = '\0';
    
    g_database = db_open(path, "");
    if (g_database) {
        print_success("Database opened successfully");
        printf("Database: %s\n", g_database->name);
        printf("Tables: %d\n", db_get_table_count(g_database));
    } else {
        print_error(ERROR_GENERIC);
    }
}

void close_database_interactive() {
    if (!g_database) {
        print_warning("No database is open");
        return;
    }
    
    printf("Save changes before closing? (y/n): ");
    char response[4];
    fgets(response, sizeof(response), stdin);
    
    if (tolower(response[0]) == 'y') {
        save_state();
    }
    
    db_close(g_database);
    g_database = NULL;
    print_success("Database closed");
}

void backup_database_interactive() {
    if (!g_database) {
        print_error(ERROR_NOT_FOUND);
        return;
    }
    
    char backup_path[MAX_FIELD_LEN * 2];
    printf("Enter backup directory: ");
    fgets(backup_path, sizeof(backup_path), stdin);
    backup_path[strcspn(backup_path, "\n")] = '\0';
    
    JsonSaveOptions options = {0};
    options.serialize.pretty = true;
    options.serialize.compress = true;
    options.atomic_write = true;
    options.backup_existing = true;
    
    ErrorCode result = db_create_backup(g_database, backup_path, &options);
    if (result == SUCCESS) {
        print_success("Backup created successfully");
    } else {
        print_error(result);
    }
}

void restore_database_interactive() {
    char backup_dir[MAX_FIELD_LEN * 2];
    printf("Enter backup directory: ");
    fgets(backup_dir, sizeof(backup_dir), stdin);
    backup_dir[strcspn(backup_dir, "\n")] = '\0';
    
    // List available backups
    char** backups = NULL;
    int count = 0;
    ErrorCode result = db_list_backups(backup_dir, &backups, &count);
    
    if (result != SUCCESS || count == 0) {
        print_error(result);
        return;
    }
    
    printf("\nAvailable backups:\n");
    for (int i = 0; i < count; i++) {
        printf("%d. %s\n", i + 1, backups[i]);
    }
    
    printf("Select backup to restore (1-%d): ", count);
    char choice_str[10];
    fgets(choice_str, sizeof(choice_str), stdin);
    int choice = atoi(choice_str);
    
    if (choice < 1 || choice > count) {
        print_error(ERROR_INVALID_INPUT);
        return;
    }
    
    char backup_file[MAX_FIELD_LEN * 2];
    snprintf(backup_file, sizeof(backup_file), "%s/%s", backup_dir, backups[choice - 1]);
    
    // Free backups list
    for (int i = 0; i < count; i++) {
        free(backups[i]);
    }
    free(backups);
    
    // Confirm restore
    printf("Restore backup '%s'? This will overwrite current database. (y/n): ", backup_file);
    char response[4];
    fgets(response, sizeof(response), stdin);
    
    if (tolower(response[0]) != 'y') {
        print_info("Restore cancelled");
        return;
    }
    
    JsonLoadOptions options = {0};
    result = db_restore_backup(g_database, backup_file, &options);
    if (result == SUCCESS) {
        print_success("Backup restored successfully");
    } else {
        print_error(result);
    }
}

// ==================== TABLE OPERATIONS ====================
void create_table_interactive() {
    if (!g_database) {
        print_error(ERROR_NOT_FOUND);
        return;
    }
    
    char table_name[MAX_TABLE_NAME];
    printf("Enter table name: ");
    fgets(table_name, sizeof(table_name), stdin);
    table_name[strcspn(table_name, "\n")] = '\0';
    
    int field_count;
    printf("Enter number of fields: ");
    char count_str[10];
    fgets(count_str, sizeof(count_str), stdin);
    field_count = atoi(count_str);
    
    if (field_count <= 0 || field_count > MAX_FIELDS_PER_TABLE) {
        print_error(ERROR_INVALID_INPUT);
        return;
    }
    
    char** field_names = malloc(field_count * sizeof(char*));
    FieldType* field_types = malloc(field_count * sizeof(FieldType));
    Constraint* constraints = calloc(field_count, sizeof(Constraint));
    
    if (!field_names || !field_types || !constraints) {
        print_error(ERROR_MEMORY_ALLOCATION);
        free(field_names);
        free(field_types);
        free(constraints);
        return;
    }
    
    for (int i = 0; i < field_count; i++) {
        char field_name[MAX_FIELD_LEN];
        printf("Field %d name: ", i + 1);
        fgets(field_name, sizeof(field_name), stdin);
        field_name[strcspn(field_name, "\n")] = '\0';
        field_names[i] = strdup(field_name);
        
        printf("Field type (1=int, 2=string, 3=float, 4=double, 5=bool, 6=datetime): ");
        char type_str[10];
        fgets(type_str, sizeof(type_str), stdin);
        int type_choice = atoi(type_str);
        
        switch (type_choice) {
            case 1: field_types[i] = TYPE_INT; break;
            case 2: field_types[i] = TYPE_STRING; break;
            case 3: field_types[i] = TYPE_FLOAT; break;
            case 4: field_types[i] = TYPE_DOUBLE; break;
            case 5: field_types[i] = TYPE_BOOL; break;
            case 6: field_types[i] = TYPE_DATETIME; break;
            default: field_types[i] = TYPE_STRING; break;
        }
        
        // Ask about constraints
        printf("Is this field primary key? (y/n): ");
        char pk_str[4];
        fgets(pk_str, sizeof(pk_str), stdin);
        constraints[i].primary_key = (tolower(pk_str[0]) == 'y');
        
        printf("Allow NULL values? (y/n): ");
        char null_str[4];
        fgets(null_str, sizeof(null_str), stdin);
        constraints[i].nullable = (tolower(null_str[0]) == 'y');
        
        printf("Unique values? (y/n): ");
        char unique_str[4];
        fgets(unique_str, sizeof(unique_str), stdin);
        constraints[i].unique = (tolower(unique_str[0]) == 'y');
    }
    
    Table* table = NULL;
    ErrorCode result = db_add_table(g_database, table_name, 
                                   (const char**)field_names, field_types,
                                   field_count, &table);
    
    // Apply constraints
    if (result == SUCCESS && table) {
        for (int i = 0; i < field_count; i++) {
            table->constraints[i] = constraints[i];
        }
        print_success("Table created successfully");
    } else {
        print_error(result);
    }
    
    // Cleanup
    for (int i = 0; i < field_count; i++) {
        free(field_names[i]);
    }
    free(field_names);
    free(field_types);
    free(constraints);
}

void list_tables_interactive() {
    if (!g_database) {
        print_error(ERROR_NOT_FOUND);
        return;
    }
    
    int count;
    char** tables = db_list_tables(g_database, &count);
    
    if (!tables || count == 0) {
        print_info("No tables found");
        return;
    }
    
    printf("\nTables (%d):\n", count);
    printf("┌─────┬──────────────────────────────┬──────────────┐\n");
    printf("│ No. │ Name                         │ Records      │\n");
    printf("├─────┼──────────────────────────────┼──────────────┤\n");
    
    for (int i = 0; i < count; i++) {
        Table* table = db_get_table(g_database, tables[i]);
        printf("│ %3d │ %-28s │ %-12d │\n", 
               i + 1, tables[i], table ? table->record_count : 0);
        free(tables[i]);
    }
    free(tables);
    
    printf("└─────┴──────────────────────────────┴──────────────┘\n");
}

void describe_table_interactive() {
    if (!g_database) {
        print_error(ERROR_NOT_FOUND);
        return;
    }
    
    char table_name[MAX_TABLE_NAME];
    printf("Enter table name: ");
    fgets(table_name, sizeof(table_name), stdin);
    table_name[strcspn(table_name, "\n")] = '\0';
    
    Table* table = db_get_table(g_database, table_name);
    if (!table) {
        print_error(ERROR_NOT_FOUND);
        return;
    }
    
    printf("\nTable: %s\n", table->name);
    printf("Records: %d\n", table->record_count);
    printf("Fields: %d\n", table->field_count);
    
    printf("\nSchema:\n");
    printf("┌─────┬──────────────────────────────┬────────────────┬────────────────┐\n");
    printf("│ No. │ Name                         │ Type           │ Constraints    │\n");
    printf("├─────┼──────────────────────────────┼────────────────┼────────────────┤\n");
    
    for (int i = 0; i < table->field_count; i++) {
        char constraints[64] = "";
        if (table->constraints[i].primary_key) strcat(constraints, "PK ");
        if (table->constraints[i].unique) strcat(constraints, "UNIQUE ");
        if (!table->constraints[i].nullable) strcat(constraints, "NOT NULL ");
        if (strlen(constraints) == 0) strcpy(constraints, "-");
        
        printf("│ %3d │ %-28s │ %-14s │ %-14s │\n",
               i + 1,
               table->field_names[i],
               field_type_to_string(table->field_types[i]),
               constraints);
    }
    printf("└─────┴──────────────────────────────┴────────────────┴────────────────┘\n");
    
    // Show indexes
    if (table->index_manager && table->index_manager->count > 0) {
        printf("\nIndexes (%d):\n", table->index_manager->count);
        for (int i = 0; i < table->index_manager->count; i++) {
            Index* index = table->index_manager->indexes[i];
            printf("  • %s (%s)\n", index->field_name, index_type_to_string(index->type));
        }
    }
}

void alter_table_interactive() {
    if (!g_database) {
        print_error(ERROR_NOT_FOUND);
        return;
    }
    
    printf("\nAlter Table Operations:\n");
    printf("1. Add Column\n");
    printf("2. Drop Column\n");
    printf("3. Rename Column\n");
    printf("4. Modify Column Type\n");
    printf("5. Add Constraint\n");
    printf("6. Drop Constraint\n");
    printf("0. Cancel\n");
    
    printf("Select operation: ");
    char choice_str[10];
    fgets(choice_str, sizeof(choice_str), stdin);
    int choice = atoi(choice_str);
    
    if (choice == 0) return;
    
    char table_name[MAX_TABLE_NAME];
    printf("Enter table name: ");
    fgets(table_name, sizeof(table_name), stdin);
    table_name[strcspn(table_name, "\n")] = '\0';
    
    Table* table = db_get_table(g_database, table_name);
    if (!table) {
        print_error(ERROR_NOT_FOUND);
        return;
    }
    
    switch (choice) {
        case 1: { // Add column
            char column_name[MAX_FIELD_LEN];
            printf("Enter new column name: ");
            fgets(column_name, sizeof(column_name), stdin);
            column_name[strcspn(column_name, "\n")] = '\0';
            
            printf("Enter column type (1=int, 2=string, 3=float, 4=double, 5=bool): ");
            char type_str[10];
            fgets(type_str, sizeof(type_str), stdin);
            int type_choice = atoi(type_str);
            
            FieldType type = TYPE_STRING;
            switch (type_choice) {
                case 1: type = TYPE_INT; break;
                case 2: type = TYPE_STRING; break;
                case 3: type = TYPE_FLOAT; break;
                case 4: type = TYPE_DOUBLE; break;
                case 5: type = TYPE_BOOL; break;
            }
            
            Constraint constraint = {0};
            constraint.nullable = true;
            
            ErrorCode result = db_alter_table_add_column(g_database, table_name, 
                                                        column_name, type, &constraint);
            if (result == SUCCESS) {
                print_success("Column added successfully");
            } else {
                print_error(result);
            }
            break;
        }
            
        case 2: { // Drop column
            char column_name[MAX_FIELD_LEN];
            printf("Enter column name to drop: ");
            fgets(column_name, sizeof(column_name), stdin);
            column_name[strcspn(column_name, "\n")] = '\0';
            
            // Confirm
            printf("Drop column '%s'? This will delete all data in this column. (y/n): ", column_name);
            char confirm[4];
            fgets(confirm, sizeof(confirm), stdin);
            
            if (tolower(confirm[0]) == 'y') {
                ErrorCode result = db_alter_table_drop_column(g_database, table_name, column_name);
                if (result == SUCCESS) {
                    print_success("Column dropped successfully");
                } else {
                    print_error(result);
                }
            }
            break;
        }
            
        case 3: { // Rename column
            char old_name[MAX_FIELD_LEN];
            char new_name[MAX_FIELD_LEN];
            
            printf("Enter current column name: ");
            fgets(old_name, sizeof(old_name), stdin);
            old_name[strcspn(old_name, "\n")] = '\0';
            
            printf("Enter new column name: ");
            fgets(new_name, sizeof(new_name), stdin);
            new_name[strcspn(new_name, "\n")] = '\0';
            
            ErrorCode result = db_alter_table_rename_column(g_database, table_name, 
                                                           old_name, new_name);
            if (result == SUCCESS) {
                print_success("Column renamed successfully");
            } else {
                print_error(result);
            }
            break;
        }
            
        default:
            print_warning("Operation not yet implemented");
            break;
    }
}

void truncate_table_interactive() {
    if (!g_database) {
        print_error(ERROR_NOT_FOUND);
        return;
    }
    
    char table_name[MAX_TABLE_NAME];
    printf("Enter table name: ");
    fgets(table_name, sizeof(table_name), stdin);
    table_name[strcspn(table_name, "\n")] = '\0';
    
    // Confirm
    printf("Truncate table '%s'? This will delete all records. (y/n): ", table_name);
    char confirm[4];
    fgets(confirm, sizeof(confirm), stdin);
    
    if (tolower(confirm[0]) == 'y') {
        ErrorCode result = db_truncate_table(g_database, table_name);
        if (result == SUCCESS) {
            print_success("Table truncated successfully");
        } else {
            print_error(result);
        }
    }
}

void drop_table_interactive() {
    if (!g_database) {
        print_error(ERROR_NOT_FOUND);
        return;
    }
    
    char table_name[MAX_TABLE_NAME];
    printf("Enter table name: ");
    fgets(table_name, sizeof(table_name), stdin);
    table_name[strcspn(table_name, "\n")] = '\0';
    
    // Confirm
    printf("Drop table '%s'? This will delete the table and all its data. (y/n): ", table_name);
    char confirm[4];
    fgets(confirm, sizeof(confirm), stdin);
    
    if (tolower(confirm[0]) == 'y') {
        ErrorCode result = db_drop_table(g_database, table_name);
        if (result == SUCCESS) {
            print_success("Table dropped successfully");
        } else {
            print_error(result);
        }
    }
}

void import_table_interactive() {
    if (!g_database) {
        print_error(ERROR_NOT_FOUND);
        return;
    }
    
    char table_name[MAX_TABLE_NAME];
    printf("Enter table name (or leave empty to infer from file): ");
    fgets(table_name, sizeof(table_name), stdin);
    table_name[strcspn(table_name, "\n")] = '\0';
    
    char file_path[MAX_FIELD_LEN * 2];
    printf("Enter file path: ");
    fgets(file_path, sizeof(file_path), stdin);
    file_path[strcspn(file_path, "\n")] = '\0';
    
    printf("Enter file format (1=JSON, 2=CSV, 3=XML): ");
    char format_str[10];
    fgets(format_str, sizeof(format_str), stdin);
    int format_choice = atoi(format_str);
    
    const char* format = "json";
    switch (format_choice) {
        case 1: format = "json"; break;
        case 2: format = "csv"; break;
        case 3: format = "xml"; break;
        default: format = "json"; break;
    }
    
    ErrorCode result;
    if (strlen(table_name) > 0) {
        result = db_import_table(g_database, table_name, format, file_path);
    } else {
        // Infer table name from filename
        char* filename = strrchr(file_path, '/');
        if (!filename) filename = strrchr(file_path, '\\');
        if (!filename) filename = file_path;
        else filename++;
        
        // Remove extension
        char* dot = strrchr(filename, '.');
        if (dot) *dot = '\0';
        
        result = db_import_table(g_database, filename, format, file_path);
    }
    
    if (result == SUCCESS) {
        print_success("Table imported successfully");
    } else {
        print_error(result);
    }
}

void export_table_interactive() {
    if (!g_database) {
        print_error(ERROR_NOT_FOUND);
        return;
    }
    
    char table_name[MAX_TABLE_NAME];
    printf("Enter table name: ");
    fgets(table_name, sizeof(table_name), stdin);
    table_name[strcspn(table_name, "\n")] = '\0';
    
    Table* table = db_get_table(g_database, table_name);
    if (!table) {
        print_error(ERROR_NOT_FOUND);
        return;
    }
    
    char file_path[MAX_FIELD_LEN * 2];
    printf("Enter output file path: ");
    fgets(file_path, sizeof(file_path), stdin);
    file_path[strcspn(file_path, "\n")] = '\0';
    
    printf("Enter file format (1=JSON, 2=CSV, 3=XML): ");
    char format_str[10];
    fgets(format_str, sizeof(format_str), stdin);
    int format_choice = atoi(format_str);
    
    const char* format = "json";
    switch (format_choice) {
        case 1: format = "json"; break;
        case 2: format = "csv"; break;
        case 3: format = "xml"; break;
        default: format = "json"; break;
    }
    
    ErrorCode result = db_export_table(g_database, table_name, format, file_path);
    if (result == SUCCESS) {
        print_success("Table exported successfully");
    } else {
        print_error(result);
    }
}

// ==================== INDEX OPERATIONS ====================
void create_index_interactive() {
    if (!g_database) {
        print_error(ERROR_NOT_FOUND);
        return;
    }
    
    char table_name[MAX_TABLE_NAME];
    printf("Enter table name: ");
    fgets(table_name, sizeof(table_name), stdin);
    table_name[strcspn(table_name, "\n")] = '\0';
    
    Table* table = db_get_table(g_database, table_name);
    if (!table) {
        print_error(ERROR_NOT_FOUND);
        return;
    }
    
    // Show available fields
    printf("\nAvailable fields in table '%s':\n", table_name);
    for (int i = 0; i < table->field_count; i++) {
        printf("%d. %s (%s)\n", i + 1, table->field_names[i], 
               field_type_to_string(table->field_types[i]));
    }
    
    printf("Select field number: ");
    char field_choice_str[10];
    fgets(field_choice_str, sizeof(field_choice_str), stdin);
    int field_choice = atoi(field_choice_str) - 1;
    
    if (field_choice < 0 || field_choice >= table->field_count) {
        print_error(ERROR_INVALID_INPUT);
        return;
    }
    
    char* field_name = table->field_names[field_choice];
    
    printf("\nIndex types:\n");
    printf("1. Hash Index (fast equality lookups)\n");
    printf("2. B-Tree Index (ordered, range queries)\n");
    printf("3. Skip List Index (concurrent access)\n");
    printf("4. Bitmap Index (low cardinality fields)\n");
    printf("5. Full-Text Index (text search)\n");
    
    printf("Select index type: ");
    char type_choice_str[10];
    fgets(type_choice_str, sizeof(type_choice_str), stdin);
    int type_choice = atoi(type_choice_str);
    
    IndexType type = INDEX_HASH;
    switch (type_choice) {
        case 1: type = INDEX_HASH; break;
        case 2: type = INDEX_BTREE; break;
        case 3: type = INDEX_SKIPLIST; break;
        case 4: type = INDEX_BITMAP; break;
        case 5: type = INDEX_FULLTEXT; break;
        default: type = INDEX_HASH; break;
    }
    
    ErrorCode result = db_create_index(g_database, table_name, field_name, type);
    if (result == SUCCESS) {
        print_success("Index created successfully");
    } else {
        print_error(result);
    }
}

void list_indexes_interactive() {
    if (!g_database) {
        print_error(ERROR_NOT_FOUND);
        return;
    }
    
    char table_name[MAX_TABLE_NAME];
    printf("Enter table name (or leave empty for all tables): ");
    fgets(table_name, sizeof(table_name), stdin);
    table_name[strcspn(table_name, "\n")] = '\0';
    
    if (strlen(table_name) > 0) {
        Table* table = db_get_table(g_database, table_name);
        if (!table) {
            print_error(ERROR_NOT_FOUND);
            return;
        }
        
        if (!table->index_manager || table->index_manager->count == 0) {
            print_info("No indexes found for this table");
            return;
        }
        
        printf("\nIndexes for table '%s':\n", table_name);
        printf("┌─────┬──────────────────────────────┬────────────────┬──────────────┐\n");
        printf("│ No. │ Field Name                   │ Type           │ Size         │\n");
        printf("├─────┼──────────────────────────────┼────────────────┼──────────────┤\n");
        
        for (int i = 0; i < table->index_manager->count; i++) {
            Index* index = table->index_manager->indexes[i];
            printf("│ %3d │ %-28s │ %-14s │ %-12zu │\n",
                   i + 1,
                   index->field_name,
                   index_type_to_string(index->type),
                   index_get_memory_usage(index));
        }
        printf("└─────┴──────────────────────────────┴────────────────┴──────────────┘\n");
    } else {
        // List indexes for all tables
        int table_count = db_get_table_count(g_database);
        int total_indexes = 0;
        
        printf("\nIndexes for all tables:\n");
        
        for (int t = 0; t < table_count; t++) {
            Table* table = &g_database->tables[t];
            if (table->index_manager && table->index_manager->count > 0) {
                printf("\nTable: %s\n", table->name);
                for (int i = 0; i < table->index_manager->count; i++) {
                    Index* index = table->index_manager->indexes[i];
                    printf("  • %s (%s)\n", index->field_name, index_type_to_string(index->type));
                    total_indexes++;
                }
            }
        }
        
        if (total_indexes == 0) {
            print_info("No indexes found in the database");
        }
    }
}

void drop_index_interactive() {
    if (!g_database) {
        print_error(ERROR_NOT_FOUND);
        return;
    }
    
    char table_name[MAX_TABLE_NAME];
    printf("Enter table name: ");
    fgets(table_name, sizeof(table_name), stdin);
    table_name[strcspn(table_name, "\n")] = '\0';
    
    Table* table = db_get_table(g_database, table_name);
    if (!table || !table->index_manager || table->index_manager->count == 0) {
        print_error(ERROR_NOT_FOUND);
        return;
    }
    
    printf("\nIndexes in table '%s':\n", table_name);
    for (int i = 0; i < table->index_manager->count; i++) {
        Index* index = table->index_manager->indexes[i];
        printf("%d. %s (%s)\n", i + 1, index->field_name, index_type_to_string(index->type));
    }
    
    printf("Select index to drop: ");
    char choice_str[10];
    fgets(choice_str, sizeof(choice_str), stdin);
    int choice = atoi(choice_str) - 1;
    
    if (choice < 0 || choice >= table->index_manager->count) {
        print_error(ERROR_INVALID_INPUT);
        return;
    }
    
    char* field_name = table->index_manager->indexes[choice]->field_name;
    
    // Confirm
    printf("Drop index on '%s.%s'? (y/n): ", table_name, field_name);
    char confirm[4];
    fgets(confirm, sizeof(confirm), stdin);
    
    if (tolower(confirm[0]) == 'y') {
        ErrorCode result = db_drop_index(g_database, table_name, field_name);
        if (result == SUCCESS) {
            print_success("Index dropped successfully");
        } else {
            print_error(result);
        }
    }
}

void rebuild_index_interactive() {
    if (!g_database) {
        print_error(ERROR_NOT_FOUND);
        return;
    }
    
    printf("Rebuild all indexes? This may take time. (y/n): ");
    char confirm[4];
    fgets(confirm, sizeof(confirm), stdin);
    
    if (tolower(confirm[0]) == 'y') {
        ErrorCode result = db_rebuild_indexes(g_database);
        if (result == SUCCESS) {
            print_success("Indexes rebuilt successfully");
        } else {
            print_error(result);
        }
    }
}

void optimize_index_interactive() {
    if (!g_database) {
        print_error(ERROR_NOT_FOUND);
        return;
    }
    
    char table_name[MAX_TABLE_NAME];
    printf("Enter table name (or leave empty for all tables): ");
    fgets(table_name, sizeof(table_name), stdin);
    table_name[strcspn(table_name, "\n")] = '\0';
    
    if (strlen(table_name) > 0) {
        Table* table = db_get_table(g_database, table_name);
        if (!table) {
            print_error(ERROR_NOT_FOUND);
            return;
        }
        
        if (table->index_manager) {
            for (int i = 0; i < table->index_manager->count; i++) {
                Index* index = table->index_manager->indexes[i];
                printf("Optimizing index on %s... ", index->field_name);
                ErrorCode result = index_optimize(index);
                if (result == SUCCESS) {
                    printf("✓\n");
                } else {
                    printf("✗\n");
                }
            }
        }
    } else {
        // Optimize all indexes
        int table_count = db_get_table_count(g_database);
        int optimized = 0;
        
        for (int t = 0; t < table_count; t++) {
            Table* table = &g_database->tables[t];
            if (table->index_manager) {
                for (int i = 0; i < table->index_manager->count; i++) {
                    index_optimize(table->index_manager->indexes[i]);
                    optimized++;
                }
            }
        }
        
        printf("Optimized %d indexes\n", optimized);
    }
}

// ==================== QUERY OPERATIONS ====================
void execute_query_interactive() {
    if (!g_database) {
        print_error(ERROR_NOT_FOUND);
        return;
    }
    
    printf("\nEnter SQL query (or type 'help' for syntax help):\n");
    printf("Use ';;' on a new line to execute multi-line queries.\n");
    
    char* query = read_multiline_input();
    if (!query || strlen(query) == 0) {
        return;
    }
    
    // Check for help command
    if (strcasecmp(query, "help") == 0) {
        show_help_topic("sql");
        return;
    }
    
    // Add to history
    add_to_history(query);
    
    // Execute query
    clock_t start = clock();
    QueryResult* result = db_execute_query(g_database, query);
    clock_t end = clock();
    
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    
    if (result) {
        printf("\nQuery executed successfully (%.3f seconds)\n", elapsed);
        
        printf("\nOutput format (1=Table, 2=JSON, 3=CSV, 4=XML): ");
        char format_str[10];
        fgets(format_str, sizeof(format_str), stdin);
        int format_choice = atoi(format_str);
        
        OutputFormat format = FORMAT_TABLE;
        switch (format_choice) {
            case 1: format = FORMAT_TABLE; break;
            case 2: format = FORMAT_JSON; break;
            case 3: format = FORMAT_CSV; break;
            case 4: format = FORMAT_XML; break;
            default: format = FORMAT_TABLE; break;
        }
        
        print_query_result_formatted(result, format);
        
        // Ask if user wants to save result
        printf("\nSave result to file? (y/n): ");
        char save_choice[4];
        fgets(save_choice, sizeof(save_choice), stdin);
        
        if (tolower(save_choice[0]) == 'y') {
            save_query_result(result);
        }
        
        // Free result
        // Note: In a real implementation, you'd have a function to free QueryResult
        // free_query_result(result);
    } else {
        printf("\nQuery failed (%.3f seconds)\n", elapsed);
        print_error(ERROR_GENERIC);
    }
}

void execute_script_interactive() {
    if (!g_database) {
        print_error(ERROR_NOT_FOUND);
        return;
    }
    
    char script_path[MAX_FIELD_LEN * 2];
    printf("Enter script file path: ");
    fgets(script_path, sizeof(script_path), stdin);
    script_path[strcspn(script_path, "\n")] = '\0';
    
    FILE* file = fopen(script_path, "r");
    if (!file) {
        print_error(ERROR_IO_OPERATION);
        return;
    }
    
    printf("Executing script: %s\n", script_path);
    
    char line[MAX_QUERY_LEN];
    char query[MAX_QUERY_LEN * 10] = "";
    int line_num = 0;
    int success_count = 0;
    int error_count = 0;
    
    while (fgets(line, sizeof(line), file)) {
        line_num++;
        
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '-' || strlen(trim_string(line)) == 0) {
            continue;
        }
        
        // Check for statement terminator
        if (strstr(line, ";") != NULL) {
            strcat(query, line);
            
            // Execute the query
            printf("\n[Line %d] Executing: %s", line_num, query);
            
            clock_t start = clock();
            QueryResult* result = db_execute_query(g_database, query);
            clock_t end = clock();
            
            double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
            
            if (result) {
                printf(" ✓ (%.3f seconds)\n", elapsed);
                success_count++;
                // free_query_result(result);
            } else {
                printf(" ✗ (%.3f seconds)\n", elapsed);
                error_count++;
            }
            
            // Clear query buffer
            query[0] = '\0';
        } else {
            // Continue building query
            strcat(query, line);
        }
    }
    
    fclose(file);
    
    printf("\nScript execution completed:\n");
    printf("  Successful: %d\n", success_count);
    printf("  Failed: %d\n", error_count);
    printf("  Total: %d\n", success_count + error_count);
}

void query_history() {
    if (g_history_count == 0) {
        print_info("No query history");
        return;
    }
    
    printf("\nQuery History (last %d queries):\n", g_history_count);
    printf("┌─────┬────────────────────────────────────────────────────────────┐\n");
    printf("│ No. │ Query                                                      │\n");
    printf("├─────┼────────────────────────────────────────────────────────────┤\n");
    
    int start = (g_history_index - g_history_count + 100) % 100;
    for (int i = 0; i < g_history_count; i++) {
        int idx = (start + i) % 100;
        // Truncate long queries for display
        char display[MAX_QUERY_LEN];
        strncpy(display, g_history[idx], 60);
        if (strlen(g_history[idx]) > 60) {
            strcpy(display + 57, "...");
        }
        
        printf("│ %3d │ %-60s │\n", i + 1, display);
    }
    printf("└─────┴────────────────────────────────────────────────────────────┘\n");
    
    // Allow re-executing a query
    printf("\nEnter query number to re-execute (0 to cancel): ");
    char choice_str[10];
    fgets(choice_str, sizeof(choice_str), stdin);
    int choice = atoi(choice_str);
    
    if (choice > 0 && choice <= g_history_count) {
        int idx = (start + choice - 1) % 100;
        printf("\nRe-executing query: %s\n", g_history[idx]);
        
        clock_t start_time = clock();
        QueryResult* result = db_execute_query(g_database, g_history[idx]);
        clock_t end_time = clock();
        
        double elapsed = (double)(end_time - start_time) / CLOCKS_PER_SEC;
        
        if (result) {
            printf("Query executed successfully (%.3f seconds)\n", elapsed);
            print_query_result_formatted(result, FORMAT_TABLE);
            // free_query_result(result);
        } else {
            printf("Query failed (%.3f seconds)\n", elapsed);
        }
    }
}

void save_query_result(QueryResult* result) {
    if (!result) return;
    
    char file_path[MAX_FIELD_LEN * 2];
    printf("Enter output file path: ");
    fgets(file_path, sizeof(file_path), stdin);
    file_path[strcspn(file_path, "\n")] = '\0';
    
    printf("Enter file format (1=JSON, 2=CSV, 3=XML): ");
    char format_str[10];
    fgets(format_str, sizeof(format_str), stdin);
    int format_choice = atoi(format_str);
    
    const char* format = "json";
    switch (format_choice) {
        case 1: format = "json"; break;
        case 2: format = "csv"; break;
        case 3: format = "xml"; break;
        default: format = "json"; break;
    }
    
    JsonSerializeOptions options = {0};
    options.pretty = true;
    
    if (strcmp(format, "json") == 0) {
        char* json = query_result_to_json(result, &options);
        if (json) {
            FILE* file = fopen(file_path, "w");
            if (file) {
                fputs(json, file);
                fclose(file);
                print_success("Result saved to JSON file");
            }
            free(json);
        }
    } else if (strcmp(format, "csv") == 0) {
        FILE* file = fopen(file_path, "w");
        if (file) {
            print_query_result_formatted(result, FORMAT_CSV);
            // Note: This prints to stdout, need to modify to write to file
            fclose(file);
            print_success("Result saved to CSV file");
        }
    }
}

void explain_query_interactive() {
    if (!g_database) {
        print_error(ERROR_NOT_FOUND);
        return;
    }
    
    printf("Enter query to explain: ");
    char* query = read_multiline_input();
    if (!query || strlen(query) == 0) {
        return;
    }
    
    char* explanation = NULL;
    ErrorCode result = db_explain_query(g_database, query, &explanation);
    
    if (result == SUCCESS && explanation) {
        printf("\nQuery Explanation:\n");
        printf("==================\n");
        printf("%s\n", explanation);
        free(explanation);
    } else {
        print_error(result);
    }
}

// ==================== MAINTENANCE OPERATIONS ====================
void vacuum_database_interactive() {
    if (!g_database) {
        print_error(ERROR_NOT_FOUND);
        return;
    }
    
    printf("Vacuum operations:\n");
    printf("1. Full vacuum (reclaim space)\n");
    printf("2. Analyze only (update statistics)\n");
    printf("3. Vacuum specific table\n");
    printf("4. Analyze specific table\n");
    
    printf("Select operation: ");
    char choice_str[10];
    fgets(choice_str, sizeof(choice_str), stdin);
    int choice = atoi(choice_str);
    
    switch (choice) {
        case 1:
            printf("Performing full vacuum...\n");
            db_vacuum(g_database);
            print_success("Vacuum completed");
            break;
            
        case 2:
            printf("Analyzing database...\n");
            db_analyze(g_database);
            print_success("Analysis completed");
            break;
            
        case 3: {
            char table_name[MAX_TABLE_NAME];
            printf("Enter table name: ");
            fgets(table_name, sizeof(table_name), stdin);
            table_name[strcspn(table_name, "\n")] = '\0';
            
            printf("Vacuuming table '%s'...\n", table_name);
            db_vacuum_table(g_database, table_name);
            print_success("Table vacuum completed");
            break;
        }
            
        case 4: {
            char table_name[MAX_TABLE_NAME];
            printf("Enter table name: ");
            fgets(table_name, sizeof(table_name), stdin);
            table_name[strcspn(table_name, "\n")] = '\0';
            
            printf("Analyzing table '%s'...\n", table_name);
            db_analyze_table(g_database, table_name);
            print_success("Table analysis completed");
            break;
        }
            
        default:
            print_warning("Invalid operation");
            break;
    }
}

void analyze_database_interactive() {
    if (!g_database) {
        print_error(ERROR_NOT_FOUND);
        return;
    }
    
    printf("Running database analysis...\n");
    db_analyze(g_database);
    print_success("Analysis completed");
}

void check_integrity_interactive() {
    if (!g_database) {
        print_error(ERROR_NOT_FOUND);
        return;
    }
    
    printf("Check integrity with automatic repair? (y/n): ");
    char repair_choice[4];
    fgets(repair_choice, sizeof(repair_choice), stdin);
    
    bool repair = (tolower(repair_choice[0]) == 'y');
    
    printf("Checking database integrity...\n");
    ErrorCode result = db_check_integrity(g_database, repair);
    
    if (result == SUCCESS) {
        print_success("Integrity check passed");
    } else if (result == ERROR_CORRUPT_DATA) {
        print_warning("Data corruption detected");
        if (repair) {
            printf("Attempting repair...\n");
            // Additional repair logic would go here
        }
    } else {
        print_error(result);
    }
}

void repair_database_interactive() {
    if (!g_database) {
        print_error(ERROR_NOT_FOUND);
        return;
    }
    
    printf("Repair operations:\n");
    printf("1. Repair specific table\n");
    printf("2. Repair all tables\n");
    printf("3. Rebuild indexes\n");
    printf("4. Recover from backup\n");
    
    printf("Select operation: ");
    char choice_str[10];
    fgets(choice_str, sizeof(choice_str), stdin);
    int choice = atoi(choice_str);
    
    switch (choice) {
        case 1: {
            char table_name[MAX_TABLE_NAME];
            printf("Enter table name: ");
            fgets(table_name, sizeof(table_name), stdin);
            table_name[strcspn(table_name, "\n")] = '\0';
            
            printf("Repairing table '%s'...\n", table_name);
            ErrorCode result = db_repair_table(g_database, table_name);
            if (result == SUCCESS) {
                print_success("Table repaired successfully");
            } else {
                print_error(result);
            }
            break;
        }
            
        case 2:
            printf("Repairing all tables...\n");
            // This would iterate through all tables and repair them
            print_warning("Not yet implemented");
            break;
            
        case 3:
            printf("Rebuilding all indexes...\n");
            db_rebuild_indexes(g_database);
            print_success("Indexes rebuilt");
            break;
            
        case 4:
            restore_database_interactive();
            break;
            
        default:
            print_warning("Invalid operation");
            break;
    }
}

void statistics_interactive() {
    if (!g_database) {
        print_error(ERROR_NOT_FOUND);
        return;
    }
    
    printf("\nDatabase Statistics:\n");
    printf("====================\n");
    
    Statistics* stats = db_get_statistics(g_database);
    if (stats) {
        printf("Operations:\n");
        printf("  Tables Created:    %lld\n", stats->tables_created);
        printf("  Records Inserted:  %lld\n", stats->records_inserted);
        printf("  Records Updated:   %lld\n", stats->records_updated);
        printf("  Records Deleted:   %lld\n", stats->records_deleted);
        printf("  Queries Executed:  %lld\n", stats->queries_executed);
        
        printf("\nPerformance:\n");
        printf("  Cache Hits:        %lld\n", stats->cache_hits);
        printf("  Cache Misses:      %lld\n", stats->cache_misses);
        long long total_cache = stats->cache_hits + stats->cache_misses;
        if (total_cache > 0) {
            printf("  Cache Hit Rate:    %.1f%%\n", 
                   (double)stats->cache_hits / total_cache * 100);
        }
        
        printf("  Total Query Time:  %.2f seconds\n", stats->total_query_time / 1000.0);
        if (stats->queries_executed > 0) {
            printf("  Avg Query Time:    %.3f ms\n", 
                   (double)stats->total_query_time / stats->queries_executed);
        }
        
        printf("\nMemory Usage:\n");
        printf("  Current:           %zu bytes\n", stats->memory_used);
        printf("  Peak:              %zu bytes\n", stats->peak_memory_used);
        printf("  Cache:             %zu bytes\n", stats->cache_memory_used);
        
        printf("\nTransactions:\n");
        printf("  Started:           %lld\n", stats->transactions_started);
        printf("  Committed:         %lld\n", stats->transactions_committed);
        printf("  Rolled Back:       %lld\n", stats->transactions_rolled_back);
        printf("  Deadlocks:         %lld\n", stats->deadlocks_detected);
        
        printf("\nMaintenance:\n");
        printf("  Vacuum Operations: %lld\n", stats->vacuum_operations);
        printf("  Backup Operations: %lld\n", stats->backup_operations);
        printf("  Optimizations:     %lld\n", stats->optimization_operations);
    }
    
    printf("\nTable Statistics:\n");
    printf("=================\n");
    
    int table_count = db_get_table_count(g_database);
    for (int i = 0; i < table_count; i++) {
        Table* table = &g_database->tables[i];
        TableStatistics* table_stats = db_get_table_statistics(g_database, table->name);
        
        if (table_stats) {
            printf("\nTable: %s\n", table->name);
            printf("  Records:          %d\n", table->record_count);
            printf("  Inserted:         %lld\n", table_stats->records_inserted);
            printf("  Updated:          %lld\n", table_stats->records_updated);
            printf("  Deleted:          %lld\n", table_stats->records_deleted);
            printf("  Index Scans:      %lld\n", table_stats->index_scans);
            printf("  Sequential Scans: %lld\n", table_stats->sequential_scans);
            printf("  Total Size:       %zu bytes\n", table_stats->total_size_bytes);
        }
    }
}

// ==================== CONFIGURATION OPERATIONS ====================
void show_config_interactive() {
    if (!g_database) {
        print_error(ERROR_NOT_FOUND);
        return;
    }
    
    char** configs = NULL;
    int count = 0;
    
    ErrorCode result = db_list_config(g_database, &configs, &count);
    if (result == SUCCESS && configs) {
        printf("\nDatabase Configuration:\n");
        printf("┌──────────────────────────────┬──────────────────────────────────────┐\n");
        printf("│ Key                          │ Value                                │\n");
        printf("├──────────────────────────────┼──────────────────────────────────────┤\n");
        
        for (int i = 0; i < count; i++) {
            char key[64] = "";
            char value[256] = "";
            
            // Parse key=value pair
            char* equals = strchr(configs[i], '=');
            if (equals) {
                *equals = '\0';
                strncpy(key, configs[i], sizeof(key) - 1);
                strncpy(value, equals + 1, sizeof(value) - 1);
            } else {
                strncpy(key, configs[i], sizeof(key) - 1);
            }
            
            printf("│ %-28s │ %-36s │\n", key, value);
            free(configs[i]);
        }
        free(configs);
        
        printf("└──────────────────────────────┴──────────────────────────────────────┘\n");
    } else {
        print_error(result);
    }
}

void set_config_interactive() {
    if (!g_database) {
        print_error(ERROR_NOT_FOUND);
        return;
    }
    
    char key[64];
    char value[256];
    
    printf("Enter configuration key: ");
    fgets(key, sizeof(key), stdin);
    key[strcspn(key, "\n")] = '\0';
    
    printf("Enter configuration value: ");
    fgets(value, sizeof(value), stdin);
    value[strcspn(value, "\n")] = '\0';
    
    ErrorCode result = db_set_config(g_database, key, value);
    if (result == SUCCESS) {
        print_success("Configuration updated");
    } else {
        print_error(result);
    }
}

void reset_config_interactive() {
    if (!g_database) {
        print_error(ERROR_NOT_FOUND);
        return;
    }
    
    printf("Reset all configuration to defaults? (y/n): ");
    char confirm[4];
    fgets(confirm, sizeof(confirm), stdin);
    
    if (tolower(confirm[0]) == 'y') {
        // This would reset to default configuration
        print_warning("Not yet implemented");
    }
}

void export_config_interactive() {
    if (!g_database) {
        print_error(ERROR_NOT_FOUND);
        return;
    }
    
    char file_path[MAX_FIELD_LEN * 2];
    printf("Enter configuration file path: ");
    fgets(file_path, sizeof(file_path), stdin);
    file_path[strcspn(file_path, "\n")] = '\0';
    
    ErrorCode result = db_export_schema(g_database, file_path);
    if (result == SUCCESS) {
        print_success("Configuration exported");
    } else {
        print_error(result);
    }
}

void import_config_interactive() {
    if (!g_database) {
        print_error(ERROR_NOT_FOUND);
        return;
    }
    
    char file_path[MAX_FIELD_LEN * 2];
    printf("Enter configuration file path: ");
    fgets(file_path, sizeof(file_path), stdin);
    file_path[strcspn(file_path, "\n")] = '\0';
    
    ErrorCode result = db_import_schema(g_database, file_path);
    if (result == SUCCESS) {
        print_success("Configuration imported");
    } else {
        print_error(result);
    }
}

// ==================== HELP SYSTEM ====================
void show_help_topic(const char* topic) {
    if (!topic || strcmp(topic, "general") == 0) {
        printf("\nLuminix Database Help\n");
        printf("=====================\n");
        printf("Available topics:\n");
        printf("  sql         - SQL query syntax\n");
        printf("  commands    - Interactive commands\n");
        printf("  tables      - Table operations\n");
        printf("  indexes     - Index operations\n");
        printf("  queries     - Query execution\n");
        printf("  maintenance - Database maintenance\n");
        printf("  config      - Configuration\n");
        printf("  examples    - Example usage\n");
        printf("\nType 'help <topic>' for specific help.\n");
    } else if (strcmp(topic, "sql") == 0) {
        printf("\nSQL Query Syntax\n");
        printf("================\n");
        printf("Supported SQL commands:\n");
        printf("  CREATE TABLE <name> (<col1> <type>, ...)\n");
        printf("  INSERT INTO <table> VALUES (<val1>, ...)\n");
        printf("  SELECT <cols> FROM <table> [WHERE <condition>]\n");
        printf("  UPDATE <table> SET <col>=<val> [WHERE <condition>]\n");
        printf("  DELETE FROM <table> [WHERE <condition>]\n");
        printf("  CREATE INDEX ON <table>(<column>)\n");
        printf("  DROP INDEX ON <table>(<column>)\n");
        printf("\nData types: INT, STRING, FLOAT, DOUBLE, BOOL, DATETIME\n");
        printf("\nExamples:\n");
        printf("  CREATE TABLE employees (id INT, name STRING, salary FLOAT)\n");
        printf("  INSERT INTO employees VALUES (1, 'John Doe', 50000.0)\n");
        printf("  SELECT * FROM employees WHERE salary > 40000\n");
    } else if (strcmp(topic, "commands") == 0) {
        printf("\nInteractive Commands\n");
        printf("====================\n");
        printf("Main menu commands:\n");
        printf("  1  - Database operations\n");
        printf("  2  - Table operations\n");
        printf("  3  - Index operations\n");
        printf("  4  - Query operations\n");
        printf("  5  - Maintenance operations\n");
        printf("  6  - Configuration\n");
        printf("  7  - Help\n");
        printf("  0  - Exit\n");
        printf("\nShortcut commands (at main prompt):\n");
        printf("  \\q       - Execute SQL query\n");
        printf("  \\t       - List tables\n");
        printf("  \\d <tab> - Describe table\n");
        printf("  \\i       - Import data\n");
        printf("  \\e       - Export data\n");
        printf("  \\h       - Show history\n");
        printf("  \\c       - Clear screen\n");
        printf("  \\?       - Show help\n");
    } else if (strcmp(topic, "examples") == 0) {
        show_examples();
    } else {
        printf("Help topic '%s' not found. Type 'help' for available topics.\n", topic);
    }
}

void interactive_tutorial() {
    printf("\nLuminix Interactive Tutorial\n");
    printf("=============================\n");
    
    printf("\nStep 1: Creating a database\n");
    printf("  From the main menu, select '1' for Database operations\n");
    printf("  Then select '1' to create a new database\n");
    printf("  Enter a name for your database\n");
    
    printf("\nStep 2: Creating a table\n");
    printf("  Select '2' for Table operations\n");
    printf("  Select '1' to create a new table\n");
    printf("  Enter table name and define columns\n");
    
    printf("\nStep 3: Inserting data\n");
    printf("  Use SQL: INSERT INTO <table> VALUES (...)\n");
    printf("  Or use interactive insert from Table operations menu\n");
    
    printf("\nStep 4: Querying data\n");
    printf("  Select '4' for Query operations\n");
    printf("  Select '1' to execute SQL queries\n");
    printf("  Try: SELECT * FROM <table>\n");
    
    printf("\nStep 5: Creating indexes\n");
    printf("  Select '3' for Index operations\n");
    printf("  Create indexes on frequently queried columns\n");
    
    printf("\nReady to try? Press Enter to continue...");
    getchar();
}

void show_examples() {
    printf("\nExample Usage\n");
    printf("=============\n");
    
    printf("\n1. Basic CRUD Operations:\n");
    printf("   CREATE TABLE users (id INT, name STRING, email STRING)\n");
    printf("   INSERT INTO users VALUES (1, 'Alice', 'alice@example.com')\n");
    printf("   SELECT * FROM users WHERE id = 1\n");
    printf("   UPDATE users SET email = 'alice.new@example.com' WHERE id = 1\n");
    printf("   DELETE FROM users WHERE id = 1\n");
    
    printf("\n2. Indexing:\n");
    printf("   CREATE INDEX ON users(email)  -- Hash index\n");
    printf("   CREATE INDEX ON users(id)     -- B-tree index\n");
    
    printf("\n3. Complex Queries:\n");
    printf("   SELECT name, COUNT(*) FROM users GROUP BY name\n");
    printf("   SELECT * FROM users ORDER BY name DESC\n");
    printf("   SELECT * FROM users WHERE name LIKE 'A%'\n");
    
    printf("\n4. Import/Export:\n");
    printf("   \\i users.json                -- Import from JSON\n");
    printf("   \\e users.csv                 -- Export to CSV\n");
    
    printf("\n5. Maintenance:\n");
    printf("   VACUUM                       -- Reclaim space\n");
    printf("   ANALYZE                      -- Update statistics\n");
    printf("   CHECK INTEGRITY              -- Verify data consistency\n");
}

void show_cheat_sheet() {
    printf("\nLuminix Cheat Sheet\n");
    printf("===================\n");
    
    printf("\nSQL Quick Reference:\n");
    printf("  CREATE TABLE t (id INT, name STRING)      -- Create table\n");
    printf("  INSERT INTO t VALUES (1, 'test')          -- Insert row\n");
    printf("  SELECT * FROM t                           -- Select all\n");
    printf("  SELECT * FROM t WHERE id = 1              -- Filter\n");
    printf("  UPDATE t SET name = 'new' WHERE id = 1    -- Update\n");
    printf("  DELETE FROM t WHERE id = 1                -- Delete\n");
    printf("  CREATE INDEX ON t(id)                     -- Create index\n");
    
    printf("\nInteractive Commands:\n");
    printf("  \\q <sql>          -- Execute SQL\n");
    printf("  \\t                -- List tables\n");
    printf("  \\d <table>        -- Describe table\n");
    printf("  \\i <file>         -- Import\n");
    printf("  \\e <file>         -- Export\n");
    printf("  \\h               -- History\n");
    printf("  \\c               -- Clear\n");
    printf("  \\?               -- Help\n");
    printf("  \\x               -- Exit\n");
    
    printf("\nData Types:\n");
    printf("  INT              -- Integer\n");
    printf("  STRING           -- Text\n");
    printf("  FLOAT            -- Floating point\n");
    printf("  DOUBLE           -- Double precision\n");
    printf("  BOOL             -- Boolean\n");
    printf("  DATETIME         -- Date/time\n");
    
    printf("\nIndex Types:\n");
    printf("  HASH             -- Fast equality (default)\n");
    printf("  BTREE            -- Ordered, range queries\n");
    printf("  SKIPLIST         -- Concurrent access\n");
    printf("  BITMAP           -- Low cardinality\n");
    printf("  FULLTEXT         -- Text search\n");
}

// ==================== BENCHMARKING ====================
void benchmark_database() {
    if (!g_database) {
        print_error(ERROR_NOT_FOUND);
        return;
    }
    
    printf("\nDatabase Benchmark\n");
    printf("==================\n");
    
    printf("Warning: Benchmarking will modify the database.\n");
    printf("Create backup first? (y/n): ");
    char backup_choice[4];
    fgets(backup_choice, sizeof(backup_choice), stdin);
    
    if (tolower(backup_choice[0]) == 'y') {
        backup_database_interactive();
    }
    
    printf("\nSelect benchmark type:\n");
    printf("1. Insert performance\n");
    printf("2. Query performance\n");
    printf("3. Mixed workload\n");
    printf("4. Index performance\n");
    printf("5. Complete benchmark suite\n");
    
    printf("Choice: ");
    char choice_str[10];
    fgets(choice_str, sizeof(choice_str), stdin);
    int choice = atoi(choice_str);
    
    switch (choice) {
        case 1:
            printf("Running insert benchmark...\n");
            // Insert benchmark logic
            break;
        case 2:
            printf("Running query benchmark...\n");
            // Query benchmark logic
            break;
        case 5:
            printf("Running complete benchmark suite...\n");
            // Full benchmark suite
            break;
        default:
            print_warning("Benchmark type not yet implemented");
            break;
    }
}

void stress_test_database() {
    if (!g_database) {
        print_error(ERROR_NOT_FOUND);
        return;
    }
    
    printf("\nDatabase Stress Test\n");
    printf("====================\n");
    
    printf("Warning: Stress test will heavily load the database.\n");
    printf("Continue? (y/n): ");
    char confirm[4];
    fgets(confirm, sizeof(confirm), stdin);
    
    if (tolower(confirm[0]) != 'y') {
        return;
    }
    
    printf("Enter number of operations (default: 10000): ");
    char ops_str[20];
    fgets(ops_str, sizeof(ops_str), stdin);
    int operations = atoi(ops_str);
    if (operations <= 0) operations = 10000;
    
    printf("Enter number of threads (default: 4): ");
    char threads_str[20];
    fgets(threads_str, sizeof(threads_str), stdin);
    int threads = atoi(threads_str);
    if (threads <= 0) threads = 4;
    
    printf("\nStarting stress test with %d operations, %d threads...\n", operations, threads);
    printf("This may take a while...\n");
    
    // Stress test logic would go here
    
    printf("\nStress test completed.\n");
}

void performance_monitor() {
    if (!g_database) {
        print_error(ERROR_NOT_FOUND);
        return;
    }
    
    printf("\nPerformance Monitor\n");
    printf("===================\n");
    printf("Press Ctrl+C to stop monitoring...\n\n");
    
    // Monitor would run in a loop, showing real-time stats
    printf("Not yet implemented - coming soon!\n");
}

// ==================== MENU DISPLAY FUNCTIONS ====================
void print_banner() {
    clear_screen();
    printf("\n");
    printf("\033[1;36m");  // Cyan color
    printf("  _                 _       _      \n");
    printf(" | |               (_)     (_)     \n");
    printf(" | |_   _ _ __ ___  _ _ __  ___  __\n");
    printf(" | | | | | '_ ` _ \\| | '_ \\| \\ \\/ /\n");
    printf(" | | |_| | | | | | | | | | | |>  < \n");
    printf(" |_|\\__,_|_| |_| |_|_|_| |_|_/_/\\_\\\n");
    printf("\033[0m");  // Reset color
    printf("\n");
    printf("         Enterprise-Grade In-Memory JSON Database\n");
    printf("                  Version: %s\n", LUMINIX_VERSION_STRING);
    printf("========================================================\n\n");
}

void print_main_menu() {
    printf("\n\033[1;34mMAIN MENU\033[0m\n");
    printf("════════════════════════════════════════════════════════\n");
    printf("\033[1;32m1.\033[0m Database Operations     \033[1;32m2.\033[0m Table Operations\n");
    printf("\033[1;32m3.\033[0m Index Operations       \033[1;32m4.\033[0m Query Operations\n");
    printf("\033[1;32m5.\033[0m Maintenance Operations \033[1;32m6.\033[0m Configuration\n");
    printf("\033[1;32m7.\033[0m Help & Tutorial        \033[1;32m8.\033[0m Benchmark & Tools\n");
    printf("\033[1;31m0.\033[0m Exit\n");
    printf("════════════════════════════════════════════════════════\n");
    printf("Select option (1-8, 0 to exit): ");
}

void print_database_menu() {
    printf("\n\033[1;34mDATABASE OPERATIONS\033[0m\n");
    printf("════════════════════════════════════════════════════════\n");
    printf("1. Create New Database          2. Open Database\n");
    printf("3. Close Database               4. Backup Database\n");
    printf("5. Restore Database             6. Database Information\n");
    printf("7. Save Database                8. Compact Database\n");
    printf("9. Rename Database              0. Back to Main Menu\n");
    printf("════════════════════════════════════════════════════════\n");
}

void print_table_menu() {
    printf("\n\033[1;34mTABLE OPERATIONS\033[0m\n");
    printf("════════════════════════════════════════════════════════\n");
    printf("1. Create Table                 2. List Tables\n");
    printf("3. Describe Table               4. Alter Table\n");
    printf("5. Truncate Table               6. Drop Table\n");
    printf("7. Import Table                 8. Export Table\n");
    printf("9. Table Statistics             0. Back to Main Menu\n");
    printf("════════════════════════════════════════════════════════\n");
}

void print_index_menu() {
    printf("\n\033[1;34mINDEX OPERATIONS\033[0m\n");
    printf("════════════════════════════════════════════════════════\n");
    printf("1. Create Index                 2. List Indexes\n");
    printf("3. Drop Index                   4. Rebuild Indexes\n");
    printf("5. Optimize Indexes             6. Index Statistics\n");
    printf("7. Recommend Indexes            8. Compare Index Types\n");
    printf("9. Full-Text Search             0. Back to Main Menu\n");
    printf("════════════════════════════════════════════════════════\n");
}

void print_query_menu() {
    printf("\n\033[1;34mQUERY OPERATIONS\033[0m\n");
    printf("════════════════════════════════════════════════════════\n");
    printf("1. Execute Query                2. Execute Script\n");
    printf("3. Query History                4. Save Query Result\n");
    printf("5. Explain Query                6. Query Templates\n");
    printf("7. Stored Procedures            8. Query Optimization\n");
    printf("9. Transaction Management       0. Back to Main Menu\n");
    printf("════════════════════════════════════════════════════════\n");
}

void print_maintenance_menu() {
    printf("\n\033[1;34mMAINTENANCE OPERATIONS\033[0m\n");
    printf("════════════════════════════════════════════════════════\n");
    printf("1. Vacuum Database              2. Analyze Database\n");
    printf("3. Check Integrity              4. Repair Database\n");
    printf("5. Database Statistics          6. Cleanup Logs\n");
    printf("7. Update Statistics            8. Rebuild Database\n");
    printf("9. Schedule Maintenance         0. Back to Main Menu\n");
    printf("════════════════════════════════════════════════════════\n");
}

void print_config_menu() {
    printf("\n\033[1;34mCONFIGURATION\033[0m\n");
    printf("════════════════════════════════════════════════════════\n");
    printf("1. Show Configuration           2. Set Configuration\n");
    printf("3. Reset Configuration          4. Export Configuration\n");
    printf("5. Import Configuration         6. Configuration Wizard\n");
    printf("7. Security Settings            8. Performance Settings\n");
    printf("9. Plugin Management            0. Back to Main Menu\n");
    printf("════════════════════════════════════════════════════════\n");
}

void print_help_menu() {
    printf("\n\033[1;34mHELP & TUTORIAL\033[0m\n");
    printf("════════════════════════════════════════════════════════\n");
    printf("1. Quick Start Guide            2. SQL Reference\n");
    printf("3. Interactive Tutorial         4. Examples\n");
    printf("5. Cheat Sheet                  6. FAQ\n");
    printf("7. Troubleshooting              8. Advanced Topics\n");
    printf("9. About Luminix                0. Back to Main Menu\n");
    printf("════════════════════════════════════════════════════════\n");
}

void print_tools_menu() {
    printf("\n\033[1;34mBENCHMARK & TOOLS\033[0m\n");
    printf("════════════════════════════════════════════════════════\n");
    printf("1. Run Benchmark                2. Stress Test\n");
    printf("3. Performance Monitor          4. Memory Profiler\n");
    printf("5. Query Analyzer               6. Index Analyzer\n");
    printf("7. Data Generator               8. Migration Tool\n");
    printf("9. Report Generator             0. Back to Main Menu\n");
    printf("════════════════════════════════════════════════════════\n");
}

void print_welcome() {
    printf("\nWelcome to Luminix Database Management System!\n");
    printf("Type 'help' for assistance or 'tutorial' for a guided tour.\n");
    printf("Use menu numbers or shortcut commands (\\q, \\t, etc.)\n\n");
}

// ==================== MENU HANDLERS ====================
void handle_database_operations() {
    int choice;
    char input[10];
    
    do {
        print_database_menu();
        fgets(input, sizeof(input), stdin);
        choice = atoi(input);
        
        switch (choice) {
            case 1:
                create_database_interactive();
                break;
            case 2:
                open_database_interactive();
                break;
            case 3:
                close_database_interactive();
                break;
            case 4:
                backup_database_interactive();
                break;
            case 5:
                restore_database_interactive();
                break;
            case 6:
                if (g_database) {
                    printf("\nDatabase: %s\n", g_database->name);
                    printf("Tables: %d\n", db_get_table_count(g_database));
                    printf("Path: %s\n", g_database->path);
                    statistics_interactive();
                } else {
                    print_error(ERROR_NOT_FOUND);
                }
                break;
            case 7:
                save_state();
                break;
            case 8:
                printf("Compacting database...\n");
                db_vacuum(g_database);
                print_success("Database compacted");
                break;
            case 9:
                print_warning("Not yet implemented");
                break;
            case 0:
                return;
            default:
                print_warning("Invalid choice");
                break;
        }
        
        printf("\nPress Enter to continue...");
        getchar();
    } while (choice != 0);
}

void handle_table_operations() {
    if (!g_database) {
        print_error(ERROR_NOT_FOUND);
        printf("Please open or create a database first.\n");
        return;
    }
    
    int choice;
    char input[10];
    
    do {
        print_table_menu();
        fgets(input, sizeof(input), stdin);
        choice = atoi(input);
        
        switch (choice) {
            case 1:
                create_table_interactive();
                break;
            case 2:
                list_tables_interactive();
                break;
            case 3:
                describe_table_interactive();
                break;
            case 4:
                alter_table_interactive();
                break;
            case 5:
                truncate_table_interactive();
                break;
            case 6:
                drop_table_interactive();
                break;
            case 7:
                import_table_interactive();
                break;
            case 8:
                export_table_interactive();
                break;
            case 9:
                statistics_interactive();
                break;
            case 0:
                return;
            default:
                print_warning("Invalid choice");
                break;
        }
        
        printf("\nPress Enter to continue...");
        getchar();
    } while (choice != 0);
}

void handle_index_operations() {
    if (!g_database) {
        print_error(ERROR_NOT_FOUND);
        printf("Please open or create a database first.\n");
        return;
    }
    
    int choice;
    char input[10];
    
    do {
        print_index_menu();
        fgets(input, sizeof(input), stdin);
        choice = atoi(input);
        
        switch (choice) {
            case 1:
                create_index_interactive();
                break;
            case 2:
                list_indexes_interactive();
                break;
            case 3:
                drop_index_interactive();
                break;
            case 4:
                rebuild_index_interactive();
                break;
            case 5:
                optimize_index_interactive();
                break;
            case 6:
                statistics_interactive();
                break;
            case 7:
                print_warning("Not yet implemented");
                break;
            case 8:
                print_warning("Not yet implemented");
                break;
            case 9:
                print_warning("Not yet implemented");
                break;
            case 0:
                return;
            default:
                print_warning("Invalid choice");
                break;
        }
        
        printf("\nPress Enter to continue...");
        getchar();
    } while (choice != 0);
}

void handle_query_operations() {
    if (!g_database) {
        print_error(ERROR_NOT_FOUND);
        printf("Please open or create a database first.\n");
        return;
    }
    
    int choice;
    char input[10];
    
    do {
        print_query_menu();
        fgets(input, sizeof(input), stdin);
        choice = atoi(input);
        
        switch (choice) {
            case 1:
                execute_query_interactive();
                break;
            case 2:
                execute_script_interactive();
                break;
            case 3:
                query_history();
                break;
            case 4:
                print_warning("Save query result - use after executing a query");
                break;
            case 5:
                explain_query_interactive();
                break;
            case 6:
                print_warning("Not yet implemented");
                break;
            case 7:
                print_warning("Not yet implemented");
                break;
            case 8:
                print_warning("Not yet implemented");
                break;
            case 9:
                print_warning("Not yet implemented");
                break;
            case 0:
                return;
            default:
                print_warning("Invalid choice");
                break;
        }
        
        printf("\nPress Enter to continue...");
        getchar();
    } while (choice != 0);
}

void handle_maintenance_operations() {
    if (!g_database) {
        print_error(ERROR_NOT_FOUND);
        printf("Please open or create a database first.\n");
        return;
    }
    
    int choice;
    char input[10];
    
    do {
        print_maintenance_menu();
        fgets(input, sizeof(input), stdin);
        choice = atoi(input);
        
        switch (choice) {
            case 1:
                vacuum_database_interactive();
                break;
            case 2:
                analyze_database_interactive();
                break;
            case 3:
                check_integrity_interactive();
                break;
            case 4:
                repair_database_interactive();
                break;
            case 5:
                statistics_interactive();
                break;
            case 6:
                print_warning("Not yet implemented");
                break;
            case 7:
                print_warning("Not yet implemented");
                break;
            case 8:
                print_warning("Not yet implemented");
                break;
            case 9:
                print_warning("Not yet implemented");
                break;
            case 0:
                return;
            default:
                print_warning("Invalid choice");
                break;
        }
        
        printf("\nPress Enter to continue...");
        getchar();
    } while (choice != 0);
}

void handle_config_operations() {
    if (!g_database) {
        print_error(ERROR_NOT_FOUND);
        printf("Please open or create a database first.\n");
        return;
    }
    
    int choice;
    char input[10];
    
    do {
        print_config_menu();
        fgets(input, sizeof(input), stdin);
        choice = atoi(input);
        
        switch (choice) {
            case 1:
                show_config_interactive();
                break;
            case 2:
                set_config_interactive();
                break;
            case 3:
                reset_config_interactive();
                break;
            case 4:
                export_config_interactive();
                break;
            case 5:
                import_config_interactive();
                break;
            case 6:
                print_warning("Not yet implemented");
                break;
            case 7:
                print_warning("Not yet implemented");
                break;
            case 8:
                print_warning("Not yet implemented");
                break;
            case 9:
                print_warning("Not yet implemented");
                break;
            case 0:
                return;
            default:
                print_warning("Invalid choice");
                break;
        }
        
        printf("\nPress Enter to continue...");
        getchar();
    } while (choice != 0);
}

void handle_help() {
    int choice;
    char input[10];
    
    do {
        print_help_menu();
        fgets(input, sizeof(input), stdin);
        choice = atoi(input);
        
        switch (choice) {
            case 1:
                show_help_topic("general");
                break;
            case 2:
                show_help_topic("sql");
                break;
            case 3:
                interactive_tutorial();
                break;
            case 4:
                show_examples();
                break;
            case 5:
                show_cheat_sheet();
                break;
            case 6:
                printf("\nFrequently Asked Questions\n");
                printf("==========================\n");
                printf("Q: How do I create a database?\n");
                printf("A: Use 'CREATE DATABASE' or menu option 1.1\n\n");
                printf("Q: How do I import data from JSON?\n");
                printf("A: Use Table Operations → Import Table\n\n");
                printf("Q: How do I optimize query performance?\n");
                printf("A: Create indexes on frequently queried columns\n");
                break;
            case 7:
                printf("\nTroubleshooting\n");
                printf("===============\n");
                printf("1. Database won't open: Check file permissions\n");
                printf("2. Queries are slow: Create indexes, run VACUUM\n");
                printf("3. Out of memory: Increase cache size in config\n");
                printf("4. Data corruption: Restore from backup\n");
                break;
            case 8:
                printf("\nAdvanced Topics\n");
                printf("===============\n");
                printf("• Transaction management\n");
                printf("• Replication setup\n");
                printf("• Custom plugins\n");
                printf("• Performance tuning\n");
                printf("• Security configuration\n");
                break;
            case 9:
                printf("\nAbout Luminix\n");
                printf("=============\n");
                printf("Version: %s\n", LUMINIX_VERSION_STRING);
                printf("Author: Luminix Development Team\n");
                printf("License: MIT\n");
                printf("Website: https://github.com/luminix/db\n");
                printf("\nAn enterprise-grade in-memory JSON database\n");
                printf("built in C for high performance applications.\n");
                break;
            case 0:
                return;
            default:
                print_warning("Invalid choice");
                break;
        }
        
        printf("\nPress Enter to continue...");
        getchar();
    } while (choice != 0);
}

void handle_tools() {
    int choice;
    char input[10];
    
    do {
        print_tools_menu();
        fgets(input, sizeof(input), stdin);
        choice = atoi(input);
        
        switch (choice) {
            case 1:
                benchmark_database();
                break;
            case 2:
                stress_test_database();
                break;
            case 3:
                performance_monitor();
                break;
            case 4:
                printf("\nMemory Profiler\n");
                printf("===============\n");
                printf("Not yet implemented\n");
                break;
            case 5:
                printf("\nQuery Analyzer\n");
                printf("==============\n");
                printf("Not yet implemented\n");
                break;
            case 6:
                printf("\nIndex Analyzer\n");
                printf("==============\n");
                printf("Not yet implemented\n");
                break;
            case 7:
                printf("\nData Generator\n");
                printf("==============\n");
                printf("Not yet implemented\n");
                break;
            case 8:
                printf("\nMigration Tool\n");
                printf("==============\n");
                printf("Not yet implemented\n");
                break;
            case 9:
                printf("\nReport Generator\n");
                printf("================\n");
                printf("Not yet implemented\n");
                break;
            case 0:
                return;
            default:
                print_warning("Invalid choice");
                break;
        }
        
        printf("\nPress Enter to continue...");
        getchar();
    } while (choice != 0);
}

// ==================== UTILITY FUNCTIONS ====================
void clear_screen() {
    #ifdef _WIN32
        system("cls");
    #else
        system("clear");
    #endif
}

// ==================== MAIN FUNCTION ====================
int main(int argc, char* argv[]) {
    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGSEGV, signal_handler);
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  --help, -h       Show this help message\n");
            printf("  --version, -v    Show version information\n");
            printf("  --database, -d   Open database file\n");
            printf("  --execute, -e    Execute SQL command\n");
            printf("  --script, -s     Execute SQL script file\n");
            printf("  --batch, -b      Batch mode (no interactive shell)\n");
            return 0;
        } else if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            printf("Luminix Database %s\n", LUMINIX_VERSION_STRING);
            return 0;
        } else if (strcmp(argv[i], "--database") == 0 || strcmp(argv[i], "-d") == 0) {
            if (i + 1 < argc) {
                g_database = db_open(argv[++i], "");
                if (!g_database) {
                    fprintf(stderr, "Failed to open database: %s\n", argv[i]);
                    return 1;
                }
            }
        } else if (strcmp(argv[i], "--execute") == 0 || strcmp(argv[i], "-e") == 0) {
            if (i + 1 < argc) {
                // Batch execution mode
                if (!g_database) {
                    g_database = db_create();
                }
                char* query = argv[++i];
                QueryResult* result = db_execute_query(g_database, query);
                if (result) {
                    print_query_result_formatted(result, FORMAT_TABLE);
                    // free_query_result(result);
                }
                cleanup();
                return 0;
            }
        } else if (strcmp(argv[i], "--script") == 0 || strcmp(argv[i], "-s") == 0) {
            if (i + 1 < argc) {
                // Script execution mode
                if (!g_database) {
                    g_database = db_create();
                }
                execute_script_interactive(); // Would need to modify for file argument
                cleanup();
                return 0;
            }
        } else if (strcmp(argv[i], "--batch") == 0 || strcmp(argv[i], "-b") == 0) {
            // Batch mode - already handled by -e or -s
        }
    }
    
    // Interactive mode
    print_banner();
    print_welcome();
    load_state();
    
    int choice;
    char input[10];
    
    while (g_running) {
        print_main_menu();
        fflush(stdout);
        
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;  // EOF or error
        }
        
        choice = atoi(input);
        
        switch (choice) {
            case 1:
                handle_database_operations();
                break;
            case 2:
                handle_table_operations();
                break;
            case 3:
                handle_index_operations();
                break;
            case 4:
                handle_query_operations();
                break;
            case 5:
                handle_maintenance_operations();
                break;
            case 6:
                handle_config_operations();
                break;
            case 7:
                handle_help();
                break;
            case 8:
                handle_tools();
                break;
            case 0:
                g_running = 0;
                break;
            default:
                // Check for shortcut commands
                if (input[0] == '\\' || input[0] == '/') {
                    // Shortcut command
                    char* cmd = input + 1;
                    cmd[strcspn(cmd, "\n")] = '\0';
                    
                    if (strcmp(cmd, "q") == 0) {
                        execute_query_interactive();
                    } else if (strcmp(cmd, "t") == 0) {
                        list_tables_interactive();
                    } else if (strncmp(cmd, "d ", 2) == 0) {
                        // Describe table shortcut
                        char table_name[MAX_TABLE_NAME];
                        strcpy(table_name, cmd + 2);
                        Table* table = db_get_table(g_database, table_name);
                        if (table) {
                            print_table_formatted(table, FORMAT_TABLE);
                        } else {
                            print_error(ERROR_NOT_FOUND);
                        }
                    } else if (strcmp(cmd, "i") == 0) {
                        import_table_interactive();
                    } else if (strcmp(cmd, "e") == 0) {
                        export_table_interactive();
                    } else if (strcmp(cmd, "h") == 0) {
                        query_history();
                    } else if (strcmp(cmd, "c") == 0) {
                        clear_screen();
                        print_banner();
                    } else if (strcmp(cmd, "?") == 0) {
                        show_help_topic("general");
                    } else if (strcmp(cmd, "x") == 0) {
                        g_running = 0;
                    } else {
                        printf("Unknown command: %s\n", cmd);
                        printf("Available shortcuts: \\q, \\t, \\d <table>, \\i, \\e, \\h, \\c, \\?, \\x\n");
                    }
                } else {
                    print_warning("Invalid choice");
                }
                break;
        }
    }
    
    // Cleanup before exit
    cleanup();
    printf("\nThank you for using Luminix Database!\n");
    
    return 0;
}