#include "json_io.h"
#include "database.h" 
#include "utils.h"
#include "index.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdint.h>

// ==================== INTERNAL MACROS ====================
#define JSON_BUFFER_SIZE 4096
#define JSON_MAX_DEPTH 128
#define JSON_INDENT_SIZE 2
#define JSON_MAX_ERROR_MSG 256

// ==================== INTERNAL STRUCTURES ====================

// JSON parsing context
typedef struct JsonParseContext {
    const char* json;
    size_t position;
    size_t length;
    int depth;
    char error_msg[JSON_MAX_ERROR_MSG];
    Database* db;
    Table* current_table;
    Record* current_record;
} JsonParseContext;

// JSON token types
typedef enum {
    TOKEN_NONE,
    TOKEN_OBJECT_START,    // {
    TOKEN_OBJECT_END,      // }
    TOKEN_ARRAY_START,     // [
    TOKEN_ARRAY_END,       // ]
    TOKEN_STRING,
    TOKEN_NUMBER,
    TOKEN_BOOLEAN,
    TOKEN_NULL,
    TOKEN_COLON,          // :
    TOKEN_COMMA,          // ,
    TOKEN_EOF
} JsonTokenType;

// JSON token
typedef struct JsonToken {
    JsonTokenType type;
    char* value;
    size_t length;
    size_t position;
} JsonToken;

// JSON serializer state
typedef struct JsonSerializeState {
    char* buffer;
    size_t size;
    size_t capacity;
    int indent_level;
    bool pretty;
    bool escape_unicode;
    bool sort_keys;
} JsonSerializeState;

// ==================== INTERNAL FUNCTIONS ====================

// Memory management
static void* json_malloc(size_t size) {
    void* ptr = malloc(size);
    if (!ptr) {
        LOG_ERROR("Failed to allocate %zu bytes for JSON", size);
        return NULL;
    }
    return ptr;
}

static void* json_calloc(size_t count, size_t size) {
    void* ptr = calloc(count, size);
    if (!ptr) {
        LOG_ERROR("Failed to allocate %zu bytes for JSON", count * size);
        return NULL;
    }
    return ptr;
}

static void* json_realloc(void* ptr, size_t size) {
    void* new_ptr = realloc(ptr, size);
    if (!new_ptr && size > 0) {
        LOG_ERROR("Failed to reallocate %zu bytes for JSON", size);
        free(ptr);
        return NULL;
    }
    return new_ptr;
}

// String utilities
static char* json_strdup(const char* str) {
    if (!str) return NULL;
    size_t len = strlen(str) + 1;
    char* copy = (char*)json_malloc(len);
    if (copy) {
        memcpy(copy, str, len);
    }
    return copy;
}

static char* json_strndup(const char* str, size_t n) {
    if (!str) return NULL;
    size_t len = strnlen(str, n);
    char* copy = (char*)json_malloc(len + 1);
    if (copy) {
        memcpy(copy, str, len);
        copy[len] = '\0';
    }
    return copy;
}

// JSON escaping
static size_t json_escape_string(const char* src, char* dst, size_t dst_size) {
    if (!src) return 0;
    
    size_t src_len = strlen(src);
    size_t dst_len = 0;
    
    for (size_t i = 0; i < src_len && dst_len < dst_size - 1; i++) {
        unsigned char c = src[i];
        
        switch (c) {
            case '"':  // Quotation mark
                if (dst_len + 2 < dst_size) {
                    dst[dst_len++] = '\\';
                    dst[dst_len++] = '"';
                }
                break;
            case '\\': // Reverse solidus
                if (dst_len + 2 < dst_size) {
                    dst[dst_len++] = '\\';
                    dst[dst_len++] = '\\';
                }
                break;
            case '\b': // Backspace
                if (dst_len + 2 < dst_size) {
                    dst[dst_len++] = '\\';
                    dst[dst_len++] = 'b';
                }
                break;
            case '\f': // Form feed
                if (dst_len + 2 < dst_size) {
                    dst[dst_len++] = '\\';
                    dst[dst_len++] = 'f';
                }
                break;
            case '\n': // Newline
                if (dst_len + 2 < dst_size) {
                    dst[dst_len++] = '\\';
                    dst[dst_len++] = 'n';
                }
                break;
            case '\r': // Carriage return
                if (dst_len + 2 < dst_size) {
                    dst[dst_len++] = '\\';
                    dst[dst_len++] = 'r';
                }
                break;
            case '\t': // Horizontal tab
                if (dst_len + 2 < dst_size) {
                    dst[dst_len++] = '\\';
                    dst[dst_len++] = 't';
                }
                break;
            default:
                // Control characters (0x00-0x1F)
                if (c < 0x20) {
                    if (dst_len + 6 < dst_size) { // \uXXXX
                        snprintf(dst + dst_len, dst_size - dst_len, "\\u%04x", c);
                        dst_len += 6;
                    }
                } else {
                    dst[dst_len++] = c;
                }
                break;
        }
    }
    
    dst[dst_len] = '\0';
    return dst_len;
}

static char* json_escape_string_alloc(const char* str) {
    if (!str) return NULL;
    
    // Calculate maximum possible escaped length
    size_t max_len = strlen(str) * 6 + 1; // Each char could become \uXXXX
    char* escaped = (char*)json_malloc(max_len);
    if (!escaped) return NULL;
    
    json_escape_string(str, escaped, max_len);
    return escaped;
}

// JSON parsing helpers
static void json_skip_whitespace(JsonParseContext* ctx) {
    while (ctx->position < ctx->length && isspace(ctx->json[ctx->position])) {
        ctx->position++;
    }
}

static bool json_match_string(JsonParseContext* ctx, const char* str) {
    size_t len = strlen(str);
    if (ctx->position + len > ctx->length) {
        return false;
    }
    
    if (strncmp(ctx->json + ctx->position, str, len) == 0) {
        ctx->position += len;
        return true;
    }
    
    return false;
}

static JsonToken json_parse_token(JsonParseContext* ctx) {
    JsonToken token = {TOKEN_NONE, NULL, 0, ctx->position};
    
    json_skip_whitespace(ctx);
    
    if (ctx->position >= ctx->length) {
        token.type = TOKEN_EOF;
        return token;
    }
    
    char c = ctx->json[ctx->position];
    
    switch (c) {
        case '{':
            token.type = TOKEN_OBJECT_START;
            ctx->position++;
            break;
        case '}':
            token.type = TOKEN_OBJECT_END;
            ctx->position++;
            break;
        case '[':
            token.type = TOKEN_ARRAY_START;
            ctx->position++;
            break;
        case ']':
            token.type = TOKEN_ARRAY_END;
            ctx->position++;
            break;
        case ':':
            token.type = TOKEN_COLON;
            ctx->position++;
            break;
        case ',':
            token.type = TOKEN_COMMA;
            ctx->position++;
            break;
        case '"':
            // Parse string
            {
                ctx->position++; // Skip opening quote
                size_t start = ctx->position;
                bool escaped = false;
                
                while (ctx->position < ctx->length) {
                    char c2 = ctx->json[ctx->position];
                    
                    if (!escaped) {
                        if (c2 == '"') {
                            break;
                        } else if (c2 == '\\') {
                            escaped = true;
                        }
                    } else {
                        escaped = false;
                    }
                    
                    ctx->position++;
                }
                
                if (ctx->position >= ctx->length) {
                    snprintf(ctx->error_msg, JSON_MAX_ERROR_MSG,
                            "Unterminated string at position %zu", start);
                    token.type = TOKEN_NONE;
                    return token;
                }
                
                token.type = TOKEN_STRING;
                token.length = ctx->position - start;
                token.value = json_strndup(ctx->json + start, token.length);
                ctx->position++; // Skip closing quote
            }
            break;
        case 't':
            if (json_match_string(ctx, "true")) {
                token.type = TOKEN_BOOLEAN;
                token.value = json_strdup("true");
                token.length = 4;
            }
            break;
        case 'f':
            if (json_match_string(ctx, "false")) {
                token.type = TOKEN_BOOLEAN;
                token.value = json_strdup("false");
                token.length = 5;
            }
            break;
        case 'n':
            if (json_match_string(ctx, "null")) {
                token.type = TOKEN_NULL;
                token.value = json_strdup("null");
                token.length = 4;
            }
            break;
        default:
            // Parse number
            if (c == '-' || (c >= '0' && c <= '9')) {
                size_t start = ctx->position;
                
                // Optional minus sign
                if (c == '-') {
                    ctx->position++;
                }
                
                // Integer part
                while (ctx->position < ctx->length &&
                       ctx->json[ctx->position] >= '0' &&
                       ctx->json[ctx->position] <= '9') {
                    ctx->position++;
                }
                
                // Decimal part
                if (ctx->position < ctx->length && ctx->json[ctx->position] == '.') {
                    ctx->position++;
                    while (ctx->position < ctx->length &&
                           ctx->json[ctx->position] >= '0' &&
                           ctx->json[ctx->position] <= '9') {
                        ctx->position++;
                    }
                }
                
                // Exponent part
                if (ctx->position < ctx->length &&
                    (ctx->json[ctx->position] == 'e' ||
                     ctx->json[ctx->position] == 'E')) {
                    ctx->position++;
                    
                    if (ctx->position < ctx->length &&
                        (ctx->json[ctx->position] == '+' ||
                         ctx->json[ctx->position] == '-')) {
                        ctx->position++;
                    }
                    
                    while (ctx->position < ctx->length &&
                           ctx->json[ctx->position] >= '0' &&
                           ctx->json[ctx->position] <= '9') {
                        ctx->position++;
                    }
                }
                
                token.type = TOKEN_NUMBER;
                token.length = ctx->position - start;
                token.value = json_strndup(ctx->json + start, token.length);
            } else {
                snprintf(ctx->error_msg, JSON_MAX_ERROR_MSG,
                        "Unexpected character '%c' at position %zu", c, ctx->position);
            }
            break;
    }
    
    return token;
}

