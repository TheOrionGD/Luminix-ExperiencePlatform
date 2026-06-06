#include "parser.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static char* trim_spaces(const char* str) {
    if (!str) return NULL;
    while (isspace((unsigned char)*str)) str++;
    if (*str == '\0') return strdup("");
    const char* end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    size_t len = end - str + 1;
    char* trimmed = malloc(len + 1);
    if (trimmed) {
        memcpy(trimmed, str, len);
        trimmed[len] = '\0';
    }
    return trimmed;
}

static char* extract_token(const char* str, const char* start_keyword, const char* end_characters) {
    if (!str || !start_keyword) return NULL;
    char* lower_str = strdup(str);
    if (!lower_str) return NULL;
    for (int i = 0; lower_str[i]; i++) lower_str[i] = tolower(lower_str[i]);
    char* lower_kw = strdup(start_keyword);
    if (!lower_kw) {
        free(lower_str);
        return NULL;
    }
    for (int i = 0; lower_kw[i]; i++) lower_kw[i] = tolower(lower_kw[i]);
    
    char* pos = strstr(lower_str, lower_kw);
    if (!pos) {
        free(lower_str);
        free(lower_kw);
        return NULL;
    }
    
    size_t start_idx = (pos - lower_str) + strlen(start_keyword);
    const char* actual_start = str + start_idx;
    while (isspace((unsigned char)*actual_start)) actual_start++;
    
    size_t token_len = 0;
    while (actual_start[token_len] != '\0' && !strchr(end_characters, actual_start[token_len])) {
        token_len++;
    }
    
    char* result = malloc(token_len + 1);
    if (result) {
        memcpy(result, actual_start, token_len);
        result[token_len] = '\0';
        char* trimmed = trim_spaces(result);
        free(result);
        result = trimmed;
    }
    
    free(lower_str);
    free(lower_kw);
    return result;
}

Query* parse_query(const char* query_str) {
    if (!query_str) return NULL;
    
    Query* query = malloc(sizeof(Query));
    if (!query) return NULL;
    
    query->type = QUERY_SELECT;
    query->table = NULL;
    query->fields = NULL;
    query->field_count = 0;
    query->conditions = NULL;
    query->condition_count = 0;
    
    while (isspace((unsigned char)*query_str)) query_str++;
    
    char* lower_query = strdup(query_str);
    if (!lower_query) {
        free(query);
        return NULL;
    }
    for (int i = 0; lower_query[i]; i++) lower_query[i] = tolower(lower_query[i]);
    
    if (strncmp(lower_query, "select", 6) == 0) {
        query->type = QUERY_SELECT;
        query->table = extract_token(query_str, "from", " \t\r\n(Ww");
        
        char* fields_token = extract_token(query_str, "select", "Ff");
        if (fields_token) {
            int count = 0;
            char* temp = strdup(fields_token);
            char* tok = strtok(temp, ",");
            while (tok) {
                count++;
                tok = strtok(NULL, ",");
            }
            free(temp);
            
            if (count > 0) {
                query->fields = malloc(count * sizeof(char*));
                query->field_count = count;
                temp = strdup(fields_token);
                tok = strtok(temp, ",");
                int idx = 0;
                while (tok) {
                    query->fields[idx++] = trim_spaces(tok);
                    tok = strtok(NULL, ",");
                }
                free(temp);
            }
            free(fields_token);
        }
    } else if (strncmp(lower_query, "insert", 6) == 0) {
        query->type = QUERY_INSERT;
        query->table = extract_token(query_str, "into", " \t\r\n(");
    } else if (strncmp(lower_query, "update", 6) == 0) {
        query->type = QUERY_UPDATE;
        query->table = extract_token(query_str, "update", " \t\r\nS");
    } else if (strncmp(lower_query, "delete", 6) == 0) {
        query->type = QUERY_DELETE;
        query->table = extract_token(query_str, "from", " \t\r\nW");
    } else if (strncmp(lower_query, "create", 6) == 0) {
        query->type = QUERY_CREATE_TABLE;
        query->table = extract_token(query_str, "table", " \t\r\n(");
    } else if (strncmp(lower_query, "drop", 4) == 0) {
        query->type = QUERY_DROP_TABLE;
        query->table = extract_token(query_str, "table", " \t\r\n;");
    }
    
    char* where_pos = strstr(lower_query, "where");
    if (where_pos) {
        size_t where_idx = where_pos - lower_query + 5;
        const char* conds_part = query_str + where_idx;
        while (isspace((unsigned char)*conds_part)) conds_part++;
        
        Condition* cond = parse_condition(conds_part);
        if (cond) {
            query->conditions = cond;
            query->condition_count = 1;
        }
    }
    
    free(lower_query);
    return query;
}

void free_query(Query* query) {
    if (query) {
        free(query->table);
        if (query->fields) {
            for (int i = 0; i < query->field_count; i++) {
                free(query->fields[i]);
            }
            free(query->fields);
        }
        if (query->conditions) {
            for (int i = 0; i < query->condition_count; i++) {
                free(query->conditions[i].field);
                free(query->conditions[i].op);
                free(query->conditions[i].value);
            }
            free(query->conditions);
        }
        free(query);
    }
}

Condition* parse_condition(const char* cond_str) {
    if (!cond_str) return NULL;
    Condition* cond = malloc(sizeof(Condition));
    if (!cond) return NULL;
    
    cond->field = NULL;
    cond->op = NULL;
    cond->value = NULL;
    
    const char* ops[] = { "!=", ">=", "<=", "=", ">", "<" };
    int op_found_idx = -1;
    const char* op_pos = NULL;
    for (int i = 0; i < 6; i++) {
        op_pos = strstr(cond_str, ops[i]);
        if (op_pos) {
            op_found_idx = i;
            break;
        }
    }
    
    if (!op_pos) {
        free(cond);
        return NULL;
    }
    
    size_t field_len = op_pos - cond_str;
    char* field = malloc(field_len + 1);
    if (field) {
        memcpy(field, cond_str, field_len);
        field[field_len] = '\0';
        cond->field = trim_spaces(field);
        free(field);
    }
    
    cond->op = strdup(ops[op_found_idx]);
    
    const char* val_start = op_pos + strlen(ops[op_found_idx]);
    cond->value = trim_spaces(val_start);
    
    if (cond->value && (cond->value[0] == '\'' || cond->value[0] == '"')) {
        size_t vlen = strlen(cond->value);
        if (vlen >= 2 && cond->value[vlen-1] == cond->value[0]) {
            char* stripped = malloc(vlen - 1);
            if (stripped) {
                memcpy(stripped, cond->value + 1, vlen - 2);
                stripped[vlen - 2] = '\0';
                free(cond->value);
                cond->value = stripped;
            }
        }
    }
    
    return cond;
}
