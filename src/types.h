// types.h
#ifndef TYPES_H
#define TYPES_H

#include <stdbool.h>

#define MAX_TABLE_NAME 64
#define MAX_FIELD_NAME 64
#define MAX_INDEX_NAME 64

// Error codes (re-exported for convenience)
typedef enum {
    SUCCESS = 0,
    ERROR_NOT_FOUND = -1,
    ERROR_DUPLICATE_KEY = -2,
    ERROR_MEMORY_ALLOCATION = -3,
    ERROR_INVALID_PARAMETER = -4,
    ERROR_TABLE_FULL = -5,
    ERROR_TRANSACTION_CONFLICT = -6,
    ERROR_TYPE_MISMATCH = -7,
    ERROR_CONSTRAINT_VIOLATION = -8,
    ERROR_INDEX_EXISTS = -9,
    ERROR_NOT_IMPLEMENTED = -10
} ErrorCode;

// Field types (re-exported for convenience)
typedef enum {
    TYPE_UNKNOWN = 0,
    TYPE_INT,
    TYPE_STRING,
    TYPE_FLOAT,
    TYPE_DOUBLE,
    TYPE_BOOL,
    TYPE_DATETIME,
    TYPE_BLOB,
    TYPE_NULL
} FieldType;

// Index types (re-exported for convenience)
typedef enum {
    INDEX_HASH,
    INDEX_BTREE,
    INDEX_SKIPLIST,
    INDEX_BITMAP,
    INDEX_FULLTEXT
} IndexType;

#endif // TYPES_H