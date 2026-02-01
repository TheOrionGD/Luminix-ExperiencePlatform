#include "database.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>

// ==================== COMPLETE TYPE DEFINITIONS ====================

// Complete Index structure definition
struct Index {
    char table_name[MAX_TABLE_NAME];
    char field_name[MAX_FIELD_LEN];
    IndexType type;
    void* data;  // Implementation-specific data
};

// Complete TableStatistics structure
struct TableStatistics {
    long long records_inserted;
    long long records_updated;
    long long records_deleted;
    long long index_scans;
    long long sequential_scans;
};
// Add QueryType enum definition
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

// Complete Cache structure (simple stub)
struct Cache {
    int policy;
    int size;
    void* data;
};

// Complete ParsedQuery structure
struct ParsedQuery {
    QueryType type;
    char* table_name;
    char* field_name;
    char* condition;
    int int_value;
    char* string_value;
};

// Complete QueryResult structure
struct QueryResult {
    int row_count;
    int column_count;
    char** column_names;
    FieldType* column_types;
    Field** rows;
};

// ==================== INTERNAL HELPER FUNCTIONS ====================

// Generate unique ID for record
static int generate_record_id(Table* table) {
    static int global_counter = 1000;
    int max_id = 0;
    
    // Find maximum ID in table
    for (Record* curr = table->records; curr != NULL; curr = curr->next) {
        if (curr->id > max_id) {
            max_id = curr->id;
        }
    }
    
    // Use max + 1 or global counter
    return max_id + 1 > global_counter ? max_id + 1 : global_counter++;
}

// Validate field value against type
static ErrorCode validate_field_value(FieldType type, const char* value_str) {
    if (!value_str) return ERROR_INVALID_INPUT;
    
    switch (type) {
        case TYPE_INT: {
            char* endptr;
            strtol(value_str, &endptr, 10);
            if (*endptr != '\0' && *endptr != '\n') {
                return ERROR_TYPE_MISMATCH;
            }
            break;
        }
        case TYPE_FLOAT: {
            char* endptr;
            strtof(value_str, &endptr);
            if (*endptr != '\0' && *endptr != '\n') {
                return ERROR_TYPE_MISMATCH;
            }
            break;
        }
        case TYPE_DOUBLE: {
            char* endptr;
            strtod(value_str, &endptr);
            if (*endptr != '\0' && *endptr != '\n') {
                return ERROR_TYPE_MISMATCH;
            }
            break;
        }
        case TYPE_BOOL: {
            char* lower = strdup(value_str);
            if (!lower) return ERROR_MEMORY_ALLOCATION;
            
            for (int i = 0; lower[i]; i++) {
                lower[i] = tolower(lower[i]);
            }
            
            int valid = (strcmp(lower, "true") == 0 ||
                         strcmp(lower, "false") == 0 ||
                         strcmp(lower, "1") == 0 ||
                         strcmp(lower, "0") == 0 ||
                         strcmp(lower, "yes") == 0 ||
                         strcmp(lower, "no") == 0);
            free(lower);
            
            if (!valid) return ERROR_TYPE_MISMATCH;
            break;
        }
        case TYPE_STRING:
            // All strings are valid
            break;
        case TYPE_DATETIME:
            // Basic datetime validation (YYYY-MM-DD HH:MM:SS)
            if (strlen(value_str) < 19) return ERROR_TYPE_MISMATCH;
            break;
        case TYPE_BLOB:
            // Blobs are binary data, validation depends on encoding
            break;
        case TYPE_NULL:
            // Always valid
            break;
        default:
            return ERROR_TYPE_MISMATCH;
    }
    
    return SUCCESS;
}

// Convert string to field value
static ErrorCode string_to_field_value(Field* field, const char* value_str) {
    if (!field || !value_str) return ERROR_INVALID_INPUT;
    
    ErrorCode validation = validate_field_value(field->type, value_str);
    if (validation != SUCCESS) return validation;
    
    switch (field->type) {
        case TYPE_INT:
            field->value.int_value = atoi(value_str);
            break;
        case TYPE_STRING:
            strncpy(field->value.string_value, value_str, MAX_FIELD_LEN);
            field->value.string_value[MAX_FIELD_LEN - 1] = '\0';
            break;
        case TYPE_FLOAT:
            field->value.float_value = atof(value_str);
            break;
        case TYPE_DOUBLE:
            field->value.double_value = atof(value_str);
            break;
        case TYPE_BOOL:
            field->value.bool_value = (strcasecmp(value_str, "true") == 0 ||
                                       strcasecmp(value_str, "1") == 0 ||
                                       strcasecmp(value_str, "yes") == 0);
            break;
        case TYPE_DATETIME:
            strncpy(field->value.string_value, value_str, MAX_FIELD_LEN);
            field->value.string_value[MAX_FIELD_LEN - 1] = '\0';
            break;
        case TYPE_BLOB:
            // For simplicity, treat as string
            strncpy(field->value.string_value, value_str, MAX_FIELD_LEN);
            field->value.string_value[MAX_FIELD_LEN - 1] = '\0';
            break;
        case TYPE_NULL:
            // Set to default values
            field->value.int_value = 0;
            break;
    }
    
    return SUCCESS;
}

// Create a simple index manager
static IndexManager* create_simple_index_manager() {
    IndexManager* manager = (IndexManager*)malloc(sizeof(IndexManager));
    if (!manager) return NULL;
    
    manager->indices = NULL;
    manager->index_count = 0;
    manager->capacity = 0;
    
    return manager;
}

// Find index by table and field name
static Index* find_index_in_manager(IndexManager* manager, const char* table_name, const char* field_name) {
    if (!manager || !manager->indices) return NULL;
    
    for (int i = 0; i < manager->index_count; i++) {
        Index* idx = manager->indices[i];
        if (idx && strcmp(idx->table_name, table_name) == 0 && 
            strcmp(idx->field_name, field_name) == 0) {
            return idx;
        }
    }
    
    return NULL;
}

// Add index to manager
static ErrorCode add_index_to_manager(IndexManager* manager, Index* index) {
    if (!manager || !index) return ERROR_INVALID_INPUT;
    
    // Resize if needed
    if (manager->index_count >= manager->capacity) {
        int new_capacity = manager->capacity == 0 ? 4 : manager->capacity * 2;
        Index** new_indices = (Index**)realloc(manager->indices, new_capacity * sizeof(Index*));
        if (!new_indices) return ERROR_MEMORY_ALLOCATION;
        
        manager->indices = new_indices;
        manager->capacity = new_capacity;
    }
    
    manager->indices[manager->index_count++] = index;
    return SUCCESS;
}

