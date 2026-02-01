/**
 * Query Parser Implementation
 */

#include "parser.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

Query* parse_query(const char* query_str) {
    // TODO: Implement query parsing
    Query* query = malloc(sizeof(Query));
    if (!query) return NULL;
    
    query->type = QUERY_SELECT;
    query->table = NULL;
    query->conditions = NULL;
    
    return query;
}

void free_query(Query* query) {
    if (query) {
        free(query->table);
        // TODO: Free conditions
        free(query);
    }
}

Condition* parse_condition(const char* cond_str) {
    // TODO: Implement condition parsing
    return NULL;
}