static void json_free_token(JsonToken* token) {
    if (token->value) {
        free(token->value);
        token->value = NULL;
    }
}

// JSON serialization helpers
static bool json_serialize_ensure_capacity(JsonSerializeState* state, size_t additional) {
    if (state->size + additional + 1 > state->capacity) {
        size_t new_capacity = state->capacity * 2;
        while (new_capacity < state->size + additional + 1) {
            new_capacity *= 2;
        }
        
        char* new_buffer = (char*)json_realloc(state->buffer, new_capacity);
        if (!new_buffer) {
            return false;
        }
        
        state->buffer = new_buffer;
        state->capacity = new_capacity;
    }
    return true;
}

static bool json_serialize_append(JsonSerializeState* state, const char* str, size_t len) {
    if (!json_serialize_ensure_capacity(state, len)) {
        return false;
    }
    
    memcpy(state->buffer + state->size, str, len);
    state->size += len;
    state->buffer[state->size] = '\0';
    return true;
}

static bool json_serialize_append_string(JsonSerializeState* state, const char* str) {
    if (!str) {
        return json_serialize_append(state, "null", 4);
    }
    
    char* escaped = json_escape_string_alloc(str);
    if (!escaped) {
        return false;
    }
    
    bool result = json_serialize_append(state, "\"", 1) &&
                  json_serialize_append(state, escaped, strlen(escaped)) &&
                  json_serialize_append(state, "\"", 1);
    
    free(escaped);
    return result;
}

static bool json_serialize_indent(JsonSerializeState* state) {
    if (!state->pretty) return true;
    
    if (!json_serialize_append(state, "\n", 1)) {
        return false;
    }
    
    int indent_spaces = state->indent_level * JSON_INDENT_SIZE;
    for (int i = 0; i < indent_spaces; i++) {
        if (!json_serialize_append(state, " ", 1)) {
            return false;
        }
    }
    
    return true;
}

// Field value serialization
static bool json_serialize_field_value(JsonSerializeState* state, const Field* field) {
    if (!field) {
        return json_serialize_append(state, "null", 4);
    }
    
    switch (field->type) {
        case TYPE_INT:
            {
                char buffer[32];
                int len = snprintf(buffer, sizeof(buffer), "%d", field->value.int_value);
                return json_serialize_append(state, buffer, len);
            }
        case TYPE_STRING:
            return json_serialize_append_string(state, field->value.string_value);
        case TYPE_FLOAT:
            {
                char buffer[32];
                int len = snprintf(buffer, sizeof(buffer), "%.6f", field->value.float_value);
                return json_serialize_append(state, buffer, len);
            }
        case TYPE_DOUBLE:
            {
                char buffer[32];
                int len = snprintf(buffer, sizeof(buffer), "%.10f", field->value.double_value);
                return json_serialize_append(state, buffer, len);
            }
        case TYPE_BOOL:
            return json_serialize_append(state, field->value.bool_value ? "true" : "false",
                                        field->value.bool_value ? 4 : 5);
        case TYPE_DATETIME:
            {
                char buffer[64];
                // Assuming timestamp is stored as time_t in the union
                time_t timestamp_val;
                memcpy(&timestamp_val, &field->value, sizeof(time_t));
                struct tm* tm_info = localtime(&timestamp_val);
                strftime(buffer, sizeof(buffer), "\"%Y-%m-%d %H:%M:%S\"", tm_info);
                return json_serialize_append(state, buffer, strlen(buffer));
            }
        case TYPE_BLOB:
            {
                // Skip BLOB serialization for now - it's complex and may not be needed
                return json_serialize_append(state, "\"\"", 2);
            }
        case TYPE_NULL:
            return json_serialize_append(state, "null", 4);
        default:
            return json_serialize_append(state, "\"\"", 2);
    }
}

// Database serialization
static bool json_serialize_database_metadata(JsonSerializeState* state, Database* db) {
    if (!json_serialize_append(state, "{\n", 2)) return false;
    state->indent_level++;
    
    if (!json_serialize_indent(state)) return false;
    if (!json_serialize_append(state, "\"metadata\": {\n", 14)) return false;
    state->indent_level++;
    
    // Database name
    if (!json_serialize_indent(state)) return false;
    if (!json_serialize_append(state, "\"name\": ", 8)) return false;
    //if (!json_serialize_append_string(state, db->name)) return false;
    if (!json_serialize_append(state, ",\n", 2)) return false;
    
    // Version
    if (!json_serialize_indent(state)) return false;
    if (!json_serialize_append(state, "\"version\": \"", 12)) return false;
    if (!json_serialize_append(state, LUMINIX_VERSION_STRING, strlen(LUMINIX_VERSION_STRING))) return false;
    if (!json_serialize_append(state, "\",\n", 3)) return false;
    
    // Creation time
    if (!json_serialize_indent(state)) return false;
    if (!json_serialize_append(state, "\"created_at\": ", 14)) return false;
    char timestamp[64];
    time_t now = time(NULL);
    strftime(timestamp, sizeof(timestamp), "\"%Y-%m-%d %H:%M:%S\"", localtime(&now));
    if (!json_serialize_append(state, timestamp, strlen(timestamp))) return false;
    if (!json_serialize_append(state, ",\n", 2)) return false;
    
    // Table count
    if (!json_serialize_indent(state)) return false;
    if (!json_serialize_append(state, "\"table_count\": ", 15)) return false;
    char count_str[32];
    snprintf(count_str, sizeof(count_str), "%d", db->table_count);
    if (!json_serialize_append(state, count_str, strlen(count_str))) return false;
    
    state->indent_level--;
    if (!json_serialize_indent(state)) return false;
    if (!json_serialize_append(state, "},\n", 3)) return false;
    
    return true;
}

static bool json_serialize_table_schema(JsonSerializeState* state, Table* table) {
    if (!json_serialize_indent(state)) return false;
    if (!json_serialize_append(state, "\"schema\": {\n", 12)) return false;
    state->indent_level++;
    
    // Table name
    if (!json_serialize_indent(state)) return false;
    if (!json_serialize_append(state, "\"name\": ", 8)) return false;
    if (!json_serialize_append_string(state, table->name)) return false;
    if (!json_serialize_append(state, ",\n", 2)) return false;
    
    // Field definitions
    if (!json_serialize_indent(state)) return false;
    if (!json_serialize_append(state, "\"fields\": [\n", 12)) return false;
    state->indent_level++;
    
    for (int i = 0; i < table->field_count; i++) {
        if (!json_serialize_indent(state)) return false;
        if (!json_serialize_append(state, "{\n", 2)) return false;
        state->indent_level++;
        
        // Field name
        if (!json_serialize_indent(state)) return false;
        if (!json_serialize_append(state, "\"name\": ", 8)) return false;
        if (!json_serialize_append_string(state, table->field_names[i])) return false;
        if (!json_serialize_append(state, ",\n", 2)) return false;
        
        // Field type
        if (!json_serialize_indent(state)) return false;
        if (!json_serialize_append(state, "\"type\": ", 8)) return false;
        const char* type_str = field_type_to_string(table->field_types[i]);
        if (!json_serialize_append_string(state, type_str)) return false;
        
        // Constraints
        Constraint* constraint = &table->constraints[i];
        if (constraint->primary_key || constraint->unique || !constraint->nullable) {
            if (!json_serialize_append(state, ",\n", 2)) return false;
            if (!json_serialize_indent(state)) return false;
            if (!json_serialize_append(state, "\"constraints\": [", 16)) return false;
            
            bool first = true;
            if (constraint->primary_key) {
                if (!json_serialize_append(state, "\"PRIMARY KEY\"", 13)) return false;
                first = false;
            }
            if (constraint->unique) {
                if (!first) {
                    if (!json_serialize_append(state, ", ", 2)) return false;
                }
                if (!json_serialize_append(state, "\"UNIQUE\"", 8)) return false;
                first = false;
            }
            if (!constraint->nullable) {
                if (!first) {
                    if (!json_serialize_append(state, ", ", 2)) return false;
                }
                if (!json_serialize_append(state, "\"NOT NULL\"", 10)) return false;
            }
            
            if (!json_serialize_append(state, "]", 1)) return false;
        }
        
        state->indent_level--;
        if (!json_serialize_indent(state)) return false;
        if (!json_serialize_append(state, "}", 1)) return false;
        
        if (i < table->field_count - 1) {
            if (!json_serialize_append(state, ",\n", 2)) return false;
        } else {
            if (!json_serialize_append(state, "\n", 1)) return false;
        }
    }
    
    state->indent_level--;
    if (!json_serialize_indent(state)) return false;
    if (!json_serialize_append(state, "],\n", 3)) return false;
    
    // Skip index information for now since we don't have the correct structure
    // if (table->index_manager && table->index_manager->index_count > 0) {
    //     ...
    // }
    
    // Record count
    if (!json_serialize_indent(state)) return false;
    if (!json_serialize_append(state, "\"record_count\": ", 16)) return false;
    char record_count_str[32];
    snprintf(record_count_str, sizeof(record_count_str), "%d", table->record_count);
    if (!json_serialize_append(state, record_count_str, strlen(record_count_str))) return false;
    
    state->indent_level--;
    if (!json_serialize_indent(state)) return false;
    if (!json_serialize_append(state, "}", 1)) return false;
    
    return true;
}