// Remove index from manager
static ErrorCode remove_index_from_manager(IndexManager* manager, const char* table_name, const char* field_name) {
    if (!manager || !manager->indices) return ERROR_NOT_FOUND;
    
    for (int i = 0; i < manager->index_count; i++) {
        Index* idx = manager->indices[i];
        if (idx && strcmp(idx->table_name, table_name) == 0 && 
            strcmp(idx->field_name, field_name) == 0) {
            // Shift remaining indices
            for (int j = i; j < manager->index_count - 1; j++) {
                manager->indices[j] = manager->indices[j + 1];
            }
            manager->index_count--;
            return SUCCESS;
        }
    }
    
    return ERROR_NOT_FOUND;
}

// Free index manager
static void free_index_manager(IndexManager* manager) {
    if (!manager) return;
    
    if (manager->indices) {
        for (int i = 0; i < manager->index_count; i++) {
            if (manager->indices[i]) {
                free(manager->indices[i]->data);
                free(manager->indices[i]);
            }
        }
        free(manager->indices);
    }
    free(manager);
}

// Create a simple index
static Index* create_simple_index(const char* table_name, const char* field_name, IndexType type) {
    Index* index = (Index*)malloc(sizeof(Index));
    if (!index) return NULL;
    
    strncpy(index->table_name, table_name, MAX_TABLE_NAME);
    index->table_name[MAX_TABLE_NAME - 1] = '\0';
    
    strncpy(index->field_name, field_name, MAX_FIELD_LEN);
    index->field_name[MAX_FIELD_LEN - 1] = '\0';
    
    index->type = type;
    index->data = NULL;
    
    return index;
}

// Free index
static void free_index(Index* index) {
    if (index) {
        if (index->data) {
            free(index->data);
        }
        free(index);
    }
}

// Index operations (simple implementations)
static Record* index_search_simple(Index* index, int key) {
    // Simple implementation - returns NULL
    // In a real implementation, this would search the index data structure
    return NULL;
}

static ErrorCode index_insert_simple(Index* index, int key, Record* record) {
    // Simple implementation - always succeeds
    // In a real implementation, this would insert into the index data structure
    return SUCCESS;
}

static ErrorCode index_delete_simple(Index* index, int key) {
    // Simple implementation - always succeeds
    // In a real implementation, this would delete from the index data structure
    return SUCCESS;
}

static ErrorCode index_update_simple(Index* index, int old_key, int new_key, Record* record) {
    // Simple implementation - delete old, insert new
    ErrorCode err = index_delete_simple(index, old_key);
    if (err != SUCCESS) return err;
    
    return index_insert_simple(index, new_key, record);
}

// Convert index type to string
static const char* index_type_to_string_simple(IndexType type) {
    switch (type) {
        case INDEX_HASH: return "HASH";
        case INDEX_BTREE: return "BTREE";
        case INDEX_SKIPLIST: return "SKIPLIST";
        case INDEX_BITMAP: return "BITMAP";
        case INDEX_FULLTEXT: return "FULLTEXT";
        default: return "UNKNOWN";
    }
}

// Create simple statistics
static DatabaseStatistics* create_simple_statistics() {
    DatabaseStatistics* stats = (DatabaseStatistics*)malloc(sizeof(DatabaseStatistics));
    if (!stats) return NULL;
    
    memset(stats, 0, sizeof(DatabaseStatistics));
    return stats;
}

// Update statistics
static void update_simple_statistics(DatabaseStatistics* stats, StatType stat_type, long long value) {
    if (!stats) return;
    
    switch (stat_type) {
        case STAT_TABLES_CREATED: stats->tables_created += value; break;
        case STAT_RECORDS_INSERTED: stats->records_inserted += value; break;
        case STAT_RECORDS_UPDATED: stats->records_updated += value; break;
        case STAT_RECORDS_DELETED: stats->records_deleted += value; break;
        case STAT_QUERIES_EXECUTED: stats->queries_executed += value; break;
        case STAT_CACHE_HITS: stats->cache_hits += value; break;
        case STAT_CACHE_MISSES: stats->cache_misses += value; break;
        case STAT_TRANSACTIONS_STARTED: stats->transactions_started += value; break;
        case STAT_TRANSACTIONS_COMMITTED: stats->transactions_committed += value; break;
        case STAT_TRANSACTIONS_ROLLED_BACK: stats->transactions_rolled_back += value; break;
        case STAT_INDEXES_CREATED: stats->indexes_created += value; break;
        case STAT_INDEXES_DROPPED: stats->indexes_dropped += value; break;
        case STAT_VACUUM_OPERATIONS: stats->vacuum_operations += value; break;
        case STAT_OPTIMIZATION_OPERATIONS: stats->optimization_operations += value; break;
        default: break;
    }
}

// Free statistics
static void free_simple_statistics(DatabaseStatistics* stats) {
    if (stats) free(stats);
}

// Create table statistics
static TableStatistics* create_simple_table_statistics() {
    TableStatistics* stats = (TableStatistics*)malloc(sizeof(TableStatistics));
    if (!stats) return NULL;
    
    memset(stats, 0, sizeof(TableStatistics));
    return stats;
}

// Update table statistics
static void update_simple_table_statistics(TableStatistics* stats, StatType stat_type, long long value) {
    if (!stats) return;
    
    switch (stat_type) {
        case STAT_RECORDS_INSERTED: stats->records_inserted += value; break;
        case STAT_RECORDS_UPDATED: stats->records_updated += value; break;
        case STAT_RECORDS_DELETED: stats->records_deleted += value; break;
        default: break;
    }
}

// Free table statistics
static void free_simple_table_statistics(TableStatistics* stats) {
    if (stats) free(stats);
}

// Create simple cache (stub implementation)
static Cache* create_simple_cache(int policy, int size) {
    Cache* cache = (Cache*)malloc(sizeof(Cache));
    if (!cache) return NULL;
    
    cache->policy = policy;
    cache->size = size;
    cache->data = NULL;
    
    return cache;
}

// Free cache
static void free_simple_cache(Cache* cache) {
    if (cache) {
        if (cache->data) free(cache->data);
        free(cache);
    }
}

// Cache operations (stub implementations)
static QueryResult* cache_get_simple(Cache* cache, const char* query) {
    return NULL; // Simple stub - always cache miss
}

