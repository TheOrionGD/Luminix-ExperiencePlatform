#include "cli.h"
#include "database.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

extern Database* g_database;
extern void print_query_result_formatted(QueryResult* result, int format);

static int starts_with_case(const char* str, const char* prefix) {
    size_t prefix_len = strlen(prefix);
    if (strlen(str) < prefix_len) return 0;
    for (size_t i = 0; i < prefix_len; i++) {
        if (tolower((unsigned char)str[i]) != tolower((unsigned char)prefix[i])) {
            return 0;
        }
    }
    return 1;
}

void cli_init(void) {
    printf("Luminix CLI Initialized. Type 'help' for available commands.\n");
}

void cli_handle_command(const char* command) {
    if (!command || strlen(command) == 0) return;
    
    while (isspace((unsigned char)*command)) command++;
    
    if (starts_with_case(command, "help")) {
        printf("Available CLI commands:\n");
        printf("  help                       - Show this help message\n");
        printf("  status                     - Show database connection status\n");
        printf("  exit | quit                - Exit the CLI\n");
        printf("  Any SQL query (e.g. SELECT) - Execute query on the active database\n");
        return;
    }
    
    if (starts_with_case(command, "status")) {
        if (g_database) {
            printf("Connected to database. Active tables: %d\n", db_get_table_count(g_database));
        } else {
            printf("Disconnected (no active database).\n");
        }
        return;
    }
    
    if (starts_with_case(command, "select") || 
        starts_with_case(command, "insert") || 
        starts_with_case(command, "update") || 
        starts_with_case(command, "delete") || 
        starts_with_case(command, "create") || 
        starts_with_case(command, "drop")) {
        
        if (!g_database) {
            printf("Error: No active database. Use the main menu to open or create one first.\n");
            return;
        }
        
        printf("Executing query: %s\n", command);
        QueryResult* result = db_execute_query(g_database, command);
        if (result) {
            print_query_result_formatted(result, 1);
        } else {
            printf("Error executing query.\n");
        }
        return;
    }
    
    printf("Unknown command: %s. Type 'help' for help.\n", command);
}

void cli_run(void) {
    char input[256];
    
    printf("Luminix Database CLI (Type 'exit' to quit)\n");
    
    while (1) {
        printf("> ");
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }
        
        input[strcspn(input, "\n")] = 0;
        
        if (strcmp(input, "exit") == 0 || strcmp(input, "quit") == 0) {
            break;
        }
        
        cli_handle_command(input);
    }
}
