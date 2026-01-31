#include "database.h"
#include "utils.h"
#include "index.h"
#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

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
            if (strcasecmp(value_str, "true") != 0 &&
                strcasecmp(value_str, "false") != 0 &&
                strcasecmp(value_str, "1") != 0 &&
                strcasecmp(value_str, "0") != 0 &&
                strcasecmp(value_str, "yes") != 0 &&
                strcasecmp(value_str, "no") != 0) {
                return ERROR_TYPE_MISMATCH;
            }
            break;
        }
        case TYPE_STRING:
            // All strings are valid
            break;
        case TYPE_DATETIME:
            // TODO: Validate datetime format
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
            // TODO: Parse datetime
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

// Update indexes for a record
static ErrorCode update_indexes_for_record(Table* table, Record* record, int old_id) {
    if (!table || !record) return ERROR_INVALID_INPUT;
    
    IndexManager* manager = table->index_manager;
    if (!manager) return SUCCESS;  // No indexes to update
    
    for (int i = 0; i < manager->count; i++) {
        Index* index = manager->indexes[i];
        
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
                // For non-integer fields, use hash of string representation
                // This is simplified - in production, would handle all types
                continue;
        }
        
        // If old_id != -1, update existing index entry
        if (old_id != -1) {
            index_update(index, old_id, key, record);
        } else {
            index_insert(index, key, record);
        }
    }
    
    return SUCCESS;
}

// Remove indexes for a record
static ErrorCode remove_indexes_for_record(Table* table, int record_id) {
    if (!table) return ERROR_INVALID_INPUT;
    
    IndexManager* manager = table->index_manager;
    if (!manager) return SUCCESS;
    
    for (int i = 0; i < manager->count; i++) {
        Index* index = manager->indexes[i];
        index_delete(index, record_id);
    }
    
    return SUCCESS;
}

// ==================== DATABASE CORE FUNCTIONS ====================

Database* db_create() {
    LOG_DEBUG("Creating new database");
    
    Database* db = (Database*)malloc(sizeof(Database));
    if (!db) {
        LOG_ERROR("Failed to allocate database");
        return NULL;
    }
    
    db->tables = NULL;
    db->table_count = 0;
    db->table_capacity = 0;
    db->index_manager = create_index_manager();
    db->transaction_active = 0;
    db->transaction_level = 0;
    db->cache = create_cache(CACHE_LRU, QUERY_CACHE_SIZE);
    db->statistics = create_statistics();
    
    LOG_INFO("Database created successfully");
    return db;
}

