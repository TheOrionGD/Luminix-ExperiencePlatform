#ifndef DATABASE_H
#define DATABASE_H

#include "config.h"
#include "index.h"
#include <stdbool.h>
#include <time.h>

// ==================== FORWARD DECLARATIONS ====================
typedef struct Database Database;
typedef struct Table Table;
typedef struct Record Record;
typedef struct Field Field;
typedef struct QueryResult QueryResult;
typedef struct Cache Cache;
typedef struct Statistics Statistics;
typedef struct TableStatistics TableStatistics;

// ==================== FIELD STRUCTURE ====================
typedef struct Field {
    char name[MAX_FIELD_LEN];
    FieldType type;
    union {
        int int_value;
        float float_value;
        double double_value;
        char string_value[MAX_FIELD_LEN];
        bool bool_value;
        time_t datetime_value;
        void* blob_value;
        size_t blob_size;
    } value;
} Field;

// ==================== CONSTRAINT STRUCTURE ====================
typedef struct Constraint {
    bool primary_key;
    bool foreign_key;
    bool unique;
    bool nullable;
    bool auto_increment;
    char default_value[MAX_FIELD_LEN];
    char check_expression[MAX_QUERY_LEN];
    char references_table[MAX_TABLE_NAME];
    char references_field[MAX_FIELD_LEN];
} Constraint;

// ==================== RECORD STRUCTURE ====================
typedef struct Record {
    int id;
    Field* fields;
    int field_count;
    struct Record* next;
    struct Record* prev;           // For doubly-linked list
    time_t timestamp;              // Last modification time
    unsigned int version;          // Optimistic locking version
    void* transaction_data;        // For MVCC (Multi-Version Concurrency Control)
} Record;

// ==================== TABLE STATISTICS ====================
typedef struct TableStatistics {
    long long records_inserted;
    long long records_updated;
    long long records_deleted;
    long long index_scans;
    long long sequential_scans;
    long long query_execution_time;
    size_t total_size_bytes;
    time_t last_vacuum_time;
    time_t last_analyze_time;
    int avg_record_size;
    int distinct_values[MAX_FIELDS_PER_TABLE];
} TableStatistics;

// ==================== TABLE STRUCTURE ====================
typedef struct Table {
    char name[MAX_TABLE_NAME];
    Record* records;                // Head of linked list
    Record* tail;                   // Tail of linked list (for fast append)
    int record_count;
    int capacity;
    int next_record_id;
    
    // Schema information
    char** field_names;
    FieldType* field_types;
    Constraint* constraints;
    int field_count;
    
    // Indexing
    IndexManager* index_manager;
    
    // Statistics
    TableStatistics* statistics;
    
    // Locking
    pthread_rwlock_t lock;          // Reader-writer lock for concurrency
    
    // Partitioning (future feature)
    int partition_key_field;
    int partition_count;
    Table** partitions;
    
    // Triggers (future feature)
    void** triggers;
    int trigger_count;
} Table;

// ==================== QUERY RESULT STRUCTURE ====================
typedef struct QueryResult {
    char** column_names;
    FieldType* column_types;
    Field** rows;
    int row_count;
    int column_count;
    time_t execution_time;
    size_t memory_used;
    bool cached;
    struct QueryResult* next;       // For linked list of results
} QueryResult;

// ==================== CACHE STRUCTURE ====================
typedef struct CacheEntry {
    char* key;
    QueryResult* value;
    time_t timestamp;
    size_t size;
    unsigned int access_count;
    struct CacheEntry* next;
    struct CacheEntry* prev;
} CacheEntry;

typedef struct Cache {
    CacheEntry** table;             // Hash table for O(1) lookups
    CacheEntry* head;               // LRU head (most recently used)
    CacheEntry* tail;               // LRU tail (least recently used)
    size_t capacity;
    size_t size;
    int max_entries;
    CachePolicy policy;
    pthread_mutex_t lock;
} Cache;

