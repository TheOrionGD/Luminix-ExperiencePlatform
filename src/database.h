#ifndef DATABASE_H
#define DATABASE_H
#define NOT_FOUND ERROR_NOT_FOUND 
#include <stdbool.h>
#include <time.h>

// Constants
#define MAX_TABLE_NAME 64
#define MAX_FIELD_LEN 256
#define MAX_FIELDS_PER_TABLE 32
#define MAX_TABLES 100
#define MAX_RECORDS_PER_TABLE 100000
#define MAX_INDEXES_PER_TABLE 10
#define INITIAL_CAPACITY 100
#define BATCH_INSERT_SIZE 1000
#define QUERY_CACHE_SIZE 100
#define MAX_TRANSACTION_LEVEL 10
#define DEFAULT_BTREE_DEGREE 3
#define DEFAULT_SKIPLIST_MAX_LEVEL 16

// Field types
typedef enum {
    TYPE_INT,
    TYPE_STRING,
    TYPE_FLOAT,
    TYPE_DOUBLE,
    TYPE_BOOL,
    TYPE_DATETIME,
    TYPE_BLOB,
    TYPE_NULL,
    TYPE_UNKNOWN
} FieldType;

// Index types
typedef enum {
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

// Error codes
typedef enum {
    SUCCESS = 0,
    ERROR_INVALID_INPUT,
    ERROR_MEMORY_ALLOCATION,
    ERROR_NOT_FOUND,
    ERROR_DUPLICATE_KEY,
    ERROR_TABLE_FULL,
    ERROR_INDEX_EXISTS,
    ERROR_TYPE_MISMATCH,
    ERROR_CONSTRAINT_VIOLATION,
    ERROR_TRANSACTION_CONFLICT
} ErrorCode;

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

// Index structure (forward declaration)
typedef struct Index Index;

// Index manager
typedef struct {
    Index** indices;
    int index_count;
    int capacity;
} IndexManager;

// Table statistics
typedef struct TableStatistics TableStatistics;

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
} DatabaseStatistics;

// Cache structure (forward declaration)
typedef struct Cache Cache;

// Query result (forward declaration)
typedef struct QueryResult QueryResult;

// Parsed query (forward declaration)
typedef struct ParsedQuery ParsedQuery;

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

// Function prototypes
Database* db_create();
void db_free(Database* db);
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
QueryResult* db_execute_query(Database* db, const char* query);
ErrorCode db_create_index(Database* db, const char* table_name, 
                         const char* field_name, IndexType type);
ErrorCode db_drop_index(Database* db, const char* table_name, const char* field_name);
ErrorCode db_begin_transaction(Database* db);
ErrorCode db_commit_transaction(Database* db);
ErrorCode db_rollback_transaction(Database* db);
DatabaseStatistics* db_get_statistics(Database* db);
TableStatistics* db_get_table_statistics(Database* db, const char* table_name);
ErrorCode db_reset_statistics(Database* db);
ErrorCode db_vacuum(Database* db);
ErrorCode db_optimize(Database* db);
int db_get_table_count(Database* db);
int db_get_record_count(Database* db, const char* table_name);
char** db_list_tables(Database* db, int* out_count);
void db_print_schema(Database* db);
void db_print_statistics(Database* db);
const char* field_type_to_string(FieldType type);

// Internal functions (for table operations)
ErrorCode table_insert_record(Table* table, Field* values, int* out_id);
Record* table_find_record(Table* table, int id);
ErrorCode table_update_record(Table* table, int id, Field* new_values);
ErrorCode table_delete_record(Table* table, int id);
void table_free(Table* table);

#endif // DATABASE_H