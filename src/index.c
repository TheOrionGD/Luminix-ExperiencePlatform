#include "index.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <assert.h>

// ==================== HASH INDEX IMPLEMENTATION ====================

#define HASH_PRIME 31
#define INITIAL_HASH_SIZE 101

// Simple hash function
static unsigned int hash_function(int key, int capacity) {
    return ((unsigned int)key * HASH_PRIME) % capacity;
}

// Create a new hash index
HashIndex* hash_index_create(int capacity) {
    HashIndex* hash_index = (HashIndex*)malloc(sizeof(HashIndex));
    if (!hash_index) return NULL;
    
    hash_index->capacity = capacity > 0 ? capacity : INITIAL_HASH_SIZE;
    hash_index->size = 0;
    hash_index->buckets = (HashNode**)calloc(hash_index->capacity, sizeof(HashNode*));
    
    if (!hash_index->buckets) {
        free(hash_index);
        return NULL;
    }
    
    return hash_index;
}

// Hash index insert
static int hash_index_insert(HashIndex* hash_index, int key, Record* record) {
    if (!hash_index || !record) return INVALID_INPUT;
    
    // Check if key already exists
    unsigned int index = hash_function(key, hash_index->capacity);
    HashNode* current = hash_index->buckets[index];
    
    while (current) {
        if (current->key == key) {
            // Update existing record
            current->record = record;
            return SUCCESS;
        }
        current = current->next;
    }
    
    // Create new node
    HashNode* new_node = (HashNode*)malloc(sizeof(HashNode));
    if (!new_node) return ERROR;
    
    new_node->key = key;
    new_node->record = record;
    new_node->next = hash_index->buckets[index];
    hash_index->buckets[index] = new_node;
    hash_index->size++;
    
    // Resize if load factor > 0.75
    if ((float)hash_index->size / hash_index->capacity > LOAD_FACTOR) {
        // Double the capacity
        int new_capacity = hash_index->capacity * 2;
        HashNode** new_buckets = (HashNode**)calloc(new_capacity, sizeof(HashNode*));
        if (!new_buckets) return ERROR;
        
        // Rehash all elements
        for (int i = 0; i < hash_index->capacity; i++) {
            HashNode* node = hash_index->buckets[i];
            while (node) {
                HashNode* next = node->next;
                unsigned int new_index = hash_function(node->key, new_capacity);
                node->next = new_buckets[new_index];
                new_buckets[new_index] = node;
                node = next;
            }
        }
        
        free(hash_index->buckets);
        hash_index->buckets = new_buckets;
        hash_index->capacity = new_capacity;
    }
    
    return SUCCESS;
}

// Hash index search
static Record* hash_index_search(HashIndex* hash_index, int key) {
    if (!hash_index) return NULL;
    
    unsigned int index = hash_function(key, hash_index->capacity);
    HashNode* current = hash_index->buckets[index];
    
    while (current) {
        if (current->key == key) {
            return current->record;
        }
        current = current->next;
    }
    
    return NULL;
}

// Hash index delete
static int hash_index_delete(HashIndex* hash_index, int key) {
    if (!hash_index) return INVALID_INPUT;
    
    unsigned int index = hash_function(key, hash_index->capacity);
    HashNode* current = hash_index->buckets[index];
    HashNode* prev = NULL;
    
    while (current) {
        if (current->key == key) {
            if (prev) {
                prev->next = current->next;
            } else {
                hash_index->buckets[index] = current->next;
            }
            
            free(current);
            hash_index->size--;
            return SUCCESS;
        }
        prev = current;
        current = current->next;
    }
    
    return NOT_FOUND;
}

// Free hash index
static void hash_index_free(HashIndex* hash_index) {
    if (!hash_index) return;
    
    for (int i = 0; i < hash_index->capacity; i++) {
        HashNode* node = hash_index->buckets[i];
        while (node) {
            HashNode* next = node->next;
            free(node);
            node = next;
        }
    }
    
    free(hash_index->buckets);
    free(hash_index);
}

// ==================== B-TREE IMPLEMENTATION ====================