ErrorCode db_add_table(Database* db, const char* name, const char** field_names, 
                      FieldType* types, int field_count, Table** out_table) {
    VALIDATE_PTR(db);
    VALIDATE_PTR(name);
    VALIDATE_PTR(field_names);
    VALIDATE_PTR(types);
    VALIDATE_RANGE(field_count, 1, MAX_FIELDS_PER_TABLE);
    
    LOG_DEBUG("Adding table '%s' with %d fields", name, field_count);
    
    // Check if table already exists
    for (int i = 0; i < db->table_count; i++) {
        if (strcmp(db->tables[i].name, name) == 0) {
            LOG_WARN("Table '%s' already exists", name);
            return ERROR_DUPLICATE_KEY;
        }
    }
    
    // Check table count limit
    if (db->table_count >= MAX_TABLES) {
        LOG_ERROR("Maximum table limit reached (%d)", MAX_TABLES);
        return ERROR_TABLE_FULL;
    }
    
    // Resize tables array if needed
    if (db->table_count >= db->table_capacity) {
        int new_capacity = db->table_capacity == 0 ? 4 : db->table_capacity * 2;
        if (new_capacity > MAX_TABLES) new_capacity = MAX_TABLES;
        
        Table* new_tables = (Table*)realloc(db->tables, new_capacity * sizeof(Table));
        if (!new_tables) {
            LOG_ERROR("Failed to reallocate tables array");
            return ERROR_MEMORY_ALLOCATION;
        }
        db->tables = new_tables;
        db->table_capacity = new_capacity;
        
        LOG_DEBUG("Resized table array to capacity %d", new_capacity);
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
    
    // Allocate field information
    table->field_names = (char**)malloc(field_count * sizeof(char*));
    table->field_types = (FieldType*)malloc(field_count * sizeof(FieldType));
    table->constraints = (Constraint*)calloc(field_count, sizeof(Constraint));
    
    if (!table->field_names || !table->field_types || !table->constraints) {
        LOG_ERROR("Failed to allocate table field arrays");
        free(table->field_names);
        free(table->field_types);
        free(table->constraints);
        return ERROR_MEMORY_ALLOCATION;
    }
    
    // Initialize index manager for this table
    table->index_manager = create_index_manager();
    if (!table->index_manager) {
        LOG_ERROR("Failed to create index manager for table");
        free(table->field_names);
        free(table->field_types);
        free(table->constraints);
        return ERROR_MEMORY_ALLOCATION;
    }
    
    // Copy field information
    for (int i = 0; i < field_count; i++) {
        table->field_names[i] = strdup(field_names[i]);
        table->field_types[i] = types[i];
        
        if (!table->field_names[i]) {
            LOG_ERROR("Failed to duplicate field name");
            // Cleanup already allocated fields
            for (int j = 0; j < i; j++) free(table->field_names[j]);
            free(table->field_names);
            free(table->field_types);
            free(table->constraints);
            index_manager_free(table->index_manager);
            return ERROR_MEMORY_ALLOCATION;
        }
        
        // Set default constraints
        table->constraints[i].nullable = true;
        table->constraints[i].unique = false;
        table->constraints[i].primary_key = (i == 0);  // First field is primary key by default
        table->constraints[i].foreign_key = false;
    }
    
    db->table_count++;
    update_statistics(db->statistics, STAT_TABLES_CREATED, 1);
    
    LOG_INFO("Table '%s' created successfully with %d fields", name, field_count);
    
    if (out_table) {
        *out_table = table;
    }
    
    return SUCCESS;
}

Table* db_get_table(Database* db, const char* table_name) {
    VALIDATE_PTR(db);
    VALIDATE_PTR(table_name);
    
    for (int i = 0; i < db->table_count; i++) {
        if (strcmp(db->tables[i].name, table_name) == 0) {
            return &db->tables[i];
        }
    }
    
    LOG_DEBUG("Table '%s' not found", table_name);
    return NULL;
}

// ==================== RECORD OPERATIONS ====================

ErrorCode table_insert_record(Table* table, Field* values, int* out_id) {
    VALIDATE_PTR(table);
    VALIDATE_PTR(values);
    
    LOG_DEBUG("Inserting record into table '%s'", table->name);
    
    // Check record count limit
    if (table->record_count >= MAX_RECORDS_PER_TABLE) {
        LOG_ERROR("Maximum record limit reached for table '%s' (%d)", 
                  table->name, MAX_RECORDS_PER_TABLE);
        return ERROR_TABLE_FULL;
    }
    
    // Validate field count
    if (values[0].type == TYPE_UNKNOWN) {
        // values array size doesn't match field_count
        LOG_ERROR("Field count mismatch for table '%s'", table->name);
        return ERROR_INVALID_INPUT;
    }
    
    // Check constraints
    for (int i = 0; i < table->field_count; i++) {
        if (!table->constraints[i].nullable) {
            // Check for NULL values
            if (values[i].type == TYPE_NULL) {
                LOG_ERROR("Non-nullable field '%s' cannot be NULL", table->field_names[i]);
                return ERROR_CONSTRAINT_VIOLATION;
            }
        }
        
        if (table->constraints[i].unique || table->constraints[i].primary_key) {
            // Check for uniqueness
            // TODO: Implement uniqueness check using indexes
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
        LOG_ERROR("Duplicate record ID %d in table '%s'", record_id, table->name);
        return ERROR_DUPLICATE_KEY;
    }
    
    // Create new record
    Record* new_record = (Record*)malloc(sizeof(Record));
    if (!new_record) {
        LOG_ERROR("Failed to allocate record");
        return ERROR_MEMORY_ALLOCATION;
    }
    
    new_record->id = record_id;
    new_record->field_count = table->field_count;
    new_record->fields = (Field*)malloc(table->field_count * sizeof(Field));
    new_record->next = NULL;
    new_record->timestamp = time(NULL);
    new_record->version = 1;
    
    if (!new_record->fields) {
        LOG_ERROR("Failed to allocate record fields");
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
    
    // Insert at beginning of linked list (fastest)
    new_record->next = table->records;
    table->records = new_record;
    table->record_count++;
    
    // Update next_record_id
    if (record_id >= table->next_record_id) {
        table->next_record_id = record_id + 1;
    }
    
    // Update indexes
    ErrorCode index_result = update_indexes_for_record(table, new_record, -1);
    if (index_result != SUCCESS) {
        LOG_WARN("Failed to update indexes for record %d: %d", record_id, index_result);
        // Continue anyway - indexes are optional
    }
    
    // Update statistics
    if (table->statistics) {
        update_table_statistics(table->statistics, STAT_RECORDS_INSERTED, 1);
    }
    
    LOG_DEBUG("Record inserted with ID %d", record_id);
    
    if (out_id) {
        *out_id = record_id;
    }
    
    return SUCCESS;
}

ErrorCode db_insert_record(Database* db, const char* table_name, Field* values, int* out_id) {
    VALIDATE_PTR(db);
    VALIDATE_PTR(table_name);
    VALIDATE_PTR(values);
    
    LOG_DEBUG("Inserting record into table '%s'", table_name);
    
    Table* table = db_get_table(db, table_name);
    if (!table) {
        LOG_ERROR("Table '%s' not found", table_name);
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
    VALIDATE_PTR(db);
    VALIDATE_PTR(table_name);
    VALIDATE_PTR(values_array);
    VALIDATE_RANGE(count, 1, BATCH_INSERT_SIZE);
    
    LOG_DEBUG("Batch inserting %d records into table '%s'", count, table_name);
    
    Table* table = db_get_table(db, table_name);
    if (!table) {
        LOG_ERROR("Table '%s' not found", table_name);
        return ERROR_NOT_FOUND;
    }
    
    // Check capacity
    if (table->record_count + count > MAX_RECORDS_PER_TABLE) {
        LOG_ERROR("Insufficient space for %d records in table '%s'", count, table_name);
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
            LOG_WARN("Failed to insert record %d in batch: %d", i, result);
            final_result = result;
            break;
        }
    }
    
    if (successful_inserts == count) {
        db_commit_transaction(db);
        if (out_ids) *out_ids = ids;
        LOG_INFO("Batch insert completed: %d records inserted", count);
    } else {
        db_rollback_transaction(db);
        if (ids) free(ids);
        LOG_ERROR("Batch insert partially failed: %d/%d records inserted", 
                  successful_inserts, count);
    }
    
    return final_result;
}

Record* table_find_record(Table* table, int id) {
    VALIDATE_PTR(table);
    
    LOG_DEBUG("Finding record %d in table '%s'", id, table->name);
    
    // First try to use index
    if (table->index_manager && table->index_manager->count > 0) {
        // Look for primary key index
        for (int i = 0; i < table->index_manager->count; i++) {
            Index* index = table->index_manager->indexes[i];
            if (strcmp(index->field_name, "id") == 0 || 
                table->constraints[0].primary_key) {
                Record* record = index_search(index, id);
                if (record) {
                    LOG_DEBUG("Record %d found using index", id);
                    return record;
                }
                break;
            }
        }
    }
    
    // Fallback to linear search
    for (Record* curr = table->records; curr != NULL; curr = curr->next) {
        if (curr->id == id) {
            LOG_DEBUG("Record %d found via linear search", id);
            return curr;
        }
    }
    
    LOG_DEBUG("Record %d not found in table '%s'", id, table->name);
    return NULL;
}

Record* db_find_record(Database* db, const char* table_name, int id) {
    VALIDATE_PTR(db);
    VALIDATE_PTR(table_name);
    
    Table* table = db_get_table(db, table_name);
    if (!table) {
        LOG_ERROR("Table '%s' not found", table_name);
        return NULL;
    }
    
    return table_find_record(table, id);
}

Record* db_find_record_by_field(Database* db, const char* table_name, 
                                const char* field_name, const void* value) {
    VALIDATE_PTR(db);
    VALIDATE_PTR(table_name);
    VALIDATE_PTR(field_name);
    VALIDATE_PTR(value);
    
    LOG_DEBUG("Finding record in table '%s' where %s = ...", table_name, field_name);
    
    Table* table = db_get_table(db, table_name);
    if (!table) {
        LOG_ERROR("Table '%s' not found", table_name);
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
        LOG_ERROR("Field '%s' not found in table '%s'", field_name, table_name);
        return NULL;
    }
    
    // Try using index first
    Index* index = find_index(table->index_manager, table_name, field_name);
    if (index) {
        // Convert value to key based on field type
        int key = 0;
        FieldType field_type = table->field_types[field_idx];
        
        switch (field_type) {
            case TYPE_INT:
                key = *(int*)value;
                break;
            default:
                // For now, only support integer indexes
                break;
        }
        
        if (key != 0) {
            Record* record = index_search(index, key);
            if (record) {
                LOG_DEBUG("Found record using index on field '%s'", field_name);
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
            LOG_DEBUG("Found record via linear search on field '%s'", field_name);
            return curr;
        }
    }
    
    LOG_DEBUG("No record found with %s = ...", field_name);
    return NULL;
}

Record** db_find_all_records_by_field(Database* db, const char* table_name,
                                      const char* field_name, const void* value,
                                      int* out_count) {
    VALIDATE_PTR(db);
    VALIDATE_PTR(table_name);
    VALIDATE_PTR(field_name);
    VALIDATE_PTR(value);
    VALIDATE_PTR(out_count);
    
    LOG_DEBUG("Finding all records in table '%s' where %s = ...", table_name, field_name);
    
    *out_count = 0;
    
    Table* table = db_get_table(db, table_name);
    if (!table) {
        LOG_ERROR("Table '%s' not found", table_name);
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
        LOG_ERROR("Field '%s' not found in table '%s'", field_name, table_name);
        return NULL;
    }
    
    // Allocate result array (max size is table record count)
    Record** results = (Record**)malloc(table->record_count * sizeof(Record*));
    if (!results) {
        LOG_ERROR("Failed to allocate results array");
        return NULL;
    }
    
    int count = 0;
    FieldType field_type = table->field_types[field_idx];
    
    // Linear search (for now)
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
    LOG_DEBUG("Found %d matching records", count);
    
    return results;
}

ErrorCode table_update_record(Table* table, int id, Field* new_values) {
    VALIDATE_PTR(table);
    VALIDATE_PTR(new_values);
    
    LOG_DEBUG("Updating record %d in table '%s'", id, table->name);
    
    Record* record = table_find_record(table, id);
    if (!record) {
        LOG_ERROR("Record %d not found in table '%s'", id, table->name);
        return ERROR_NOT_FOUND;
    }
    
    // Store old field values for index update
    Field* old_values = NULL;
    if (table->index_manager && table->index_manager->count > 0) {
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
                    LOG_ERROR("Duplicate primary key %d", new_id);
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
        for (int i = 0; i < table->index_manager->count; i++) {
            Index* index = table->index_manager->indexes[i];
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
                        index_update(index, old_key, new_key, record);
                    }
                }
            }
        }
        free(old_values);
    }
    
    // Update statistics
    if (table->statistics) {
        update_table_statistics(table->statistics, STAT_RECORDS_UPDATED, 1);
    }
    
    LOG_INFO("Record %d updated successfully", id);
    return SUCCESS;
}

ErrorCode db_update_record(Database* db, const char* table_name, int id, Field* new_values) {
    VALIDATE_PTR(db);
    VALIDATE_PTR(table_name);
    VALIDATE_PTR(new_values);
    
    LOG_DEBUG("Updating record %d in table '%s'", id, table_name);
    
    Table* table = db_get_table(db, table_name);
    if (!table) {
        LOG_ERROR("Table '%s' not found", table_name);
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
    VALIDATE_PTR(table);
    
    LOG_DEBUG("Deleting record %d from table '%s'", id, table->name);
    
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
                update_table_statistics(table->statistics, STAT_RECORDS_DELETED, 1);
            }
            
            LOG_INFO("Record %d deleted successfully", id);
            return SUCCESS;
        }
        prev = curr;
        curr = curr->next;
    }
    
    LOG_ERROR("Record %d not found in table '%s'", id, table->name);
    return ERROR_NOT_FOUND;
}