static bool json_serialize_table_records(JsonSerializeState* state, Table* table) {
    if (!json_serialize_indent(state)) return false;
    if (!json_serialize_append(state, "\"records\": [\n", 13)) return false;
    state->indent_level++;
    
    Record* record = table->records;
    bool first_record = true;
    
    while (record) {
        if (!first_record) {
            if (!json_serialize_append(state, ",\n", 2)) return false;
        }
        first_record = false;
        
        if (!json_serialize_indent(state)) return false;
        if (!json_serialize_append(state, "{\n", 2)) return false;
        state->indent_level++;
        
        // Record ID
        if (!json_serialize_indent(state)) return false;
        if (!json_serialize_append(state, "\"id\": ", 6)) return false;
        char id_str[32];
        snprintf(id_str, sizeof(id_str), "%d", record->id);
        if (!json_serialize_append(state, id_str, strlen(id_str))) return false;
        if (!json_serialize_append(state, ",\n", 2)) return false;
        
        // Field values
        if (!json_serialize_indent(state)) return false;
        if (!json_serialize_append(state, "\"fields\": {\n", 12)) return false;
        state->indent_level++;
        
        for (int i = 0; i < table->field_count; i++) {
            if (!json_serialize_indent(state)) return false;
            if (!json_serialize_append(state, "\"", 1)) return false;
            if (!json_serialize_append(state, table->field_names[i], strlen(table->field_names[i]))) return false;
            if (!json_serialize_append(state, "\": ", 3)) return false;
            
            if (!json_serialize_field_value(state, &record->fields[i])) return false;
            
            if (i < table->field_count - 1) {
                if (!json_serialize_append(state, ",\n", 2)) return false;
            } else {
                if (!json_serialize_append(state, "\n", 1)) return false;
            }
        }
        
        state->indent_level--;
        if (!json_serialize_indent(state)) return false;
        if (!json_serialize_append(state, "}", 1)) return false;
        
        // Metadata
        if (!json_serialize_append(state, ",\n", 2)) return false;
        if (!json_serialize_indent(state)) return false;
        if (!json_serialize_append(state, "\"metadata\": {\n", 14)) return false;
        state->indent_level++;
        
        // Timestamp
        if (!json_serialize_indent(state)) return false;
        if (!json_serialize_append(state, "\"timestamp\": ", 13)) return false;
        char ts_str[64];
        strftime(ts_str, sizeof(ts_str), "\"%Y-%m-%d %H:%M:%S\"", localtime(&record->timestamp));
        if (!json_serialize_append(state, ts_str, strlen(ts_str))) return false;
        if (!json_serialize_append(state, ",\n", 2)) return false;
        
        // Version
        if (!json_serialize_indent(state)) return false;
        if (!json_serialize_append(state, "\"version\": ", 11)) return false;
        char version_str[32];
        snprintf(version_str, sizeof(version_str), "%u", record->version);
        if (!json_serialize_append(state, version_str, strlen(version_str))) return false;
        
        state->indent_level--;
        if (!json_serialize_indent(state)) return false;
        if (!json_serialize_append(state, "}", 1)) return false;
        
        state->indent_level--;
        if (!json_serialize_indent(state)) return false;
        if (!json_serialize_append(state, "}", 1)) return false;
        
        record = record->next;
    }
    
    if (!first_record) {
        if (!json_serialize_append(state, "\n", 1)) return false;
    }
    
    state->indent_level--;
    if (!json_serialize_indent(state)) return false;
    if (!json_serialize_append(state, "]", 1)) return false;
    
    return true;
}

// JSON parsing for database
static ErrorCode json_parse_database_metadata(JsonParseContext* ctx) {
    JsonToken token = json_parse_token(ctx);
    
    if (token.type != TOKEN_OBJECT_START) {
        snprintf(ctx->error_msg, JSON_MAX_ERROR_MSG,
                "Expected object start for metadata, got token type %d", token.type);
        json_free_token(&token);
        return ERROR_INVALID_INPUT;
    }
    json_free_token(&token);
    
    while (1) {
        token = json_parse_token(ctx);
        
        if (token.type == TOKEN_OBJECT_END) {
            json_free_token(&token);
            break;
        }
        
        if (token.type != TOKEN_STRING) {
            snprintf(ctx->error_msg, JSON_MAX_ERROR_MSG,
                    "Expected string key in metadata, got token type %d", token.type);
            json_free_token(&token);
            return ERROR_INVALID_INPUT;
        }
        
        char* key = token.value;
        json_free_token(&token);
        
        token = json_parse_token(ctx);
        if (token.type != TOKEN_COLON) {
            snprintf(ctx->error_msg, JSON_MAX_ERROR_MSG,
                    "Expected colon after key '%s'", key);
            free(key);
            json_free_token(&token);
            return ERROR_INVALID_INPUT;
        }
        json_free_token(&token);
        
        token = json_parse_token(ctx);
        
        // Skip metadata values for now
        if (strcmp(key, "name") == 0 && token.type == TOKEN_STRING) {
            //strncpy(field.name, token.value, MAX_TABLE_NAME - 1);
            //field.name[MAX_TABLE_NAME - 1] = '\0';
        }
        
        free(key);
        json_free_token(&token);
        
        token = json_parse_token(ctx);
        if (token.type == TOKEN_COMMA) {
            json_free_token(&token);
            continue;
        } else if (token.type == TOKEN_OBJECT_END) {
            json_free_token(&token);
            break;
        } else {
            snprintf(ctx->error_msg, JSON_MAX_ERROR_MSG,
                    "Expected comma or object end in metadata");
            json_free_token(&token);
            return ERROR_INVALID_INPUT;
        }
    }
    
    return SUCCESS;
}