static void cache_put_simple(Cache* cache, const char* query, QueryResult* result) {
    // Simple stub - does nothing
}

static void cache_clear_simple(Cache* cache) {
    // Simple stub - does nothing
}

// Parse query (stub implementation)
static ParsedQuery* parse_simple_query(const char* query) {
    // Simple stub implementation
    ParsedQuery* parsed = (ParsedQuery*)malloc(sizeof(ParsedQuery));
    if (!parsed) return NULL;
    
    // Very basic parsing - just set default values
    parsed->type = QUERY_SELECT;
    parsed->table_name = NULL;
    parsed->field_name = NULL;
    parsed->condition = NULL;
    parsed->int_value = 0;
    parsed->string_value = NULL;
    
    return parsed;
}

// Free parsed query
static void free_simple_parsed_query(ParsedQuery* query) {
    if (query) {
        if (query->table_name) free(query->table_name);
        if (query->field_name) free(query->field_name);
        if (query->condition) free(query->condition);
        if (query->string_value) free(query->string_value);
        free(query);
    }
}

// Execute query (stub implementation)
static QueryResult* execute_simple_query(Database* db, ParsedQuery* parsed) {
    // Simple stub - returns empty result
    QueryResult* result = (QueryResult*)malloc(sizeof(QueryResult));
    if (!result) return NULL;
    
    result->row_count = 0;
    result->column_count = 0;
    result->column_names = NULL;
    result->column_types = NULL;
    result->rows = NULL;
    
    return result;
}

// Clone query result (stub implementation)
static QueryResult* clone_simple_query_result(QueryResult* result) {
    if (!result) return NULL;
    
    QueryResult* clone = (QueryResult*)malloc(sizeof(QueryResult));
    if (!clone) return NULL;
    
    memcpy(clone, result, sizeof(QueryResult));
    return clone;
}

// Free query result (stub implementation)
static void free_simple_query_result(QueryResult* result) {
    if (result) {
        if (result->column_names) {
            for (int i = 0; i < result->column_count; i++) {
                if (result->column_names[i]) free(result->column_names[i]);
            }
            free(result->column_names);
        }
        if (result->column_types) free(result->column_types);
        if (result->rows) {
            for (int i = 0; i < result->row_count; i++) {
                if (result->rows[i]) free(result->rows[i]);
            }
            free(result->rows);
        }
        free(result);
    }
}

// Update indexes for a record
static ErrorCode update_indexes_for_record(Table* table, Record* record, int old_id) {
    if (!table || !record) return ERROR_INVALID_INPUT;
    
    IndexManager* manager = table->index_manager;
    if (!manager) return SUCCESS;  // No indexes to update
    
    for (int i = 0; i < manager->index_count; i++) {
        Index* index = manager->indices[i];
        if (!index) continue;
        
        // Find field value to index
        int field_idx = -1;
        for (int j = 0; j < table->field_count; j++) {
            if (strcmp(table->field_names[j], index->field_name) == 0) {
                field_idx = j;
                break;
            }
        }
        
        if (field_idx == -1) continue;
        
        // Get key value from record
        int key = 0;
        Field* field = &record->fields[field_idx];
        
        switch (field->type) {
            case TYPE_INT:
                key = field->value.int_value;
                break;
            default:
                continue;
        }
        
        // If old_id != -1, update existing index entry
        if (old_id != -1) {
            index_update_simple(index, old_id, key, record);
        } else {
            index_insert_simple(index, key, record);
        }
    }
    
    return SUCCESS;
}

// Remove indexes for a record
static ErrorCode remove_indexes_for_record(Table* table, int record_id) {
    if (!table) return ERROR_INVALID_INPUT;
    
    IndexManager* manager = table->index_manager;
    if (!manager) return SUCCESS;
    
    for (int i = 0; i < manager->index_count; i++) {
        Index* index = manager->indices[i];
        if (index) {
            index_delete_simple(index, record_id);
        }
    }
    
    return SUCCESS;
}

// ==================== DATABASE CORE FUNCTIONS ====================

Database* db_create() {
    Database* db = (Database*)malloc(sizeof(Database));
    if (!db) {
        return NULL;
    }
    
    db->tables = NULL;
    db->table_count = 0;
    db->table_capacity = 0;
    db->index_manager = create_simple_index_manager();
    db->transaction_active = 0;
    db->transaction_level = 0;
    db->cache = create_simple_cache(CACHE_LRU, QUERY_CACHE_SIZE);
    db->statistics = create_simple_statistics();
    
    // Initialize transaction start time
    db->transaction_start_time = 0;
    
    return db;
}

ErrorCode db_add_table(Database* db, const char* name, const char** field_names, 
                      FieldType* types, int field_count, Table** out_table) {
    if (!db || !name || !field_names || !types || field_count < 1 || field_count > MAX_FIELDS_PER_TABLE) {
        return ERROR_INVALID_INPUT;
    }
    
    // Check if table already exists
    for (int i = 0; i < db->table_count; i++) {
        if (strcmp(db->tables[i].name, name) == 0) {
            return ERROR_DUPLICATE_KEY;
        }
    }
    
    // Check table count limit
    if (db->table_count >= MAX_TABLES) {
        return ERROR_TABLE_FULL;
    }
    
    // Resize tables array if needed
    if (db->table_count >= db->table_capacity) {
        int new_capacity = db->table_capacity == 0 ? 4 : db->table_capacity * 2;
        if (new_capacity > MAX_TABLES) new_capacity = MAX_TABLES;
        
        Table* new_tables = (Table*)realloc(db->tables, new_capacity * sizeof(Table));
        if (!new_tables) {
            return ERROR_MEMORY_ALLOCATION;
        }
        db->tables = new_tables;
        db->table_capacity = new_capacity;
    }
    
    // Initialize new table
    Table* table = &db->tables[db->table_count];
    strncpy(table->name, name, MAX_TABLE_NAME);
    table->name[MAX_TABLE_NAME - 1] = '\0';
    table->records = NULL;
    table->record_count = 0;
    table->capacity = INITIAL_CAPACITY;
    table->field_count = field_count;
    table->next_record_id = 1;
    table->statistics = create_simple_table_statistics();
    
    // Allocate field information
    table->field_names = (char**)malloc(field_count * sizeof(char*));
    table->field_types = (FieldType*)malloc(field_count * sizeof(FieldType));
    table->constraints = (Constraint*)calloc(field_count, sizeof(Constraint));
    
    if (!table->field_names || !table->field_types || !table->constraints) {
        free(table->field_names);
        free(table->field_types);
        free(table->constraints);
        return ERROR_MEMORY_ALLOCATION;
    }
    
    // Initialize index manager for this table
    table->index_manager = create_simple_index_manager();
    if (!table->index_manager) {
        free(table->field_names);
        free(table->field_types);
        free(table->constraints);
        free_simple_table_statistics(table->statistics);
        return ERROR_MEMORY_ALLOCATION;
    }
    
    // Copy field information
    for (int i = 0; i < field_count; i++) {
        table->field_names[i] = strdup(field_names[i]);
        table->field_types[i] = types[i];
        
        if (!table->field_names[i]) {
            // Cleanup already allocated fields
            for (int j = 0; j < i; j++) free(table->field_names[j]);
            free(table->field_names);
            free(table->field_types);
            free(table->constraints);
            free_simple_table_statistics(table->statistics);
            free_index_manager(table->index_manager);
            return ERROR_MEMORY_ALLOCATION;
        }
        
        // Set default constraints
        table->constraints[i].nullable = true;
        table->constraints[i].unique = false;
        table->constraints[i].primary_key = (i == 0);  // First field is primary key by default
        table->constraints[i].foreign_key = false;
    }
    
    db->table_count++;
    update_simple_statistics(db->statistics, STAT_TABLES_CREATED, 1);
    
    if (out_table) {
        *out_table = table;
    }
    
    return SUCCESS;
}