ErrorCode db_delete_record(Database* db, const char* table_name, int id) {
    VALIDATE_PTR(db);
    VALIDATE_PTR(table_name);
    
    LOG_DEBUG("Deleting record %d from table '%s'", id, table_name);
    
    Table* table = db_get_table(db, table_name);
    if (!table) {
        LOG_ERROR("Table '%s' not found", table_name);
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
    VALIDATE_PTR(db);
    VALIDATE_PTR(table_name);
    VALIDATE_PTR(field_name);
    VALIDATE_PTR(value);
    
    LOG_DEBUG("Deleting records from table '%s' where %s = ...", table_name, field_name);
    
    if (out_count) *out_count = 0;
    
    Table* table = db_get_table(db, table_name);
    if (!table) {
        LOG_ERROR("Table '%s' not found", table_name);
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
        LOG_ERROR("Field '%s' not found in table '%s'", field_name, table_name);
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
            
            // Current pointer is now invalid, update prev stays the same
        } else {
            prev = curr;
        }
        
        curr = next;
    }
    
    if (deleted_count > 0) {
        db_commit_transaction(db);
        if (out_count) *out_count = deleted_count;
        LOG_INFO("Deleted %d records from table '%s'", deleted_count, table_name);
        return SUCCESS;
    } else {
        db_rollback_transaction(db);
        LOG_DEBUG("No records matched the deletion criteria");
        return SUCCESS;  // No error, just nothing deleted
    }
}