static ErrorCode json_parse_table_schema(JsonParseContext* ctx, Table** out_table) {
    JsonToken token = json_parse_token(ctx);
    
    if (token.type != TOKEN_OBJECT_START) {
        snprintf(ctx->error_msg, JSON_MAX_ERROR_MSG,
                "Expected object start for schema, got token type %d", token.type);
        json_free_token(&token);
        return ERROR_INVALID_INPUT;
    }
    json_free_token(&token);
    
    char table_name[MAX_TABLE_NAME] = "";
    char** field_names = NULL;
    FieldType* field_types = NULL;
    int field_count = 0;
    int field_capacity = 0;
    
    while (1) {
        token = json_parse_token(ctx);
        
        if (token.type == TOKEN_OBJECT_END) {
            json_free_token(&token);
            break;
        }
        
        if (token.type != TOKEN_STRING) {
            snprintf(ctx->error_msg, JSON_MAX_ERROR_MSG,
                    "Expected string key in schema, got token type %d", token.type);
            goto cleanup_error;
        }
        
        char* key = token.value;
        json_free_token(&token);
        
        token = json_parse_token(ctx);
        if (token.type != TOKEN_COLON) {
            snprintf(ctx->error_msg, JSON_MAX_ERROR_MSG,
                    "Expected colon after key '%s'", key);
            free(key);
            json_free_token(&token);
            goto cleanup_error;
        }
        json_free_token(&token);
        
        if (strcmp(key, "name") == 0) {
            token = json_parse_token(ctx);
            if (token.type != TOKEN_STRING) {
                snprintf(ctx->error_msg, JSON_MAX_ERROR_MSG,
                        "Expected string for table name");
                free(key);
                json_free_token(&token);
                goto cleanup_error;
            }
            strncpy(table_name, token.value, MAX_TABLE_NAME - 1);
            table_name[MAX_TABLE_NAME - 1] = '\0';
            json_free_token(&token);
        } else if (strcmp(key, "fields") == 0) {
            token = json_parse_token(ctx);
            if (token.type != TOKEN_ARRAY_START) {
                snprintf(ctx->error_msg, JSON_MAX_ERROR_MSG,
                        "Expected array start for fields");
                free(key);
                json_free_token(&token);
                goto cleanup_error;
            }
            json_free_token(&token);
            
            // Parse fields array
            while (1) {
                token = json_parse_token(ctx);
                if (token.type == TOKEN_ARRAY_END) {
                    json_free_token(&token);
                    break;
                }
                
                if (token.type != TOKEN_OBJECT_START) {
                    snprintf(ctx->error_msg, JSON_MAX_ERROR_MSG,
                            "Expected object for field definition");
                    json_free_token(&token);
                    goto cleanup_error;
                }
                json_free_token(&token);
                
                char* field_name = NULL;
                FieldType field_type = TYPE_STRING;
                
                // Parse field object
                while (1) {
                    token = json_parse_token(ctx);
                    if (token.type == TOKEN_OBJECT_END) {
                        json_free_token(&token);
                        break;
                    }
                    
                    if (token.type != TOKEN_STRING) {
                        snprintf(ctx->error_msg, JSON_MAX_ERROR_MSG,
                                "Expected string key in field definition");
                        if (field_name) free(field_name);
                        json_free_token(&token);
                        goto cleanup_error;
                    }
                    
                    char* field_key = token.value;
                    json_free_token(&token);
                    
                    token = json_parse_token(ctx);
                    if (token.type != TOKEN_COLON) {
                        snprintf(ctx->error_msg, JSON_MAX_ERROR_MSG,
                                "Expected colon after field key '%s'", field_key);
                        free(field_key);
                        if (field_name) free(field_name);
                        json_free_token(&token);
                        goto cleanup_error;
                    }
                    json_free_token(&token);
                    
                    token = json_parse_token(ctx);
                    
                    if (strcmp(field_key, "name") == 0) {
                        if (token.type != TOKEN_STRING) {
                            snprintf(ctx->error_msg, JSON_MAX_ERROR_MSG,
                                    "Expected string for field name");
                            free(field_key);
                            if (field_name) free(field_name);
                            json_free_token(&token);
                            goto cleanup_error;
                        }
                        field_name = json_strdup(token.value);
                    } else if (strcmp(field_key, "type") == 0) {
                        if (token.type != TOKEN_STRING) {
                            snprintf(ctx->error_msg, JSON_MAX_ERROR_MSG,
                                    "Expected string for field type");
                            free(field_key);
                            if (field_name) free(field_name);
                            json_free_token(&token);
                            goto cleanup_error;
                        }
                        field_type = string_to_field_type(token.value);
                    }
                    // Ignore constraints for now
                    
                    free(field_key);
                    json_free_token(&token);
                    
                    token = json_parse_token(ctx);
                    if (token.type == TOKEN_COMMA) {
                        json_free_token(&token);
                        continue;
                    } else if (token.type == TOKEN_OBJECT_END) {
                        json_free_token(&token);
                        break;
                    } else {
                        snprintf(ctx->error_msg, JSON_MAX_ERROR_MSG,
                                "Expected comma or object end in field definition");
                        if (field_name) free(field_name);
                        json_free_token(&token);
                        goto cleanup_error;
                    }
                }
                
                // Add field to arrays
                if (field_count >= field_capacity) {
                    int new_capacity = field_capacity == 0 ? 4 : field_capacity * 2;
                    char** new_names = (char**)json_realloc(field_names, new_capacity * sizeof(char*));
                    FieldType* new_types = (FieldType*)json_realloc(field_types, new_capacity * sizeof(FieldType));
                    
                    if (!new_names || !new_types) {
                        free(new_names);
                        free(new_types);
                        if (field_name) free(field_name);
                        goto cleanup_error;
                    }
                    
                    field_names = new_names;
                    field_types = new_types;
                    field_capacity = new_capacity;
                }
                
                field_names[field_count] = field_name;
                field_types[field_count] = field_type;
                field_count++;
                
                token = json_parse_token(ctx);
                if (token.type == TOKEN_COMMA) {
                    json_free_token(&token);
                    continue;
                } else if (token.type == TOKEN_ARRAY_END) {
                    json_free_token(&token);
                    break;
                } else {
                    snprintf(ctx->error_msg, JSON_MAX_ERROR_MSG,
                            "Expected comma or array end in fields array");
                    json_free_token(&token);
                    goto cleanup_error;
                }
            }
        } else {
            // Skip other keys for now
            token = json_parse_token(ctx);
            json_free_token(&token);
        }
        
        free(key);
        
        token = json_parse_token(ctx);
        if (token.type == TOKEN_COMMA) {
            json_free_token(&token);
            continue;
        } else if (token.type == TOKEN_OBJECT_END) {
            json_free_token(&token);
            break;
        } else {
            snprintf(ctx->error_msg, JSON_MAX_ERROR_MSG,
                    "Expected comma or object end in schema");
            json_free_token(&token);
            goto cleanup_error;
        }
    }
    
    // Create table
    if (strlen(table_name) > 0 && field_count > 0) {
        ErrorCode result = db_add_table(ctx->db, table_name, 
                                       (const char**)field_names, field_types,
                                       field_count, out_table);
        
        // Cleanup temporary arrays
        for (int i = 0; i < field_count; i++) {
            free(field_names[i]);
        }
        free(field_names);
        free(field_types);
        
        return result;
    }
    
cleanup_error:
    // Cleanup on error
    for (int i = 0; i < field_count; i++) {
        free(field_names[i]);
    }
    free(field_names);
    free(field_types);
    
    return ERROR_INVALID_INPUT;
}

static Field json_parse_field_value(JsonParseContext* ctx, FieldType expected_type) {
    Field field = {0};
    field.type = expected_type;
    
    JsonToken token = json_parse_token(ctx);
    
    if (token.type == TOKEN_NULL) {
        field.type = TYPE_NULL;
        json_free_token(&token);
        return field;
    }
    
    switch (expected_type) {
        case TYPE_INT:
            if (token.type == TOKEN_NUMBER) {
                field.value.int_value = atoi(token.value);
            } else if (token.type == TOKEN_STRING) {
                field.value.int_value = atoi(token.value);
            }
            break;
        case TYPE_STRING:
            if (token.type == TOKEN_STRING) {
                strncpy(field.value.string_value, token.value, MAX_FIELD_LEN - 1);
                field.value.string_value[MAX_FIELD_LEN - 1] = '\0';
            } else if (token.type == TOKEN_NUMBER) {
                strncpy(field.value.string_value, token.value, MAX_FIELD_LEN - 1);
                field.value.string_value[MAX_FIELD_LEN - 1] = '\0';
            }
            break;
        case TYPE_FLOAT:
            if (token.type == TOKEN_NUMBER) {
                field.value.float_value = atof(token.value);
            } else if (token.type == TOKEN_STRING) {
                field.value.float_value = atof(token.value);
            }
            break;
        case TYPE_DOUBLE:
            if (token.type == TOKEN_NUMBER) {
                field.value.double_value = atof(token.value);
            } else if (token.type == TOKEN_STRING) {
                field.value.double_value = atof(token.value);
            }
            break;
        case TYPE_BOOL:
            if (token.type == TOKEN_BOOLEAN) {
                field.value.bool_value = strcmp(token.value, "true") == 0;
            } else if (token.type == TOKEN_NUMBER) {
                field.value.bool_value = atoi(token.value) != 0;
            } else if (token.type == TOKEN_STRING) {
                field.value.bool_value = strcasecmp(token.value, "true") == 0 ||
                                        strcasecmp(token.value, "1") == 0 ||
                                        strcasecmp(token.value, "yes") == 0;
            }
            break;
        case TYPE_DATETIME:
            if (token.type == TOKEN_STRING) {
                struct tm tm = {0};
                if (strptime(token.value, "%Y-%m-%d %H:%M:%S", &tm) != NULL) {
                    time_t timestamp_val = mktime(&tm);
                    memcpy(&field.value, &timestamp_val, sizeof(time_t));
                }
            }
            break;
        default:
            break;
    }
    
    json_free_token(&token);
    return field;
}