Table* db_get_table(Database* db, const char* table_name) {
    if (!db || !table_name) return NULL;
    
    for (int i = 0; i < db->table_count; i++) {
        if (strcmp(db->tables[i].name, table_name) == 0) {
            return &db->tables[i];
        }
    }
    
    return NULL;
}

// ==================== RECORD OPERATIONS ====================

ErrorCode table_insert_record(Table* table, Field* values, int* out_id) {
    if (!table || !values) return ERROR_INVALID_INPUT;
    
    // Check record count limit
    if (table->record_count >= MAX_RECORDS_PER_TABLE) {
        return ERROR_TABLE_FULL;
    }
    
    // Validate field count
    if (values[0].type == TYPE_UNKNOWN) {
        return ERROR_INVALID_INPUT;
    }
    
    // Check constraints
    for (int i = 0; i < table->field_count; i++) {
        if (!table->constraints[i].nullable) {
            // Check for NULL values
            if (values[i].type == TYPE_NULL) {
                return ERROR_CONSTRAINT_VIOLATION;
            }
        }
    }
    
    // Generate ID if not provided
    int record_id = -1;
    for (int i = 0; i < table->field_count; i++) {
        if (table->constraints[i].primary_key) {
            if (values[i].type == TYPE_INT) {
                record_id = values[i].value.int_value;
                break;
            }
        }
    }
    
    if (record_id == -1) {
        record_id = generate_record_id(table);
    }
    
    // Check for duplicate ID
    if (table_find_record(table, record_id) != NULL) {
        return ERROR_DUPLICATE_KEY;
    }
    
    // Create new record
    Record* new_record = (Record*)malloc(sizeof(Record));
    if (!new_record) {
        return ERROR_MEMORY_ALLOCATION;
    }
    
    new_record->id = record_id;
    new_record->field_count = table->field_count;
    new_record->fields = (Field*)malloc(table->field_count * sizeof(Field));
    new_record->next = NULL;
    new_record->timestamp = time(NULL);
    new_record->version = 1;
    
    if (!new_record->fields) {
        free(new_record);
        return ERROR_MEMORY_ALLOCATION;
    }
    
    // Copy field values
    for (int i = 0; i < table->field_count; i++) {
        strncpy(new_record->fields[i].name, table->field_names[i], MAX_FIELD_LEN);
        new_record->fields[i].name[MAX_FIELD_LEN - 1] = '\0';
        new_record->fields[i].type = table->field_types[i];
        new_record->fields[i].value = values[i].value;
    }
    
    // Insert at beginning of linked list
    new_record->next = table->records;
    table->records = new_record;
    table->record_count++;
    
    // Update next_record_id
    if (record_id >= table->next_record_id) {
        table->next_record_id = record_id + 1;
    }
    
    // Update indexes
    update_indexes_for_record(table, new_record, -1);
    
    // Update statistics
    if (table->statistics) {
        update_simple_table_statistics(table->statistics, STAT_RECORDS_INSERTED, 1);
    }
    
    if (out_id) {
        *out_id = record_id;
    }
    
    return SUCCESS;
}

ErrorCode db_insert_record(Database* db, const char* table_name, Field* values, int* out_id) {
    if (!db || !table_name || !values) return ERROR_INVALID_INPUT;
    
    Table* table = db_get_table(db, table_name);
    if (!table) {
        return ERROR_NOT_FOUND;
    }
    
    // Start transaction if not already in one
    bool local_transaction = false;
    if (!db->transaction_active) {
        ErrorCode tx_result = db_begin_transaction(db);
        if (tx_result != SUCCESS) return tx_result;
        local_transaction = true;
    }
    
    ErrorCode result = table_insert_record(table, values, out_id);
    
    if (local_transaction) {
        if (result == SUCCESS) {
            db_commit_transaction(db);
        } else {
            db_rollback_transaction(db);
        }
    }
    
    return result;
}

ErrorCode db_batch_insert_records(Database* db, const char* table_name, 
                                  Field** values_array, int count, int** out_ids) {
    if (!db || !table_name || !values_array || count < 1 || count > BATCH_INSERT_SIZE) {
        return ERROR_INVALID_INPUT;
    }
    
    Table* table = db_get_table(db, table_name);
    if (!table) {
        return ERROR_NOT_FOUND;
    }
    
    // Check capacity
    if (table->record_count + count > MAX_RECORDS_PER_TABLE) {
        return ERROR_TABLE_FULL;
    }
    
    // Start transaction
    ErrorCode tx_result = db_begin_transaction(db);
    if (tx_result != SUCCESS) return tx_result;
    
    int* ids = NULL;
    if (out_ids) {
        ids = (int*)malloc(count * sizeof(int));
        if (!ids) {
            db_rollback_transaction(db);
            return ERROR_MEMORY_ALLOCATION;
        }
    }
    
    ErrorCode final_result = SUCCESS;
    int successful_inserts = 0;
    
    for (int i = 0; i < count; i++) {
        int record_id = -1;
        ErrorCode result = table_insert_record(table, values_array[i], &record_id);
        
        if (result == SUCCESS) {
            if (ids) ids[successful_inserts] = record_id;
            successful_inserts++;
        } else {
            final_result = result;
            break;
        }
    }
    
    if (successful_inserts == count) {
        db_commit_transaction(db);
        if (out_ids) *out_ids = ids;
    } else {
        db_rollback_transaction(db);
        if (ids) free(ids);
    }
    
    return final_result;
}