// ==================== QUERY OPERATIONS ====================

QueryResult* db_execute_query(Database* db, const char* query) {
    VALIDATE_PTR(db);
    VALIDATE_PTR(query);
    
    LOG_DEBUG("Executing query: %s", query);
    
    // Check cache first
    if (db->cache) {
        QueryResult* cached = cache_get(db->cache, query);
        if (cached) {
            LOG_DEBUG("Query result retrieved from cache");
            update_statistics(db->statistics, STAT_CACHE_HITS, 1);
            return query_result_clone(cached);
        }
        update_statistics(db->statistics, STAT_CACHE_MISSES, 1);
    }
    
    // Parse query
    ParsedQuery* parsed = parse_query(query);
    if (!parsed) {
        LOG_ERROR("Failed to parse query: %s", query);
        return NULL;
    }
    
    // Execute based on query type
    QueryResult* result = NULL;
    
    switch (parsed->type) {
        case QUERY_SELECT:
            result = execute_select_query(db, parsed);
            break;
        case QUERY_INSERT:
            result = execute_insert_query(db, parsed);
            break;
        case QUERY_UPDATE:
            result = execute_update_query(db, parsed);
            break;
        case QUERY_DELETE:
            result = execute_delete_query(db, parsed);
            break;
        case QUERY_CREATE_TABLE:
            result = execute_create_table_query(db, parsed);
            break;
        case QUERY_CREATE_INDEX:
            result = execute_create_index_query(db, parsed);
            break;
        default:
            LOG_ERROR("Unsupported query type: %d", parsed->type);
            break;
    }
    
    // Cache the result if it's a SELECT query
    if (result && db->cache && parsed->type == QUERY_SELECT) {
        cache_put(db->cache, query, query_result_clone(result));
    }
    
    // Cleanup
    free_parsed_query(parsed);
    
    update_statistics(db->statistics, STAT_QUERIES_EXECUTED, 1);
    
    return result;
}