// Create a new B-Tree node
static BTreeNode* btree_create_node(int t, bool leaf) {
    BTreeNode* node = (BTreeNode*)malloc(sizeof(BTreeNode));
    if (!node) return NULL;
    
    node->t = t;
    node->n = 0;
    node->leaf = leaf;
    node->keys = (int*)malloc((2 * t - 1) * sizeof(int));
    node->records = (Record**)malloc((2 * t - 1) * sizeof(Record*));
    node->children = (BTreeNode**)malloc((2 * t) * sizeof(BTreeNode*));
    
    if (!node->keys || !node->records || !node->children) {
        free(node->keys);
        free(node->records);
        free(node->children);
        free(node);
        return NULL;
    }
    
    for (int i = 0; i < (2 * t); i++) {
        node->children[i] = NULL;
    }
    
    return node;
}

// B-Tree search
static Record* btree_search(BTreeNode* node, int key) {
    int i = 0;
    while (i < node->n && key > node->keys[i]) {
        i++;
    }
    
    if (i < node->n && key == node->keys[i]) {
        return node->records[i];
    }
    
    if (node->leaf) {
        return NULL;
    }
    
    return btree_search(node->children[i], key);
}

// B-Tree insert (non-full node)
static void btree_insert_non_full(BTreeNode* node, int key, Record* record) {
    int i = node->n - 1;
    
    if (node->leaf) {
        // Find location and shift keys
        while (i >= 0 && key < node->keys[i]) {
            node->keys[i + 1] = node->keys[i];
            node->records[i + 1] = node->records[i];
            i--;
        }
        
        node->keys[i + 1] = key;
        node->records[i + 1] = record;
        node->n++;
    } else {
        // Find child to insert into
        while (i >= 0 && key < node->keys[i]) {
            i--;
        }
        i++;
        
        // If child is full, split it
        if (node->children[i]->n == (2 * node->t - 1)) {
            // Split child
            BTreeNode* z = btree_create_node(node->t, node->children[i]->leaf);
            BTreeNode* y = node->children[i];
            
            // Move middle key to parent
            for (int j = node->n; j > i; j--) {
                node->children[j + 1] = node->children[j];
            }
            node->children[i + 1] = z;
            
            for (int j = node->n - 1; j >= i; j--) {
                node->keys[j + 1] = node->keys[j];
                node->records[j + 1] = node->records[j];
            }
            
            node->keys[i] = y->keys[node->t - 1];
            node->records[i] = y->records[node->t - 1];
            node->n++;
            
            // Copy keys to new node
            for (int j = 0; j < node->t - 1; j++) {
                z->keys[j] = y->keys[j + node->t];
                z->records[j] = y->records[j + node->t];
            }
            
            if (!y->leaf) {
                for (int j = 0; j < node->t; j++) {
                    z->children[j] = y->children[j + node->t];
                }
            }
            
            y->n = node->t - 1;
            z->n = node->t - 1;
            
            // Determine which child to insert into
            if (key > node->keys[i]) {
                i++;
            }
        }
        
        btree_insert_non_full(node->children[i], key, record);
    }
}

// B-Tree insert
static int btree_insert(BTreeIndex* btree, int key, Record* record) {
    if (!btree || !record) return INVALID_INPUT;
    
    // Check if key already exists
    if (btree_search(btree->root, key) != NULL) {
        return DUPLICATE_KEY;
    }
    
    BTreeNode* root = btree->root;
    
    if (root->n == (2 * btree->t - 1)) {
        // Root is full, need to split
        BTreeNode* s = btree_create_node(btree->t, false);
        if (!s) return ERROR;
        
        s->children[0] = root;
        btree->root = s;
        
        // Split the old root
        BTreeNode* z = btree_create_node(btree->t, root->leaf);
        if (!z) {
            free(s);
            return ERROR;
        }
        
        // Move middle key to new root
        for (int i = 0; i < btree->t - 1; i++) {
            z->keys[i] = root->keys[i + btree->t];
            z->records[i] = root->records[i + btree->t];
        }
        
        if (!root->leaf) {
            for (int i = 0; i < btree->t; i++) {
                z->children[i] = root->children[i + btree->t];
            }
        }
        
        s->keys[0] = root->keys[btree->t - 1];
        s->records[0] = root->records[btree->t - 1];
        s->children[1] = z;
        s->n = 1;
        root->n = btree->t - 1;
        z->n = btree->t - 1;
        
        // Insert into appropriate child
        if (key < s->keys[0]) {
            btree_insert_non_full(root, key, record);
        } else {
            btree_insert_non_full(z, key, record);
        }
    } else {
        btree_insert_non_full(root, key, record);
    }
    
    btree->size++;
    return SUCCESS;
}

