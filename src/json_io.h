#ifndef JSON_IO_H
#define JSON_IO_H
#include "config.h"

#include <stdio.h>     // For FILE
#include <stdlib.h>    // For malloc, free
#include <string.h>    // For string functions
#include <time.h>      // For strptime
#include <pthread.h>   // For pthread_rwlock_t

#include "database.h"


// ==================== JSON CONFIGURATION ====================

#define JSON_MAX_DEPTH 128
#define JSON_BUFFER_SIZE 4096
#define JSON_MAX_ERROR_MSG 256
#define JSON_INDENT_SIZE 2
#define JSON_MAX_PATH_LENGTH 1024
#define JSON_MAX_KEY_LENGTH 256

// ==================== JSON TYPES ====================

typedef enum {
    JSON_TYPE_NULL,
    JSON_TYPE_BOOLEAN,
    JSON_TYPE_NUMBER,
    JSON_TYPE_STRING,
    JSON_TYPE_ARRAY,
    JSON_TYPE_OBJECT
} JsonType;

typedef enum {
    JSON_NUMBER_INTEGER,
    JSON_NUMBER_FLOAT,
    JSON_NUMBER_DOUBLE
} JsonNumberType;

// ==================== JSON VALUE ====================

typedef struct JsonValue JsonValue;
typedef struct JsonArray JsonArray;
typedef struct JsonObject JsonObject;
typedef struct JsonMember JsonMember;

struct JsonValue {
    JsonType type;
    union {
        bool boolean;
        long long integer;
        double number;
        char* string;
        JsonArray* array;
        JsonObject* object;
    } value;
    size_t size;  // For strings: length, for arrays: count, for objects: member count
    JsonValue* parent;
    time_t timestamp;
    unsigned int ref_count;
};

struct JsonArray {
    JsonValue** values;
    size_t capacity;
    size_t count;
};

struct JsonMember {
    char* key;
    JsonValue* value;
    JsonMember* next;
};

struct JsonObject {
    JsonMember* head;
    JsonMember* tail;
    size_t count;
    JsonMember** hash_table;
    size_t hash_capacity;
};

// ==================== JSON SERIALIZATION OPTIONS ====================

typedef struct JsonSerializeOptions {
    bool pretty;                // Pretty print with indentation
    bool sort_keys;             // Sort object keys alphabetically
    bool escape_unicode;        // Escape non-ASCII characters
    bool escape_slashes;        // Escape forward slashes
    bool ensure_ascii;          // Ensure ASCII output
    int indent_size;            // Indentation size for pretty printing
    bool include_null_values;   // Include null values in output
    bool include_metadata;      // Include metadata (timestamps, etc.)
    bool include_data;          // Include actual data
    bool compress;              // Compress output (remove whitespace)
    bool human_readable;        // Optimize for human readability
    char* date_format;          // Custom date format string
    bool base64_blobs;          // Encode BLOBs as base64
    int float_precision;        // Precision for floating point numbers
    bool escape_html;           // Escape HTML characters
} JsonSerializeOptions;

typedef struct JsonSaveOptions {
    JsonSerializeOptions serialize;
    bool atomic_write;          // Use atomic file writes
    bool backup_existing;       // Backup existing file
    bool validate_before_save;  // Validate before saving
    bool compress_file;         // Compress the file (gzip)
    bool encrypt_file;          // Encrypt the file
    char* encryption_key;       // Encryption key (if encrypting)
    int compression_level;      // Compression level (0-9)
    bool incremental;           // Incremental save
    char* checksum_algorithm;   // Checksum algorithm (MD5, SHA256, etc.)
} JsonSaveOptions;

// ==================== JSON PARSING OPTIONS ====================

typedef struct JsonParseOptions {
    bool allow_comments;        // Allow C-style comments
    bool allow_trailing_commas; // Allow trailing commas
    bool allow_single_quotes;   // Allow single quotes for strings
    bool allow_unquoted_keys;   // Allow unquoted object keys
    bool allow_hex_numbers;     // Allow hexadecimal numbers
    bool allow_inf_nan;         // Allow Infinity and NaN
    bool allow_control_chars;   // Allow control characters in strings
    int max_depth;              // Maximum nesting depth
    size_t max_size;            // Maximum input size
    bool strict;                // Strict JSON parsing
    bool validate_utf8;         // Validate UTF-8 sequences
    bool ignore_unknown_fields; // Ignore unknown fields
    bool date_strings;          // Parse date strings as timestamps
    bool base64_strings;        // Parse base64 strings as binary
} JsonParseOptions;

typedef struct JsonLoadOptions {
    JsonParseOptions parse;
    bool validate_schema;       // Validate against schema
    char* schema_file;          // Schema file for validation
    bool repair;                // Attempt to repair corrupted data
    bool lazy_parse;            // Parse lazily (on-demand)
    bool memory_mapped;         // Use memory-mapped files
    bool verify_checksum;       // Verify checksum
    bool decompress;            // Decompress if compressed
    bool decrypt;               // Decrypt if encrypted
    char* decryption_key;       // Decryption key
} JsonLoadOptions;

// ==================== JSON DIFF & PATCH ====================

typedef enum {
    JSON_DIFF_ADD,
    JSON_DIFF_REMOVE,
    JSON_DIFF_REPLACE,
    JSON_DIFF_MOVE,
    JSON_DIFF_COPY,
    JSON_DIFF_TEST
} JsonDiffOperation;