// ==================== INDEX OPERATIONS ====================

ErrorCode db_create_index(Database* db, const char* table_name, 
                         const char* field_name, IndexType type) {
    VALIDATE_PTR(db);
    VALIDATE_PTR(table_name);
    VALIDATE_PTR(field_name);
    
    LOG_DEBUG("Creating %s index on %s.%s", 
              index_type_to_string(type), table_name, field_name);
    
    Table* table = db_get_table(db, table_name);
    if (!table) {
        LOG_ERROR("Table '%s' not found", table_name);
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
        LOG_ERROR("Field '%s' not found in table '%s'", field_name, table_name);
        return ERROR_NOT_FOUND;
    }
    
    // Check if index already exists
    if (find_index(table->index_manager, table_name, field_name) != NULL) {
        LOG_WARN("Index already exists on %s.%s", table_name, field_name);
        return ERROR_INDEX_EXISTS;
    }
    
    // Check index limit
    if (table->index_manager->count >= MAX_INDEXES_PER_TABLE) {
        LOG_ERROR("Maximum index limit reached for table '%s' (%d)", 
                  table_name, MAX_INDEXES_PER_TABLE);
        return ERROR_TABLE_FULL;
    }
    
    // Create index
    Index* index = NULL;
    switch (type) {
        case INDEX_HASH:
            index = create_hash_index(table_name, field_name);
            break;
        case INDEX_BTREE:
            index = create_btree_index(table_name, field_name, DEFAULT_BTREE_DEGREE);
            break;
        case INDEX_SKIPLIST:
            index = create_skiplist_index(table_name, field_name, DEFAULT_SKIPLIST_MAX_LEVEL);
            break;
        default:
            LOG_ERROR("Unsupported index type: %d", type);
            return ERROR_INVALID_INPUT;
    }
    
    if (!index) {
        LOG_ERROR("Failed to create index");
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
                // For non-integer fields, we need a different approach
                // For now, skip indexing non-integer fields
                continue;
        }
        
        if (key != 0) {
            ErrorCode result = index_insert(index, key, curr);
            if (result != SUCCESS) {
                LOG_WARN("Failed to index record %d: %d", curr->id, result);
            }
        }
    }
    
    // Add index to manager
    ErrorCode result = add_index(table->index_manager, index);
    if (result != SUCCESS) {
        LOG_ERROR("Failed to add index to manager: %d", result);
        index_free(index);
        return result;
    }
    
    LOG_INFO("%s index created successfully on %s.%s", 
             index_type_to_string(type), table_name, field_name);
    update_statistics(db->statistics, STAT_INDEXES_CREATED, 1);
    
    return SUCCESS;
}

