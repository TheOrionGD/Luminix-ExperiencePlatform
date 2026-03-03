// database.h
#ifndef DATABASE_H
#define DATABASE_H

#include "types.h"
#include "config.h"
#include <stdbool.h>
#include <time.h>
#include <stddef.h>

// Constants
#define MAX_FIELD_LEN 256
#define MAX_TABLES 100
#define MAX_FIELDS_PER_TABLE 50
#define MAX_RECORDS_PER_TABLE 1000000
#define MAX_INDEXES_PER_TABLE 10
#define INITIAL_CAPACITY 100
#define QUERY_CACHE_SIZE 100
#define MAX_TRANSACTION_LEVEL 10
#define DEFAULT_BTREE_DEGREE 3
#define DEFAULT_SKIPLIST_MAX_LEVEL 16
#define BATCH_INSERT_SIZE 1000
#define MAX_FIELD_NAME 64
#define MAX_TABLE_NAME 64
#define MAX_INDEX_NAME 64

// Cache policies
#define CACHE_LRU 0
#define CACHE_FIFO 1
#define CACHE_LFU 2

// Statistics constants
typedef enum {
    STAT_TABLES_CREATED,
    STAT_RECORDS_INSERTED,
    STAT_RECORDS_UPDATED,
    STAT_RECORDS_DELETED,
    STAT_QUERIES_EXECUTED,
    STAT_CACHE_HITS,
    STAT_CACHE_MISSES,
    STAT_TRANSACTIONS_STARTED,
    STAT_TRANSACTIONS_COMMITTED,
    STAT_TRANSACTIONS_ROLLED_BACK,
    STAT_INDEXES_CREATED,
    STAT_INDEXES_DROPPED,
    STAT_VACUUM_OPERATIONS,
    STAT_OPTIMIZATION_OPERATIONS
} StatType;

// Error codes
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

// Field types
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

// Index types
typedef enum {
    INDEX_HASH,
    INDEX_BTREE,
    INDEX_SKIPLIST,
    INDEX_BITMAP,
    INDEX_FULLTEXT
} IndexType;

// Query types
typedef enum {
    QUERY_SELECT,
    QUERY_INSERT,
    QUERY_UPDATE,
    QUERY_DELETE,
    QUERY_CREATE_TABLE,
    QUERY_DROP_TABLE,
    QUERY_CREATE_INDEX,
    QUERY_DROP_INDEX
} QueryType;

// Forward declarations
typedef struct Index Index;
typedef struct IndexManager IndexManager;
typedef struct TableStatistics TableStatistics;
typedef struct Cache Cache;
typedef struct QueryResult QueryResult;
typedef struct ParsedQuery ParsedQuery;

// Field value union
typedef union {
    int int_value;
    char string_value[MAX_FIELD_LEN];
    float float_value;
    double double_value;
    bool bool_value;
} FieldValue;

// Field structure
typedef struct {
    char name[MAX_FIELD_LEN];
    FieldType type;
    FieldValue value;
} Field;

// Constraint structure
typedef struct {
    bool nullable;
    bool unique;
    bool primary_key;
    bool foreign_key;
    char references_table[MAX_TABLE_NAME];
    char references_field[MAX_FIELD_LEN];
} Constraint;

// Record structure
typedef struct Record {
    int id;
    Field* fields;
    int field_count;
    time_t timestamp;
    int version;
    struct Record* next;
} Record;

// Index structure
struct Index {
    char table_name[MAX_TABLE_NAME];
    char field_name[MAX_FIELD_LEN];
    IndexType type;
    void* data;  // Implementation-specific data
};

// IndexManager structure
struct IndexManager {
    Index** indices;
    int index_count;
    int capacity;
};

// Table statistics structure
struct TableStatistics {
    long long records_inserted;
    long long records_updated;
    long long records_deleted;
    long long index_scans;
    long long sequential_scans;
};

// Table structure
typedef struct {
    char name[MAX_TABLE_NAME];
    char** field_names;
    FieldType* field_types;
    Constraint* constraints;
    int field_count;
    Record* records;
    int record_count;
    int capacity;
    int next_record_id;
    IndexManager* index_manager;
    TableStatistics* statistics;
} Table;