static ErrorCode json_parse_table_records(JsonParseContext* ctx, Table* table) {
    JsonToken token = json_parse_token(ctx);
    
    if (token.type != TOKEN_ARRAY_START) {
        snprintf(ctx->error_msg, JSON_MAX_ERROR_MSG,
                "Expected array start for records, got token type %d", token.type);
        json_free_token(&token);
        return ERROR_INVALID_INPUT;
    }
    json_free_token(&token);
    
    while (1) {
        token = json_parse_token(ctx);
        if (token.type == TOKEN_ARRAY_END) {
            json_free_token(&token);
            break;
        }
        
        if (token.type != TOKEN_OBJECT_START) {
            snprintf(ctx->error_msg, JSON_MAX_ERROR_MSG,
                    "Expected object for record, got token type %d", token.type);
            json_free_token(&token);
            return ERROR_INVALID_INPUT;
        }
        json_free_token(&token);
        
        int record_id = -1;
        Field* field_values = (Field*)json_calloc(table->field_count, sizeof(Field));
        if (!field_values) {
            return ERROR_MEMORY_ALLOCATION;
        }
        
        // Initialize fields with table schema types
        for (int i = 0; i < table->field_count; i++) {
            field_values[i].type = table->field_types[i];
            // Initialize with default values
            switch (table->field_types[i]) {
                case TYPE_INT:
                    field_values[i].value.int_value = 0;
                    break;
                case TYPE_STRING:
                    field_values[i].value.string_value[0] = '\0';
                    break;
                case TYPE_FLOAT:
                    field_values[i].value.float_value = 0.0f;
                    break;
                case TYPE_DOUBLE:
                    field_values[i].value.double_value = 0.0;
                    break;
                case TYPE_BOOL:
                    field_values[i].value.bool_value = false;
                    break;
                default:
                    break;
            }
        }
        
        // Parse record object
        while (1) {
            token = json_parse_token(ctx);
            if (token.type == TOKEN_OBJECT_END) {
                json_free_token(&token);
                break;
            }
            
            if (token.type != TOKEN_STRING) {
                snprintf(ctx->error_msg, JSON_MAX_ERROR_MSG,
                        "Expected string key in record");
                free(field_values);
                json_free_token(&token);
                return ERROR_INVALID_INPUT;
            }
            
            char* key = token.value;
            json_free_token(&token);
            
            token = json_parse_token(ctx);
            if (token.type != TOKEN_COLON) {
                snprintf(ctx->error_msg, JSON_MAX_ERROR_MSG,
                        "Expected colon after key '%s'", key);
                free(key);
                free(field_values);
                json_free_token(&token);
                return ERROR_INVALID_INPUT;
            }
            json_free_token(&token);
            
            if (strcmp(key, "id") == 0) {
                token = json_parse_token(ctx);
                if (token.type == TOKEN_NUMBER) {
                    record_id = atoi(token.value);
                }
                json_free_token(&token);
            } else if (strcmp(key, "fields") == 0) {
                token = json_parse_token(ctx);
                if (token.type != TOKEN_OBJECT_START) {
                    snprintf(ctx->error_msg, JSON_MAX_ERROR_MSG,
                            "Expected object for fields");
                    free(key);
                    free(field_values);
                    json_free_token(&token);
                    return ERROR_INVALID_INPUT;
                }
                json_free_token(&token);
                
                // Parse fields object
                while (1) {
                    token = json_parse_token(ctx);
                    if (token.type == TOKEN_OBJECT_END) {
                        json_free_token(&token);
                        break;
                    }
                    
                    if (token.type != TOKEN_STRING) {
                        snprintf(ctx->error_msg, JSON_MAX_ERROR_MSG,
                                "Expected string key in fields object");
                        free(key);
                        free(field_values);
                        json_free_token(&token);
                        return ERROR_INVALID_INPUT;
                    }
                    
                    char* field_key = token.value;
                    json_free_token(&token);
                    
                    token = json_parse_token(ctx);
                    if (token.type != TOKEN_COLON) {
                        snprintf(ctx->error_msg, JSON_MAX_ERROR_MSG,
                                "Expected colon after field key '%s'", field_key);
                        free(field_key);
                        free(key);
                        free(field_values);
                        json_free_token(&token);
                        return ERROR_INVALID_INPUT;
                    }
                    json_free_token(&token);
                    
                    // Find field index
                    int field_idx = -1;
                    for (int i = 0; i < table->field_count; i++) {
                        if (strcmp(table->field_names[i], field_key) == 0) {
                            field_idx = i;
                            break;
                        }
                    }
                    
                    if (field_idx != -1) {
                        Field parsed_value = json_parse_field_value(ctx, table->field_types[field_idx]);
                        field_values[field_idx].value = parsed_value.value;
                        if (parsed_value.type == TYPE_NULL) {
                            field_values[field_idx].type = TYPE_NULL;
                        }
                    } else {
                        // Skip unknown field
                        token = json_parse_token(ctx);
                        json_free_token(&token);
                    }
                    
                    free(field_key);
                    
                    token = json_parse_token(ctx);
                    if (token.type == TOKEN_COMMA) {
                        json_free_token(&token);
                        continue;
                    } else if (token.type == TOKEN_OBJECT_END) {
                        json_free_token(&token);
                        break;
                    } else {
                        snprintf(ctx->error_msg, JSON_MAX_ERROR_MSG,
                                "Expected comma or object end in fields object");
                        free(key);
                        free(field_values);
                        json_free_token(&token);
                        return ERROR_INVALID_INPUT;
                    }
                }
            } else {
                // Skip metadata and other keys
                token = json_parse_token(ctx);
                json_free_token(&token);
            }
            
            free(key);
            
            token = json_parse_token(ctx);
            if (token.type == TOKEN_COMMA) {
                json_free_token(&token);
                continue;
            } else if (token.type == TOKEN_OBJECT_END) {
                json_free_token(&token);
                break;
            } else {
                snprintf(ctx->error_msg, JSON_MAX_ERROR_MSG,
                        "Expected comma or object end in record");
                free(field_values);
                json_free_token(&token);
                return ERROR_INVALID_INPUT;
            }
        }
        
        // Insert record
        if (record_id != -1) {
            // Set ID field if it's the first field
            for (int i = 0; i < table->field_count; i++) {
                if (table->constraints[i].primary_key) {
                    field_values[i].value.int_value = record_id;
                    field_values[i].type = TYPE_INT;
                    break;
                }
            }
            
            ErrorCode result = table_insert_record(table, field_values, NULL);
            if (result != SUCCESS && result != ERROR_DUPLICATE_KEY) {
                free(field_values);
                return result;
            }
        }
        
        free(field_values);
        
        token = json_parse_token(ctx);
        if (token.type == TOKEN_COMMA) {
            json_free_token(&token);
            continue;
        } else if (token.type == TOKEN_ARRAY_END) {
            json_free_token(&token);
            break;
        } else {
            snprintf(ctx->error_msg, JSON_MAX_ERROR_MSG,
                    "Expected comma or array end in records");
            json_free_token(&token);
            return ERROR_INVALID_INPUT;
        }
    }
    
    return SUCCESS;
}

// ==================== PUBLIC API IMPLEMENTATION ====================

// Database serialization/deserialization
ErrorCode db_save_to_file(Database* db, const char* filename, JsonSaveOptions* options) {
    VALIDATE_PTR(db);
    VALIDATE_PTR(filename);
    
    LOG_DEBUG("Saving database to file: %s", filename);
    
    JsonSerializeState state = {0};
    bool default_options = true;
    
    if (options) {
        // Use simpler field access - assume options has basic fields
        state.pretty = true;  // Default to pretty printing
        state.sort_keys = false;
        state.escape_unicode = true;
        default_options = false;
    } else {
        state.pretty = true;
        state.sort_keys = false;
        state.escape_unicode = true;
    }
    
    state.capacity = JSON_BUFFER_SIZE;
    state.buffer = (char*)json_malloc(state.capacity);
    
    if (!state.buffer) {
        return ERROR_MEMORY_ALLOCATION;
    }
    
    state.buffer[0] = '\0';
    
    // Start database object
    if (!json_serialize_append(&state, "{", 1)) {
        free(state.buffer);
        return ERROR_MEMORY_ALLOCATION;
    }
    state.indent_level++;
    
    // Always serialize metadata for now
    if (!json_serialize_database_metadata(&state, db)) {
        free(state.buffer);
        return ERROR_MEMORY_ALLOCATION;
    }
    if (!json_serialize_append(&state, ",", 1)) {
        free(state.buffer);
        return ERROR_MEMORY_ALLOCATION;
    }
    
    // Serialize tables
    if (!json_serialize_indent(&state)) {
        free(state.buffer);
        return ERROR_MEMORY_ALLOCATION;
    }
    if (!json_serialize_append(&state, "\"tables\": [", 11)) {
        free(state.buffer);
        return ERROR_MEMORY_ALLOCATION;
    }
    state.indent_level++;
    
    for (int t = 0; t < db->table_count; t++) {
        if (t > 0) {
            if (!json_serialize_append(&state, ",", 1)) {
                free(state.buffer);
                return ERROR_MEMORY_ALLOCATION;
            }
        }
        
        if (!json_serialize_indent(&state)) {
            free(state.buffer);
            return ERROR_MEMORY_ALLOCATION;
        }
        if (!json_serialize_append(&state, "{\n", 2)) {
            free(state.buffer);
            return ERROR_MEMORY_ALLOCATION;
        }
        state.indent_level++;
        
        Table* table = &db->tables[t];
        
        // Serialize schema
        if (!json_serialize_table_schema(&state, table)) {
            free(state.buffer);
            return ERROR_MEMORY_ALLOCATION;
        }
        
        // Always serialize records for now
        if (!json_serialize_append(&state, ",\n", 2)) {
            free(state.buffer);
            return ERROR_MEMORY_ALLOCATION;
        }
        if (!json_serialize_table_records(&state, table)) {
            free(state.buffer);
            return ERROR_MEMORY_ALLOCATION;
        }
        
        state.indent_level--;
        if (!json_serialize_indent(&state)) {
            free(state.buffer);
            return ERROR_MEMORY_ALLOCATION;
        }
        if (!json_serialize_append(&state, "}", 1)) {
            free(state.buffer);
            return ERROR_MEMORY_ALLOCATION;
        }
    }
    
    state.indent_level--;
    if (!json_serialize_indent(&state)) {
        free(state.buffer);
        return ERROR_MEMORY_ALLOCATION;
    }
    if (!json_serialize_append(&state, "]", 1)) {
        free(state.buffer);
        return ERROR_MEMORY_ALLOCATION;
    }
    
    // End database object
    state.indent_level--;
    if (!json_serialize_indent(&state)) {
        free(state.buffer);
        return ERROR_MEMORY_ALLOCATION;
    }
    if (!json_serialize_append(&state, "}", 1)) {
        free(state.buffer);
        return ERROR_MEMORY_ALLOCATION;
    }
    
    // Write to file
    FILE* file = fopen(filename, "wb");
    if (!file) {
        LOG_ERROR("Failed to open file for writing: %s", filename);
        free(state.buffer);
        return ERROR_IO_OPERATION;
    }
    
    size_t written = fwrite(state.buffer, 1, state.size, file);
    fclose(file);
    
    if (written != state.size) {
        LOG_ERROR("Failed to write complete data to file: %s", filename);
        free(state.buffer);
        return ERROR_IO_OPERATION;
    }
    
    LOG_INFO("Database saved successfully to %s (%zu bytes)", filename, state.size);
    free(state.buffer);
    
    return SUCCESS;
}