typedef struct JsonDiffEntry {
    JsonDiffOperation op;
    char* path;
    JsonValue* value;
    JsonValue* old_value;
    char* from;                // For move/copy operations
    struct JsonDiffEntry* next;
} JsonDiffEntry;

typedef struct JsonDiff {
    JsonDiffEntry* entries;
    size_t count;
    bool identical;
    double similarity;         // 0.0 to 1.0
    char* summary;
} JsonDiff;

typedef struct JsonPatch {
    JsonDiffEntry* operations;
    size_t count;
    char* description;
    time_t created;
    char* author;
} JsonPatch;

// ==================== JSON SCHEMA ====================

typedef struct JsonSchema JsonSchema;
typedef struct JsonSchemaValidator JsonSchemaValidator;

typedef enum {
    SCHEMA_TYPE_NULL,
    SCHEMA_TYPE_BOOLEAN,
    SCHEMA_TYPE_INTEGER,
    SCHEMA_TYPE_NUMBER,
    SCHEMA_TYPE_STRING,
    SCHEMA_TYPE_ARRAY,
    SCHEMA_TYPE_OBJECT,
    SCHEMA_TYPE_ANY
} JsonSchemaType;

typedef struct JsonSchemaConstraint {
    JsonSchemaType type;
    bool required;
    bool nullable;
    union {
        struct {
            long long minimum;
            long long maximum;
            long long exclusive_minimum;
            long long exclusive_maximum;
            long long multiple_of;
        } integer;
        struct {
            double minimum;
            double maximum;
            double exclusive_minimum;
            double exclusive_maximum;
            double multiple_of;
        } number;
        struct {
            size_t min_length;
            size_t max_length;
            char* pattern;
            char* format;  // date-time, email, uri, etc.
        } string;
        struct {
            size_t min_items;
            size_t max_items;
            bool unique_items;
            JsonSchema* items;
            JsonSchema** any_of_items;
            size_t any_of_count;
        } array;
        struct {
            size_t min_properties;
            size_t max_properties;
            char** required_properties;
            size_t required_count;
            JsonSchema** properties;
            size_t property_count;
            JsonSchema* additional_properties;
            bool allow_additional;
        } object;
    } constraints;
    JsonValue* enum_values;
    size_t enum_count;
    JsonValue* default_value;
    char* description;
    char* title;
    struct JsonSchemaConstraint* next;
} JsonSchemaConstraint;

struct JsonSchema {
    char* id;
    char* schema;  // JSON Schema version
    char* title;
    char* description;
    JsonSchemaType type;
    JsonSchemaConstraint* constraints;
    JsonSchema** definitions;
    size_t definition_count;
    bool draft_validation;  // Support draft-04, draft-06, draft-07
};

struct JsonSchemaValidator {
    JsonSchema* schema;
    bool strict;
    bool validate_formats;
    bool validate_enum;
    void* user_data;
    int (*custom_validator)(JsonValue*, JsonSchemaConstraint*, void*);
};

// ==================== JSON QUERY (JSONPath) ====================

typedef struct JsonPathNode JsonPathNode;
typedef struct JsonPathQuery JsonPathQuery;
typedef struct JsonPathResult JsonPathResult;

typedef enum {
    PATH_ROOT,           // $
    PATH_CURRENT,        // @
    PATH_PROPERTY,       // .name or ['name']
    PATH_WILDCARD,       // .* or [*]
    PATH_ARRAY_INDEX,    // [index] or [start:end:step]
    PATH_ARRAY_SLICE,    // [start:end]
    PATH_FILTER,         // [?expression]
    PATH_SCRIPT,         // [(expression)]
    PATH_UNION,          // [a,b,c]
    PATH_RECURSIVE,      // ..
    PATH_FUNCTION        // function()
} JsonPathNodeType;

typedef enum {
    OP_EQUAL,
    OP_NOT_EQUAL,
    OP_LESS,
    OP_LESS_EQUAL,
    OP_GREATER,
    OP_GREATER_EQUAL,
    OP_AND,
    OP_OR,
    OP_NOT,
    OP_IN,
    OP_SUBSET,
    OP_SIZE,
    OP_EMPTY,
    OP_MATCH,  // regex match
    OP_CONTAINS
} JsonPathOperator;

typedef struct JsonPathExpression {
    JsonPathOperator op;
    JsonValue* left;
    JsonValue* right;
    struct JsonPathExpression* next;
} JsonPathExpression;

struct JsonPathNode {
    JsonPathNodeType type;
    union {
        char* property;
        long long index;
        struct {
            long long start;
            long long end;
            long long step;
        } slice;
        JsonPathExpression* filter;
        char* script;
        char** union_paths;
        size_t union_count;
        struct {
            char* name;
            JsonPathNode** arguments;
            size_t arg_count;
        } function;
    } value;
    JsonPathNode* next;
};

struct JsonPathQuery {
    JsonPathNode* root;
    char* original;
    bool normalize;
    bool suppress_exceptions;
    void* cache;
};

struct JsonPathResult {
    JsonValue** values;
    size_t count;
    size_t capacity;
    char** paths;
    double execution_time;
    size_t memory_used;
};

// ==================== JSON TRANSFORM ====================

typedef struct JsonTransformRule JsonTransformRule;
typedef struct JsonTransformer JsonTransformer;

