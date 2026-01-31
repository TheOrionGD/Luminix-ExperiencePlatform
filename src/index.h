#ifndef INDEX_H
#define INDEX_H

#include "database.h"
#include <stdbool.h>

// ===============================
// Hash Index for Fast Lookups
// ===============================

typedef struct HashNode {
    int key;                // Record ID
    Record* record;         // Pointer to record
    struct HashNode* next;  // For chaining
} HashNode;

typedef struct HashIndex {
    HashNode** buckets;     // Array of buckets
    int capacity;           // Number of buckets
    int size;               // Number of elements
    float load_factor;      // When to resize
} HashIndex;

// ===============================
// B-Tree Index for Range Queries
// ===============================

#define BTREE_MIN_DEGREE 2
#define BTREE_MAX_KEYS (2*BTREE_MIN_DEGREE - 1)
#define BTREE_MAX_CHILDREN (2*BTREE_MIN_DEGREE)

typedef struct BTreeNode {
    bool leaf;                     // Is this a leaf node?
    int key_count;                 // Number of keys
    int keys[BTREE_MAX_KEYS];      // Array of keys (record IDs)
    Record* records[BTREE_MAX_KEYS]; // Array of record pointers
    struct BTreeNode* children[BTREE_MAX_CHILDREN]; // Child pointers
} BTreeNode;

typedef struct BTreeIndex {
    BTreeNode* root;               // Root of B-Tree
    int size;                      // Number of elements
} BTreeIndex;

// ===============================
// Field-Based Index (Secondary Index)
// ===============================

typedef struct FieldIndexEntry {
    char* value;                   // Field value as string
    Record* record;                // Pointer to record
    struct FieldIndexEntry* next;  // Next entry for same value
} FieldIndexEntry;

typedef struct FieldIndexNode {
    char* key;                     // Field value
    FieldIndexEntry* entries;      // Linked list of records
    struct FieldIndexNode* left;   // Left child (for BST)
    struct FieldIndexEntry* right; // Right child (for BST)
} FieldIndexNode;

typedef struct FieldIndex {
    char field_name[MAX_FIELD_LEN]; // Name of indexed field
    FieldType field_type;           // Type of indexed field
    FieldIndexNode* root;           // Root of BST
    int size;                       // Number of unique values
} FieldIndex;

// ===============================
// Composite Index
// ===============================

typedef struct CompositeKey {
    char** values;                 // Array of field values
    int count;                     // Number of fields
} CompositeKey;

typedef struct CompositeIndexEntry {
    CompositeKey key;              // Composite key
    Record* record;                // Pointer to record
    struct CompositeIndexEntry* next;
} CompositeIndexEntry;

typedef struct CompositeIndex {
    char** field_names;            // Names of indexed fields
    int field_count;               // Number of fields in index
    CompositeIndexEntry** buckets; // Hash table buckets
    int capacity;                  // Hash table capacity
    int size;                      // Number of entries
} CompositeIndex;

// ===============================
// Table Index Manager
// ===============================

typedef struct TableIndex {
    HashIndex* primary_index;      // Primary key index (ID)
    FieldIndex** secondary_indices;// Array of secondary indices
    int secondary_count;           // Number of secondary indices
    BTreeIndex* btree_index;       // B-Tree for range queries
    CompositeIndex** composite_indices; // Composite indices
    int composite_count;
    char table_name[MAX_TABLE_NAME];
} TableIndex;

// ===============================
// Index Manager
// ===============================

typedef struct IndexManager {
    TableIndex** table_indices;    // Array of table indices
    int table_count;               // Number of tables with indices
    int capacity;                  // Array capacity
} IndexManager;

// ===============================
// Hash Index Functions
// ===============================

HashIndex* hash_index_create(int initial_capacity);
void hash_index_free(HashIndex* index);
int hash_index_insert(HashIndex* index, int key, Record* record);
Record* hash_index_search(HashIndex* index, int key);
int hash_index_delete(HashIndex* index, int key);
void hash_index_resize(HashIndex* index, int new_capacity);
int hash_function(int key, int capacity);
void hash_index_print(HashIndex* index);

// ===============================
// B-Tree Index Functions
// ===============================