Database* db_load_from_file(const char* filename, JsonLoadOptions* options) {
    VALIDATE_PTR(filename);
    
    LOG_DEBUG("Loading database from file: %s", filename);
    
    // Read file
    FILE* file = fopen(filename, "rb");
    if (!file) {
        LOG_ERROR("Failed to open file for reading: %s", filename);
        return NULL;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size <= 0) {
        LOG_ERROR("Empty or invalid file: %s", filename);
        fclose(file);
        return NULL;
    }
    
    // Read file content
    char* json_content = (char*)json_malloc(file_size + 1);
    if (!json_content) {
        LOG_ERROR("Failed to allocate memory for JSON content");
        fclose(file);
        return NULL;
    }
    
    size_t read_size = fread(json_content, 1, file_size, file);
    fclose(file);
    
    if (read_size != (size_t)file_size) {
        LOG_ERROR("Failed to read complete file: %s", filename);
        free(json_content);
        return NULL;
    }
    
    json_content[file_size] = '\0';
    
    // Create database
    Database* db = db_create();
    if (!db) {
        LOG_ERROR("Failed to create database");
        free(json_content);
        return NULL;
    }
    
    // Parse JSON
    JsonParseContext ctx = {0};
    ctx.json = json_content;
    ctx.length = file_size;
    ctx.db = db;
    
    JsonToken token = json_parse_token(&ctx);
    if (token.type != TOKEN_OBJECT_START) {
        LOG_ERROR("Invalid JSON: expected object start");
        json_free_token(&token);
        free(json_content);
        db_free(db);
        return NULL;
    }
    json_free_token(&token);
    
    ErrorCode result = SUCCESS;
    
    while (1) {
        token = json_parse_token(&ctx);
        if (token.type == TOKEN_OBJECT_END) {
            json_free_token(&token);
            break;
        }
        
        if (token.type != TOKEN_STRING) {
            LOG_ERROR("Invalid JSON: expected string key");
            json_free_token(&token);
            result = ERROR_INVALID_INPUT;
            break;
        }
        
        char* key = token.value;
        json_free_token(&token);
        
        token = json_parse_token(&ctx);
        if (token.type != TOKEN_COLON) {
            LOG_ERROR("Invalid JSON: expected colon after key '%s'", key);
            free(key);
            json_free_token(&token);
            result = ERROR_INVALID_INPUT;
            break;
        }
        json_free_token(&token);
        
        if (strcmp(key, "metadata") == 0) {
            result = json_parse_database_metadata(&ctx);
        } else if (strcmp(key, "tables") == 0) {
            token = json_parse_token(&ctx);
            if (token.type != TOKEN_ARRAY_START) {
                LOG_ERROR("Invalid JSON: expected array for tables");
                free(key);
                json_free_token(&token);
                result = ERROR_INVALID_INPUT;
                break;
            }
            json_free_token(&token);
            
            // Parse tables array
            while (1) {
                token = json_parse_token(&ctx);
                if (token.type == TOKEN_ARRAY_END) {
                    json_free_token(&token);
                    break;
                }
                
                if (token.type != TOKEN_OBJECT_START) {
                    LOG_ERROR("Invalid JSON: expected object for table");
                    free(key);
                    json_free_token(&token);
                    result = ERROR_INVALID_INPUT;
                    break;
                }
                json_free_token(&token);
                
                // Parse table
                Table* table = NULL;
                result = json_parse_table_schema(&ctx, &table);
                if (result != SUCCESS) {
                    free(key);
                    break;
                }
                
                // Check if we should parse records
                token = json_parse_token(&ctx);
                if (token.type == TOKEN_COMMA) {
                    // Parse records
                    token = json_parse_token(&ctx);
                    if (token.type == TOKEN_STRING && strcmp(token.value, "records") == 0) {
                        json_free_token(&token);
                        
                        token = json_parse_token(&ctx);
                        if (token.type != TOKEN_COLON) {
                            LOG_ERROR("Invalid JSON: expected colon after 'records'");
                            free(key);
                            json_free_token(&token);
                            result = ERROR_INVALID_INPUT;
                            break;
                        }
                        json_free_token(&token);
                        
                        result = json_parse_table_records(&ctx, table);
                        if (result != SUCCESS) {
                            free(key);
                            break;
                        }
                        
                        token = json_parse_token(&ctx);
                    } else {
                        // Skip unknown key
                        json_free_token(&token);
                        token = json_parse_token(&ctx); // colon
                        json_free_token(&token);
                        token = json_parse_token(&ctx); // value
                        json_free_token(&token);
                        token = json_parse_token(&ctx); // comma or object end
                    }
                }
                
                if (token.type == TOKEN_COMMA) {
                    json_free_token(&token);
                    continue;
                } else if (token.type == TOKEN_OBJECT_END) {
                    json_free_token(&token);
                    
                    token = json_parse_token(&ctx);
                    if (token.type == TOKEN_COMMA) {
                        json_free_token(&token);
                        continue;
                    } else if (token.type == TOKEN_ARRAY_END) {
                        json_free_token(&token);
                        break;
                    } else {
                        LOG_ERROR("Invalid JSON: expected comma or array end after table");
                        free(key);
                        result = ERROR_INVALID_INPUT;
                        break;
                    }
                } else {
                    LOG_ERROR("Invalid JSON: expected comma or object end in table");
                    free(key);
                    json_free_token(&token);
                    result = ERROR_INVALID_INPUT;
                    break;
                }
            }
        } else {
            // Skip unknown key
            token = json_parse_token(&ctx);
            json_free_token(&token);
        }
        
        free(key);
        
        if (result != SUCCESS) {
            break;
        }
        
        token = json_parse_token(&ctx);
        if (token.type == TOKEN_COMMA) {
            json_free_token(&token);
            continue;
        } else if (token.type == TOKEN_OBJECT_END) {
            json_free_token(&token);
            break;
        } else {
            LOG_ERROR("Invalid JSON: expected comma or object end");
            json_free_token(&token);
            result = ERROR_INVALID_INPUT;
            break;
        }
    }
    
    free(json_content);
    
    if (result != SUCCESS) {
        LOG_ERROR("Failed to parse JSON: %s", ctx.error_msg);
        db_free(db);
        return NULL;
    }
    
    LOG_INFO("Database loaded successfully from %s", filename);
    return db;
}

// Record serialization
char* record_to_json(Record* record, Table* table, JsonSerializeOptions* options) {
    VALIDATE_PTR(record);
    VALIDATE_PTR(table);
    
    JsonSerializeState state = {0};
    state.pretty = options ? options->pretty : false;
    state.sort_keys = options ? options->sort_keys : false;
    state.capacity = JSON_BUFFER_SIZE;
    state.buffer = (char*)json_malloc(state.capacity);
    
    if (!state.buffer) {
        return NULL;
    }
    
    state.buffer[0] = '\0';
    
    // Start record object
    if (!json_serialize_append(&state, "{", 1)) {
        free(state.buffer);
        return NULL;
    }
    state.indent_level++;
    
    // Record ID
    if (state.pretty) {
        if (!json_serialize_indent(&state)) {
            free(state.buffer);
            return NULL;
        }
    }
    if (!json_serialize_append(&state, "\"id\": ", 6)) {
        free(state.buffer);
        return NULL;
    }
    char id_str[32];
    snprintf(id_str, sizeof(id_str), "%d", record->id);
    if (!json_serialize_append(&state, id_str, strlen(id_str))) {
        free(state.buffer);
        return NULL;
    }
    if (!json_serialize_append(&state, ",", 1)) {
        free(state.buffer);
        return NULL;
    }
    
    if (state.pretty) {
        if (!json_serialize_append(&state, "\n", 1)) {
            free(state.buffer);
            return NULL;
        }
    }
    
    // Fields
    if (state.pretty) {
        if (!json_serialize_indent(&state)) {
            free(state.buffer);
            return NULL;
        }
    }
    if (!json_serialize_append(&state, "\"fields\": {", 11)) {
        free(state.buffer);
        return NULL;
    }
    state.indent_level++;
    
    for (int i = 0; i < table->field_count; i++) {
        if (i > 0) {
            if (!json_serialize_append(&state, ",", 1)) {
                free(state.buffer);
                return NULL;
            }
        }
        
        if (state.pretty) {
            if (!json_serialize_append(&state, "\n", 1)) {
                free(state.buffer);
                return NULL;
            }
            if (!json_serialize_indent(&state)) {
                free(state.buffer);
                return NULL;
            }
        }
        
        if (!json_serialize_append(&state, "\"", 1)) {
            free(state.buffer);
            return NULL;
        }
        if (!json_serialize_append(&state, table->field_names[i], strlen(table->field_names[i]))) {
            free(state.buffer);
            return NULL;
        }
        if (!json_serialize_append(&state, "\": ", 3)) {
            free(state.buffer);
            return NULL;
        }
        
        if (!json_serialize_field_value(&state, &record->fields[i])) {
            free(state.buffer);
            return NULL;
        }
    }
    
    if (state.pretty) {
        if (!json_serialize_append(&state, "\n", 1)) {
            free(state.buffer);
            return NULL;
        }
        state.indent_level--;
        if (!json_serialize_indent(&state)) {
            free(state.buffer);
            return NULL;
        }
    } else {
        state.indent_level--;
    }
    
    if (!json_serialize_append(&state, "}", 1)) {
        free(state.buffer);
        return NULL;
    }
    
    // End record object
    state.indent_level--;
    if (state.pretty) {
        if (!json_serialize_append(&state, "\n", 1)) {
            free(state.buffer);
            return NULL;
        }
        if (!json_serialize_indent(&state)) {
            free(state.buffer);
            return NULL;
        }
    }
    if (!json_serialize_append(&state, "}", 1)) {
        free(state.buffer);
        return NULL;
    }
    
    return state.buffer;
}