Record* table_find_record(Table* table, int id) {
    if (!table) return NULL;
    
    // First try to use index
    if (table->index_manager && table->index_manager->index_count > 0) {
        // Look for primary key index
        for (int i = 0; i < table->index_manager->index_count; i++) {
            Index* index = table->index_manager->indices[i];
            if (!index) continue;
            
            if (strcmp(index->field_name, "id") == 0 || 
                table->constraints[0].primary_key) {
                Record* record = index_search_simple(index, id);
                if (record) {
                    return record;
                }
                break;
            }
        }
    }
    
    // Fallback to linear search
    for (Record* curr = table->records; curr != NULL; curr = curr->next) {
        if (curr->id == id) {
            return curr;
        }
    }
    
    return NULL;
}

Record* db_find_record(Database* db, const char* table_name, int id) {
    if (!db || !table_name) return NULL;
    
    Table* table = db_get_table(db, table_name);
    if (!table) {
        return NULL;
    }
    
    return table_find_record(table, id);
}

Record* db_find_record_by_field(Database* db, const char* table_name, 
                                const char* field_name, const void* value) {
    if (!db || !table_name || !field_name || !value) return NULL;
    
    Table* table = db_get_table(db, table_name);
    if (!table) {
        return NULL;
    }
    
    // Find field index
    int field_idx = -1;
    for (int i = 0; i < table->field_count; i++) {
        if (strcmp(table->field_names[i], field_name) == 0) {
            field_idx = i;
            break;
        }
    }
    
    if (field_idx == -1) {
        return NULL;
    }
    
    // Try using index first
    Index* index = find_index_in_manager(table->index_manager, table_name, field_name);
    if (index) {
        // Convert value to key based on field type
        int key = 0;
        FieldType field_type = table->field_types[field_idx];
        
        switch (field_type) {
            case TYPE_INT:
                key = *(int*)value;
                break;
            default:
                break;
        }
        
        if (key != 0) {
            Record* record = index_search_simple(index, key);
            if (record) {
                return record;
            }
        }
    }
    
    // Fallback to linear search
    for (Record* curr = table->records; curr != NULL; curr = curr->next) {
        Field* field = &curr->fields[field_idx];
        
        bool match = false;
        switch (field->type) {
            case TYPE_INT:
                match = (field->value.int_value == *(int*)value);
                break;
            case TYPE_STRING:
                match = (strcmp(field->value.string_value, (char*)value) == 0);
                break;
            case TYPE_FLOAT:
                match = (field->value.float_value == *(float*)value);
                break;
            case TYPE_DOUBLE:
                match = (field->value.double_value == *(double*)value);
                break;
            case TYPE_BOOL:
                match = (field->value.bool_value == *(bool*)value);
                break;
            default:
                continue;
        }
        
        if (match) {
            return curr;
        }
    }
    
    return NULL;
}

Record** db_find_all_records_by_field(Database* db, const char* table_name,
                                      const char* field_name, const void* value,
                                      int* out_count) {
    if (!db || !table_name || !field_name || !value || !out_count) {
        return NULL;
    }
    
    *out_count = 0;
    
    Table* table = db_get_table(db, table_name);
    if (!table) {
        return NULL;
    }
    
    // Find field index
    int field_idx = -1;
    for (int i = 0; i < table->field_count; i++) {
        if (strcmp(table->field_names[i], field_name) == 0) {
            field_idx = i;
            break;
        }
    }
    
    if (field_idx == -1) {
        return NULL;
    }
    
    // Allocate result array
    Record** results = (Record**)malloc(table->record_count * sizeof(Record*));
    if (!results) {
        return NULL;
    }
    
    int count = 0;
    FieldType field_type = table->field_types[field_idx];
    
    // Linear search
    for (Record* curr = table->records; curr != NULL; curr = curr->next) {
        Field* field = &curr->fields[field_idx];
        
        bool match = false;
        switch (field_type) {
            case TYPE_INT:
                match = (field->value.int_value == *(int*)value);
                break;
            case TYPE_STRING:
                match = (strcmp(field->value.string_value, (char*)value) == 0);
                break;
            case TYPE_FLOAT:
                match = (field->value.float_value == *(float*)value);
                break;
            case TYPE_DOUBLE:
                match = (field->value.double_value == *(double*)value);
                break;
            case TYPE_BOOL:
                match = (field->value.bool_value == *(bool*)value);
                break;
            default:
                continue;
        }
        
        if (match) {
            results[count++] = curr;
        }
    }
    
    // Resize array to actual count
    if (count > 0) {
        Record** trimmed_results = (Record**)realloc(results, count * sizeof(Record*));
        if (trimmed_results) {
            results = trimmed_results;
        }
    } else {
        free(results);
        results = NULL;
    }
    
    *out_count = count;
    
    return results;
}