ErrorCode db_drop_index(Database* db, const char* table_name, const char* field_name) {
    VALIDATE_PTR(db);
    VALIDATE_PTR(table_name);
    VALIDATE_PTR(field_name);
    
    LOG_DEBUG("Dropping index on %s.%s", table_name, field_name);
    
    Table* table = db_get_table(db, table_name);
    if (!table) {
        LOG_ERROR("Table '%s' not found", table_name);
        return ERROR_NOT_FOUND;
    }
    
    ErrorCode result = remove_index(table->index_manager, table_name, field_name);
    if (result == SUCCESS) {
        LOG_INFO("Index dropped successfully from %s.%s", table_name, field_name);
        update_statistics(db->statistics, STAT_INDEXES_DROPPED, 1);
    } else {
        LOG_ERROR("Index not found on %s.%s", table_name, field_name);
    }
    
    return result;
}

// ==================== TRANSACTION MANAGEMENT ====================

ErrorCode db_begin_transaction(Database* db) {
    VALIDATE_PTR(db);
    
    if (db->transaction_active && db->transaction_level >= MAX_TRANSACTION_LEVEL) {
        LOG_ERROR("Maximum transaction nesting level reached");
        return ERROR_TRANSACTION_CONFLICT;
    }
    
    if (!db->transaction_active) {
        // Initialize transaction
        db->transaction_active = 1;
        db->transaction_level = 1;
        db->transaction_start_time = time(NULL);
        
        // Create savepoint for rollback
        // TODO: Implement savepoint system
        
        LOG_DEBUG("Transaction started");
    } else {
        // Nested transaction (savepoint)
        db->transaction_level++;
        LOG_DEBUG("Nested transaction level %d", db->transaction_level);
    }
    
    update_statistics(db->statistics, STAT_TRANSACTIONS_STARTED, 1);
    return SUCCESS;
}

ErrorCode db_commit_transaction(Database* db) {
    VALIDATE_PTR(db);
    
    if (!db->transaction_active) {
        LOG_ERROR("No active transaction to commit");
        return ERROR_TRANSACTION_CONFLICT;
    }
    
    if (db->transaction_level > 1) {
        // Nested transaction - just decrement level
        db->transaction_level--;
        LOG_DEBUG("Nested transaction committed, level now %d", db->transaction_level);
        return SUCCESS;
    }
    
    // Commit the transaction
    db->transaction_active = 0;
    db->transaction_level = 0;
    
    // TODO: Implement actual commit logic
    // - Write to WAL
    // - Flush changes
    // - Release locks
    
    LOG_DEBUG("Transaction committed successfully");
    update_statistics(db->statistics, STAT_TRANSACTIONS_COMMITTED, 1);
    return SUCCESS;
}

ErrorCode db_rollback_transaction(Database* db) {
    VALIDATE_PTR(db);
    
    if (!db->transaction_active) {
        LOG_ERROR("No active transaction to rollback");
        return ERROR_TRANSACTION_CONFLICT;
    }
    
    // Rollback all changes
    // TODO: Implement actual rollback logic
    // - Restore from savepoints
    // - Release locks
    
    db->transaction_active = 0;
    db->transaction_level = 0;
    
    LOG_DEBUG("Transaction rolled back");
    update_statistics(db->statistics, STAT_TRANSACTIONS_ROLLED_BACK, 1);
    return SUCCESS;
}