Record* json_to_record(const char* json_str, Table* table) {
    VALIDATE_PTR(json_str);
    VALIDATE_PTR(table);
    
    JsonParseContext ctx = {0};
    ctx.json = json_str;
    ctx.length = strlen(json_str);
    ctx.current_table = table;
    
    JsonToken token = json_parse_token(&ctx);
    if (token.type != TOKEN_OBJECT_START) {
        LOG_ERROR("Invalid JSON: expected object start for record");
        json_free_token(&token);
        return NULL;
    }
    json_free_token(&token);
    
    int record_id = -1;
    Field* field_values = (Field*)json_calloc(table->field_count, sizeof(Field));
    if (!field_values) {
        return NULL;
    }
    
    ErrorCode result = SUCCESS;
    
    while (1) {
        token = json_parse_token(&ctx);
        if (token.type == TOKEN_OBJECT_END) {
            json_free_token(&token);
            break;
        }
        
        if (token.type != TOKEN_STRING) {
            LOG_ERROR("Invalid JSON: expected string key in record");
            result = ERROR_INVALID_INPUT;
            break;
        }
        
        char* key = token.value;
        json_free_token(&token);
        
        token = json_parse_token(&ctx);
        if (token.type != TOKEN_COLON) {
            LOG_ERROR("Invalid JSON: expected colon after key '%s'", key);
            free(key);
            result = ERROR_INVALID_INPUT;
            break;
        }
        json_free_token(&token);
        
        if (strcmp(key, "id") == 0) {
            token = json_parse_token(&ctx);
            if (token.type == TOKEN_NUMBER) {
                record_id = atoi(token.value);
            } else {
                LOG_ERROR("Invalid JSON: expected number for id");
                free(key);
                json_free_token(&token);
                result = ERROR_INVALID_INPUT;
                break;
            }
            json_free_token(&token);
        } else if (strcmp(key, "fields") == 0) {
            token = json_parse_token(&ctx);
            if (token.type != TOKEN_OBJECT_START) {
                LOG_ERROR("Invalid JSON: expected object for fields");
                free(key);
                json_free_token(&token);
                result = ERROR_INVALID_INPUT;
                break;
            }
            json_free_token(&token);
            
            // Parse fields object
            while (1) {
                token = json_parse_token(&ctx);
                if (token.type == TOKEN_OBJECT_END) {
                    json_free_token(&token);
                    break;
                }
                
                if (token.type != TOKEN_STRING) {
                    LOG_ERROR("Invalid JSON: expected string key in fields");
                    free(key);
                    result = ERROR_INVALID_INPUT;
                    break;
                }
                
                char* field_key = token.value;
                json_free_token(&token);
                
                token = json_parse_token(&ctx);
                if (token.type != TOKEN_COLON) {
                    LOG_ERROR("Invalid JSON: expected colon after field key '%s'", field_key);
                    free(field_key);
                    free(key);
                    result = ERROR_INVALID_INPUT;
                    break;
                }
                json_free_token(&token);
                
                // Find field index
                int field_idx = -1;
                for (int i = 0; i < table->field_count; i++) {
                    if (strcmp(table->field_names[i], field_key) == 0) {
                        field_idx = i;
                        break;
                    }
                }
                
                if (field_idx != -1) {
                    Field parsed_value = json_parse_field_value(&ctx, table->field_types[field_idx]);
                    field_values[field_idx].value = parsed_value.value;
                    if (parsed_value.type == TYPE_NULL) {
                        field_values[field_idx].type = TYPE_NULL;
                    }
                } else {
                    // Skip unknown field
                    token = json_parse_token(&ctx);
                    json_free_token(&token);
                }
                
                free(field_key);
                
                token = json_parse_token(&ctx);
                if (token.type == TOKEN_COMMA) {
                    json_free_token(&token);
                    continue;
                } else if (token.type == TOKEN_OBJECT_END) {
                    json_free_token(&token);
                    break;
                } else {
                    LOG_ERROR("Invalid JSON: expected comma or object end in fields");
                    free(key);
                    result = ERROR_INVALID_INPUT;
                    break;
                }
            }
        } else {
            // Skip other keys
            token = json_parse_token(&ctx);
            json_free_token(&token);
        }
        
        free(key);
        
        token = json_parse_token(&ctx);
        if (token.type == TOKEN_COMMA) {
            json_free_token(&token);
            continue;
        } else if (token.type == TOKEN_OBJECT_END) {
            json_free_token(&token);
            break;
        } else {
            LOG_ERROR("Invalid JSON: expected comma or object end in record");
            result = ERROR_INVALID_INPUT;
            break;
        }
    }
    
    if (result != SUCCESS) {
        free(field_values);
        return NULL;
    }
    
    if (record_id == -1) {
        LOG_ERROR("Record ID not found in JSON");
        free(field_values);
        return NULL;
    }
    
    // Create record
    Record* record = (Record*)json_malloc(sizeof(Record));
    if (!record) {
        free(field_values);
        return NULL;
    }
    
    record->id = record_id;
    record->field_count = table->field_count;
    record->fields = field_values;
    record->next = NULL;
    record->timestamp = time(NULL);
    record->version = 1;
    
    return record;
}

// Table serialization
char* table_to_json(Table* table, JsonSerializeOptions* options) {
    VALIDATE_PTR(table);
    
    JsonSerializeState state = {0};
    state.pretty = options ? options->pretty : true;
    state.sort_keys = options ? options->sort_keys : false;
    state.capacity = JSON_BUFFER_SIZE;
    state.buffer = (char*)json_malloc(state.capacity);
    
    if (!state.buffer) {
        return NULL;
    }
    
    state.buffer[0] = '\0';
    
    // Start table object
    if (!json_serialize_append(&state, "{", 1)) {
        free(state.buffer);
        return NULL;
    }
    state.indent_level++;
    
    // Serialize schema
    if (!json_serialize_table_schema(&state, table)) {
        free(state.buffer);
        return NULL;
    }
    
    // Serialize records if requested
    if (options && options->include_data) {
        if (!json_serialize_append(&state, ",\n", 2)) {
            free(state.buffer);
            return NULL;
        }
        if (!json_serialize_table_records(&state, table)) {
            free(state.buffer);
            return NULL;
        }
    }
    
    // End table object
    state.indent_level--;
    if (!json_serialize_indent(&state)) {
        free(state.buffer);
        return NULL;
    }
    if (!json_serialize_append(&state, "}", 1)) {
        free(state.buffer);
        return NULL;
    }
    
    return state.buffer;
}

Table* json_to_table(const char* json_str, Database* db) {
    VALIDATE_PTR(json_str);
    VALIDATE_PTR(db);
    
    JsonParseContext ctx = {0};
    ctx.json = json_str;
    ctx.length = strlen(json_str);
    ctx.db = db;
    
    Table* table = NULL;
    ErrorCode result = json_parse_table_schema(&ctx, &table);
    
    if (result != SUCCESS) {
        return NULL;
    }
    
    // Check if there are records to parse
    JsonToken token = json_parse_token(&ctx);
    if (token.type == TOKEN_COMMA) {
        // Parse records
        token = json_parse_token(&ctx);
        if (token.type == TOKEN_STRING && strcmp(token.value, "records") == 0) {
            json_free_token(&token);
            
            token = json_parse_token(&ctx);
            if (token.type != TOKEN_COLON) {
                LOG_ERROR("Invalid JSON: expected colon after 'records'");
                json_free_token(&token);
                return NULL;
            }
            json_free_token(&token);
            
            result = json_parse_table_records(&ctx, table);
            if (result != SUCCESS) {
                return NULL;
            }
        }
    }
    
    return table;
}

// JSON validation
ErrorCode validate_json(const char* json_str, char** error_msg) {
    VALIDATE_PTR(json_str);
    
    JsonParseContext ctx = {0};
    ctx.json = json_str;
    ctx.length = strlen(json_str);
    
    JsonToken token = json_parse_token(&ctx);
    if (token.type == TOKEN_NONE) {
        if (error_msg) {
            *error_msg = json_strdup(ctx.error_msg);
        }
        return ERROR_INVALID_INPUT;
    }
    
    // Basic validation: check if it's a valid JSON value
    if (token.type != TOKEN_OBJECT_START && token.type != TOKEN_ARRAY_START &&
        token.type != TOKEN_STRING && token.type != TOKEN_NUMBER &&
        token.type != TOKEN_BOOLEAN && token.type != TOKEN_NULL) {
        if (error_msg) {
            *error_msg = json_strdup("Invalid JSON: expected value");
        }
        json_free_token(&token);
        return ERROR_INVALID_INPUT;
    }
    
    json_free_token(&token);
    
    // Parse the rest to ensure it's complete
    token = json_parse_token(&ctx);
    if (token.type != TOKEN_EOF) {
        if (error_msg) {
            *error_msg = json_strdup("Invalid JSON: extra characters after value");
        }
        json_free_token(&token);
        return ERROR_INVALID_INPUT;
    }
    json_free_token(&token);
    
    return SUCCESS;
}

// JSON pretty printing
char* json_pretty_print(const char* json_str, int indent) {
    VALIDATE_PTR(json_str);
    
    // Simple pretty printer - in production, you'd want a more sophisticated one
    JsonSerializeOptions options = {0};
    options.pretty = true;
    
    // Try to parse and re-serialize
    JsonParseContext ctx = {0};
    ctx.json = json_str;
    ctx.length = strlen(json_str);
    
    // For now, just return a copy with basic formatting
    // A full implementation would parse and re-serialize with proper formatting
    char* result = json_strdup(json_str);
    
    // Basic indentation (simplified)
    if (indent > 0 && result) {
        // This is a very basic implementation
        char* formatted = (char*)json_malloc(strlen(result) * 2); // Estimate
        if (formatted) {
            int level = 0;
            size_t j = 0;
            bool in_string = false;
            bool escaped = false;
            
            for (size_t i = 0; result[i] && j < strlen(result) * 2 - 1; i++) {
                char c = result[i];
                
                if (!in_string) {
                    if (c == '{' || c == '[') {
                        formatted[j++] = c;
                        formatted[j++] = '\n';
                        level++;
                        for (int k = 0; k < level * indent && j < strlen(result) * 2 - 1; k++) {
                            formatted[j++] = ' ';
                        }
                    } else if (c == '}' || c == ']') {
                        formatted[j++] = '\n';
                        level--;
                        for (int k = 0; k < level * indent && j < strlen(result) * 2 - 1; k++) {
                            formatted[j++] = ' ';
                        }
                        formatted[j++] = c;
                    } else if (c == ',') {
                        formatted[j++] = c;
                        formatted[j++] = '\n';
                        for (int k = 0; k < level * indent && j < strlen(result) * 2 - 1; k++) {
                            formatted[j++] = ' ';
                        }
                    } else if (c == ':') {
                        formatted[j++] = c;
                        formatted[j++] = ' ';
                    } else {
                        formatted[j++] = c;
                        if (c == '"') {
                            in_string = true;
                        }
                    }
                } else {
                    formatted[j++] = c;
                    if (!escaped && c == '"') {
                        in_string = false;
                    }
                    escaped = (c == '\\' && !escaped);
                }
            }
            
            formatted[j] = '\0';
            free(result);
            result = formatted;
        }
    }
    
    return result;
}