ErrorCode table_update_record(Table* table, int id, Field* new_values) {
    if (!table || !new_values) return ERROR_INVALID_INPUT;
    
    Record* record = table_find_record(table, id);
    if (!record) {
        return ERROR_NOT_FOUND;
    }
    
    // Store old field values for index update
    Field* old_values = NULL;
    if (table->index_manager && table->index_manager->index_count > 0) {
        old_values = (Field*)malloc(table->field_count * sizeof(Field));
        if (old_values) {
            memcpy(old_values, record->fields, table->field_count * sizeof(Field));
        }
    }
    
    // Update field values
    for (int i = 0; i < table->field_count; i++) {
        // Check constraints if updating primary key
        if (table->constraints[i].primary_key) {
            if (new_values[i].type != TYPE_NULL) {
                int new_id = 0;
                if (new_values[i].type == TYPE_INT) {
                    new_id = new_values[i].value.int_value;
                }
                
                if (new_id != id && table_find_record(table, new_id) != NULL) {
                    if (old_values) free(old_values);
                    return ERROR_DUPLICATE_KEY;
                }
            }
        }
        
        // Update the value
        record->fields[i].value = new_values[i].value;
        if (new_values[i].type != TYPE_UNKNOWN) {
            record->fields[i].type = new_values[i].type;
        }
    }
    
    // Update record metadata
    record->timestamp = time(NULL);
    record->version++;
    
    // Update indexes if any fields changed that are indexed
    if (old_values && table->index_manager) {
        for (int i = 0; i < table->index_manager->index_count; i++) {
            Index* index = table->index_manager->indices[i];
            if (!index) continue;
            
            int field_idx = -1;
            
            for (int j = 0; j < table->field_count; j++) {
                if (strcmp(table->field_names[j], index->field_name) == 0) {
                    field_idx = j;
                    break;
                }
            }
            
            if (field_idx != -1) {
                // Check if the indexed field changed
                bool changed = false;
                Field* old_field = &old_values[field_idx];
                Field* new_field = &record->fields[field_idx];
                
                switch (old_field->type) {
                    case TYPE_INT:
                        changed = (old_field->value.int_value != new_field->value.int_value);
                        break;
                    case TYPE_STRING:
                        changed = (strcmp(old_field->value.string_value, 
                                         new_field->value.string_value) != 0);
                        break;
                    case TYPE_FLOAT:
                        changed = (old_field->value.float_value != new_field->value.float_value);
                        break;
                    default:
                        changed = true;
                }
                
                if (changed) {
                    int old_key = 0, new_key = 0;
                    
                    if (old_field->type == TYPE_INT) {
                        old_key = old_field->value.int_value;
                        new_key = new_field->value.int_value;
                    }
                    
                    if (old_key != 0 || new_key != 0) {
                        index_update_simple(index, old_key, new_key, record);
                    }
                }
            }
        }
        free(old_values);
    }
    
    // Update statistics
    if (table->statistics) {
        update_simple_table_statistics(table->statistics, STAT_RECORDS_UPDATED, 1);
    }
    
    return SUCCESS;
}

ErrorCode db_update_record(Database* db, const char* table_name, int id, Field* new_values) {
    if (!db || !table_name || !new_values) return ERROR_INVALID_INPUT;
    
    Table* table = db_get_table(db, table_name);
    if (!table) {
        return ERROR_NOT_FOUND;
    }
    
    // Start transaction if not already in one
    bool local_transaction = false;
    if (!db->transaction_active) {
        ErrorCode tx_result = db_begin_transaction(db);
        if (tx_result != SUCCESS) return tx_result;
        local_transaction = true;
    }
    
    ErrorCode result = table_update_record(table, id, new_values);
    
    if (local_transaction) {
        if (result == SUCCESS) {
            db_commit_transaction(db);
        } else {
            db_rollback_transaction(db);
        }
    }
    
    return result;
}

ErrorCode table_delete_record(Table* table, int id) {
    if (!table) return ERROR_INVALID_INPUT;
    
    Record* prev = NULL;
    Record* curr = table->records;
    
    while (curr != NULL) {
        if (curr->id == id) {
            // Remove from linked list
            if (prev == NULL) {
                table->records = curr->next;
            } else {
                prev->next = curr->next;
            }
            
            // Remove from indexes
            remove_indexes_for_record(table, id);
            
            // Free memory
            free(curr->fields);
            free(curr);
            table->record_count--;
            
            // Update statistics
            if (table->statistics) {
                update_simple_table_statistics(table->statistics, STAT_RECORDS_DELETED, 1);
            }
            
            return SUCCESS;
        }
        prev = curr;
        curr = curr->next;
    }
    
    return ERROR_NOT_FOUND;
}

ErrorCode db_delete_record(Database* db, const char* table_name, int id) {
    if (!db || !table_name) return ERROR_INVALID_INPUT;
    
    Table* table = db_get_table(db, table_name);
    if (!table) {
        return ERROR_NOT_FOUND;
    }
    
    // Start transaction if not already in one
    bool local_transaction = false;
    if (!db->transaction_active) {
        ErrorCode tx_result = db_begin_transaction(db);
        if (tx_result != SUCCESS) return tx_result;
        local_transaction = true;
    }
    
    ErrorCode result = table_delete_record(table, id);
    
    if (local_transaction) {
        if (result == SUCCESS) {
            db_commit_transaction(db);
        } else {
            db_rollback_transaction(db);
        }
    }
    
    return result;
}

ErrorCode db_delete_records_by_field(Database* db, const char* table_name,
                                     const char* field_name, const void* value,
                                     int* out_count) {
    if (!db || !table_name || !field_name || !value) return ERROR_INVALID_INPUT;
    
    if (out_count) *out_count = 0;
    
    Table* table = db_get_table(db, table_name);
    if (!table) {
        return ERROR_NOT_FOUND;
    }
    
    // Find field index
    int field_idx = -1;
    for (int i = 0; i < table->field_count; i++) {
        if (strcmp(table->field_names[i], field_name) == 0) {
            field_idx = i;
            break;
        }
    }
    
    if (field_idx == -1) {
        return ERROR_NOT_FOUND;
    }
    
    // Start transaction
    ErrorCode tx_result = db_begin_transaction(db);
    if (tx_result != SUCCESS) return tx_result;
    
    int deleted_count = 0;
    Record* prev = NULL;
    Record* curr = table->records;
    FieldType field_type = table->field_types[field_idx];
    
    while (curr != NULL) {
        Record* next = curr->next;
        Field* field = &curr->fields[field_idx];
        
        bool match = false;
        switch (field_type) {
            case TYPE_INT:
                match = (field->value.int_value == *(int*)value);
                break;
            case TYPE_STRING:
                match = (strcmp(field->value.string_value, (char*)value) == 0);
                break;
            case TYPE_FLOAT:
                match = (field->value.float_value == *(float*)value);
                break;
            case TYPE_DOUBLE:
                match = (field->value.double_value == *(double*)value);
                break;
            case TYPE_BOOL:
                match = (field->value.bool_value == *(bool*)value);
                break;
            default:
                break;
        }
        
        if (match) {
            // Remove from linked list
            if (prev == NULL) {
                table->records = next;
            } else {
                prev->next = next;
            }
            
            // Remove from indexes
            remove_indexes_for_record(table, curr->id);
            
            // Free memory
            free(curr->fields);
            free(curr);
            table->record_count--;
            deleted_count++;
        } else {
            prev = curr;
        }
        
        curr = next;
    }
    
    if (deleted_count > 0) {
        db_commit_transaction(db);
        if (out_count) *out_count = deleted_count;
        return SUCCESS;
    } else {
        db_rollback_transaction(db);
        return SUCCESS;
    }
}

// ==================== QUERY OPERATIONS ====================