typedef enum {
    TRANSFORM_MAP,
    TRANSFORM_FILTER,
    TRANSFORM_REDUCE,
    TRANSFORM_SORT,
    TRANSFORM_GROUP,
    TRANSFORM_FLATTEN,
    TRANSFORM_UNWRAP,
    TRANSFORM_RENAME,
    TRANSFORM_DEFAULT,
    TRANSFORM_CONCAT,
    TRANSFORM_MERGE,
    TRANSFORM_SPLIT
} JsonTransformType;

struct JsonTransformRule {
    JsonTransformType type;
    char* input_path;
    char* output_path;
    JsonValue* parameters;
    JsonValue* condition;
    struct JsonTransformRule* next;
};

struct JsonTransformer {
    JsonTransformRule* rules;
    size_t rule_count;
    bool in_place;
    bool preserve_structure;
    JsonSchema* output_schema;
    void* user_data;
    JsonValue* (*custom_transform)(JsonValue*, JsonTransformRule*, void*);
};

// ==================== JSON STREAMING ====================

typedef struct JsonStreamParser JsonStreamParser;
typedef struct JsonStreamWriter JsonStreamWriter;

typedef enum {
    STREAM_OBJECT_START,
    STREAM_OBJECT_END,
    STREAM_ARRAY_START,
    STREAM_ARRAY_END,
    STREAM_KEY,
    STREAM_VALUE,
    STREAM_EOF,
    STREAM_ERROR
} JsonStreamEventType;

typedef struct JsonStreamEvent {
    JsonStreamEventType type;
    JsonValue* value;
    char* key;
    size_t depth;
    size_t position;
    void* user_data;
} JsonStreamEvent;

typedef void (*JsonStreamCallback)(JsonStreamEvent* event, void* user_data);

struct JsonStreamParser {
    FILE* file;
    char* buffer;
    size_t buffer_size;
    size_t position;
    size_t bytes_read;
    bool eof;
    JsonStreamCallback callback;
    void* user_data;
    int depth;
    JsonParseOptions options;
    char error_msg[JSON_MAX_ERROR_MSG];
};

struct JsonStreamWriter {
    FILE* file;
    char* buffer;
    size_t buffer_size;
    size_t position;
    JsonSerializeOptions options;
    int depth;
    bool pretty;
    int indent;
    bool first_item;
};

// ==================== JSON STATISTICS ====================

typedef struct JsonStats {
    size_t total_size;
    size_t object_count;
    size_t array_count;
    size_t string_count;
    size_t number_count;
    size_t boolean_count;
    size_t null_count;
    size_t string_bytes;
    size_t max_depth;
    double parse_time;
    double serialize_time;
    size_t memory_used;
    size_t element_count;
    size_t key_count;
    size_t duplicate_keys;
    struct {
        size_t objects;
        size_t arrays;
        size_t strings;
        size_t numbers;
    } per_depth[JSON_MAX_DEPTH];
} JsonStats;

// ==================== JSON MERGE STRATEGIES ====================

typedef enum {
    MERGE_OVERWRITE,        // New values overwrite old
    MERGE_PRESERVE,         // Old values are preserved
    MERGE_DEEP,             // Recursive merge
    MERGE_CONCAT,           // Concatenate arrays
    MERGE_UNION,            // Union of arrays (unique)
    MERGE_INTERSECTION,     // Intersection of arrays
    MERGE_CUSTOM            // Custom merge function
} JsonMergeStrategy;

// ==================== JSON COMPRESSION ====================

typedef enum {
    JSON_COMPRESS_NONE,
    JSON_COMPRESS_MINIFY,   // Remove whitespace
    JSON_COMPRESS_GZIP,
    JSON_COMPRESS_DEFLATE,
    JSON_COMPRESS_BROTLI,
    JSON_COMPRESS_LZ4,
    JSON_COMPRESS_ZSTD,
    JSON_COMPRESS_CUSTOM
} JsonCompressionType;

// ==================== JSON ENCRYPTION ====================

typedef enum {
    JSON_ENCRYPT_NONE,
    JSON_ENCRYPT_AES_256,
    JSON_ENCRYPT_CHACHA20,
    JSON_ENCRYPT_CUSTOM
} JsonEncryptionType;

// ==================== JSON VALIDATION RESULT ====================

typedef struct JsonValidationError {
    char* path;
    char* message;
    char* schema_path;
    JsonValue* value;
    struct JsonValidationError* next;
} JsonValidationError;

typedef struct JsonValidationResult {
    bool valid;
    JsonValidationError* errors;
    size_t error_count;
    double validation_time;
    JsonStats* stats;
    char* schema_id;
} JsonValidationResult;

// ==================== JSON CACHE ====================

typedef struct JsonCacheEntry {
    char* key;
    JsonValue* value;
    time_t timestamp;
    time_t expires;
    size_t size;
    unsigned int hits;
    struct JsonCacheEntry* next;
    struct JsonCacheEntry* prev;
} JsonCacheEntry;

typedef struct JsonCache {
    JsonCacheEntry** table;
    JsonCacheEntry* head;
    JsonCacheEntry* tail;
    size_t capacity;
    size_t size;
    size_t max_size;
    time_t default_ttl;
    pthread_rwlock_t lock;
    long long hits;
    long long misses;
    long long evictions;
} JsonCache;

// ==================== JSON DATABASE SERIALIZATION API ====================