// B-Tree delete
static int btree_delete(BTreeIndex* btree, int key) {
    // Simplified delete - in production, implement full B-tree delete algorithm
    // For now, we'll use a simpler approach for demo
    
    if (!btree) return INVALID_INPUT;
    
    // Search for the key
    BTreeNode* node = btree->root;
    int idx = -1;
    
    // This is a simplified version - full B-tree delete is complex
    // We'll implement a placeholder
    printf("B-Tree delete not fully implemented yet\n");
    return ERROR;
}

// Free B-Tree
static void btree_free(BTreeNode* node) {
    if (!node) return;
    
    if (!node->leaf) {
        for (int i = 0; i <= node->n; i++) {
            btree_free(node->children[i]);
        }
    }
    
    free(node->keys);
    free(node->records);
    free(node->children);
    free(node);
}

static void btree_index_free(BTreeIndex* btree) {
    if (!btree) return;
    btree_free(btree->root);
    free(btree);
}

// ==================== SKIP LIST IMPLEMENTATION ====================

// Random level generator for skip list
static int random_level(int max_level) {
    int level = 1;
    while ((rand() % 2) == 0 && level < max_level) {
        level++;
    }
    return level;
}

// Create skip list node
static SkipListNode* skiplist_create_node(int level, int key, Record* record) {
    SkipListNode* node = (SkipListNode*)malloc(sizeof(SkipListNode));
    if (!node) return NULL;
    
    node->key = key;
    node->record = record;
    node->level = level;
    node->forward = (SkipListNode**)malloc((level + 1) * sizeof(SkipListNode*));
    
    if (!node->forward) {
        free(node);
        return NULL;
    }
    
    for (int i = 0; i <= level; i++) {
        node->forward[i] = NULL;
    }
    
    return node;
}

// Skip list insert
static int skiplist_insert(SkipListIndex* skiplist, int key, Record* record) {
    if (!skiplist || !record) return INVALID_INPUT;
    
    SkipListNode* update[skiplist->max_level + 1];
    SkipListNode* current = skiplist->header;
    
    // Find update positions
    for (int i = skiplist->level; i >= 0; i--) {
        while (current->forward[i] != NULL && current->forward[i]->key < key) {
            current = current->forward[i];
        }
        update[i] = current;
    }
    
    current = current->forward[0];
    
    // Check if key already exists
    if (current != NULL && current->key == key) {
        // Update existing record
        current->record = record;
        return SUCCESS;
    }
    
    // Generate random level for new node
    int new_level = random_level(skiplist->max_level);
    
    // If new node has higher level, update header pointers
    if (new_level > skiplist->level) {
        for (int i = skiplist->level + 1; i <= new_level; i++) {
            update[i] = skiplist->header;
        }
        skiplist->level = new_level;
    }
    
    // Create new node
    SkipListNode* new_node = skiplist_create_node(new_level, key, record);
    if (!new_node) return ERROR;
    
    // Update forward pointers
    for (int i = 0; i <= new_level; i++) {
        new_node->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = new_node;
    }
    
    skiplist->size++;
    return SUCCESS;
}

// Skip list search
static Record* skiplist_search(SkipListIndex* skiplist, int key) {
    if (!skiplist) return NULL;
    
    SkipListNode* current = skiplist->header;
    
    for (int i = skiplist->level; i >= 0; i--) {
        while (current->forward[i] != NULL && current->forward[i]->key < key) {
            current = current->forward[i];
        }
    }
    
    current = current->forward[0];
    
    if (current != NULL && current->key == key) {
        return current->record;
    }
    
    return NULL;
}

// Skip list delete
static int skiplist_delete(SkipListIndex* skiplist, int key) {
    if (!skiplist) return INVALID_INPUT;
    
    SkipListNode* update[skiplist->max_level + 1];
    SkipListNode* current = skiplist->header;
    
    // Find update positions
    for (int i = skiplist->level; i >= 0; i--) {
        while (current->forward[i] != NULL && current->forward[i]->key < key) {
            current = current->forward[i];
        }
        update[i] = current;
    }
    
    current = current->forward[0];
    
    if (current == NULL || current->key != key) {
        return NOT_FOUND;
    }
    
    // Update forward pointers
    for (int i = 0; i <= skiplist->level; i++) {
        if (update[i]->forward[i] != current) {
            break;
        }
        update[i]->forward[i] = current->forward[i];
    }
    
    // Update skiplist level if needed
    while (skiplist->level > 0 && skiplist->header->forward[skiplist->level] == NULL) {
        skiplist->level--;
    }
    
    free(current->forward);
    free(current);
    skiplist->size--;
    
    return SUCCESS;
}