QueryResult* db_execute_query(Database* db, const char* query) {
    if (!db || !query) return NULL;
    
    // Check cache first
    if (db->cache) {
        QueryResult* cached = cache_get_simple(db->cache, query);
        if (cached) {
            update_simple_statistics(db->statistics, STAT_CACHE_HITS, 1);
            return clone_simple_query_result(cached);
        }
        update_simple_statistics(db->statistics, STAT_CACHE_MISSES, 1);
    }
    
    // Parse query
    ParsedQuery* parsed = parse_simple_query(query);
    if (!parsed) {
        return NULL;
    }
    
    // Execute based on query type
    QueryResult* result = execute_simple_query(db, parsed);
    
    // Cache the result if it's a SELECT query
    if (result && db->cache && parsed->type == QUERY_SELECT) {
        QueryResult* cached_copy = clone_simple_query_result(result);
        if (cached_copy) {
            cache_put_simple(db->cache, query, cached_copy);
        }
    }
    
    // Cleanup
    free_simple_parsed_query(parsed);
    
    update_simple_statistics(db->statistics, STAT_QUERIES_EXECUTED, 1);
    
    return result;
}

// ==================== INDEX OPERATIONS ====================

ErrorCode db_create_index(Database* db, const char* table_name, 
                         const char* field_name, IndexType type) {
    if (!db || !table_name || !field_name) return ERROR_INVALID_INPUT;
    
    Table* table = db_get_table(db, table_name);
    if (!table) {
        return ERROR_NOT_FOUND;
    }
    
    // Check if field exists
    int field_idx = -1;
    for (int i = 0; i < table->field_count; i++) {
        if (strcmp(table->field_names[i], field_name) == 0) {
            field_idx = i;
            break;
        }
    }
    
    if (field_idx == -1) {
        return ERROR_NOT_FOUND;
    }
    
    // Check if index already exists
    if (find_index_in_manager(table->index_manager, table_name, field_name) != NULL) {
        return ERROR_INDEX_EXISTS;
    }
    
    // Check index limit
    if (table->index_manager->index_count >= MAX_INDEXES_PER_TABLE) {
        return ERROR_TABLE_FULL;
    }
    
    // Create index
    Index* index = create_simple_index(table_name, field_name, type);
    if (!index) {
        return ERROR_MEMORY_ALLOCATION;
    }
    
    // Populate index with existing records
    for (Record* curr = table->records; curr != NULL; curr = curr->next) {
        Field* field = &curr->fields[field_idx];
        int key = 0;
        
        switch (field->type) {
            case TYPE_INT:
                key = field->value.int_value;
                break;
            default:
                continue;
        }
        
        if (key != 0) {
            index_insert_simple(index, key, curr);
        }
    }
    
    // Add index to manager
    ErrorCode result = add_index_to_manager(table->index_manager, index);
    if (result != SUCCESS) {
        free_index(index);
        return result;
    }
    
    update_simple_statistics(db->statistics, STAT_INDEXES_CREATED, 1);
    
    return SUCCESS;
}

ErrorCode db_drop_index(Database* db, const char* table_name, const char* field_name) {
    if (!db || !table_name || !field_name) return ERROR_INVALID_INPUT;
    
    Table* table = db_get_table(db, table_name);
    if (!table) {
        return ERROR_NOT_FOUND;
    }
    
    ErrorCode result = remove_index_from_manager(table->index_manager, table_name, field_name);
    if (result == SUCCESS) {
        update_simple_statistics(db->statistics, STAT_INDEXES_DROPPED, 1);
    }
    
    return result;
}

// ==================== TRANSACTION MANAGEMENT ====================

ErrorCode db_begin_transaction(Database* db) {
    if (!db) return ERROR_INVALID_INPUT;
    
    if (db->transaction_active && db->transaction_level >= MAX_TRANSACTION_LEVEL) {
        return ERROR_TRANSACTION_CONFLICT;
    }
    
    if (!db->transaction_active) {
        // Initialize transaction
        db->transaction_active = 1;
        db->transaction_level = 1;
        db->transaction_start_time = time(NULL);
    } else {
        // Nested transaction (savepoint)
        db->transaction_level++;
    }
    
    update_simple_statistics(db->statistics, STAT_TRANSACTIONS_STARTED, 1);
    return SUCCESS;
}

ErrorCode db_commit_transaction(Database* db) {
    if (!db) return ERROR_INVALID_INPUT;
    
    if (!db->transaction_active) {
        return ERROR_TRANSACTION_CONFLICT;
    }
    
    if (db->transaction_level > 1) {
        // Nested transaction - just decrement level
        db->transaction_level--;
        return SUCCESS;
    }
    
    // Commit the transaction
    db->transaction_active = 0;
    db->transaction_level = 0;
    
    update_simple_statistics(db->statistics, STAT_TRANSACTIONS_COMMITTED, 1);
    return SUCCESS;
}

ErrorCode db_rollback_transaction(Database* db) {
    if (!db) return ERROR_INVALID_INPUT;
    
    if (!db->transaction_active) {
        return ERROR_TRANSACTION_CONFLICT;
    }
    
    // Rollback all changes
    db->transaction_active = 0;
    db->transaction_level = 0;
    
    update_simple_statistics(db->statistics, STAT_TRANSACTIONS_ROLLED_BACK, 1);
    return SUCCESS;
}

// ==================== STATISTICS & MONITORING ====================

DatabaseStatistics* db_get_statistics(Database* db) {
    if (!db) return NULL;
    return db->statistics;
}

TableStatistics* db_get_table_statistics(Database* db, const char* table_name) {
    if (!db || !table_name) return NULL;
    
    Table* table = db_get_table(db, table_name);
    if (!table) {
        return NULL;
    }
    
    return table->statistics;
}

ErrorCode db_reset_statistics(Database* db) {
    if (!db) return ERROR_INVALID_INPUT;
    
    if (db->statistics) {
        memset(db->statistics, 0, sizeof(DatabaseStatistics));
    }
    
    for (int i = 0; i < db->table_count; i++) {
        if (db->tables[i].statistics) {
            memset(db->tables[i].statistics, 0, sizeof(TableStatistics));
        }
    }
    
    return SUCCESS;
}

// ==================== MAINTENANCE OPERATIONS ====================