// ==================== STATISTICS STRUCTURE ====================
typedef struct Statistics {
    // Database operations
    long long tables_created;
    long long tables_dropped;
    long long records_inserted;
    long long records_updated;
    long long records_deleted;
    long long queries_executed;
    
    // Index operations
    long long indexes_created;
    long long indexes_dropped;
    long long index_scans;
    long long sequential_scans;
    
    // Cache statistics
    long long cache_hits;
    long long cache_misses;
    long long cache_evictions;
    
    // Transaction statistics
    long long transactions_started;
    long long transactions_committed;
    long long transactions_rolled_back;
    long long deadlocks_detected;
    
    // Performance metrics
    long long total_query_time;
    long long avg_query_time;
    long long max_query_time;
    long long min_query_time;
    
    // Memory usage
    size_t memory_used;
    size_t peak_memory_used;
    size_t cache_memory_used;
    
    // Time tracking
    time_t start_time;
    time_t last_reset_time;
    
    // Connection statistics
    int active_connections;
    int total_connections;
    int failed_connections;
    
    // Maintenance operations
    long long vacuum_operations;
    long long backup_operations;
    long long optimization_operations;
} Statistics;

// ==================== TRANSACTION STRUCTURE ====================
typedef struct Transaction {
    int id;
    IsolationLevel isolation_level;
    time_t start_time;
    time_t timeout;
    Database* db;
    void* savepoint;                // For rollback
    Record** modified_records;      // For MVCC
    int modified_count;
    bool read_only;
    bool committed;
    struct Transaction* next;
} Transaction;

// ==================== CONNECTION STRUCTURE ====================
typedef struct Connection {
    int id;
    Database* db;
    Transaction* transaction;
    time_t connect_time;
    time_t last_activity;
    char client_address[64];
    char username[32];
    bool authenticated;
    int query_count;
    struct Connection* next;
} Connection;

// ==================== DATABASE STRUCTURE ====================
typedef struct Database {
    // Core data
    Table* tables;
    int table_count;
    int table_capacity;
    
    // Indexing
    IndexManager* index_manager;    // Global index manager
    
    // Caching
    Cache* cache;                   // Query result cache
    
    // Statistics
    Statistics* statistics;         // Performance statistics
    
    // Transactions
    Transaction* active_transactions;
    int transaction_active;
    int transaction_level;
    time_t transaction_start_time;
    
    // Connections
    Connection* connections;
    int connection_count;
    
    // Configuration
    char name[MAX_TABLE_NAME];
    char path[MAX_FIELD_LEN * 2];
    StorageType storage_type;
    CompressionType compression;
    EncryptionType encryption;
    bool auto_vacuum;
    bool auto_optimize;
    
    // Locks for concurrency
    pthread_rwlock_t schema_lock;   // For schema modifications
    pthread_mutex_t transaction_lock;
    pthread_mutex_t connection_lock;
    
    // WAL (Write-Ahead Logging) for durability
    FILE* wal_file;
    char wal_path[MAX_FIELD_LEN * 2];
    size_t wal_size;
    
    // Replication (future feature)
    bool is_replica;
    char master_host[64];
    int master_port;
    
    // Backup information
    time_t last_backup_time;
    char backup_path[MAX_FIELD_LEN * 2];
} Database;

// ==================== QUERY PARSING ====================
typedef enum {
    QUERY_SELECT,
    QUERY_INSERT,
    QUERY_UPDATE,
    QUERY_DELETE,
    QUERY_CREATE_TABLE,
    QUERY_DROP_TABLE,
    QUERY_CREATE_INDEX,
    QUERY_DROP_INDEX,
    QUERY_BEGIN_TRANSACTION,
    QUERY_COMMIT,
    QUERY_ROLLBACK,
    QUERY_EXPLAIN,
    QUERY_VACUUM,
    QUERY_OPTIMIZE
} QueryType;

typedef struct ParsedQuery {
    QueryType type;
    char table_name[MAX_TABLE_NAME];
    char** fields;
    int field_count;
    Field** values;
    int value_count;
    char* where_condition;
    char* order_by;
    char* group_by;
    int limit;
    int offset;
    bool distinct;
    void* ast;  // Abstract Syntax Tree for complex queries
} ParsedQuery;

// ==================== DATABASE CORE API ====================