// Free skip list
static void skiplist_free(SkipListIndex* skiplist) {
    if (!skiplist) return;
    
    SkipListNode* current = skiplist->header;
    while (current != NULL) {
        SkipListNode* next = current->forward[0];
        free(current->forward);
        free(current);
        current = next;
    }
    
    free(skiplist);
}

// ==================== INDEX WRAPPER FUNCTIONS ====================

// Create hash index wrapper
Index* create_hash_index(const char* table_name, const char* field_name) {
    Index* index = (Index*)malloc(sizeof(Index));
    if (!index) return NULL;
    
    strncpy(index->table_name, table_name, MAX_TABLE_NAME);
    strncpy(index->field_name, field_name, MAX_FIELD_LEN);
    index->type = INDEX_HASH;
    
    HashIndex* hash_index = hash_index_create(INITIAL_HASH_SIZE);
    if (!hash_index) {
        free(index);
        return NULL;
    }
    
    index->impl.hash = hash_index;
    return index;
}

// Create B-Tree index wrapper
Index* create_btree_index(const char* table_name, const char* field_name, int degree) {
    Index* index = (Index*)malloc(sizeof(Index));
    if (!index) return NULL;
    
    strncpy(index->table_name, table_name, MAX_TABLE_NAME);
    strncpy(index->field_name, field_name, MAX_FIELD_LEN);
    index->type = INDEX_BTREE;
    
    BTreeIndex* btree = (BTreeIndex*)malloc(sizeof(BTreeIndex));
    if (!btree) {
        free(index);
        return NULL;
    }
    
    btree->t = degree > 2 ? degree : 3;  // Minimum degree of 3
    btree->root = btree_create_node(btree->t, true);
    btree->size = 0;
    
    if (!btree->root) {
        free(btree);
        free(index);
        return NULL;
    }
    
    index->impl.btree = btree;
    return index;
}

// Create skip list index wrapper
Index* create_skiplist_index(const char* table_name, const char* field_name, int max_level) {
    Index* index = (Index*)malloc(sizeof(Index));
    if (!index) return NULL;
    
    strncpy(index->table_name, table_name, MAX_TABLE_NAME);
    strncpy(index->field_name, field_name, MAX_FIELD_LEN);
    index->type = INDEX_SKIPLIST;
    
    SkipListIndex* skiplist = (SkipListIndex*)malloc(sizeof(SkipListIndex));
    if (!skiplist) {
        free(index);
        return NULL;
    }
    
    skiplist->max_level = max_level > 0 ? max_level : 16;
    skiplist->level = 0;
    skiplist->size = 0;
    
    // Create header node
    skiplist->header = skiplist_create_node(skiplist->max_level, -1, NULL);
    if (!skiplist->header) {
        free(skiplist);
        free(index);
        return NULL;
    }
    
    index->impl.skiplist = skiplist;
    return index;
}

// Generic index insert
int index_insert(Index* index, int key, Record* record) {
    if (!index || !record) return INVALID_INPUT;
    
    switch (index->type) {
        case INDEX_HASH:
            return hash_index_insert(index->impl.hash, key, record);
        case INDEX_BTREE:
            return btree_insert(index->impl.btree, key, record);
        case INDEX_SKIPLIST:
            return skiplist_insert(index->impl.skiplist, key, record);
        default:
            return ERROR;
    }
}

// Generic index search
Record* index_search(Index* index, int key) {
    if (!index) return NULL;
    
    switch (index->type) {
        case INDEX_HASH:
            return hash_index_search(index->impl.hash, key);
        case INDEX_BTREE:
            return btree_search(index->impl.btree->root, key);
        case INDEX_SKIPLIST:
            return skiplist_search(index->impl.skiplist, key);
        default:
            return NULL;
    }
}

