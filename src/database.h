// database.h
#ifndef DATABASE_H
#define DATABASE_H

#include "types.h"
#include "config.h"
#include <stdbool.h>
#include <time.h>
#include <stddef.h>

// Constants and policies are now in config.h and types.h

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

// Types are defined in types.h

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
    char name[MAX_INDEX_NAME];
    char table_name[MAX_TABLE_NAME];
    char field_name[MAX_FIELD_LEN];
    IndexType type;
    void* data;  // Implementation-specific data
    size_t size;
};

// IndexManager defined in index.h

// Table statistics structure
struct TableStatistics {
    long long records_inserted;
    long long records_updated;
    long long records_deleted;
    long long index_scans;
    long long sequential_scans;
    size_t total_size_bytes;
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
char* get_database_name(Database* db);
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
FieldType string_to_field_type(const char* str);

// Internal table functions
ErrorCode table_insert_record(Table* table, Field* values, int* out_id);
Record* table_find_record(Table* table, int id);
ErrorCode table_update_record(Table* table, int id, Field* new_values);
ErrorCode table_delete_record(Table* table, int id);
void table_free(Table* table);


ErrorCode db_explain_query(Database* db, const char* query, char** explanation);
ErrorCode db_analyze(Database* db);
ErrorCode db_vacuum_table(Database* db, const char* table_name);
ErrorCode db_analyze_table(Database* db, const char* table_name);
ErrorCode db_check_integrity(Database* db, bool repair);
ErrorCode db_repair_table(Database* db, const char* table_name);
ErrorCode db_list_config(Database* db, char*** configs, int* count);
ErrorCode db_set_config(Database* db, const char* key, const char* value);
ErrorCode db_export_schema(Database* db, const char* file_path);
ErrorCode db_import_schema(Database* db, const char* file_path);
ErrorCode db_drop_table(Database* db, const char* table_name);
ErrorCode db_import_table(Database* db, const char* table_name, const char* format, const char* file_path);
ErrorCode db_export_table(Database* db, const char* table_name, const char* format, const char* file_path);
ErrorCode db_rebuild_indexes(Database* db);
ErrorCode db_alter_table_add_column(Database* db, const char* table_name, const char* column_name, FieldType type, void* constraint);
ErrorCode db_alter_table_drop_column(Database* db, const char* table_name, const char* column_name);
ErrorCode db_alter_table_rename_column(Database* db, const char* table_name, const char* old_name, const char* new_name);
ErrorCode db_truncate_table(Database* db, const char* table_name);

#endif // DATABASE_H