// Database lifecycle
Database* db_create(const char* name);
Database* db_open(const char* path, const char* name);
ErrorCode db_close(Database* db);
ErrorCode db_delete(Database* db);
ErrorCode db_backup(Database* db, const char* backup_path);
ErrorCode db_restore(Database* db, const char* backup_path);

// Table operations
ErrorCode db_add_table(Database* db, const char* name, 
                      const char** field_names, FieldType* types, 
                      int field_count, Table** out_table);
ErrorCode db_drop_table(Database* db, const char* table_name);
ErrorCode db_rename_table(Database* db, const char* old_name, const char* new_name);
ErrorCode db_truncate_table(Database* db, const char* table_name);
Table* db_get_table(Database* db, const char* table_name);
char** db_list_tables(Database* db, int* out_count);
ErrorCode db_alter_table_add_column(Database* db, const char* table_name,
                                   const char* column_name, FieldType type,
                                   Constraint* constraint);
ErrorCode db_alter_table_drop_column(Database* db, const char* table_name,
                                    const char* column_name);
ErrorCode db_alter_table_rename_column(Database* db, const char* table_name,
                                      const char* old_name, const char* new_name);

// Record operations
ErrorCode db_insert_record(Database* db, const char* table_name, 
                          Field* values, int* out_id);
ErrorCode db_batch_insert_records(Database* db, const char* table_name,
                                 Field** values_array, int count, int** out_ids);
Record* db_find_record(Database* db, const char* table_name, int id);
Record* db_find_record_by_field(Database* db, const char* table_name,
                               const char* field_name, const void* value);
Record** db_find_all_records_by_field(Database* db, const char* table_name,
                                     const char* field_name, const void* value,
                                     int* out_count);
ErrorCode db_update_record(Database* db, const char* table_name, 
                          int id, Field* new_values);
ErrorCode db_delete_record(Database* db, const char* table_name, int id);
ErrorCode db_delete_records_by_field(Database* db, const char* table_name,
                                    const char* field_name, const void* value,
                                    int* out_count);

// Query execution
QueryResult* db_execute_query(Database* db, const char* query);
QueryResult* db_execute_prepared_query(Database* db, const char* query_template,
                                      Field** params, int param_count);
ErrorCode db_explain_query(Database* db, const char* query, char** explanation);

// Index operations
ErrorCode db_create_index(Database* db, const char* table_name,
                         const char* field_name, IndexType type);
ErrorCode db_drop_index(Database* db, const char* table_name, const char* field_name);
ErrorCode db_rebuild_indexes(Database* db);
ErrorCode db_reindex_table(Database* db, const char* table_name);
IndexManager* db_get_index_manager(Database* db);

// Transaction management
ErrorCode db_begin_transaction(Database* db);
ErrorCode db_begin_transaction_ex(Database* db, IsolationLevel level);
ErrorCode db_commit_transaction(Database* db);
ErrorCode db_rollback_transaction(Database* db);
ErrorCode db_set_savepoint(Database* db, const char* savepoint_name);
ErrorCode db_rollback_to_savepoint(Database* db, const char* savepoint_name);
ErrorCode db_release_savepoint(Database* db, const char* savepoint_name);

// Connection management
Connection* db_create_connection(Database* db, const char* client_address);
ErrorCode db_close_connection(Connection* conn);
ErrorCode db_authenticate_connection(Connection* conn, const char* username,
                                    const char* password);
ErrorCode db_set_connection_timeout(Connection* conn, int seconds);

// Statistics and monitoring
Statistics* db_get_statistics(Database* db);
TableStatistics* db_get_table_statistics(Database* db, const char* table_name);
ErrorCode db_reset_statistics(Database* db);
ErrorCode db_get_performance_metrics(Database* db, char*** metrics, int* count);
ErrorCode db_get_locks_status(Database* db, char*** locks, int* count);

// Maintenance operations
ErrorCode db_vacuum(Database* db);
ErrorCode db_vacuum_table(Database* db, const char* table_name);
ErrorCode db_optimize(Database* db);
ErrorCode db_analyze(Database* db);
ErrorCode db_analyze_table(Database* db, const char* table_name);
ErrorCode db_check_integrity(Database* db, bool repair);
ErrorCode db_repair_table(Database* db, const char* table_name);