// Core serialization/deserialization
ErrorCode db_save_to_file(Database* db, const char* filename, JsonSaveOptions* options);
Database* db_load_from_file(const char* filename, JsonLoadOptions* options);
ErrorCode db_save_to_string(Database* db, char** json_str, JsonSerializeOptions* options);
Database* db_load_from_string(const char* json_str, JsonLoadOptions* options);

// Incremental save/load
ErrorCode db_save_incremental(Database* db, const char* filename, JsonSaveOptions* options);
ErrorCode db_load_incremental(Database* db, const char* filename, JsonLoadOptions* options);
ErrorCode db_save_delta(Database* db, const char* filename, Database* base, JsonSaveOptions* options);
Database* db_load_delta(const char* filename, Database* base, JsonLoadOptions* options);

// Backup and restore
ErrorCode db_create_backup(Database* db, const char* backup_dir, JsonSaveOptions* options);
ErrorCode db_restore_backup(Database* db, const char* backup_file, JsonLoadOptions* options);
ErrorCode db_list_backups(const char* backup_dir, char*** backups, int* count);
ErrorCode db_delete_backup(const char* backup_file);

// Table serialization
char* table_to_json(Table* table, JsonSerializeOptions* options);
Table* json_to_table(const char* json_str, Database* db, JsonLoadOptions* options);
ErrorCode table_save_to_file(Table* table, const char* filename, JsonSaveOptions* options);
Table* table_load_from_file(const char* filename, Database* db, JsonLoadOptions* options);

// Record serialization
char* record_to_json(Record* record, Table* table, JsonSerializeOptions* options);
Record* json_to_record(const char* json_str, Table* table, JsonLoadOptions* options);
ErrorCode record_save_to_file(Record* record, Table* table, const char* filename, JsonSaveOptions* options);
Record* record_load_from_file(const char* filename, Table* table, JsonLoadOptions* options);

// Query result serialization
char* query_result_to_json(QueryResult* result, JsonSerializeOptions* options);
QueryResult* json_to_query_result(const char* json_str, JsonLoadOptions* options);
ErrorCode query_result_save_to_file(QueryResult* result, const char* filename, JsonSaveOptions* options);
QueryResult* query_result_load_from_file(const char* filename, JsonLoadOptions* options);

// Schema serialization
char* schema_to_json(Database* db, JsonSerializeOptions* options);
ErrorCode schema_save_to_file(Database* db, const char* filename, JsonSaveOptions* options);
ErrorCode schema_load_from_file(Database* db, const char* filename, JsonLoadOptions* options);
ErrorCode schema_validate_json(Database* db, const char* json_str, JsonValidationResult** result);

// ==================== JSON VALUE API ====================

// Creation and destruction
JsonValue* json_value_create_null();
JsonValue* json_value_create_boolean(bool value);
JsonValue* json_value_create_integer(long long value);
JsonValue* json_value_create_number(double value);
JsonValue* json_value_create_string(const char* value);
JsonValue* json_value_create_array();
JsonValue* json_value_create_object();
void json_value_free(JsonValue* value);
JsonValue* json_value_clone(JsonValue* value);
JsonValue* json_value_deep_clone(JsonValue* value);

// Type checking
JsonType json_value_get_type(JsonValue* value);
bool json_value_is_null(JsonValue* value);
bool json_value_is_boolean(JsonValue* value);
bool json_value_is_number(JsonValue* value);
bool json_value_is_integer(JsonValue* value);
bool json_value_is_double(JsonValue* value);
bool json_value_is_string(JsonValue* value);
bool json_value_is_array(JsonValue* value);
bool json_value_is_object(JsonValue* value);

// Value access
bool json_value_get_boolean(JsonValue* value);
long long json_value_get_integer(JsonValue* value);
double json_value_get_number(JsonValue* value);
const char* json_value_get_string(JsonValue* value);
size_t json_value_get_string_length(JsonValue* value);

// Array operations
size_t json_array_size(JsonValue* array);
JsonValue* json_array_get(JsonValue* array, size_t index);
ErrorCode json_array_set(JsonValue* array, size_t index, JsonValue* value);
ErrorCode json_array_append(JsonValue* array, JsonValue* value);
ErrorCode json_array_insert(JsonValue* array, size_t index, JsonValue* value);
ErrorCode json_array_remove(JsonValue* array, size_t index);
ErrorCode json_array_clear(JsonValue* array);
JsonValue* json_array_find(JsonValue* array, JsonValue* value);
ssize_t json_array_index_of(JsonValue* array, JsonValue* value);

// Object operations
size_t json_object_size(JsonValue* object);
bool json_object_has(JsonValue* object, const char* key);
JsonValue* json_object_get(JsonValue* object, const char* key);
ErrorCode json_object_set(JsonValue* object, const char* key, JsonValue* value);
ErrorCode json_object_remove(JsonValue* object, const char* key);
ErrorCode json_object_clear(JsonValue* object);
char** json_object_keys(JsonValue* object, size_t* count);
JsonValue** json_object_values(JsonValue* object, size_t* count);
JsonMember* json_object_iter_begin(JsonValue* object);
JsonMember* json_object_iter_next(JsonMember* member);

// Type conversion
JsonValue* json_value_convert(JsonValue* value, JsonType target_type);
bool json_value_equal(JsonValue* a, JsonValue* b);
int json_value_compare(JsonValue* a, JsonValue* b);