// ==================== STATISTICS & MONITORING ====================

DatabaseStatistics* db_get_statistics(Database* db) {
    VALIDATE_PTR(db);
    return db->statistics;
}

TableStatistics* db_get_table_statistics(Database* db, const char* table_name) {
    VALIDATE_PTR(db);
    VALIDATE_PTR(table_name);
    
    Table* table = db_get_table(db, table_name);
    if (!table) {
        LOG_ERROR("Table '%s' not found", table_name);
        return NULL;
    }
    
    return table->statistics;
}

ErrorCode db_reset_statistics(Database* db) {
    VALIDATE_PTR(db);
    
    if (db->statistics) {
        reset_statistics(db->statistics);
    }
    
    for (int i = 0; i < db->table_count; i++) {
        if (db->tables[i].statistics) {
            reset_table_statistics(db->tables[i].statistics);
        }
    }
    
    LOG_DEBUG("Statistics reset");
    return SUCCESS;
}

// ==================== MAINTENANCE OPERATIONS ====================

ErrorCode db_vacuum(Database* db) {
    VALIDATE_PTR(db);
    
    LOG_INFO("Starting database vacuum operation");
    
    // Rebuild indexes
    for (int i = 0; i < db->table_count; i++) {
        Table* table = &db->tables[i];
        
        // Rebuild all indexes for this table
        if (table->index_manager) {
            for (int j = 0; j < table->index_manager->count; j++) {
                Index* index = table->index_manager->indexes[j];
                
                // Clear and rebuild index
                // TODO: Implement index rebuild
            }
        }
    }
    
    // Clear cache
    if (db->cache) {
        cache_clear(db->cache);
    }
    
    // Update statistics
    update_statistics(db->statistics, STAT_VACUUM_OPERATIONS, 1);
    
    LOG_INFO("Database vacuum completed");
    return SUCCESS;
}

ErrorCode db_optimize(Database* db) {
    VALIDATE_PTR(db);
    
    LOG_INFO("Starting database optimization");
    
    // TODO: Implement optimization
    // - Reorganize data
    // - Update query plans
    // - Compact storage
    
    update_statistics(db->statistics, STAT_OPTIMIZATION_OPERATIONS, 1);
    
    LOG_INFO("Database optimization completed");
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
    VALIDATE_PTR(db);
    VALIDATE_PTR(out_count);
    
    *out_count = 0;
    
    if (db->table_count == 0) {
        return NULL;
    }
    
    char** tables = (char**)malloc(db->table_count * sizeof(char*));
    if (!tables) {
        LOG_ERROR("Failed to allocate table list");
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
    
    LOG_DEBUG("Freeing table '%s'", table->name);
    
    // Free field names
    for (int i = 0; i < table->field_count; i++) {
        free(table->field_names[i]);
    }
    free(table->field_names);
    free(table->field_types);
    free(table->constraints);
    
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
        index_manager_free(table->index_manager);
    }
    
    // Free statistics
    if (table->statistics) {
        free_table_statistics(table->statistics);
    }
    
    LOG_DEBUG("Table '%s' freed", table->name);
}

void db_free(Database* db) {
    if (!db) return;
    
    LOG_INFO("Freeing database");
    
    // Free all tables
    for (int i = 0; i < db->table_count; i++) {
        table_free(&db->tables[i]);
    }
    free(db->tables);
    
    // Free index manager
    if (db->index_manager) {
        index_manager_free(db->index_manager);
    }
    
    // Free cache
    if (db->cache) {
        cache_free(db->cache);
    }
    
    // Free statistics
    if (db->statistics) {
        free_statistics(db->statistics);
    }
    
    free(db);
    LOG_INFO("Database freed successfully");
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
        
        printf("Indexes: %d\n", table->index_manager ? table->index_manager->count : 0);
        if (table->index_manager && table->index_manager->count > 0) {
            for (int j = 0; j < table->index_manager->count; j++) {
                printf("  - %s (%s)\n", table->index_manager->indexes[j]->field_name,
                       index_type_to_string(table->index_manager->indexes[j]->type));
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