// Cache management
ErrorCode db_clear_cache(Database* db);
ErrorCode db_set_cache_size(Database* db, size_t size);
ErrorCode db_set_cache_policy(Database* db, CachePolicy policy);
Cache* db_get_cache(Database* db);

// Configuration
ErrorCode db_set_config(Database* db, const char* key, const char* value);
ErrorCode db_get_config(Database* db, const char* key, char* value, size_t size);
ErrorCode db_list_config(Database* db, char*** configs, int* count);

// Backup and recovery
ErrorCode db_create_checkpoint(Database* db);
ErrorCode db_enable_wal(Database* db, bool enable);
ErrorCode db_replay_wal(Database* db, const char* wal_path);

// Security
ErrorCode db_create_user(Database* db, const char* username, const char* password,
                        const char** permissions, int permission_count);
ErrorCode db_drop_user(Database* db, const char* username);
ErrorCode db_grant_permission(Database* db, const char* username,
                             const char* permission, const char* object);
ErrorCode db_revoke_permission(Database* db, const char* username,
                              const char* permission, const char* object);

// Utility functions
int db_get_table_count(Database* db);
int db_get_record_count(Database* db, const char* table_name);
size_t db_get_database_size(Database* db);
size_t db_get_table_size(Database* db, const char* table_name);
ErrorCode db_export_table(Database* db, const char* table_name,
                         const char* format, const char* file_path);
ErrorCode db_import_table(Database* db, const char* table_name,
                         const char* format, const char* file_path);
ErrorCode db_export_schema(Database* db, const char* file_path);
ErrorCode db_import_schema(Database* db, const char* file_path);

// Diagnostic functions
void db_print_schema(Database* db);
void db_print_statistics(Database* db);
void db_print_cache_info(Database* db);
void db_print_connection_info(Database* db);
ErrorCode db_generate_report(Database* db, const char* report_type,
                            const char* file_path);

// Internal functions (for module use)
ErrorCode table_insert_record(Table* table, Field* values, int* out_id);
Record* table_find_record(Table* table, int id);
ErrorCode table_update_record(Table* table, int id, Field* new_values);
ErrorCode table_delete_record(Table* table, int id);
void table_free(Table* table);

// Query parsing (internal)
ParsedQuery* parse_query(const char* query);
void free_parsed_query(ParsedQuery* parsed);
QueryResult* execute_select_query(Database* db, ParsedQuery* parsed);
QueryResult* execute_insert_query(Database* db, ParsedQuery* parsed);
QueryResult* execute_update_query(Database* db, ParsedQuery* parsed);
QueryResult* execute_delete_query(Database* db, ParsedQuery* parsed);
QueryResult* execute_create_table_query(Database* db, ParsedQuery* parsed);
QueryResult* execute_create_index_query(Database* db, ParsedQuery* parsed);

// Cache functions (internal)
Cache* create_cache(CachePolicy policy, size_t capacity);
void cache_free(Cache* cache);
QueryResult* cache_get(Cache* cache, const char* key);
ErrorCode cache_put(Cache* cache, const char* key, QueryResult* value);
ErrorCode cache_remove(Cache* cache, const char* key);
ErrorCode cache_clear(Cache* cache);
size_t cache_size(Cache* cache);

// Statistics functions (internal)
Statistics* create_statistics();
void free_statistics(Statistics* stats);
void update_statistics(Statistics* stats, int metric_type, long long value);
void reset_statistics(Statistics* stats);

TableStatistics* create_table_statistics();
void free_table_statistics(TableStatistics* stats);
void update_table_statistics(TableStatistics* stats, int metric_type, long long value);
void reset_table_statistics(TableStatistics* stats);

// Transaction functions (internal)
Transaction* create_transaction(Database* db, IsolationLevel level);
void free_transaction(Transaction* trans);
ErrorCode transaction_add_modified_record(Transaction* trans, Record* record);
ErrorCode transaction_commit_internal(Transaction* trans);
ErrorCode transaction_rollback_internal(Transaction* trans);