// ==================== JSON PARSING API ====================

// Parsing
JsonValue* json_parse(const char* json_str, JsonParseOptions* options);
JsonValue* json_parse_file(const char* filename, JsonParseOptions* options);
JsonValue* json_parse_stream(FILE* stream, JsonParseOptions* options);
char* json_get_parse_error();

// Serialization
char* json_serialize(JsonValue* value, JsonSerializeOptions* options);
ErrorCode json_serialize_file(JsonValue* value, const char* filename, JsonSerializeOptions* options);
ErrorCode json_serialize_stream(JsonValue* value, FILE* stream, JsonSerializeOptions* options);
size_t json_serialize_buffer(JsonValue* value, char* buffer, size_t size, JsonSerializeOptions* options);

// Validation
ErrorCode json_validate(const char* json_str, JsonParseOptions* options, char** error_msg);
ErrorCode json_validate_file(const char* filename, JsonParseOptions* options, char** error_msg);
bool json_is_valid(const char* json_str);

// ==================== JSON STREAMING API ====================

// Streaming parser
JsonStreamParser* json_stream_parser_create(FILE* file, JsonStreamCallback callback, void* user_data, JsonParseOptions* options);
JsonStreamParser* json_stream_parser_create_string(const char* str, JsonStreamCallback callback, void* user_data, JsonParseOptions* options);
ErrorCode json_stream_parser_parse(JsonStreamParser* parser);
ErrorCode json_stream_parser_parse_chunk(JsonStreamParser* parser, const char* chunk, size_t size);
ErrorCode json_stream_parser_reset(JsonStreamParser* parser);
void json_stream_parser_free(JsonStreamParser* parser);

// Streaming writer
JsonStreamWriter* json_stream_writer_create(FILE* file, JsonSerializeOptions* options);
JsonStreamWriter* json_stream_writer_create_string(JsonSerializeOptions* options);
ErrorCode json_stream_writer_begin_object(JsonStreamWriter* writer);
ErrorCode json_stream_writer_end_object(JsonStreamWriter* writer);
ErrorCode json_stream_writer_begin_array(JsonStreamWriter* writer);
ErrorCode json_stream_writer_end_array(JsonStreamWriter* writer);
ErrorCode json_stream_writer_write_key(JsonStreamWriter* writer, const char* key);
ErrorCode json_stream_writer_write_null(JsonStreamWriter* writer);
ErrorCode json_stream_writer_write_boolean(JsonStreamWriter* writer, bool value);
ErrorCode json_stream_writer_write_integer(JsonStreamWriter* writer, long long value);
ErrorCode json_stream_writer_write_number(JsonStreamWriter* writer, double value);
ErrorCode json_stream_writer_write_string(JsonStreamWriter* writer, const char* value);
ErrorCode json_stream_writer_write_value(JsonStreamWriter* writer, JsonValue* value);
ErrorCode json_stream_writer_flush(JsonStreamWriter* writer);
char* json_stream_writer_get_string(JsonStreamWriter* writer);
void json_stream_writer_free(JsonStreamWriter* writer);

// ==================== JSON PATH QUERY API ====================

// JSONPath queries
JsonPathQuery* json_path_compile(const char* path_expression);
JsonPathResult* json_path_query(JsonValue* root, JsonPathQuery* query);
JsonPathResult* json_path_query_string(const char* json_str, const char* path);
char* json_path_query_to_string(JsonValue* root, const char* path, JsonSerializeOptions* options);
void json_path_query_free_result(JsonPathResult* result);
void json_path_query_free(JsonPathQuery* query);

// Path operations
bool json_path_exists(JsonValue* root, const char* path);
JsonValue* json_path_get(JsonValue* root, const char* path);
ErrorCode json_path_set(JsonValue* root, const char* path, JsonValue* value);
ErrorCode json_path_delete(JsonValue* root, const char* path);
JsonValue* json_path_extract(JsonValue* root, const char* path);

// Path utilities
char* json_path_normalize(const char* path);
bool json_path_validate(const char* path, char** error_msg);
char** json_path_split(const char* path, size_t* count);
char* json_path_join(const char** parts, size_t count);

// ==================== JSON TRANSFORM API ====================

// Transformation
JsonValue* json_transform(JsonValue* value, JsonTransformer* transformer);
JsonValue* json_transform_with_rules(JsonValue* value, JsonTransformRule* rules);
JsonValue* json_map(JsonValue* value, JsonValue* (*mapper)(JsonValue*, void*), void* user_data);
JsonValue* json_filter(JsonValue* value, bool (*predicate)(JsonValue*, void*), void* user_data);
JsonValue* json_reduce(JsonValue* value, JsonValue* initial, JsonValue* (*reducer)(JsonValue*, JsonValue*, void*), void* user_data);

// Common transforms
JsonValue* json_flatten(JsonValue* value, const char* separator);
JsonValue* json_unflatten(JsonValue* value, const char* separator);
JsonValue* json_sort(JsonValue* value, bool ascending, JsonValue* (*key_func)(JsonValue*, void*), void* user_data);
JsonValue* json_group_by(JsonValue* value, JsonValue* (*key_func)(JsonValue*, void*), void* user_data);
JsonValue* json_rename_keys(JsonValue* value, const char** old_keys, const char** new_keys, size_t count);