// JSON merge
char* json_merge(const char* json1, const char* json2, JsonMergeStrategy strategy) {
    VALIDATE_PTR(json1);
    VALIDATE_PTR(json2);
    
    // Parse both JSON strings
    JsonParseContext ctx1 = {0};
    ctx1.json = json1;
    ctx1.length = strlen(json1);
    
    JsonParseContext ctx2 = {0};
    ctx2.json = json2;
    ctx2.length = strlen(json2);
    
    // For now, return concatenation for objects/arrays
    // A full implementation would properly merge based on strategy
    size_t total_len = strlen(json1) + strlen(json2) + 3; // +3 for comma and brackets
    char* merged = (char*)json_malloc(total_len);
    if (!merged) {
        return NULL;
    }
    
    snprintf(merged, total_len, "[%s,%s]", json1, json2);
    return merged;
}

// JSON path query (simplified)
char* json_path_query(const char* json_str, const char* path) {
    VALIDATE_PTR(json_str);
    VALIDATE_PTR(path);
    
    // Simplified JSON path implementation
    // In production, you'd want a full JSONPath implementation
    
    // For now, just return the whole JSON if path is "$"
    if (strcmp(path, "$") == 0) {
        return json_strdup(json_str);
    }
    
    // Basic implementation for simple paths like "$.key"
    if (path[0] == '$' && path[1] == '.') {
        const char* key = path + 2;
        
        // Parse JSON and look for the key
        JsonParseContext ctx = {0};
        ctx.json = json_str;
        ctx.length = strlen(json_str);
        
        JsonToken token = json_parse_token(&ctx);
        if (token.type != TOKEN_OBJECT_START) {
            json_free_token(&token);
            return NULL;
        }
        json_free_token(&token);
        
        while (1) {
            token = json_parse_token(&ctx);
            if (token.type == TOKEN_OBJECT_END) {
                json_free_token(&token);
                break;
            }
            
            if (token.type != TOKEN_STRING) {
                json_free_token(&token);
                return NULL;
            }
            
            char* current_key = token.value;
            json_free_token(&token);
            
            token = json_parse_token(&ctx);
            if (token.type != TOKEN_COLON) {
                free(current_key);
                json_free_token(&token);
                return NULL;
            }
            json_free_token(&token);
            
            if (strcmp(current_key, key) == 0) {
                free(current_key);
                
                // Get the value
                token = json_parse_token(&ctx);
                if (token.type == TOKEN_NONE) {
                    json_free_token(&token);
                    return NULL;
                }
                
                // Convert token back to JSON string
                char* result = NULL;
                if (token.type == TOKEN_STRING) {
                    result = json_escape_string_alloc(token.value);
                } else if (token.type == TOKEN_NUMBER || 
                          token.type == TOKEN_BOOLEAN || 
                          token.type == TOKEN_NULL) {
                    result = json_strdup(token.value);
                } else if (token.type == TOKEN_OBJECT_START || 
                          token.type == TOKEN_ARRAY_START) {
                    // Need to parse the whole object/array
                    // This is simplified - would need to parse recursively
                    result = json_strdup("{}"); // Placeholder
                }
                
                json_free_token(&token);
                return result;
            }
            
            free(current_key);
            
            // Skip value
            token = json_parse_token(&ctx);
            json_free_token(&token);
            
            token = json_parse_token(&ctx);
            if (token.type == TOKEN_COMMA) {
                json_free_token(&token);
                continue;
            } else if (token.type == TOKEN_OBJECT_END) {
                json_free_token(&token);
                break;
            } else {
                json_free_token(&token);
                return NULL;
            }
        }
    }
    
    return NULL;
}

// JSON statistics
JsonStats* json_get_stats(const char* json_str) {
    VALIDATE_PTR(json_str);
    
    JsonStats* stats = (JsonStats*)json_calloc(1, sizeof(JsonStats));
    if (!stats) {
        return NULL;
    }
    
    JsonParseContext ctx = {0};
    ctx.json = json_str;
    ctx.length = strlen(json_str);
    
    stats->total_size = ctx.length;
    stats->parse_time = clock();
    
    // Count elements
    JsonToken token;
    while ((token = json_parse_token(&ctx)).type != TOKEN_EOF) {
        switch (token.type) {
            case TOKEN_OBJECT_START:
                stats->object_count++;
                break;
            case TOKEN_ARRAY_START:
                stats->array_count++;
                break;
            case TOKEN_STRING:
                stats->string_count++;
                stats->string_bytes += token.length;
                break;
            case TOKEN_NUMBER:
                stats->number_count++;
                break;
            case TOKEN_BOOLEAN:
                stats->boolean_count++;
                break;
            case TOKEN_NULL:
                stats->null_count++;
                break;
            default:
                break;
        }
        json_free_token(&token);
    }
    
    stats->parse_time = clock() - stats->parse_time;
    stats->element_count = stats->string_count + stats->number_count + 
                          stats->boolean_count + stats->null_count;
    
    return stats;
}

// Cleanup
void json_free_stats(JsonStats* stats) {
    if (stats) {
        free(stats);
    }
}

// File operations
ErrorCode json_validate_file(const char* filename, char** error_msg) {
    VALIDATE_PTR(filename);
    
    FILE* file = fopen(filename, "rb");
    if (!file) {
        if (error_msg) {
            *error_msg = json_strdup("Failed to open file");
        }
        return ERROR_IO_OPERATION;
    }
    
    // Read first few bytes to check if it's JSON
    char buffer[1024];
    size_t read = fread(buffer, 1, sizeof(buffer) - 1, file);
    fclose(file);
    
    if (read == 0) {
        if (error_msg) {
            *error_msg = json_strdup("Empty file");
        }
        return ERROR_INVALID_INPUT;
    }
    
    buffer[read] = '\0';
    
    // Check if it starts with { or [
    if (buffer[0] != '{' && buffer[0] != '[') {
        if (error_msg) {
            *error_msg = json_strdup("File does not start with JSON object or array");
        }
        return ERROR_INVALID_INPUT;
    }
    
    return SUCCESS;
}

// JSON compression
char* json_compress(const char* json_str, JsonCompressionType type) {
    VALIDATE_PTR(json_str);
    
    // For now, just remove whitespace (minify)
    if (type == JSON_COMPRESS_MINIFY) {
        size_t len = strlen(json_str);
        char* compressed = (char*)json_malloc(len + 1);
        if (!compressed) {
            return NULL;
        }
        
        size_t j = 0;
        bool in_string = false;
        bool escaped = false;
        
        for (size_t i = 0; i < len; i++) {
            char c = json_str[i];
            
            if (!in_string) {
                if (!isspace(c)) {
                    compressed[j++] = c;
                    if (c == '"') {
                        in_string = true;
                    }
                }
            } else {
                compressed[j++] = c;
                if (!escaped && c == '"') {
                    in_string = false;
                }
                escaped = (c == '\\' && !escaped);
            }
        }
        
        compressed[j] = '\0';
        return compressed;
    }
    
    return json_strdup(json_str);
}

// JSON utilities
char* json_get_type(const char* json_str) {
    VALIDATE_PTR(json_str);
    
    JsonParseContext ctx = {0};
    ctx.json = json_str;
    ctx.length = strlen(json_str);
    
    JsonToken token = json_parse_token(&ctx);
    
    const char* type = "unknown";
    switch (token.type) {
        case TOKEN_OBJECT_START:
            type = "object";
            break;
        case TOKEN_ARRAY_START:
            type = "array";
            break;
        case TOKEN_STRING:
            type = "string";
            break;
        case TOKEN_NUMBER:
            type = "number";
            break;
        case TOKEN_BOOLEAN:
            type = "boolean";
            break;
        case TOKEN_NULL:
            type = "null";
            break;
        default:
            break;
    }
    
    json_free_token(&token);
    return json_strdup(type);
}

// JSON diff
JsonDiff* json_diff(const char* json1, const char* json2) {
    VALIDATE_PTR(json1);
    VALIDATE_PTR(json2);
    
    // Simplified diff - just compare strings
    JsonDiff* diff = (JsonDiff*)json_calloc(1, sizeof(JsonDiff));
    if (!diff) {
        return NULL;
    }
    
    diff->identical = (strcmp(json1, json2) == 0);
    
    // For a real implementation, you'd parse both and compare structure
    
    return diff;
}

void json_free_diff(JsonDiff* diff) {
    if (diff) {
        free(diff);
    }
}