// Helper functions
const char* field_type_to_string(FieldType type);
FieldType string_to_field_type(const char* str);
const char* index_type_to_string(IndexType type);
const char* error_code_to_string(ErrorCode code);
Field create_field_from_string(const char* name, FieldType type, const char* value);
char* field_to_string(const Field* field);
bool compare_fields(const Field* a, const Field* b, ComparisonOperator op);
ErrorCode validate_constraints(Table* table, Field* values);
ErrorCode check_foreign_key_constraint(Database* db, Table* table, Field* values);

// Memory management
void* db_malloc(size_t size);
void* db_calloc(size_t count, size_t size);
void* db_realloc(void* ptr, size_t size);
void db_free(void* ptr);

// Thread safety
ErrorCode db_lock_table_read(Database* db, const char* table_name);
ErrorCode db_lock_table_write(Database* db, const char* table_name);
ErrorCode db_unlock_table(Database* db, const char* table_name);
ErrorCode db_lock_database_read(Database* db);
ErrorCode db_lock_database_write(Database* db);
ErrorCode db_unlock_database(Database* db);

// Time utilities
time_t db_current_timestamp();
char* db_timestamp_to_string(time_t timestamp);
time_t db_string_to_timestamp(const char* str);

// String utilities
char* db_strdup(const char* str);
char* db_strndup(const char* str, size_t n);
char** db_strsplit(const char* str, const char* delim, int* count);
char* db_strjoin(const char** strings, int count, const char* delim);
bool db_strstartswith(const char* str, const char* prefix);
bool db_strendswith(const char* str, const char* suffix);
char* db_strtrim(char* str);
char* db_strupper(char* str);
char* db_strlower(char* str);

// Math utilities
int db_random_int(int min, int max);
double db_random_double(double min, double max);
bool db_double_equals(double a, double b, double epsilon);
int db_round(double value);
int db_ceil(double value);
int db_floor(double value);

// Hash functions
unsigned int db_hash_string(const char* str);
unsigned int db_hash_int(int value);
unsigned int db_hash_double(double value);
unsigned int db_hash_field(const Field* field);

// Serialization
char* db_serialize_record(Record* record, Table* table);
Record* db_deserialize_record(const char* data, Table* table);
char* db_serialize_table(Table* table);
Table* db_deserialize_table(const char* data);
char* db_serialize_database(Database* db);
Database* db_deserialize_database(const char* data);

// Compression
ErrorCode db_compress_data(const void* data, size_t size, 
                          void** compressed, size_t* compressed_size,
                          CompressionType type);
ErrorCode db_decompress_data(const void* compressed, size_t compressed_size,
                            void** data, size_t* data_size,
                            CompressionType type);

// Encryption
ErrorCode db_encrypt_data(const void* data, size_t size,
                         void** encrypted, size_t* encrypted_size,
                         EncryptionType type, const char* key);
ErrorCode db_decrypt_data(const void* encrypted, size_t encrypted_size,
                         void** data, size_t* data_size,
                         EncryptionType type, const char* key);

// Network utilities (for client-server mode)
ErrorCode db_start_server(Database* db, int port, int max_connections);
ErrorCode db_stop_server(Database* db);
ErrorCode db_connect_to_server(const char* host, int port, Database** db);
ErrorCode db_disconnect_from_server(Database* db);

// Replication utilities
ErrorCode db_setup_replication(Database* db, const char* master_host, 
                              int master_port, bool as_replica);
ErrorCode db_start_replication(Database* db);
ErrorCode db_stop_replication(Database* db);
ErrorCode db_get_replication_status(Database* db, char** status);

// Plugin system (future extension)
typedef void* (*PluginInitFunction)(Database* db, const char* config);
typedef void (*PluginCleanupFunction)(void* plugin_data);
typedef ErrorCode (*PluginHookFunction)(void* plugin_data, const char* event,
                                       void* data);

typedef struct Plugin {
    char name[64];
    char version[32];
    PluginInitFunction init;
    PluginCleanupFunction cleanup;
    PluginHookFunction* hooks;
    int hook_count;
    void* data;
    struct Plugin* next;
} Plugin;