// Transformer management
JsonTransformer* json_transformer_create();
ErrorCode json_transformer_add_rule(JsonTransformer* transformer, JsonTransformRule* rule);
void json_transformer_free(JsonTransformer* transformer);
JsonTransformRule* json_transform_rule_create(JsonTransformType type, const char* input_path, const char* output_path);
void json_transform_rule_free(JsonTransformRule* rule);

// ==================== JSON MERGE & PATCH API ====================

// Merging
JsonValue* json_merge(JsonValue* base, JsonValue* overlay, JsonMergeStrategy strategy);
JsonValue* json_merge_all(JsonValue** values, size_t count, JsonMergeStrategy strategy);
JsonValue* json_merge_with_custom(JsonValue* base, JsonValue* overlay, JsonValue* (*merger)(JsonValue*, JsonValue*, void*), void* user_data);

// Diff and patch
JsonDiff* json_diff(JsonValue* a, JsonValue* b);
char* json_diff_to_string(JsonDiff* diff, JsonSerializeOptions* options);
JsonPatch* json_create_patch(JsonValue* source, JsonValue* target);
ErrorCode json_apply_patch(JsonValue* value, JsonPatch* patch);
ErrorCode json_apply_patch_in_place(JsonValue** value, JsonPatch* patch);
JsonValue* json_patch_test(JsonValue* value, JsonPatch* patch);
void json_diff_free(JsonDiff* diff);
void json_patch_free(JsonPatch* patch);

// Patch operations
JsonPatch* json_patch_create();
ErrorCode json_patch_add_operation(JsonPatch* patch, JsonDiffOperation op, const char* path, JsonValue* value, const char* from);
ErrorCode json_patch_validate(JsonPatch* patch, JsonValue* value);
JsonPatch* json_patch_invert(JsonPatch* patch);
JsonPatch* json_patch_compose(JsonPatch* patch1, JsonPatch* patch2);

// ==================== JSON SCHEMA API ====================

// Schema handling
JsonSchema* json_schema_create();
JsonSchema* json_schema_load(const char* filename);
JsonSchema* json_schema_parse(const char* json_str);
ErrorCode json_schema_save(JsonSchema* schema, const char* filename, JsonSerializeOptions* options);
char* json_schema_serialize(JsonSchema* schema, JsonSerializeOptions* options);
void json_schema_free(JsonSchema* schema);

// Schema validation
JsonSchemaValidator* json_schema_validator_create(JsonSchema* schema);
JsonValidationResult* json_schema_validate(JsonSchemaValidator* validator, JsonValue* value);
JsonValidationResult* json_schema_validate_string(JsonSchemaValidator* validator, const char* json_str);
bool json_schema_is_valid(JsonSchemaValidator* validator, JsonValue* value);
void json_schema_validator_free(JsonSchemaValidator* validator);
void json_validation_result_free(JsonValidationResult* result);

// Schema operations
ErrorCode json_schema_add_constraint(JsonSchema* schema, JsonSchemaConstraint* constraint);
ErrorCode json_schema_add_definition(JsonSchema* schema, const char* name, JsonSchema* definition);
JsonSchema* json_schema_get_definition(JsonSchema* schema, const char* name);
JsonSchema* json_schema_dereference(JsonSchema* schema, const char* ref);
bool json_schema_equals(JsonSchema* a, JsonSchema* b);

// Constraint management
JsonSchemaConstraint* json_schema_constraint_create(JsonSchemaType type);
void json_schema_constraint_free(JsonSchemaConstraint* constraint);
ErrorCode json_schema_constraint_set_range(JsonSchemaConstraint* constraint, double min, double max, bool exclusive_min, bool exclusive_max);
ErrorCode json_schema_constraint_set_length(JsonSchemaConstraint* constraint, size_t min, size_t max);
ErrorCode json_schema_constraint_set_pattern(JsonSchemaConstraint* constraint, const char* pattern);
ErrorCode json_schema_constraint_set_enum(JsonSchemaConstraint* constraint, JsonValue** values, size_t count);
ErrorCode json_schema_constraint_set_default(JsonSchemaConstraint* constraint, JsonValue* default_value);

// ==================== JSON UTILITIES ====================

// Statistics
JsonStats* json_get_stats(JsonValue* value);
JsonStats* json_get_stats_string(const char* json_str);
void json_stats_free(JsonStats* stats);
void json_stats_print(JsonStats* stats, FILE* stream);

// Pretty printing
char* json_pretty_print(const char* json_str, int indent);
char* json_pretty_print_value(JsonValue* value, int indent);
ErrorCode json_pretty_print_file(const char* input_file, const char* output_file, int indent);

// Minification
char* json_minify(const char* json_str);
char* json_minify_value(JsonValue* value);
ErrorCode json_minify_file(const char* input_file, const char* output_file);

// Format conversion
char* json_to_xml(JsonValue* value, const char* root_name);
char* json_to_csv(JsonValue* value, const char* delimiter);
char* json_to_yaml(JsonValue* value, int indent);
JsonValue* xml_to_json(const char* xml_str);
JsonValue* csv_to_json(const char* csv_str, const char* delimiter, bool has_header);
JsonValue* yaml_to_json(const char* yaml_str);

// Encoding/decoding
char* json_base64_encode(const void* data, size_t size);
void* json_base64_decode(const char* base64, size_t* size);
char* json_url_encode(const char* str);
char* json_url_decode(const char* str);
char* json_html_escape(const char* str);
char* json_html_unescape(const char* str);

