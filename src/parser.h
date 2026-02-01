
/**
 * Parser Header File
 */

#ifndef PARSER_H
#define PARSER_H

typedef enum {
    QUERY_SELECT,
    QUERY_INSERT,
    QUERY_UPDATE,
    QUERY_DELETE,
    QUERY_CREATE,
    QUERY_DROP
} QueryType;

typedef struct {
    char* field;
    char* op;
    char* value;
} Condition;

typedef struct {
    QueryType type;
    char* table;
    char** fields;
    int field_count;
    Condition* conditions;
    int condition_count;
} Query;

Query* parse_query(const char* query_str);
void free_query(Query* query);
Condition* parse_condition(const char* cond_str);

#endif