ErrorCode db_register_plugin(Database* db, Plugin* plugin);
ErrorCode db_unregister_plugin(Database* db, const char* plugin_name);
ErrorCode db_call_plugin_hook(Database* db, const char* event, void* data);

// Macros for common operations
#define DB_FOREACH_TABLE(db, table_var) \
    for (int __i = 0; __i < (db)->table_count && ((table_var) = &(db)->tables[__i]); __i++)

#define DB_FOREACH_RECORD(table, record_var) \
    for ((record_var) = (table)->records; (record_var) != NULL; (record_var) = (record_var)->next)

#define DB_FOREACH_FIELD(record, field_var, idx) \
    for ((idx) = 0; (idx) < (record)->field_count && ((field_var) = &(record)->fields[idx]); (idx)++)

#define DB_SAFE_FREE(ptr) \
    do { \
        if (ptr) { \
            free(ptr); \
            (ptr) = NULL; \
        } \
    } while(0)

#define DB_RETURN_IF_ERROR(expr) \
    do { \
        ErrorCode __result = (expr); \
        if (__result != SUCCESS) return __result; \
    } while(0)

#define DB_LOG_IF_ERROR(expr, msg) \
    do { \
        ErrorCode __result = (expr); \
        if (__result != SUCCESS) { \
            LOG_ERROR("%s: %s", (msg), error_code_to_string(__result)); \
            return __result; \
        } \
    } while(0)

// Comparison operators for WHERE clauses
typedef enum {
    OP_EQUALS,
    OP_NOT_EQUALS,
    OP_LESS_THAN,
    OP_LESS_EQUALS,
    OP_GREATER_THAN,
    OP_GREATER_EQUALS,
    OP_LIKE,
    OP_IN,
    OP_BETWEEN,
    OP_IS_NULL,
    OP_IS_NOT_NULL
} ComparisonOperator;

// Aggregate functions
typedef enum {
    AGGR_SUM,
    AGGR_AVG,
    AGGR_MIN,
    AGGR_MAX,
    AGGR_COUNT,
    AGGR_COUNT_DISTINCT
} AggregateFunction;

// Join types
typedef enum {
    JOIN_INNER,
    JOIN_LEFT,
    JOIN_RIGHT,
    JOIN_FULL,
    JOIN_CROSS
} JoinType;

// Sort order
typedef enum {
    ORDER_ASC,
    ORDER_DESC
} SortOrder;

// Data integrity checks
typedef struct IntegrityCheck {
    bool foreign_key_checks;
    bool unique_checks;
    bool null_checks;
    bool type_checks;
    bool index_consistency;
    bool data_consistency;
} IntegrityCheck;

// Backup options
typedef struct BackupOptions {
    bool incremental;
    bool compress;
    bool encrypt;
    char* encryption_key;
    bool verify;
    int retention_days;
} BackupOptions;

// Configuration options
typedef struct DatabaseConfig {
    char name[MAX_TABLE_NAME];
    char path[MAX_FIELD_LEN * 2];
    size_t cache_size;
    CachePolicy cache_policy;
    bool auto_vacuum;
    int vacuum_threshold;
    bool auto_optimize;
    int optimize_interval;
    bool wal_enabled;
    size_t wal_size_limit;
    int checkpoint_interval;
    int max_connections;
    int connection_timeout;
    bool read_only;
    IsolationLevel default_isolation;
    CompressionType default_compression;
    EncryptionType default_encryption;
    char* encryption_key;
} DatabaseConfig;

// Function to create database with configuration
Database* db_create_ex(const DatabaseConfig* config);
ErrorCode db_reconfigure(Database* db, const DatabaseConfig* config);

// Bulk operations
typedef struct BulkOperation {
    char* table_name;
    Field** records;
    int record_count;
    bool transactional;
    bool ignore_errors;
    void* callback_data;
    void (*progress_callback)(void* data, int processed, int total);
} BulkOperation;

ErrorCode db_bulk_insert(Database* db, BulkOperation* op);
ErrorCode db_bulk_update(Database* db, BulkOperation* op);
ErrorCode db_bulk_delete(Database* db, BulkOperation* op);