// Compression
char* json_compress(const char* json_str, JsonCompressionType type, int level);
char* json_decompress(const char* compressed, size_t size, JsonCompressionType type);
ErrorCode json_compress_file(const char* input_file, const char* output_file, JsonCompressionType type, int level);
ErrorCode json_decompress_file(const char* input_file, const char* output_file, JsonCompressionType type);

// Encryption
char* json_encrypt(const char* json_str, JsonEncryptionType type, const char* key, const char* iv);
char* json_decrypt(const char* encrypted, size_t size, JsonEncryptionType type, const char* key, const char* iv);
ErrorCode json_encrypt_file(const char* input_file, const char* output_file, JsonEncryptionType type, const char* key);
ErrorCode json_decrypt_file(const char* input_file, const char* output_file, JsonEncryptionType type, const char* key);

// Checksums
char* json_checksum(JsonValue* value, const char* algorithm);
char* json_checksum_string(const char* json_str, const char* algorithm);
bool json_verify_checksum(JsonValue* value, const char* algorithm, const char* checksum);

// ==================== JSON CACHE API ====================

JsonCache* json_cache_create(size_t max_size, time_t default_ttl);
void json_cache_free(JsonCache* cache);
JsonValue* json_cache_get(JsonCache* cache, const char* key);
ErrorCode json_cache_put(JsonCache* cache, const char* key, JsonValue* value, time_t ttl);
ErrorCode json_cache_remove(JsonCache* cache, const char* key);
ErrorCode json_cache_clear(JsonCache* cache);
size_t json_cache_size(JsonCache* cache);
JsonCacheEntry** json_cache_entries(JsonCache* cache, size_t* count);
void json_cache_stats(JsonCache* cache, long long* hits, long long* misses, long long* evictions);

// ==================== JSON ERROR HANDLING ====================

typedef enum {
    JSON_ERROR_NONE,
    JSON_ERROR_SYNTAX,
    JSON_ERROR_IO,
    JSON_ERROR_MEMORY,
    JSON_ERROR_DEPTH,
    JSON_ERROR_UTF8,
    JSON_ERROR_INVALID_TYPE,
    JSON_ERROR_PATH,
    JSON_ERROR_SCHEMA,
    JSON_ERROR_TRANSFORM,
    JSON_ERROR_MERGE,
    JSON_ERROR_PATCH,
    JSON_ERROR_VALIDATION,
    JSON_ERROR_COMPRESSION,
    JSON_ERROR_ENCRYPTION
} JsonErrorCode;

typedef struct JsonError {
    JsonErrorCode code;
    char message[JSON_MAX_ERROR_MSG];
    size_t position;
    int line;
    int column;
    char* context;
    struct JsonError* inner;
} JsonError;

JsonError* json_get_last_error();
void json_clear_error();
const char* json_error_code_to_string(JsonErrorCode code);
char* json_error_to_string(JsonError* error);
void json_error_free(JsonError* error);

// ==================== JSON OPTIONS MANAGEMENT ====================

// Default options
JsonSerializeOptions* json_default_serialize_options();
JsonParseOptions* json_default_parse_options();
JsonSaveOptions* json_default_save_options();
JsonLoadOptions* json_default_load_options();

// Option management
void json_serialize_options_free(JsonSerializeOptions* options);
void json_parse_options_free(JsonParseOptions* options);
void json_save_options_free(JsonSaveOptions* options);
void json_load_options_free(JsonLoadOptions* options);
JsonSerializeOptions* json_serialize_options_copy(JsonSerializeOptions* options);
JsonParseOptions* json_parse_options_copy(JsonParseOptions* options);

// ==================== JSON VERSIONING ====================

typedef struct JsonVersionInfo {
    int major;
    int minor;
    int patch;
    char* build_date;
    char* features;
    char* json_schema_version;
} JsonVersionInfo;

JsonVersionInfo* json_get_version_info();
void json_free_version_info(JsonVersionInfo* info);

// ==================== JSON BULK OPERATIONS ====================

// Batch processing
ErrorCode json_batch_parse(const char** json_strings, size_t count, JsonValue*** results, JsonParseOptions* options);
ErrorCode json_batch_serialize(JsonValue** values, size_t count, char*** results, JsonSerializeOptions* options);
ErrorCode json_batch_validate(const char** json_strings, size_t count, bool** results, JsonParseOptions* options);

// Parallel processing
ErrorCode json_parallel_parse(const char** json_strings, size_t count, JsonValue*** results, JsonParseOptions* options, int threads);
ErrorCode json_parallel_serialize(JsonValue** values, size_t count, char*** results, JsonSerializeOptions* options, int threads);

// ==================== JSON MACROS ====================

// Convenience macros for common operations
#define JSON_OBJECT_FOREACH(obj, key, val) \
    for (JsonMember* __iter = json_object_iter_begin(obj); \
         __iter != NULL && ((key = __iter->key) != NULL) && ((val = __iter->value) != NULL); \
         __iter = json_object_iter_next(__iter))

#define JSON_ARRAY_FOREACH(arr, index, val) \
    for (size_t index = 0; index < json_array_size(arr) && ((val = json_array_get(arr, index)) != NULL); index++)

#define JSON_SAFE_FREE(ptr) \
    do { \
        if (ptr) { \
            free(ptr); \
            (ptr) = NULL; \
        } \
    } while(0)