// Generic index delete
int index_delete(Index* index, int key) {
    if (!index) return INVALID_INPUT;
    
    switch (index->type) {
        case INDEX_HASH:
            return hash_index_delete(index->impl.hash, key);
        case INDEX_BTREE:
            return btree_delete(index->impl.btree, key);
        case INDEX_SKIPLIST:
            return skiplist_delete(index->impl.skiplist, key);
        default:
            return ERROR;
    }
}

// Update index key
int index_update(Index* index, int old_key, int new_key, Record* record) {
    if (!index || !record) return INVALID_INPUT;
    
    // Delete old entry and insert new one
    int result = index_delete(index, old_key);
    if (result != SUCCESS && result != NOT_FOUND) {
        return result;
    }
    
    return index_insert(index, new_key, record);
}

// Free index
void index_free(Index* index) {
    if (!index) return;
    
    switch (index->type) {
        case INDEX_HASH:
            hash_index_free(index->impl.hash);
            break;
        case INDEX_BTREE:
            btree_index_free(index->impl.btree);
            break;
        case INDEX_SKIPLIST:
            skiplist_free(index->impl.skiplist);
            break;
    }
    
    free(index);
}

// Print index statistics
void index_print(Index* index) {
    if (!index) return;
    
    printf("\n=== Index Information ===\n");
    printf("Table: %s\n", index->table_name);
    printf("Field: %s\n", index->field_name);
    printf("Type: ");
    
    switch (index->type) {
        case INDEX_HASH:
            printf("Hash Table\n");
            printf("Size: %d\n", index->impl.hash->size);
            printf("Capacity: %d\n", index->impl.hash->capacity);
            printf("Load Factor: %.2f\n", 
                   (float)index->impl.hash->size / index->impl.hash->capacity);
            break;
        case INDEX_BTREE:
            printf("B-Tree\n");
            printf("Size: %d\n", index->impl.btree->size);
            printf("Degree: %d\n", index->impl.btree->t);
            break;
        case INDEX_SKIPLIST:
            printf("Skip List\n");
            printf("Size: %d\n", index->impl.skiplist->size);
            printf("Level: %d/%d\n", 
                   index->impl.skiplist->level, 
                   index->impl.skiplist->max_level);
            break;
    }
    printf("=======================\n");
}

// ==================== INDEX MANAGER ====================

// Create index manager
IndexManager* create_index_manager() {
    IndexManager* manager = (IndexManager*)malloc(sizeof(IndexManager));
    if (!manager) return NULL;
    
    manager->capacity = 10;
    manager->count = 0;
    manager->indexes = (Index**)malloc(manager->capacity * sizeof(Index*));
    
    if (!manager->indexes) {
        free(manager);
        return NULL;
    }
    
    return manager;
}

// Add index to manager
int add_index(IndexManager* manager, Index* index) {
    if (!manager || !index) return INVALID_INPUT;
    
    // Check if index already exists
    for (int i = 0; i < manager->count; i++) {
        if (strcmp(manager->indexes[i]->table_name, index->table_name) == 0 &&
            strcmp(manager->indexes[i]->field_name, index->field_name) == 0) {
            return DUPLICATE_KEY;
        }
    }
    
    // Resize if needed
    if (manager->count >= manager->capacity) {
        int new_capacity = manager->capacity * 2;
        Index** new_indexes = (Index**)realloc(manager->indexes, new_capacity * sizeof(Index*));
        if (!new_indexes) return ERROR;
        
        manager->indexes = new_indexes;
        manager->capacity = new_capacity;
    }
    
    manager->indexes[manager->count] = index;
    manager->count++;
    return SUCCESS;
}

// Find index
Index* find_index(IndexManager* manager, const char* table_name, const char* field_name) {
    if (!manager || !table_name || !field_name) return NULL;
    
    for (int i = 0; i < manager->count; i++) {
        if (strcmp(manager->indexes[i]->table_name, table_name) == 0 &&
            strcmp(manager->indexes[i]->field_name, field_name) == 0) {
            return manager->indexes[i];
        }
    }
    
    return NULL;
}

// Remove index
int remove_index(IndexManager* manager, const char* table_name, const char* field_name) {
    if (!manager || !table_name || !field_name) return INVALID_INPUT;
    
    for (int i = 0; i < manager->count; i++) {
        if (strcmp(manager->indexes[i]->table_name, table_name) == 0 &&
            strcmp(manager->indexes[i]->field_name, field_name) == 0) {
            
            // Free the index
            index_free(manager->indexes[i]);
            
            // Shift remaining indexes
            for (int j = i; j < manager->count - 1; j++) {
                manager->indexes[j] = manager->indexes[j + 1];
            }
            
            manager->count--;
            return SUCCESS;
        }
    }
    
    return NOT_FOUND;
}