BTreeIndex* btree_index_create();
void btree_index_free(BTreeIndex* index);
void btree_free_node(BTreeNode* node);
int btree_index_insert(BTreeIndex* index, int key, Record* record);
Record* btree_index_search(BTreeIndex* index, int key);
int btree_index_delete(BTreeIndex* index, int key);
void btree_index_range_search(BTreeIndex* index, int min_key, int max_key, Record*** results, int* result_count);
void btree_index_print(BTreeIndex* index);

// B-Tree Helper Functions
BTreeNode* btree_create_node(bool leaf);
void btree_split_child(BTreeNode* parent, int i, BTreeNode* child);
void btree_insert_nonfull(BTreeNode* node, int key, Record* record);
Record* btree_search_node(BTreeNode* node, int key);
void btree_traverse_node(BTreeNode* node, Record*** results, int* count, int* capacity);

// ===============================
// Field Index Functions
// ===============================

FieldIndex* field_index_create(const char* field_name, FieldType type);
void field_index_free(FieldIndex* index);
int field_index_insert(FieldIndex* index, const char* value, Record* record);
Record** field_index_search(FieldIndex* index, const char* value, int* result_count);
int field_index_delete(FieldIndex* index, const char* value, Record* record);
void field_index_range_search(FieldIndex* index, const char* min_value, const char* max_value, Record*** results, int* result_count);
FieldIndexNode* field_index_create_node(const char* key, Record* record);
FieldIndexEntry* field_index_create_entry(Record* record);

// BST Operations
FieldIndexNode* bst_insert(FieldIndexNode* root, const char* key, Record* record);
FieldIndexNode* bst_search(FieldIndexNode* root, const char* key);
void bst_range_search(FieldIndexNode* root, const char* min, const char* max, Record*** results, int* count, int* capacity);

// ===============================
// Composite Index Functions
// ===============================

CompositeIndex* composite_index_create(char** field_names, int field_count, int capacity);
void composite_index_free(CompositeIndex* index);
int composite_index_insert(CompositeIndex* index, Record* record);
Record* composite_index_search(CompositeIndex* index, char** values);
int composite_index_delete(CompositeIndex* index, char** values);
unsigned long composite_hash(CompositeKey* key, int capacity);
CompositeKey composite_create_key(Record* record, char** field_names, int field_count);
void composite_free_key(CompositeKey* key);

// ===============================
// Table Index Functions
// ===============================

TableIndex* table_index_create(const char* table_name);
void table_index_free(TableIndex* table_index);
int table_index_add_secondary(TableIndex* table_index, const char* field_name, FieldType type);
int table_index_add_composite(TableIndex* table_index, char** field_names, int field_count);
int table_index_insert_record(TableIndex* table_index, Record* record);
Record* table_index_find_by_id(TableIndex* table_index, int id);
Record** table_index_find_by_field(TableIndex* table_index, const char* field_name, const char* value, int* result_count);
Record** table_index_range_query(TableIndex* table_index, const char* field_name, const char* min_value, const char* max_value, int* result_count);
int table_index_delete_record(TableIndex* table_index, Record* record);

// ===============================
// Index Manager Functions
// ===============================

IndexManager* index_manager_create();
void index_manager_free(IndexManager* manager);
TableIndex* index_manager_get_table_index(IndexManager* manager, const char* table_name);
int index_manager_add_table(IndexManager* manager, const char* table_name);
int index_manager_remove_table(IndexManager* manager, const char* table_name);
int index_manager_add_secondary_index(IndexManager* manager, const char* table_name, const char* field_name, FieldType type);
int index_manager_add_composite_index(IndexManager* manager, const char* table_name, char** field_names, int field_count);
int index_manager_insert_record(IndexManager* manager, const char* table_name, Record* record);
Record* index_manager_find_record(IndexManager* manager, const char* table_name, int id);
Record** index_manager_search_records(IndexManager* manager, const char* table_name, const char* field_name, const char* value, int* result_count);
Record** index_manager_range_query(IndexManager* manager, const char* table_name, const char* field_name, const char* min, const char* max, int* result_count);
int index_manager_delete_record(IndexManager* manager, const char* table_name, Record* record);

#endif // INDEX_H