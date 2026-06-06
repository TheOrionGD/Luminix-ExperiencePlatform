#include "types.h"
#include "database.h"
#include "index.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Lifecycle
IndexManager* index_manager_create(void) {
    IndexManager* manager = malloc(sizeof(IndexManager));
    if (!manager) return NULL;
    manager->count = 0;
    manager->capacity = 16;
    manager->hash_table = malloc(sizeof(IndexEntry*) * manager->capacity);
    manager->entries = NULL;
    manager->recent_capacity = 16;
    manager->recent_count = 0;
    manager->recent_indexes = malloc(sizeof(Index*) * manager->recent_capacity);
    manager->index_capacity = 16;
    manager->index_count = 0;
    manager->indexes = malloc(sizeof(Index*) * manager->index_capacity);
    manager->max_indexes = 100;
    manager->auto_maintenance = true;
    manager->maintenance_interval = 3600;
    mutex_init(&manager->lock);
    return manager;
}

IndexManager* index_manager_create_ex(int max_indexes) {
    IndexManager* manager = index_manager_create();
    if (manager) manager->max_indexes = max_indexes;
    return manager;
}

void index_manager_free(IndexManager* manager) {
    if (!manager) return;
    for (int i = 0; i < manager->index_count; i++) {
        if (manager->indexes[i]) {
            index_free(manager->indexes[i]);
        }
    }
    free(manager->indexes);
    if (manager->hash_table) free(manager->hash_table);
    if (manager->recent_indexes) free(manager->recent_indexes);
    mutex_destroy(&manager->lock);
    free(manager);
}

// Index Management
ErrorCode index_manager_add_index(IndexManager* manager, Index* index) {
    if (!manager || !index) return ERROR_INVALID_PARAMETER;
    if (manager->index_count >= manager->index_capacity) {
        int new_capacity = manager->index_capacity == 0 ? 4 : manager->index_capacity * 2;
        Index** new_indexes = realloc(manager->indexes, new_capacity * sizeof(Index*));
        if (!new_indexes) return ERROR_MEMORY_ALLOCATION;
        manager->indexes = new_indexes;
        manager->index_capacity = new_capacity;
    }
    manager->indexes[manager->index_count++] = index;
    return SUCCESS;
}

ErrorCode index_manager_remove_index(IndexManager* manager, const char* table_name, const char* field_name) {
    if (!manager || !table_name || !field_name) return ERROR_INVALID_PARAMETER;
    for (int i = 0; i < manager->index_count; i++) {
        if (manager->indexes[i] && 
            strcmp(manager->indexes[i]->table_name, table_name) == 0 &&
            strcmp(manager->indexes[i]->field_name, field_name) == 0) {
            index_free(manager->indexes[i]);
            for (int j = i; j < manager->index_count - 1; j++) {
                manager->indexes[j] = manager->indexes[j + 1];
            }
            manager->index_count--;
            return SUCCESS;
        }
    }
    return ERROR_NOT_FOUND;
}

Index* index_manager_get_index(IndexManager* manager, const char* table_name, const char* field_name) {
    if (!manager || !table_name || !field_name) return NULL;
    for (int i = 0; i < manager->index_count; i++) {
        if (manager->indexes[i] && 
            strcmp(manager->indexes[i]->table_name, table_name) == 0 &&
            strcmp(manager->indexes[i]->field_name, field_name) == 0) {
            return manager->indexes[i];
        }
    }
    return NULL;
}

int index_manager_get_count(IndexManager* manager) {
    return manager ? manager->index_count : 0;
}

// Basic Index Lifecycle
Index* index_create(const char* table_name, const char* field_name, IndexType type, IndexConfig* config) {
    Index* index = malloc(sizeof(Index));
    if (!index) return NULL;
    strncpy(index->table_name, table_name, sizeof(index->table_name) - 1);
    index->table_name[sizeof(index->table_name) - 1] = '\0';
    strncpy(index->field_name, field_name, sizeof(index->field_name) - 1);
    index->field_name[sizeof(index->field_name) - 1] = '\0';
    index->type = type;
    index->data = NULL;
    index->size = 0;
    return index;
}

void index_free(Index* index) {
    if (index) free(index);
}

ErrorCode index_insert(Index* index, int key, Record* record) {
    if (!index) return ERROR_INVALID_PARAMETER;
    index->size++;
    return SUCCESS;
}

Record* index_search(Index* index, int key) {
    return NULL;
}

ErrorCode index_delete(Index* index, int key) {
    if (!index) return ERROR_INVALID_PARAMETER;
    if (index->size > 0) index->size--;
    return SUCCESS;
}