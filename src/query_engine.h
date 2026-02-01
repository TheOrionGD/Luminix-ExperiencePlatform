/*
 * Query Engine Header File
 */

#ifndef QUERY_ENGINE_H
#define QUERY_ENGINE_H
#include "config.h"

typedef struct {
    int success;
    char* message;
    void* data;
    int row_count;
    int affected_rows;
} QueryResult;

QueryResult* query_execute(const char* query);
void query_result_free(QueryResult* result);
int query_validate(const char* query);

#endif