ErrorCode db_vacuum(Database* db) {
    if (!db) return ERROR_INVALID_INPUT;
    
    // Rebuild indexes
    for (int i = 0; i < db->table_count; i++) {
        Table* table = &db->tables[i];
        
        if (table->index_manager) {
            for (int j = 0; j < table->index_manager->index_count; j++) {
                Index* index = table->index_manager->indices[j];
                // Index rebuild logic would go here
            }
        }
    }
    
    // Clear cache
    if (db->cache) {
        cache_clear_simple(db->cache);
    }
    
    // Update statistics
    update_simple_statistics(db->statistics, STAT_VACUUM_OPERATIONS, 1);
    
    return SUCCESS;
}

ErrorCode db_optimize(Database* db) {
    if (!db) return ERROR_INVALID_INPUT;
    
    update_simple_statistics(db->statistics, STAT_OPTIMIZATION_OPERATIONS, 1);
    
    return SUCCESS;
}

// ==================== UTILITY FUNCTIONS ====================

int db_get_table_count(Database* db) {
    return db ? db->table_count : 0;
}

int db_get_record_count(Database* db, const char* table_name) {
    if (!db || !table_name) return 0;
    
    Table* table = db_get_table(db, table_name);
    return table ? table->record_count : 0;
}

char** db_list_tables(Database* db, int* out_count) {
    if (!db || !out_count) return NULL;
    
    *out_count = 0;
    
    if (db->table_count == 0) {
        return NULL;
    }
    
    char** tables = (char**)malloc(db->table_count * sizeof(char*));
    if (!tables) {
        return NULL;
    }
    
    for (int i = 0; i < db->table_count; i++) {
        tables[i] = strdup(db->tables[i].name);
        if (!tables[i]) {
            // Cleanup on error
            for (int j = 0; j < i; j++) free(tables[j]);
            free(tables);
            return NULL;
        }
    }
    
    *out_count = db->table_count;
    return tables;
}

// ==================== CLEANUP & DESTRUCTION ====================

void table_free(Table* table) {
    if (!table) return;
    
    // Free field names
    if (table->field_names) {
        for (int i = 0; i < table->field_count; i++) {
            free(table->field_names[i]);
        }
        free(table->field_names);
    }
    
    if (table->field_types) free(table->field_types);
    if (table->constraints) free(table->constraints);
    
    // Free records
    Record* curr = table->records;
    while (curr != NULL) {
        Record* next = curr->next;
        free(curr->fields);
        free(curr);
        curr = next;
    }
    
    // Free index manager
    if (table->index_manager) {
        free_index_manager(table->index_manager);
    }
    
    // Free statistics
    if (table->statistics) {
        free_simple_table_statistics(table->statistics);
    }
}

void db_free(Database* db) {
    if (!db) return;
    
    // Free all tables
    if (db->tables) {
        for (int i = 0; i < db->table_count; i++) {
            table_free(&db->tables[i]);
        }
        free(db->tables);
    }
    
    // Free index manager
    if (db->index_manager) {
        free_index_manager(db->index_manager);
    }
    
    // Free cache
    if (db->cache) {
        free_simple_cache(db->cache);
    }
    
    // Free statistics
    if (db->statistics) {
        free_simple_statistics(db->statistics);
    }
    
    free(db);
}

// ==================== DEBUGGING & DIAGNOSTICS ====================

void db_print_schema(Database* db) {
    if (!db) {
        printf("Database is NULL\n");
        return;
    }
    
    printf("\n=== DATABASE SCHEMA ===\n");
    printf("Tables: %d\n\n", db->table_count);
    
    for (int i = 0; i < db->table_count; i++) {
        Table* table = &db->tables[i];
        
        printf("Table: %s\n", table->name);
        printf("Records: %d\n", table->record_count);
        printf("Fields:\n");
        
        for (int j = 0; j < table->field_count; j++) {
            printf("  %s (%s)", table->field_names[j], 
                   field_type_to_string(table->field_types[j]));
            
            if (table->constraints[j].primary_key) printf(" PRIMARY KEY");
            if (table->constraints[j].unique) printf(" UNIQUE");
            if (!table->constraints[j].nullable) printf(" NOT NULL");
            
            printf("\n");
        }
        
        printf("Indexes: %d\n", table->index_manager ? table->index_manager->index_count : 0);
        if (table->index_manager && table->index_manager->index_count > 0) {
            for (int j = 0; j < table->index_manager->index_count; j++) {
                Index* index = table->index_manager->indices[j];
                if (index) {
                    printf("  - %s (%s)\n", index->field_name,
                           index_type_to_string_simple(index->type));
                }
            }
        }
        
        printf("\n");
    }
}

void db_print_statistics(Database* db) {
    if (!db || !db->statistics) {
        printf("No statistics available\n");
        return;
    }
    
    printf("\n=== DATABASE STATISTICS ===\n");
    printf("Tables Created: %lld\n", db->statistics->tables_created);
    printf("Records Inserted: %lld\n", db->statistics->records_inserted);
    printf("Records Updated: %lld\n", db->statistics->records_updated);
    printf("Records Deleted: %lld\n", db->statistics->records_deleted);
    printf("Queries Executed: %lld\n", db->statistics->queries_executed);
    printf("Cache Hits: %lld\n", db->statistics->cache_hits);
    printf("Cache Misses: %lld\n", db->statistics->cache_misses);
    printf("Transactions Started: %lld\n", db->statistics->transactions_started);
    printf("Transactions Committed: %lld\n", db->statistics->transactions_committed);
    printf("Transactions Rolled Back: %lld\n", db->statistics->transactions_rolled_back);
    printf("Indexes Created: %lld\n", db->statistics->indexes_created);
    printf("Indexes Dropped: %lld\n", db->statistics->indexes_dropped);
    
    // Calculate cache hit rate
    long long total_cache_access = db->statistics->cache_hits + db->statistics->cache_misses;
    if (total_cache_access > 0) {
        double hit_rate = (double)db->statistics->cache_hits / total_cache_access * 100;
        printf("Cache Hit Rate: %.2f%%\n", hit_rate);
    }
}

// ==================== HELPER FUNCTIONS ====================

const char* field_type_to_string(FieldType type) {
    switch (type) {
        case TYPE_INT: return "INT";
        case TYPE_STRING: return "STRING";
        case TYPE_FLOAT: return "FLOAT";
        case TYPE_DOUBLE: return "DOUBLE";
        case TYPE_BOOL: return "BOOL";
        case TYPE_DATETIME: return "DATETIME";
        case TYPE_BLOB: return "BLOB";
        case TYPE_NULL: return "NULL";
        case TYPE_UNKNOWN: return "UNKNOWN";
        default: return "UNKNOWN";
    }
}