// Event system
typedef enum {
    EVENT_TABLE_CREATED,
    EVENT_TABLE_DROPPED,
    EVENT_RECORD_INSERTED,
    EVENT_RECORD_UPDATED,
    EVENT_RECORD_DELETED,
    EVENT_INDEX_CREATED,
    EVENT_INDEX_DROPPED,
    EVENT_TRANSACTION_STARTED,
    EVENT_TRANSACTION_COMMITTED,
    EVENT_TRANSACTION_ROLLED_BACK,
    EVENT_CONNECTION_OPENED,
    EVENT_CONNECTION_CLOSED,
    EVENT_QUERY_EXECUTED,
    EVENT_ERROR_OCCURRED
} DatabaseEvent;

typedef struct EventData {
    DatabaseEvent type;
    Database* db;
    void* data;
    time_t timestamp;
} EventData;

typedef void (*EventHandler)(EventData* event);

ErrorCode db_register_event_handler(Database* db, DatabaseEvent event, EventHandler handler);
ErrorCode db_unregister_event_handler(Database* db, DatabaseEvent event, EventHandler handler);
ErrorCode db_trigger_event(Database* db, DatabaseEvent event, void* data);

// Version information
typedef struct VersionInfo {
    int major;
    int minor;
    int patch;
    char* build_date;
    char* git_hash;
    char* features;
} VersionInfo;

VersionInfo* db_get_version_info();
void db_free_version_info(VersionInfo* info);

// Diagnostic information
typedef struct DiagnosticInfo {
    Database* db;
    time_t collection_time;
    Statistics* stats;
    size_t memory_usage;
    int active_connections;
    int active_transactions;
    size_t cache_usage;
    char** slow_queries;
    int slow_query_count;
    char** deadlocks;
    int deadlock_count;
} DiagnosticInfo;

DiagnosticInfo* db_collect_diagnostic_info(Database* db);
void db_free_diagnostic_info(DiagnosticInfo* info);
ErrorCode db_write_diagnostic_report(Database* db, const char* file_path);

// Migration system
typedef struct Migration {
    int version;
    char* description;
    ErrorCode (*up)(Database* db);
    ErrorCode (*down)(Database* db);
    struct Migration* next;
} Migration;

ErrorCode db_register_migration(Migration* migration);
ErrorCode db_migrate(Database* db, int target_version);
ErrorCode db_migration_status(Database* db, int* current_version, int* target_version);

// Extension points for custom data types
typedef struct CustomType {
    char name[32];
    size_t size;
    ErrorCode (*serialize)(const void* data, char** output);
    ErrorCode (*deserialize)(const char* input, void** data);
    ErrorCode (*compare)(const void* a, const void* b, int* result);
    void (*free)(void* data);
} CustomType;

ErrorCode db_register_custom_type(Database* db, CustomType* type);
ErrorCode db_unregister_custom_type(Database* db, const char* type_name);

// Full-text search support (future feature)
typedef struct FullTextIndex {
    char table_name[MAX_TABLE_NAME];
    char field_name[MAX_FIELD_LEN];
    void* index_data;
    // ... full-text index implementation details
} FullTextIndex;

ErrorCode db_create_fulltext_index(Database* db, const char* table_name,
                                  const char* field_name);
ErrorCode db_search_fulltext(Database* db, const char* table_name,
                            const char* query, QueryResult** result);

// Spatial index support (future feature)
typedef struct SpatialIndex {
    char table_name[MAX_TABLE_NAME];
    char field_name[MAX_FIELD_LEN];
    void* index_data;
    // ... spatial index implementation details
} SpatialIndex;

ErrorCode db_create_spatial_index(Database* db, const char* table_name,
                                 const char* field_name);
ErrorCode db_search_spatial(Database* db, const char* table_name,
                           const void* bounds, QueryResult** result);

// JSON support (future feature)
typedef struct JsonPath {
    char* expression;
    // ... JSON path implementation details
} JsonPath;

ErrorCode db_extract_json(Database* db, const char* table_name,
                         const char* field_name, const char* json_path,
                         QueryResult** result);
ErrorCode db_update_json(Database* db, const char* table_name, int id,
                        const char* field_name, const char* json_path,
                        const char* new_value);

#endif // DATABASE_H