// Rebuild all indexes from database
void rebuild_indexes(IndexManager* manager, Database* db) {
    if (!manager || !db) return;
    
    // Clear all existing indexes
    for (int i = 0; i < manager->count; i++) {
        index_free(manager->indexes[i]);
    }
    manager->count = 0;
    
    // For each index, rebuild from scratch
    // In a real implementation, you'd store index definitions and rebuild them
    // For now, we'll just clear them
}

// Free index manager
void index_manager_free(IndexManager* manager) {
    if (!manager) return;
    
    for (int i = 0; i < manager->count; i++) {
        index_free(manager->indexes[i]);
    }
    
    free(manager->indexes);
    free(manager);
}

// ==================== DATABASE INDEXING API ====================

// Global index manager (in production, this would be part of Database struct)
static IndexManager* global_index_manager = NULL;

// Initialize indexing system
static void init_indexing() {
    if (!global_index_manager) {
        srand(time(NULL));  // For skip list random levels
        global_index_manager = create_index_manager();
    }
}

// Create index on database field
int db_create_index(Database* db, const char* table_name, const char* field_name, IndexType type) {
    init_indexing();
    
    Table* table = db_get_table(db, table_name);
    if (!table) return NOT_FOUND;
    
    // Check if field exists
    int field_idx = -1;
    for (int i = 0; i < table->field_count; i++) {
        if (strcmp(table->field_names[i], field_name) == 0) {
            field_idx = i;
            break;
        }
    }
    
    if (field_idx == -1) return NOT_FOUND;
    
    // Create index
    Index* index = NULL;
    switch (type) {
        case INDEX_HASH:
            index = create_hash_index(table_name, field_name);
            break;
        case INDEX_BTREE:
            index = create_btree_index(table_name, field_name, 3);
            break;
        case INDEX_SKIPLIST:
            index = create_skiplist_index(table_name, field_name, 16);
            break;
        default:
            return ERROR;
    }
    
    if (!index) return ERROR;
    
    // Populate index with existing data
    for (Record* curr = table->records; curr != NULL; curr = curr->next) {
        int key = curr->id;  // For now, only support indexing by ID
        
        if (field_idx >= 0 && field_idx < curr->field_count) {
            // Get the actual field value for indexing
            switch (curr->fields[field_idx].type) {
                case TYPE_INT:
                    key = curr->fields[field_idx].value.int_value;
                    break;
                // For other types, we'd need to handle them differently
                // This is simplified - only works for integer fields
            }
        }
        
        index_insert(index, key, curr);
    }
    
    // Add to manager
    return add_index(global_index_manager, index);
}

// Find using index
Record* db_find_using_index(Database* db, const char* table_name, const char* field_name, int key) {
    init_indexing();
    
    Index* index = find_index(global_index_manager, table_name, field_name);
    if (!index) {
        // Fall back to linear search
        Table* table = db_get_table(db, table_name);
        if (!table) return NULL;
        
        // Find field index
        int field_idx = -1;
        for (int i = 0; i < table->field_count; i++) {
            if (strcmp(table->field_names[i], field_name) == 0) {
                field_idx = i;
                break;
            }
        }
        
        if (field_idx == -1) return NULL;
        
        // Linear search
        for (Record* curr = table->records; curr != NULL; curr = curr->next) {
            if (curr->fields[field_idx].type == TYPE_INT &&
                curr->fields[field_idx].value.int_value == key) {
                return curr;
            }
        }
        
        return NULL;
    }
    
    return index_search(index, key);
}

// Rebuild all indexes
int db_rebuild_indexes(Database* db) {
    init_indexing();
    
    if (!db) return INVALID_INPUT;
    
    // This would properly rebuild all indexes
    // For now, just clear and notify
    printf("Index rebuild functionality to be fully implemented.\n");
    return SUCCESS;
}

// Get index manager (for external use)
IndexManager* db_get_index_manager() {
    init_indexing();
    return global_index_manager;
}

// Cleanup indexing system
void db_indexing_cleanup() {
    if (global_index_manager) {
        index_manager_free(global_index_manager);
        global_index_manager = NULL;
    }
}