// Database statistics
typedef struct {
    long long tables_created;
    long long records_inserted;
    long long records_updated;
    long long records_deleted;
    long long queries_executed;
    long long cache_hits;
    long long cache_misses;
    long long transactions_started;
    long long transactions_committed;
    long long transactions_rolled_back;
    long long indexes_created;
    long long indexes_dropped;
    long long vacuum_operations;
    long long optimization_operations;
    double total_query_time;
    size_t memory_used;
    size_t peak_memory_used;
    size_t cache_memory_used;
    int deadlocks_detected;
    int backup_operations;
} DatabaseStatistics;

// Cache structure
struct Cache {
    int policy;
    int size;
    void* data;
};

// ParsedQuery structure
struct ParsedQuery {
    QueryType type;
    char* table_name;
    char* field_name;
    char* condition;
    int int_value;
    char* string_value;
};

// QueryResult structure
struct QueryResult {
    int row_count;
    int column_count;
    char** column_names;
    FieldType* column_types;
    Field** rows;
};

// Database structure
typedef struct {
    Table* tables;
    int table_count;
    int table_capacity;
    IndexManager* index_manager;
    Cache* cache;
    DatabaseStatistics* statistics;
    int transaction_active;
    int transaction_level;
    time_t transaction_start_time;
} Database;

// ====================== Function Prototypes ====================== //

// Database management
Database* db_create(void);
Database* db_open(const char* path, const char* mode);
ErrorCode db_close(Database* db);
void db_free(Database* db);
DatabaseStatistics* db_get_statistics(Database* db);
TableStatistics* db_get_table_statistics(Database* db, const char* table_name);
int db_get_table_count(Database* db);
int db_get_record_count(Database* db, const char* table_name);
char** db_list_tables(Database* db, int* out_count);
void db_print_schema(Database* db);
void db_print_statistics(Database* db);

// Table operations
ErrorCode db_add_table(Database* db, const char* name, const char** field_names, 
                      FieldType* types, int field_count, Table** out_table);
Table* db_get_table(Database* db, const char* table_name);
ErrorCode db_insert_record(Database* db, const char* table_name, Field* values, int* out_id);
ErrorCode db_batch_insert_records(Database* db, const char* table_name, 
                                  Field** values_array, int count, int** out_ids);
Record* db_find_record(Database* db, const char* table_name, int id);
Record* db_find_record_by_field(Database* db, const char* table_name, 
                                const char* field_name, const void* value);
Record** db_find_all_records_by_field(Database* db, const char* table_name,
                                      const char* field_name, const void* value,
                                      int* out_count);
ErrorCode db_update_record(Database* db, const char* table_name, int id, Field* new_values);
ErrorCode db_delete_record(Database* db, const char* table_name, int id);
ErrorCode db_delete_records_by_field(Database* db, const char* table_name,
                                     const char* field_name, const void* value,
                                     int* out_count);

// Index operations
ErrorCode db_create_index(Database* db, const char* table_name, const char* field_name, IndexType type);
ErrorCode db_drop_index(Database* db, const char* table_name, const char* field_name);
Index* index_manager_get_index(IndexManager* manager, const char* field_name);

// Transaction management
ErrorCode db_begin_transaction(Database* db);
ErrorCode db_commit_transaction(Database* db);
ErrorCode db_rollback_transaction(Database* db);

// Maintenance
ErrorCode db_vacuum(Database* db);
ErrorCode db_optimize(Database* db);
ErrorCode db_reset_statistics(Database* db);

// Query execution
QueryResult* db_execute_query(Database* db, const char* query);

// Utility
const char* field_type_to_string(FieldType type);

// Internal table functions
ErrorCode table_insert_record(Table* table, Field* values, int* out_id);
Record* table_find_record(Table* table, int id);
ErrorCode table_update_record(Table* table, int id, Field* new_values);
ErrorCode table_delete_record(Table* table, int id);
void table_free(Table* table);

#endif // DATABASE_H