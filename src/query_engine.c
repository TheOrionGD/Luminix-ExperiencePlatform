#include "query_engine.h"
#include "parser.h"
#include "database.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

extern Database* g_database;

EngineQueryResult* query_execute(const char* query) {
    EngineQueryResult* result = malloc(sizeof(EngineQueryResult));
    if (!result) return NULL;
    
    result->success = 0;
    result->message = NULL;
    result->data = NULL;
    result->row_count = 0;
    result->affected_rows = 0;
    
    if (!query || strlen(query) == 0) {
        result->message = strdup("Query is empty");
        return result;
    }
    
    if (!query_validate(query)) {
        result->message = strdup("Query syntax validation failed");
        return result;
    }
    
    Query* parsed = parse_query(query);
    if (!parsed) {
        result->message = strdup("Failed to parse query");
        return result;
    }
    
    if (!g_database) {
        result->success = 1;
        result->message = strdup("Query validated and parsed (No active database connection)");
        free_query(parsed);
        return result;
    }
    
    QueryResult* db_res = db_execute_query(g_database, query);
    if (db_res) {
        result->success = 1;
        result->message = strdup("Query executed successfully on active database");
        result->row_count = db_res->row_count;
        result->affected_rows = db_res->row_count;
    } else {
        result->success = 0;
        result->message = strdup("Database failed to execute query");
    }
    
    free_query(parsed);
    return result;
}

void query_result_free(EngineQueryResult* result) {
    if (result) {
        free(result->message);
        free(result->data);
        free(result);
    }
}

int query_validate(const char* query) {
    if (!query) return 0;
    while (isspace((unsigned char)*query)) query++;
    char first_word[16];
    int idx = 0;
    while (query[idx] != '\0' && !isspace((unsigned char)query[idx]) && idx < 15) {
        first_word[idx] = tolower((unsigned char)query[idx]);
        idx++;
    }
    first_word[idx] = '\0';
    
    if (strcmp(first_word, "select") == 0 ||
        strcmp(first_word, "insert") == 0 ||
        strcmp(first_word, "update") == 0 ||
        strcmp(first_word, "delete") == 0 ||
        strcmp(first_word, "create") == 0 ||
        strcmp(first_word, "drop") == 0) {
        return 1;
    }
    return 0;
}