#define JSON_RETURN_IF_ERROR(expr) \
    do { \
        ErrorCode __result = (expr); \
        if (__result != SUCCESS) return __result; \
    } while(0)

#define JSON_CHECK_NULL(ptr) \
    do { \
        if ((ptr) == NULL) { \
            LOG_ERROR("NULL pointer at %s:%d", __FILE__, __LINE__); \
            return NULL; \
        } \
    } while(0)

// Type checking macros
#define JSON_IS_NULL(val) (json_value_get_type(val) == JSON_TYPE_NULL)
#define JSON_IS_BOOLEAN(val) (json_value_get_type(val) == JSON_TYPE_BOOLEAN)
#define JSON_IS_NUMBER(val) (json_value_get_type(val) == JSON_TYPE_NUMBER)
#define JSON_IS_STRING(val) (json_value_get_type(val) == JSON_TYPE_STRING)
#define JSON_IS_ARRAY(val) (json_value_get_type(val) == JSON_TYPE_ARRAY)
#define JSON_IS_OBJECT(val) (json_value_get_type(val) == JSON_TYPE_OBJECT)

// Value access macros
#define JSON_AS_BOOLEAN(val) (json_value_get_boolean(val))
#define JSON_AS_INTEGER(val) (json_value_get_integer(val))
#define JSON_AS_NUMBER(val) (json_value_get_number(val))
#define JSON_AS_STRING(val) (json_value_get_string(val))

// ==================== JSON CALLBACK TYPES ====================

typedef void (*JsonFreeFunc)(void* ptr);
typedef JsonValue* (*JsonCloneFunc)(JsonValue* value, void* user_data);
typedef int (*JsonCompareFunc)(JsonValue* a, JsonValue* b, void* user_data);
typedef char* (*JsonSerializeFunc)(JsonValue* value, void* user_data);
typedef JsonValue* (*JsonParseFunc)(const char* str, void* user_data);

// ==================== JSON PLUGIN SYSTEM ====================

typedef struct JsonPlugin {
    char name[64];
    char version[32];
    JsonType type;
    JsonFreeFunc free_func;
    JsonCloneFunc clone_func;
    JsonCompareFunc compare_func;
    JsonSerializeFunc serialize_func;
    JsonParseFunc parse_func;
    void* data;
    struct JsonPlugin* next;
} JsonPlugin;

ErrorCode json_register_plugin(JsonPlugin* plugin);
ErrorCode json_unregister_plugin(const char* name);
JsonPlugin* json_get_plugin(const char* name);
JsonPlugin* json_get_plugin_by_type(JsonType type);

// ==================== JSON EVENT SYSTEM ====================

typedef enum {
    JSON_EVENT_PARSE_START,
    JSON_EVENT_PARSE_END,
    JSON_EVENT_SERIALIZE_START,
    JSON_EVENT_SERIALIZE_END,
    JSON_EVENT_VALIDATE_START,
    JSON_EVENT_VALIDATE_END,
    JSON_EVENT_TRANSFORM_START,
    JSON_EVENT_TRANSFORM_END,
    JSON_EVENT_MERGE_START,
    JSON_EVENT_MERGE_END,
    JSON_EVENT_ERROR
} JsonEventType;

typedef struct JsonEvent {
    JsonEventType type;
    JsonValue* data;
    void* user_data;
    time_t timestamp;
} JsonEvent;

typedef void (*JsonEventHandler)(JsonEvent* event);

ErrorCode json_register_event_handler(JsonEventType type, JsonEventHandler handler);
ErrorCode json_unregister_event_handler(JsonEventType type, JsonEventHandler handler);
ErrorCode json_trigger_event(JsonEventType type, JsonValue* data, void* user_data);

// ==================== JSON PROFILING ====================

typedef struct JsonProfileEntry {
    char* operation;
    double start_time;
    double end_time;
    size_t memory_before;
    size_t memory_after;
    struct JsonProfileEntry* next;
} JsonProfileEntry;

typedef struct JsonProfile {
    JsonProfileEntry* entries;
    size_t entry_count;
    double total_time;
    size_t peak_memory;
    bool enabled;
} JsonProfile;

JsonProfile* json_profile_start();
void json_profile_stop(JsonProfile* profile);
ErrorCode json_profile_begin(JsonProfile* profile, const char* operation);
ErrorCode json_profile_end(JsonProfile* profile, const char* operation);
char* json_profile_report(JsonProfile* profile, JsonSerializeOptions* options);
void json_profile_free(JsonProfile* profile);

// ==================== JSON TESTING ====================

typedef struct JsonTestCase {
    char* name;
    char* input;
    char* expected;
    JsonParseOptions* parse_options;
    JsonSerializeOptions* serialize_options;
    bool should_fail;
} JsonTestCase;

typedef struct JsonTestSuite {
    char* name;
    JsonTestCase* cases;
    size_t case_count;
    int passed;
    int failed;
    double total_time;
} JsonTestSuite;

JsonTestSuite* json_test_suite_create(const char* name);
ErrorCode json_test_suite_add_case(JsonTestSuite* suite, JsonTestCase* test_case);
ErrorCode json_test_suite_run(JsonTestSuite* suite);
char* json_test_suite_report(JsonTestSuite* suite, JsonSerializeOptions* options);
void json_test_suite_free(JsonTestSuite* suite);

#endif // JSON_IO_H