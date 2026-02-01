// luminix_types.h - Central type definitions
#ifndef LUMINIX_TYPES_H
#define LUMINIX_TYPES_H

// Basic configuration
#define MAX_PATH_LEN 256
#define MAX_TABLE_NAME 64
#define MAX_FIELD_NAME 64
#define MAX_STRING_LEN 1024
#define MAX_QUERY_LEN 1024
#define INITIAL_CAPACITY 100

// Error codes
typedef enum {
    ERROR_NONE = 0,
    ERROR_MEMORY,
    ERROR_IO,
    ERROR_SYNTAX,
    ERROR_NOT_FOUND,
    ERROR_DUPLICATE,
    ERROR_CONSTRAINT,
    ERROR_TYPE_MISMATCH,
    ERROR_PERMISSION,
    ERROR_TIMEOUT,
    ERROR_CORRUPT,
    ERROR_FULL,
    ERROR_LOCKED,
    ERROR_DEADLOCK,
    ERROR_VERSION,
    ERROR_INVALID
} ErrorCode;

// Field types
typedef enum {
    FIELD_INT,
    FIELD_FLOAT,
    FIELD_STRING,
    FIELD_BOOL,
    FIELD_DATETIME,
    FIELD_BINARY,
    FIELD_NULL
} FieldType;

// Index types
typedef enum {
    INDEX_NONE = 0,
    INDEX_HASH,
    INDEX_BTREE,
    INDEX_SKIPLIST,
    INDEX_BITMAP,
    INDEX_FULLTEXT
} IndexType;

// Cache policies
typedef enum {
    CACHE_LRU,
    CACHE_LFU,
    CACHE_FIFO
} CachePolicy;

// Forward declarations
typedef struct Database Database;
typedef struct Table Table;
typedef struct Field Field;
typedef struct Record Record;
typedef struct Index Index;
typedef struct IndexManager IndexManager;
typedef struct QueryResult QueryResult;

#endif
