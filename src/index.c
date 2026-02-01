#include "index.h"
#include "utils.h"
#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <assert.h>
#include <limits.h>
#include <pthread.h>
#include <ctype.h>

// ==================== INTERNAL MACROS ====================
#define MAX_LEVEL 32
#define P 0.25  // Probability for skip list
#define BTREE_MIN_DEGREE 2
#define BTREE_MAX_KEYS (2 * BTREE_MIN_DEGREE - 1)
#define INITIAL_HASH_SIZE 16
#define LOAD_FACTOR 0.75
#define INDEX_COMPACTION_THRESHOLD 0.5

// ==================== COMPLETE TYPE DEFINITIONS ====================
// ==================== HASH INDEX STRUCTURES ====================
typedef struct HashIndex {
    int capacity;
    int size;
    double load_factor;            // ADD THIS FIELD
    HashNode** buckets;
    pthread_rwlock_t* bucket_locks; // ADD THIS FIELD - For fine-grained locking
    int lock_count;                // ADD THIS FIELD
    IndexStatistics stats;         // ADD THIS FIELD
} HashIndex;

// ==================== FULL-TEXT INDEX STRUCTURES ====================
typedef struct FullTextIndex {
    FullTextIndexNode* root;
    int term_count;
    int total_documents;           // ADD THIS FIELD
    int total_occurrences;
    pthread_rwlock_t lock;
    IndexStatistics stats;         // ADD THIS FIELD
    
    // Configuration - ADD THESE FIELDS
    bool case_sensitive;
    bool stem_words;
    char** stop_words;
    int stop_word_count;
} FullTextIndex;

// ==================== SPATIAL INDEX STRUCTURES ====================
typedef struct SpatialIndex {
    SpatialIndexNode* root;
    int node_count;
    int max_capacity;
    int min_capacity;              // ADD THIS FIELD
    pthread_rwlock_t lock;
    IndexStatistics stats;         // ADD THIS FIELD
    
    // Configuration - ADD THESE FIELDS
    int dimensions;                // 2D or 3D
    bool store_points;             // Store points or just references
    double tolerance;              // For floating point comparisons
} SpatialIndex;
// Complete HashIndex structure
typedef struct HashNode {
    int key;
    Record* record;
    struct HashNode* next;
} HashNode;


// Complete BTree structures
typedef struct BTreeNode {
    int* keys;
    Record** records;
    int n;           // Current number of keys
    bool leaf;
    struct BTreeNode** children;
} BTreeNode;

typedef struct BTreeIndex {
    BTreeNode* root;
    int t;           // Minimum degree
    int size;
} BTreeIndex;

// Complete SkipList structures
typedef struct SkipListNode {
    int key;
    Record* record;
    struct SkipListNode** forward;
    int level;
} SkipListNode;

typedef struct SkipListIndex {
    SkipListNode* header;
    int max_level;
    int level;
    int size;
} SkipListIndex;

// Complete Index structure
struct Index {
    char table_name[MAX_TABLE_NAME];
    char field_name[MAX_FIELD_LEN];
    IndexType type;
    void* options;
    IndexStatistics* statistics;
    pthread_rwlock_t lock;
    
    // Implementation-specific data
    union {
        HashIndex* hash;
        BTreeIndex* btree;
        SkipListIndex* skiplist;
        void* bitmap;
        void* fulltext;
        void* spatial;
        void* generic;
    } impl;
};

// Extended index structures
typedef struct BitmapIndex {
    int* bitmap;
    int size;
    int capacity;
    Table* table;
    char field_name[MAX_FIELD_LEN];
} BitmapIndex;

typedef struct FullTextIndexNode {
    char* term;
    int* record_ids;
    int count;
    int capacity;
    struct FullTextIndexNode* left;
    struct FullTextIndexNode* right;
} FullTextIndexNode;



typedef struct SpatialIndexNode {
    double min_x, min_y, max_x, max_y;  // Bounding box
    int* record_ids;
    int count;
    int capacity;
    bool is_leaf;
    union {
        struct {
            struct SpatialIndexNode** children;
            int child_count;
        } internal;
        struct {
            Record** records;
        } leaf;
    } data;
} SpatialIndexNode;



// ==================== HASH INDEX ENHANCEMENTS ====================

// Hash function
static unsigned int hash_function(int key, int capacity) {
    // Simple hash function
    return ((unsigned int)key * 2654435761u) % capacity;
}

// Enhanced hash function with better distribution
static unsigned int jenkins_hash(const void* key, size_t length) {
    const unsigned char* data = (const unsigned char*)key;
    unsigned int hash = 0;
    
    for (size_t i = 0; i < length; ++i) {
        hash += data[i];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    
    return hash;
}

static unsigned int hash_int(int key) {
    return jenkins_hash(&key, sizeof(int));
}

static unsigned int hash_string(const char* key) {
    return jenkins_hash(key, strlen(key));
}

static unsigned int hash_field_value(const Field* field) {
    switch (field->type) {
        case TYPE_INT:
            return hash_int(field->value.int_value);
        case TYPE_STRING:
            return hash_string(field->value.string_value);
        case TYPE_FLOAT: {
            float f = field->value.float_value;
            return jenkins_hash(&f, sizeof(float));
        }
        case TYPE_DOUBLE: {
            double d = field->value.double_value;
            return jenkins_hash(&d, sizeof(double));
        }
        case TYPE_BOOL:
            return field->value.bool_value ? 1 : 0;
        default:
            return 0;
    }
}

// Hash index operations
// Hash Index implementation
HashIndex* hash_index_create(int capacity) {
    HashIndex* hash = (HashIndex*)malloc(sizeof(HashIndex));
    if (!hash) return NULL;
    
    hash->capacity = capacity > 0 ? capacity : DEFAULT_HASH_CAPACITY;
    hash->size = 0;
    hash->load_factor = DEFAULT_LOAD_FACTOR;
    
    hash->buckets = (HashNode**)calloc(hash->capacity, sizeof(HashNode*));
    if (!hash->buckets) {
        free(hash);
        return NULL;
    }
    
    // Initialize bucket locks if needed
    if (CONCURRENT_INDEXING) {
        hash->bucket_locks = (pthread_rwlock_t*)malloc(hash->capacity * sizeof(pthread_rwlock_t));
        if (hash->bucket_locks) {
            for (int i = 0; i < hash->capacity; i++) {
                pthread_rwlock_init(&hash->bucket_locks[i], NULL);
            }
        }
    }
    
    memset(&hash->stats, 0, sizeof(IndexStatistics));
    strcpy(hash->stats.index_name, "hash_index");
    hash->stats.type = INDEX_HASH;
    
    return hash;
}

static int hash_index_insert(HashIndex* hash, int key, Record* record) {
    if (!hash || !record) return ERROR_INVALID_INPUT;
    
    unsigned int index = hash_function(key, hash->capacity);
    HashNode* node = hash->buckets[index];
    
    // Check if key already exists
    while (node) {
        if (node->key == key) {
            return ERROR_DUPLICATE_KEY;
        }
        node = node->next;
    }
    
    // Create new node
    HashNode* new_node = (HashNode*)malloc(sizeof(HashNode));
    if (!new_node) return ERROR_MEMORY_ALLOCATION;
    
    new_node->key = key;
    new_node->record = record;
    new_node->next = hash->buckets[index];
    hash->buckets[index] = new_node;
    hash->size++;
    
    return SUCCESS;
}

static Record* hash_index_search(HashIndex* hash, int key) {
    if (!hash) return NULL;
    
    unsigned int index = hash_function(key, hash->capacity);
    HashNode* node = hash->buckets[index];
    
    while (node) {
        if (node->key == key) {
            return node->record;
        }
        node = node->next;
    }
    
    return NULL;
}

static int hash_index_delete(HashIndex* hash, int key) {
    if (!hash) return ERROR_INVALID_INPUT;
    
    unsigned int index = hash_function(key, hash->capacity);
    HashNode* prev = NULL;
    HashNode* current = hash->buckets[index];
    
    while (current) {
        if (current->key == key) {
            if (prev) {
                prev->next = current->next;
            } else {
                hash->buckets[index] = current->next;
            }
            
            free(current);
            hash->size--;
            return SUCCESS;
        }
        prev = current;
        current = current->next;
    }
    
    return ERROR_NOT_FOUND;
}

static void hash_index_free(HashIndex* hash) {
    if (!hash) return;
    
    for (int i = 0; i < hash->capacity; i++) {
        HashNode* node = hash->buckets[i];
        while (node) {
            HashNode* next = node->next;
            free(node);
            node = next;
        }
    }
    
    free(hash->buckets);
    free(hash);
}

// Thread-safe hash index
typedef struct ConcurrentHashIndex {
    HashIndex* hash;
    pthread_rwlock_t* bucket_locks;
    pthread_rwlock_t global_lock;
    int lock_count;
} ConcurrentHashIndex;

static ConcurrentHashIndex* concurrent_hash_create(int capacity) {
    ConcurrentHashIndex* concurrent = (ConcurrentHashIndex*)malloc(sizeof(ConcurrentHashIndex));
    if (!concurrent) return NULL;
    
    concurrent->hash = hash_index_create(capacity);
    if (!concurrent->hash) {
        free(concurrent);
        return NULL;
    }
    
    concurrent->lock_count = capacity;
    concurrent->bucket_locks = (pthread_rwlock_t*)malloc(capacity * sizeof(pthread_rwlock_t));
    if (!concurrent->bucket_locks) {
        hash_index_free(concurrent->hash);
        free(concurrent);
        return NULL;
    }
    
    for (int i = 0; i < capacity; i++) {
        pthread_rwlock_init(&concurrent->bucket_locks[i], NULL);
    }
    pthread_rwlock_init(&concurrent->global_lock, NULL);
    
    return concurrent;
}

static Record* concurrent_hash_search(ConcurrentHashIndex* concurrent, int key) {
    unsigned int index = hash_function(key, concurrent->hash->capacity);
    
    pthread_rwlock_rdlock(&concurrent->bucket_locks[index]);
    Record* result = hash_index_search(concurrent->hash, key);
    pthread_rwlock_unlock(&concurrent->bucket_locks[index]);
    
    return result;
}

static int concurrent_hash_insert(ConcurrentHashIndex* concurrent, int key, Record* record) {
    unsigned int index = hash_function(key, concurrent->hash->capacity);
    
    pthread_rwlock_wrlock(&concurrent->bucket_locks[index]);
    int result = hash_index_insert(concurrent->hash, key, record);
    pthread_rwlock_unlock(&concurrent->bucket_locks[index]);
    
    return result;
}

static void concurrent_hash_free(ConcurrentHashIndex* concurrent) {
    if (!concurrent) return;
    
    for (int i = 0; i < concurrent->lock_count; i++) {
        pthread_rwlock_destroy(&concurrent->bucket_locks[i]);
    }
    free(concurrent->bucket_locks);
    hash_index_free(concurrent->hash);
    free(concurrent);
}

// ==================== B-TREE ENHANCEMENTS ====================

// B-Tree with support for variable-sized keys
typedef struct BTreeKey {
    void* data;
    size_t size;
    FieldType type;
    Record* record;
} BTreeKey;

typedef struct EnhancedBTreeNode {
    BTreeKey* keys;
    int t;           // Minimum degree
    int n;           // Current number of keys
    bool leaf;
    struct EnhancedBTreeNode** children;
    pthread_rwlock_t lock;
} EnhancedBTreeNode;

typedef struct EnhancedBTree {
    EnhancedBTreeNode* root;
    int t;
    int size;
    FieldType key_type;
    pthread_rwlock_t lock;
} EnhancedBTree;

static int compare_btree_keys(const BTreeKey* a, const BTreeKey* b) {
    if (a->type != b->type) return a->type - b->type;
    
    switch (a->type) {
        case TYPE_INT:
            return *(int*)a->data - *(int*)b->data;
        case TYPE_STRING:
            return strcmp((char*)a->data, (char*)b->data);
        case TYPE_FLOAT:
            return (*(float*)a->data > *(float*)b->data) - 
                   (*(float*)a->data < *(float*)b->data);
        case TYPE_DOUBLE:
            return (*(double*)a->data > *(double*)b->data) - 
                   (*(double*)a->data < *(double*)b->data);
        default:
            return 0;
    }
}

static EnhancedBTreeNode* enhanced_btree_create_node(int t, bool leaf) {
    EnhancedBTreeNode* node = (EnhancedBTreeNode*)malloc(sizeof(EnhancedBTreeNode));
    if (!node) return NULL;
    
    node->t = t;
    node->n = 0;
    node->leaf = leaf;
    node->keys = (BTreeKey*)malloc((2 * t - 1) * sizeof(BTreeKey));
    node->children = leaf ? NULL : (EnhancedBTreeNode**)malloc(2 * t * sizeof(EnhancedBTreeNode*));
    
    if (!node->keys || (!leaf && !node->children)) {
        free(node->keys);
        free(node->children);
        free(node);
        return NULL;
    }
    
    pthread_rwlock_init(&node->lock, NULL);
    return node;
}

static Record* enhanced_btree_search(EnhancedBTreeNode* node, const BTreeKey* key) {
    if (!node) return NULL;
    
    int i = 0;
    while (i < node->n && compare_btree_keys(&node->keys[i], key) < 0) {
        i++;
    }
    
    if (i < node->n && compare_btree_keys(&node->keys[i], key) == 0) {
        return node->keys[i].record;
    }
    
    if (node->leaf) {
        return NULL;
    }
    
    return enhanced_btree_search(node->children[i], key);
}

static EnhancedBTree* enhanced_btree_create(FieldType key_type, int degree) {
    EnhancedBTree* tree = (EnhancedBTree*)malloc(sizeof(EnhancedBTree));
    if (!tree) return NULL;
    
    tree->t = (degree > BTREE_MIN_DEGREE) ? degree : BTREE_MIN_DEGREE;
    tree->root = enhanced_btree_create_node(tree->t, true);
    tree->size = 0;
    tree->key_type = key_type;
    
    if (!tree->root) {
        free(tree);
        return NULL;
    }
    
    pthread_rwlock_init(&tree->lock, NULL);
    return tree;
}

// Basic BTree operations
static BTreeNode* btree_create_node(int t, bool leaf) {
    BTreeNode* node = (BTreeNode*)malloc(sizeof(BTreeNode));
    if (!node) return NULL;
    
    node->keys = (int*)malloc((2 * t - 1) * sizeof(int));
    node->records = (Record**)malloc((2 * t - 1) * sizeof(Record*));
    node->children = leaf ? NULL : (BTreeNode**)malloc(2 * t * sizeof(BTreeNode*));
    node->n = 0;
    node->leaf = leaf;
    
    if (!node->keys || !node->records || (!leaf && !node->children)) {
        free(node->keys);
        free(node->records);
        free(node->children);
        free(node);
        return NULL;
    }
    
    return node;
}

static Record* btree_search(BTreeNode* node, int key) {
    if (!node) return NULL;
    
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

static int btree_insert_non_full(BTreeNode* node, int key, Record* record, int t) {
    int i = node->n - 1;
    
    if (node->leaf) {
        while (i >= 0 && key < node->keys[i]) {
            node->keys[i + 1] = node->keys[i];
            node->records[i + 1] = node->records[i];
            i--;
        }
        
        node->keys[i + 1] = key;
        node->records[i + 1] = record;
        node->n++;
        return SUCCESS;
    } else {
        while (i >= 0 && key < node->keys[i]) {
            i--;
        }
        i++;
        
        if (node->children[i]->n == 2 * t - 1) {
            // Split child if full
            // Simplified - would need full implementation
            return ERROR_TABLE_FULL;
        }
        
        return btree_insert_non_full(node->children[i], key, record, t);
    }
}

static int btree_insert(BTreeIndex* btree, int key, Record* record) {
    if (!btree || !record) return ERROR_INVALID_INPUT;
    
    BTreeNode* root = btree->root;
    
    if (root->n == 2 * btree->t - 1) {
        // Root is full, need to split
        BTreeNode* new_root = btree_create_node(btree->t, false);
        if (!new_root) return ERROR_MEMORY_ALLOCATION;
        
        new_root->children[0] = root;
        btree->root = new_root;
        
        // Split the old root
        // Simplified - would need full implementation
        return ERROR_TABLE_FULL;
    }
    
    return btree_insert_non_full(root, key, record, btree->t);
}

static void btree_index_free(BTreeIndex* btree) {
    if (!btree) return;
    
    // Free all nodes recursively
    // Simplified - would need full implementation
    
    free(btree);
}

// ==================== SKIP LIST ENHANCEMENTS ====================

// Deterministic skip list for better performance
typedef struct DeterministicSkipListNode {
    int key;
    Record* record;
    struct DeterministicSkipListNode** forward;
    int level;
    time_t timestamp;
} DeterministicSkipListNode;

typedef struct DeterministicSkipList {
    DeterministicSkipListNode* header;
    int max_level;
    int level;
    int size;
    pthread_rwlock_t lock;
} DeterministicSkipList;

static int deterministic_level(int max_level) {
    int level = 1;
    // Use bit operations for deterministic level calculation
    while ((rand() & 3) == 0 && level < max_level) {  // 25% probability
        level++;
    }
    return level;
}

static DeterministicSkipListNode* deterministic_skip_list_create_node(int level, int key, Record* record) {
    DeterministicSkipListNode* node = (DeterministicSkipListNode*)malloc(sizeof(DeterministicSkipListNode));
    if (!node) return NULL;
    
    node->key = key;
    node->record = record;
    node->level = level;
    node->timestamp = time(NULL);
    node->forward = (DeterministicSkipListNode**)malloc((level + 1) * sizeof(DeterministicSkipListNode*));
    
    if (!node->forward) {
        free(node);
        return NULL;
    }
    
    for (int i = 0; i <= level; i++) {
        node->forward[i] = NULL;
    }
    
    return node;
}

static DeterministicSkipList* deterministic_skip_list_create(int max_level) {
    DeterministicSkipList* list = (DeterministicSkipList*)malloc(sizeof(DeterministicSkipList));
    if (!list) return NULL;
    
    list->max_level = max_level;
    list->level = 0;
    list->size = 0;
    
    list->header = deterministic_skip_list_create_node(max_level, INT_MIN, NULL);
    if (!list->header) {
        free(list);
        return NULL;
    }
    
    pthread_rwlock_init(&list->lock, NULL);
    return list;
}

static Record* deterministic_skip_list_search(DeterministicSkipList* list, int key) {
    pthread_rwlock_rdlock(&list->lock);
    
    DeterministicSkipListNode* current = list->header;
    
    for (int i = list->level; i >= 0; i--) {
        while (current->forward[i] != NULL && current->forward[i]->key < key) {
            current = current->forward[i];
        }
    }
    
    current = current->forward[0];
    Record* result = (current != NULL && current->key == key) ? current->record : NULL;
    
    pthread_rwlock_unlock(&list->lock);
    return result;
}

// Basic SkipList operations
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

static int random_level(int max_level) {
    int level = 1;
    while ((rand() & 0xFFFF) < (P * 0xFFFF) && level < max_level) {
        level++;
    }
    return level;
}

static Record* skiplist_search(SkipListIndex* list, int key) {
    if (!list || !list->header) return NULL;
    
    SkipListNode* current = list->header;
    
    for (int i = list->level; i >= 0; i--) {
        while (current->forward[i] != NULL && current->forward[i]->key < key) {
            current = current->forward[i];
        }
    }
    
    current = current->forward[0];
    return (current != NULL && current->key == key) ? current->record : NULL;
}

static int skiplist_insert(SkipListIndex* list, int key, Record* record) {
    if (!list || !record) return ERROR_INVALID_INPUT;
    
    SkipListNode* update[MAX_LEVEL + 1];
    SkipListNode* current = list->header;
    
    for (int i = list->level; i >= 0; i--) {
        while (current->forward[i] != NULL && current->forward[i]->key < key) {
            current = current->forward[i];
        }
        update[i] = current;
    }
    
    current = current->forward[0];
    
    if (current != NULL && current->key == key) {
        return ERROR_DUPLICATE_KEY;
    }
    
    int level = random_level(list->max_level);
    
    if (level > list->level) {
        for (int i = list->level + 1; i <= level; i++) {
            update[i] = list->header;
        }
        list->level = level;
    }
    
    SkipListNode* new_node = skiplist_create_node(level, key, record);
    if (!new_node) return ERROR_MEMORY_ALLOCATION;
    
    for (int i = 0; i <= level; i++) {
        new_node->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = new_node;
    }
    
    list->size++;
    return SUCCESS;
}

static void skiplist_free(SkipListIndex* list) {
    if (!list) return;
    
    SkipListNode* current = list->header;
    while (current) {
        SkipListNode* next = current->forward[0];
        free(current->forward);
        free(current);
        current = next;
    }
    
    free(list);
}

// ==================== BITMAP INDEX IMPLEMENTATION ====================

static BitmapIndex* bitmap_index_create(Table* table, const char* field_name) {
    BitmapIndex* bitmap = (BitmapIndex*)malloc(sizeof(BitmapIndex));
    if (!bitmap) return NULL;
    
    bitmap->table = table;
    strncpy(bitmap->field_name, field_name, MAX_FIELD_LEN);
    bitmap->capacity = 1024;  // Initial capacity
    bitmap->size = 0;
    bitmap->bitmap = (int*)calloc(bitmap->capacity, sizeof(int));
    
    if (!bitmap->bitmap) {
        free(bitmap);
        return NULL;
    }
    
    return bitmap;
}

static int bitmap_index_insert(BitmapIndex* bitmap, int record_id, const Field* value) {
    if (!bitmap || !value) return ERROR_INVALID_INPUT;
    
    // For bitmap index, we need to know all possible values
    // This is a simplified implementation
    int bit_position = 0;
    
    switch (value->type) {
        case TYPE_INT:
            bit_position = value->value.int_value % bitmap->capacity;
            break;
        case TYPE_BOOL:
            bit_position = value->value.bool_value ? 1 : 0;
            break;
        default:
            // For other types, use hash
            bit_position = hash_field_value(value) % bitmap->capacity;
            break;
    }
    
    // Ensure capacity
    if (bit_position >= bitmap->capacity) {
        int new_capacity = bitmap->capacity * 2;
        while (new_capacity <= bit_position) new_capacity *= 2;
        
        int* new_bitmap = (int*)realloc(bitmap->bitmap, new_capacity * sizeof(int));
        if (!new_bitmap) return ERROR_MEMORY_ALLOCATION;
        
        // Initialize new memory
        memset(new_bitmap + bitmap->capacity, 0, (new_capacity - bitmap->capacity) * sizeof(int));
        
        bitmap->bitmap = new_bitmap;
        bitmap->capacity = new_capacity;
    }
    
    // Set the bit
    int bit_index = bit_position / (sizeof(int) * 8);
    int bit_offset = bit_position % (sizeof(int) * 8);
    
    bitmap->bitmap[bit_index] |= (1 << bit_offset);
    bitmap->size = MAX(bitmap->size, bit_position + 1);
    
    return SUCCESS;
}

static Record** bitmap_index_search(BitmapIndex* bitmap, const Field* value, int* out_count) {
    if (!bitmap || !value || !out_count) return NULL;
    
    *out_count = 0;
    
    // Calculate bit position for the value
    int bit_position = 0;
    switch (value->type) {
        case TYPE_INT:
            bit_position = value->value.int_value % bitmap->capacity;
            break;
        case TYPE_BOOL:
            bit_position = value->value.bool_value ? 1 : 0;
            break;
        default:
            bit_position = hash_field_value(value) % bitmap->capacity;
            break;
    }
    
    if (bit_position >= bitmap->size) return NULL;
    
    // Check if bit is set
    int bit_index = bit_position / (sizeof(int) * 8);
    int bit_offset = bit_position % (sizeof(int) * 8);
    
    if (!(bitmap->bitmap[bit_index] & (1 << bit_offset))) {
        return NULL;
    }
    
    // Collect all records with this value (simplified - would need reverse mapping)
    // For now, return empty result
    Record** results = NULL;
    *out_count = 0;
    
    return results;
}

static void bitmap_index_free(BitmapIndex* bitmap) {
    if (!bitmap) return;
    free(bitmap->bitmap);
    free(bitmap);
}

// ==================== FULL-TEXT INDEX IMPLEMENTATION ====================

static FullTextIndexNode* fulltext_create_node(const char* term) {
    FullTextIndexNode* node = (FullTextIndexNode*)malloc(sizeof(FullTextIndexNode));
    if (!node) return NULL;
    
    node->term = strdup(term);
    node->count = 0;
    node->capacity = 10;
    node->record_ids = (int*)malloc(node->capacity * sizeof(int));
    node->left = node->right = NULL;
    
    if (!node->term || !node->record_ids) {
        free(node->term);
        free(node->record_ids);
        free(node);
        return NULL;
    }
    
    return node;
}

static FullTextIndexNode* fulltext_insert_node(FullTextIndexNode* root, const char* term, int record_id) {
    if (!root) {
        root = fulltext_create_node(term);
        if (!root) return NULL;
    }
    
    int cmp = strcmp(term, root->term);
    
    if (cmp < 0) {
        root->left = fulltext_insert_node(root->left, term, record_id);
    } else if (cmp > 0) {
        root->right = fulltext_insert_node(root->right, term, record_id);
    } else {
        // Term exists, add record_id
        if (root->count >= root->capacity) {
            int new_capacity = root->capacity * 2;
            int* new_ids = (int*)realloc(root->record_ids, new_capacity * sizeof(int));
            if (!new_ids) return root;
            
            root->record_ids = new_ids;
            root->capacity = new_capacity;
        }
        
        // Avoid duplicates
        for (int i = 0; i < root->count; i++) {
            if (root->record_ids[i] == record_id) {
                return root;
            }
        }
        
        root->record_ids[root->count++] = record_id;
    }
    
    return root;
}

static FullTextIndexNode* fulltext_search_node(FullTextIndexNode* root, const char* term) {
    if (!root) return NULL;
    
    int cmp = strcmp(term, root->term);
    
    if (cmp < 0) {
        return fulltext_search_node(root->left, term);
    } else if (cmp > 0) {
        return fulltext_search_node(root->right, term);
    } else {
        return root;
    }
}

static void fulltext_free_node(FullTextIndexNode* node) {
    if (!node) return;
    
    fulltext_free_node(node->left);
    fulltext_free_node(node->right);
    
    free(node->term);
    free(node->record_ids);
    free(node);
}

// Full-text Index implementation
FullTextIndex* fulltext_index_create(void) {
    FullTextIndex* index = (FullTextIndex*)malloc(sizeof(FullTextIndex));
    if (!index) return NULL;
    
    index->root = NULL;
    index->term_count = 0;
    index->total_documents = 0;
    index->total_occurrences = 0;
    
    // Default configuration
    index->case_sensitive = false;
    index->stem_words = false;
    index->stop_words = NULL;
    index->stop_word_count = 0;
    
    pthread_rwlock_init(&index->lock, NULL);
    memset(&index->stats, 0, sizeof(IndexStatistics));
    strcpy(index->stats.index_name, "fulltext_index");
    index->stats.type = INDEX_FULLTEXT;
    
    return index;
}

static int fulltext_index_insert(FullTextIndex* index, const char* text, int record_id) {
    if (!index || !text) return ERROR_INVALID_INPUT;
    
    pthread_rwlock_wrlock(&index->lock);
    
    // Tokenize text (simplified tokenization)
    char* copy = strdup(text);
    if (!copy) {
        pthread_rwlock_unlock(&index->lock);
        return ERROR_MEMORY_ALLOCATION;
    }
    
    char* token = strtok(copy, " .,;:!?\"'()[]{}<>");
    int inserted = 0;
    
    while (token) {
        // Convert to lowercase
        for (int i = 0; token[i]; i++) {
            token[i] = tolower(token[i]);
        }
        
        // Insert token
        index->root = fulltext_insert_node(index->root, token, record_id);
        if (fulltext_search_node(index->root, token)) {
            inserted++;
            index->total_occurrences++;
        }
        
        token = strtok(NULL, " .,;:!?\"'()[]{}<>");
    }
    
    free(copy);
    index->term_count += inserted;
    
    pthread_rwlock_unlock(&index->lock);
    return SUCCESS;
}

static int* fulltext_index_search(FullTextIndex* index, const char* query, int* out_count) {
    if (!index || !query || !out_count) return NULL;
    
    pthread_rwlock_rdlock(&index->lock);
    
    *out_count = 0;
    
    // For simplicity, just search for the first word
    char* copy = strdup(query);
    if (!copy) {
        pthread_rwlock_unlock(&index->lock);
        return NULL;
    }
    
    char* token = strtok(copy, " .,;:!?\"'()[]{}<>");
    if (!token) {
        free(copy);
        pthread_rwlock_unlock(&index->lock);
        return NULL;
    }
    
    // Convert to lowercase
    for (int i = 0; token[i]; i++) {
        token[i] = tolower(token[i]);
    }
    
    FullTextIndexNode* node = fulltext_search_node(index->root, token);
    int* result = NULL;
    
    if (node) {
        result = (int*)malloc(node->count * sizeof(int));
        if (result) {
            memcpy(result, node->record_ids, node->count * sizeof(int));
            *out_count = node->count;
        }
    }
    
    free(copy);
    pthread_rwlock_unlock(&index->lock);
    
    return result;
}

static void fulltext_index_free(FullTextIndex* index) {
    if (!index) return;
    
    pthread_rwlock_wrlock(&index->lock);
    fulltext_free_node(index->root);
    pthread_rwlock_unlock(&index->lock);
    
    pthread_rwlock_destroy(&index->lock);
    free(index);
}

// ==================== SPATIAL INDEX IMPLEMENTATION (R-Tree) ====================

static SpatialIndexNode* spatial_create_node(bool is_leaf, int max_capacity) {
    SpatialIndexNode* node = (SpatialIndexNode*)malloc(sizeof(SpatialIndexNode));
    if (!node) return NULL;
    
    node->is_leaf = is_leaf;
    node->count = 0;
    node->capacity = max_capacity;
    
    if (is_leaf) {
        node->data.leaf.records = (Record**)malloc(max_capacity * sizeof(Record*));
        if (!node->data.leaf.records) {
            free(node);
            return NULL;
        }
    } else {
        node->data.internal.children = (SpatialIndexNode**)malloc((max_capacity + 1) * sizeof(SpatialIndexNode*));
        node->data.internal.child_count = 0;
        if (!node->data.internal.children) {
            free(node);
            return NULL;
        }
    }
    
    node->record_ids = (int*)malloc(max_capacity * sizeof(int));
    if (!node->record_ids) {
        if (is_leaf) free(node->data.leaf.records);
        else free(node->data.internal.children);
        free(node);
        return NULL;
    }
    
    // Initialize bounding box to invalid values
    node->min_x = node->min_y = INFINITY;
    node->max_x = node->max_y = -INFINITY;
    
    return node;
}

static void spatial_update_bounding_box(SpatialIndexNode* node, double x, double y) {
    if (x < node->min_x) node->min_x = x;
    if (x > node->max_x) node->max_x = x;
    if (y < node->min_y) node->min_y = y;
    if (y > node->max_y) node->max_y = y;
}

static double spatial_area(double min_x, double min_y, double max_x, double max_y) {
    return (max_x - min_x) * (max_y - min_y);
}

static double spatial_expansion_needed(SpatialIndexNode* node, double x, double y) {
    double new_min_x = (node->min_x < x) ? node->min_x : x;
    double new_min_y = (node->min_y < y) ? node->min_y : y;
    double new_max_x = (node->max_x > x) ? node->max_x : x;
    double new_max_y = (node->max_y > y) ? node->max_y : y;
    
    double old_area = spatial_area(node->min_x, node->min_y, node->max_x, node->max_y);
    double new_area = spatial_area(new_min_x, new_min_y, new_max_x, new_max_y);
    
    return new_area - old_area;
}

// Spatial Index implementation
SpatialIndex* spatial_index_create(int max_capacity) {
    SpatialIndex* index = (SpatialIndex*)malloc(sizeof(SpatialIndex));
    if (!index) return NULL;
    
    index->root = NULL;
    index->node_count = 0;
    index->max_capacity = max_capacity > 0 ? max_capacity : DEFAULT_SPATIAL_CAPACITY;
    index->min_capacity = max_capacity / 2;
    
    // Default configuration
    index->dimensions = 2;  // Default to 2D
    index->store_points = true;
    index->tolerance = 0.0001;
    
    pthread_rwlock_init(&index->lock, NULL);
    memset(&index->stats, 0, sizeof(IndexStatistics));
    strcpy(index->stats.index_name, "spatial_index");
    index->stats.type = INDEX_SPATIAL;
    
    return index;
}
// In index.c:
ErrorCode spatial_index_insert_point(SpatialIndex* index, double x, double y, 
                                    int record_id, Record* record) {
    if (!index || !record) return ERROR_INVALID_ARGUMENT;
    
    SpatialPoint* point = (SpatialPoint*)malloc(sizeof(SpatialPoint));
    if (!point) return ERROR_MEMORY;
    
    point->x = x;
    point->y = y;
    point->record_id = record_id;
    point->record = record;
    point->data = NULL;
    
    INDEX_LOCK_WRITE(index);
    
    // Insertion logic here...
    // For example, if you have a function that returns int:
    // int result = internal_spatial_insert(index, point);
    
    INDEX_UNLOCK(index);
    
    return SUCCESS; // Return ErrorCode type
}
static int spatial_index_insert_point(SpatialIndex* index, double x, double y, int record_id, Record* record) {
    if (!index || !index->root) return ERROR_INVALID_INPUT;
    
    pthread_rwlock_wrlock(&index->lock);
    
    SpatialIndexNode* node = index->root;
    
    // Find appropriate leaf node
    while (!node->is_leaf) {
        // Choose child that needs minimum expansion
        double min_expansion = INFINITY;
        int best_child = -1;
        
        for (int i = 0; i < node->data.internal.child_count; i++) {
            double expansion = spatial_expansion_needed(node->data.internal.children[i], x, y);
            if (expansion < min_expansion) {
                min_expansion = expansion;
                best_child = i;
            }
        }
        
        if (best_child != -1) {
            node = node->data.internal.children[best_child];
        } else {
            // Should not happen
            pthread_rwlock_unlock(&index->lock);
            return ERROR_GENERIC;
        }
    }
    
    // Insert into leaf node
    if (node->count < node->capacity) {
        node->record_ids[node->count] = record_id;
        if (record) {
            node->data.leaf.records[node->count] = record;
        }
        spatial_update_bounding_box(node, x, y);
        node->count++;
        
        pthread_rwlock_unlock(&index->lock);
        return SUCCESS;
    }
    
    // Node is full, need to split
    // This is a simplified implementation
    pthread_rwlock_unlock(&index->lock);
    return ERROR_TABLE_FULL;
}

static int* spatial_index_search_range(SpatialIndex* index, double min_x, double min_y,
                                      double max_x, double max_y, int* out_count) {
    if (!index || !index->root || !out_count) return NULL;
    
    pthread_rwlock_rdlock(&index->lock);
    
    *out_count = 0;
    int* results = NULL;
    int capacity = 100;
    results = (int*)malloc(capacity * sizeof(int));
    
    if (!results) {
        pthread_rwlock_unlock(&index->lock);
        return NULL;
    }
    
    // Simple recursive search (for production, use iterative with stack)
    SpatialIndexNode* stack[1000];
    int stack_size = 0;
    stack[stack_size++] = index->root;
    
    while (stack_size > 0) {
        SpatialIndexNode* node = stack[--stack_size];
        
        // Check if node's bounding box intersects with search range
        if (node->max_x < min_x || node->min_x > max_x ||
            node->max_y < min_y || node->min_y > max_y) {
            continue;  // No intersection
        }
        
        if (node->is_leaf) {
            // Check each point in leaf
            for (int i = 0; i < node->count; i++) {
                // In a real implementation, we would have the actual coordinates
                // For now, we'll assume all points in the node are in range
                
                if (*out_count >= capacity) {
                    capacity *= 2;
                    int* new_results = (int*)realloc(results, capacity * sizeof(int));
                    if (!new_results) {
                        free(results);
                        pthread_rwlock_unlock(&index->lock);
                        return NULL;
                    }
                    results = new_results;
                }
                
                results[(*out_count)++] = node->record_ids[i];
            }
        } else {
            // Push children onto stack
            for (int i = node->data.internal.child_count - 1; i >= 0; i--) {
                if (stack_size < 1000) {
                    stack[stack_size++] = node->data.internal.children[i];
                }
            }
        }
    }
    
    // Shrink to actual size
    if (*out_count > 0 && *out_count < capacity) {
        int* new_results = (int*)realloc(results, *out_count * sizeof(int));
        if (new_results) {
            results = new_results;
        }
    } else if (*out_count == 0) {
        free(results);
        results = NULL;
    }
    
    pthread_rwlock_unlock(&index->lock);
    return results;
}

static void spatial_index_free(SpatialIndex* index) {
    if (!index) return;
    
    pthread_rwlock_wrlock(&index->lock);
    
    // Free all nodes recursively (simplified)
    SpatialIndexNode* stack[1000];
    int stack_size = 0;
    stack[stack_size++] = index->root;
    
    while (stack_size > 0) {
        SpatialIndexNode* node = stack[--stack_size];
        
        if (node->is_leaf) {
            free(node->data.leaf.records);
        } else {
            for (int i = 0; i < node->data.internal.child_count; i++) {
                if (stack_size < 1000) {
                    stack[stack_size++] = node->data.internal.children[i];
                }
            }
            free(node->data.internal.children);
        }
        
        free(node->record_ids);
        free(node);
    }
    
    pthread_rwlock_unlock(&index->lock);
    pthread_rwlock_destroy(&index->lock);
    free(index);
}

// ==================== INDEX MANAGER ENHANCEMENTS ====================

// Index statistics
typedef struct IndexStatistics {
    char index_name[64];
    IndexType type;
    int entry_count;
    size_t memory_usage;
    double creation_time;
    double last_access_time;
    long long search_count;
    long long insert_count;
    long long delete_count;
    double avg_search_time;
    double avg_insert_time;
} IndexStatistics;

// Enhanced index manager with statistics
typedef struct EnhancedIndexManager {
    Index** indexes;
    IndexStatistics* stats;
    int count;
    int capacity;
    pthread_rwlock_t lock;
    
    // Performance monitoring
    double total_search_time;
    double total_insert_time;
    long long total_operations;
} EnhancedIndexManager;

static EnhancedIndexManager* enhanced_index_manager_create() {
    EnhancedIndexManager* manager = (EnhancedIndexManager*)malloc(sizeof(EnhancedIndexManager));
    if (!manager) return NULL;
    
    manager->capacity = 10;
    manager->count = 0;
    manager->indexes = (Index**)malloc(manager->capacity * sizeof(Index*));
    manager->stats = (IndexStatistics*)calloc(manager->capacity, sizeof(IndexStatistics));
    
    if (!manager->indexes || !manager->stats) {
        free(manager->indexes);
        free(manager->stats);
        free(manager);
        return NULL;
    }
    
    manager->total_search_time = 0;
    manager->total_insert_time = 0;
    manager->total_operations = 0;
    
    pthread_rwlock_init(&manager->lock, NULL);
    return manager;
}

static void enhanced_index_manager_update_stats(EnhancedIndexManager* manager, int index_idx,
                                               int operation_type, double time_taken) {
    if (index_idx < 0 || index_idx >= manager->count) return;
    
    IndexStatistics* stat = &manager->stats[index_idx];
    
    switch (operation_type) {
        case 0:  // Search
            stat->search_count++;
            stat->avg_search_time = (stat->avg_search_time * (stat->search_count - 1) + time_taken) / stat->search_count;
            manager->total_search_time += time_taken;
            break;
        case 1:  // Insert
            stat->insert_count++;
            stat->avg_insert_time = (stat->avg_insert_time * (stat->insert_count - 1) + time_taken) / stat->insert_count;
            manager->total_insert_time += time_taken;
            break;
        case 2:  // Delete
            stat->delete_count++;
            break;
    }
    
    stat->last_access_time = (double)clock() / CLOCKS_PER_SEC;
    manager->total_operations++;
}

static IndexStatistics* enhanced_index_manager_get_stats(EnhancedIndexManager* manager, const char* index_name) {
    if (!manager || !index_name) return NULL;
    
    pthread_rwlock_rdlock(&manager->lock);
    
    for (int i = 0; i < manager->count; i++) {
        Index* index = manager->indexes[i];
        if (index && strcmp(index_name, index->field_name) == 0) {
            IndexStatistics* result = &manager->stats[i];
            pthread_rwlock_unlock(&manager->lock);
            return result;
        }
    }
    
    pthread_rwlock_unlock(&manager->lock);
    return NULL;
}

static void enhanced_index_manager_free(EnhancedIndexManager* manager) {
    if (!manager) return;
    
    pthread_rwlock_wrlock(&manager->lock);
    
    for (int i = 0; i < manager->count; i++) {
        index_free(manager->indexes[i]);
    }
    
    free(manager->indexes);
    free(manager->stats);
    pthread_rwlock_unlock(&manager->lock);
    pthread_rwlock_destroy(&manager->lock);
    free(manager);
}

// ==================== COMPOSITE INDEX SUPPORT ====================

typedef struct CompositeIndex {
    char table_name[MAX_TABLE_NAME];
    char** field_names;
    int field_count;
    FieldType* field_types;
    IndexType index_type;
    
    // Implementation-specific data
    union {
        HashIndex* hash;
        BTreeIndex* btree;
        SkipListIndex* skiplist;
        void* custom;
    } impl;
    
    pthread_rwlock_t lock;
} CompositeIndex;

static unsigned int composite_hash(const Field* fields, int field_count) {
    unsigned int hash = 0;
    
    for (int i = 0; i < field_count; i++) {
        hash = hash * 31 + hash_field_value(&fields[i]);
    }
    
    return hash;
}

static CompositeIndex* composite_index_create(const char* table_name, 
                                             const char** field_names,
                                             FieldType* field_types,
                                             int field_count,
                                             IndexType type) {
    CompositeIndex* index = (CompositeIndex*)malloc(sizeof(CompositeIndex));
    if (!index) return NULL;
    
    strncpy(index->table_name, table_name, MAX_TABLE_NAME);
    index->field_count = field_count;
    index->index_type = type;
    
    index->field_names = (char**)malloc(field_count * sizeof(char*));
    index->field_types = (FieldType*)malloc(field_count * sizeof(FieldType));
    
    if (!index->field_names || !index->field_types) {
        free(index->field_names);
        free(index->field_types);
        free(index);
        return NULL;
    }
    
    for (int i = 0; i < field_count; i++) {
        index->field_names[i] = strdup(field_names[i]);
        index->field_types[i] = field_types[i];
        
        if (!index->field_names[i]) {
            // Cleanup on error
            for (int j = 0; j < i; j++) free(index->field_names[j]);
            free(index->field_names);
            free(index->field_types);
            free(index);
            return NULL;
        }
    }
    
    // Initialize implementation based on type
    switch (type) {
        case INDEX_HASH:
            index->impl.hash = hash_index_create(INITIAL_HASH_SIZE);
            break;
        case INDEX_BTREE:
            index->impl.btree = (BTreeIndex*)malloc(sizeof(BTreeIndex));
            if (index->impl.btree) {
                index->impl.btree->t = DEFAULT_BTREE_DEGREE;
                index->impl.btree->root = btree_create_node(DEFAULT_BTREE_DEGREE, true);
                index->impl.btree->size = 0;
            }
            break;
        case INDEX_SKIPLIST:
            index->impl.skiplist = (SkipListIndex*)malloc(sizeof(SkipListIndex));
            if (index->impl.skiplist) {
                index->impl.skiplist->max_level = DEFAULT_SKIPLIST_MAX_LEVEL;
                index->impl.skiplist->level = 0;
                index->impl.skiplist->size = 0;
                index->impl.skiplist->header = skiplist_create_node(DEFAULT_SKIPLIST_MAX_LEVEL, -1, NULL);
            }
            break;
        default:
            index->impl.custom = NULL;
            break;
    }
    
    pthread_rwlock_init(&index->lock, NULL);
    return index;
}

// ==================== INDEX COMPACTION AND OPTIMIZATION ====================

static ErrorCode hash_index_compact(HashIndex* hash) {
    if (!hash || hash->size == 0) return SUCCESS;
    
    // Calculate optimal capacity based on load factor
    int optimal_capacity = (int)(hash->size / LOAD_FACTOR) + 1;
    
    // Find next prime number
    while (1) {
        int is_prime = 1;
        for (int i = 2; i * i <= optimal_capacity; i++) {
            if (optimal_capacity % i == 0) {
                is_prime = 0;
                break;
            }
        }
        if (is_prime) break;
        optimal_capacity++;
    }
    
    if (optimal_capacity <= hash->capacity * INDEX_COMPACTION_THRESHOLD) {
        // Rehash to smaller size
        HashNode** new_buckets = (HashNode**)calloc(optimal_capacity, sizeof(HashNode*));
        if (!new_buckets) return ERROR_MEMORY_ALLOCATION;
        
        // Rehash all elements
        for (int i = 0; i < hash->capacity; i++) {
            HashNode* node = hash->buckets[i];
            while (node) {
                HashNode* next = node->next;
                unsigned int new_index = hash_function(node->key, optimal_capacity);
                node->next = new_buckets[new_index];
                new_buckets[new_index] = node;
                node = next;
            }
        }
        
        free(hash->buckets);
        hash->buckets = new_buckets;
        hash->capacity = optimal_capacity;
    }
    
    return SUCCESS;
}

static ErrorCode btree_index_rebalance(BTreeIndex* btree) {
    if (!btree || !btree->root) return SUCCESS;
    
    // Check if root has only one child and is not leaf
    if (!btree->root->leaf && btree->root->n == 0) {
        // Root has only one child, make child the new root
        BTreeNode* old_root = btree->root;
        btree->root = btree->root->children[0];
        
        // Free old root
        free(old_root->keys);
        free(old_root->records);
        free(old_root->children);
        free(old_root);
    }
    
    return SUCCESS;
}

// ==================== INDEX PERSISTENCE ====================

static ErrorCode hash_index_save(HashIndex* hash, FILE* file) {
    if (!hash || !file) return ERROR_INVALID_INPUT;
    
    // Write header
    fwrite(&hash->capacity, sizeof(int), 1, file);
    fwrite(&hash->size, sizeof(int), 1, file);
    
    // Write each bucket
    for (int i = 0; i < hash->capacity; i++) {
        // Count nodes in this bucket
        int node_count = 0;
        HashNode* node = hash->buckets[i];
        while (node) {
            node_count++;
            node = node->next;
        }
        
        fwrite(&node_count, sizeof(int), 1, file);
        
        // Write nodes
        node = hash->buckets[i];
        while (node) {
            fwrite(&node->key, sizeof(int), 1, file);
            // Note: Record pointer cannot be serialized directly
            // In production, you would serialize record data or use offset
            node = node->next;
        }
    }
    
    return SUCCESS;
}

static HashIndex* hash_index_load(FILE* file) {
    if (!file) return NULL;
    
    HashIndex* hash = (HashIndex*)malloc(sizeof(HashIndex));
    if (!hash) return NULL;
    
    // Read header
    fread(&hash->capacity, sizeof(int), 1, file);
    fread(&hash->size, sizeof(int), 1, file);
    
    hash->buckets = (HashNode**)calloc(hash->capacity, sizeof(HashNode*));
    if (!hash->buckets) {
        free(hash);
        return NULL;
    }
    
    // Read each bucket
    for (int i = 0; i < hash->capacity; i++) {
        int node_count = 0;
        fread(&node_count, sizeof(int), 1, file);
        
        HashNode* prev = NULL;
        for (int j = 0; j < node_count; j++) {
            HashNode* node = (HashNode*)malloc(sizeof(HashNode));
            if (!node) {
                // Cleanup
                hash_index_free(hash);
                return NULL;
            }
            
            fread(&node->key, sizeof(int), 1, file);
            node->record = NULL;  // Will need to be reconnected after loading all records
            node->next = NULL;
            
            if (prev) {
                prev->next = node;
            } else {
                hash->buckets[i] = node;
            }
            prev = node;
        }
    }
    
    return hash;
}

// ==================== MAIN INDEX IMPLEMENTATION ====================

// Enhanced index creation with more options
Index* create_index_ex(const char* table_name, const char* field_name, 
                      IndexType type, void* options) {
    Index* index = (Index*)malloc(sizeof(Index));
    if (!index) return NULL;
    
    strncpy(index->table_name, table_name, MAX_TABLE_NAME);
    strncpy(index->field_name, field_name, MAX_FIELD_LEN);
    index->type = type;
    index->options = options;
    index->statistics = (IndexStatistics*)calloc(1, sizeof(IndexStatistics));
    pthread_rwlock_init(&index->lock, NULL);
    
    strncpy(index->statistics->index_name, field_name, 64);
    index->statistics->type = type;
    index->statistics->creation_time = (double)clock() / CLOCKS_PER_SEC;
    
    switch (type) {
        case INDEX_HASH:
            index->impl.hash = hash_index_create(INITIAL_HASH_SIZE);
            if (options) {
                // Apply custom options
            }
            break;
        case INDEX_BTREE:
            index->impl.btree = (BTreeIndex*)malloc(sizeof(BTreeIndex));
            if (index->impl.btree) {
                int degree = options ? *(int*)options : DEFAULT_BTREE_DEGREE;
                index->impl.btree->t = degree;
                index->impl.btree->root = btree_create_node(degree, true);
                index->impl.btree->size = 0;
            }
            break;
        case INDEX_SKIPLIST:
            index->impl.skiplist = (SkipListIndex*)malloc(sizeof(SkipListIndex));
            if (index->impl.skiplist) {
                int max_level = options ? *(int*)options : DEFAULT_SKIPLIST_MAX_LEVEL;
                index->impl.skiplist->max_level = max_level;
                index->impl.skiplist->level = 0;
                index->impl.skiplist->size = 0;
                index->impl.skiplist->header = skiplist_create_node(max_level, -1, NULL);
            }
            break;
        case INDEX_BITMAP:
            index->impl.bitmap = (BitmapIndex*)malloc(sizeof(BitmapIndex));
            // Initialize bitmap index
            break;
        case INDEX_FULLTEXT:
            index->impl.fulltext = fulltext_index_create();
            break;
        default:
            index->impl.generic = NULL;
            break;
    }
    
    if (!index->impl.generic) {
        free(index->statistics);
        pthread_rwlock_destroy(&index->lock);
        free(index);
        return NULL;
    }
    
    return index;
}

// Thread-safe index operations
Record* index_search_safe(Index* index, int key) {
    if (!index) return NULL;
    
    clock_t start = clock();
    pthread_rwlock_rdlock(&index->lock);
    
    Record* result = NULL;
    switch (index->type) {
        case INDEX_HASH:
            result = hash_index_search(index->impl.hash, key);
            break;
        case INDEX_BTREE:
            result = btree_search(index->impl.btree->root, key);
            break;
        case INDEX_SKIPLIST:
            result = skiplist_search(index->impl.skiplist, key);
            break;
        default:
            break;
    }
    
    pthread_rwlock_unlock(&index->lock);
    
    // Update statistics
    double time_taken = (double)(clock() - start) / CLOCKS_PER_SEC;
    index->statistics->search_count++;
    index->statistics->avg_search_time = 
        (index->statistics->avg_search_time * (index->statistics->search_count - 1) + time_taken) / 
        index->statistics->search_count;
    index->statistics->last_access_time = (double)clock() / CLOCKS_PER_SEC;
    
    return result;
}

int index_insert_safe(Index* index, int key, Record* record) {
    if (!index || !record) return ERROR_INVALID_INPUT;
    
    clock_t start = clock();
    pthread_rwlock_wrlock(&index->lock);
    
    int result = ERROR_GENERIC;
    switch (index->type) {
        case INDEX_HASH:
            result = hash_index_insert(index->impl.hash, key, record);
            break;
        case INDEX_BTREE:
            result = btree_insert(index->impl.btree, key, record);
            break;
        case INDEX_SKIPLIST:
            result = skiplist_insert(index->impl.skiplist, key, record);
            break;
        case INDEX_FULLTEXT:
            // Full-text index needs text content
            // This would need to be handled differently
            result = ERROR_INVALID_INPUT;
            break;
        default:
            result = ERROR_INVALID_INPUT;
            break;
    }
    
    pthread_rwlock_unlock(&index->lock);
    
    // Update statistics
    if (result == SUCCESS) {
        double time_taken = (double)(clock() - start) / CLOCKS_PER_SEC;
        index->statistics->insert_count++;
        index->statistics->avg_insert_time = 
            (index->statistics->avg_insert_time * (index->statistics->insert_count - 1) + time_taken) / 
            index->statistics->insert_count;
        index->statistics->entry_count++;
        index->statistics->last_access_time = (double)clock() / CLOCKS_PER_SEC;
    }
    
    return result;
}

// Bulk index operations
ErrorCode index_bulk_insert(Index* index, int* keys, Record** records, int count) {
    if (!index || !keys || !records || count <= 0) return ERROR_INVALID_INPUT;
    
    pthread_rwlock_wrlock(&index->lock);
    
    ErrorCode overall_result = SUCCESS;
    int successful = 0;
    
    for (int i = 0; i < count; i++) {
        ErrorCode result = ERROR_GENERIC;
        
        switch (index->type) {
            case INDEX_HASH:
                result = hash_index_insert(index->impl.hash, keys[i], records[i]);
                break;
            case INDEX_BTREE:
                result = btree_insert(index->impl.btree, keys[i], records[i]);
                break;
            case INDEX_SKIPLIST:
                result = skiplist_insert(index->impl.skiplist, keys[i], records[i]);
                break;
            default:
                result = ERROR_INVALID_INPUT;
                break;
        }
        
        if (result == SUCCESS) {
            successful++;
        } else if (overall_result == SUCCESS) {
            overall_result = result;
        }
    }
    
    pthread_rwlock_unlock(&index->lock);
    
    // Update statistics
    index->statistics->insert_count += successful;
    index->statistics->entry_count += successful;
    index->statistics->last_access_time = (double)clock() / CLOCKS_PER_SEC;
    
    return overall_result;
}

// Index maintenance
ErrorCode index_optimize(Index* index) {
    if (!index) return ERROR_INVALID_INPUT;
    
    pthread_rwlock_wrlock(&index->lock);
    
    ErrorCode result = SUCCESS;
    
    switch (index->type) {
        case INDEX_HASH:
            result = hash_index_compact(index->impl.hash);
            break;
        case INDEX_BTREE:
            result = btree_index_rebalance(index->impl.btree);
            break;
        case INDEX_SKIPLIST:
            // Skip lists are self-balancing
            result = SUCCESS;
            break;
        default:
            result = ERROR_INVALID_INPUT;
            break;
    }
    
    pthread_rwlock_unlock(&index->lock);
    
    return result;
}

// Index statistics
void index_print_statistics(Index* index) {
    if (!index || !index->statistics) return;
    
    printf("\n=== Index Statistics: %s ===\n", index->field_name);
    printf("Type: %s\n", index_type_to_string(index->type));
    printf("Entries: %d\n", index->statistics->entry_count);
    printf("Searches: %lld\n", index->statistics->search_count);
    printf("Inserts: %lld\n", index->statistics->insert_count);
    printf("Deletes: %lld\n", index->statistics->delete_count);
    printf("Average Search Time: %.6f seconds\n", index->statistics->avg_search_time);
    printf("Average Insert Time: %.6f seconds\n", index->statistics->avg_insert_time);
    printf("Last Access: %.2f seconds ago\n", 
           (double)clock() / CLOCKS_PER_SEC - index->statistics->last_access_time);
    printf("===============================\n");
}

// Memory usage calculation
size_t index_get_memory_usage(Index* index) {
    if (!index) return 0;
    
    size_t usage = sizeof(Index);
    
    switch (index->type) {
        case INDEX_HASH: {
            HashIndex* hash = index->impl.hash;
            usage += sizeof(HashIndex);
            usage += hash->capacity * sizeof(HashNode*);
            
            for (int i = 0; i < hash->capacity; i++) {
                HashNode* node = hash->buckets[i];
                while (node) {
                    usage += sizeof(HashNode);
                    node = node->next;
                }
            }
            break;
        }
        case INDEX_BTREE: {
            // B-tree memory usage calculation would be complex
            // Simplified: estimate based on node count
            usage += sizeof(BTreeIndex);
            // Add more detailed calculation in production
            break;
        }
        default:
            break;
    }
    
    return usage;
}

// Index persistence
ErrorCode index_save(Index* index, const char* filename) {
    if (!index || !filename) return ERROR_INVALID_INPUT;
    
    FILE* file = fopen(filename, "wb");
    if (!file) return ERROR_IO_OPERATION;
    
    // Write index metadata
    fwrite(index->table_name, sizeof(char), MAX_TABLE_NAME, file);
    fwrite(index->field_name, sizeof(char), MAX_FIELD_LEN, file);
    fwrite(&index->type, sizeof(IndexType), 1, file);
    
    // Write index data based on type
    switch (index->type) {
        case INDEX_HASH:
            hash_index_save(index->impl.hash, file);
            break;
        // Add other index types as needed
        default:
            fclose(file);
            return ERROR_INVALID_INPUT;
    }
    
    fclose(file);
    return SUCCESS;
}

Index* index_load(const char* filename, Table* table) {
    if (!filename || !table) return NULL;
    
    FILE* file = fopen(filename, "rb");
    if (!file) return NULL;
    
    Index* index = (Index*)malloc(sizeof(Index));
    if (!index) {
        fclose(file);
        return NULL;
    }
    
    // Read metadata
    fread(index->table_name, sizeof(char), MAX_TABLE_NAME, file);
    fread(index->field_name, sizeof(char), MAX_FIELD_LEN, file);
    fread(&index->type, sizeof(IndexType), 1, file);
    
    // Check if this index belongs to the given table
    if (strcmp(index->table_name, table->name) != 0) {
        free(index);
        fclose(file);
        return NULL;
    }
    
    // Read index data based on type
    switch (index->type) {
        case INDEX_HASH:
            index->impl.hash = hash_index_load(file);
            if (!index->impl.hash) {
                free(index);
                fclose(file);
                return NULL;
            }
            break;
        default:
            free(index);
            fclose(file);
            return NULL;
    }
    
    fclose(file);
    
    // Initialize statistics
    index->statistics = (IndexStatistics*)calloc(1, sizeof(IndexStatistics));
    strncpy(index->statistics->index_name, index->field_name, 64);
    index->statistics->type = index->type;
    index->statistics->creation_time = (double)clock() / CLOCKS_PER_SEC;
    
    pthread_rwlock_init(&index->lock, NULL);
    
    return index;
}

// ==================== PUBLIC API IMPLEMENTATION ====================

// The original API functions from index.h are implemented below
// They now use the enhanced implementations internally

Index* create_hash_index(const char* table_name, const char* field_name) {
    return create_index_ex(table_name, field_name, INDEX_HASH, NULL);
}

Index* create_btree_index(const char* table_name, const char* field_name, int degree) {
    int* options = (int*)malloc(sizeof(int));
    if (options) *options = degree;
    return create_index_ex(table_name, field_name, INDEX_BTREE, options);
}

Index* create_skiplist_index(const char* table_name, const char* field_name, int max_level) {
    int* options = (int*)malloc(sizeof(int));
    if (options) *options = max_level;
    return create_index_ex(table_name, field_name, INDEX_SKIPLIST, options);
}

Index* create_bitmap_index(const char* table_name, const char* field_name) {
    return create_index_ex(table_name, field_name, INDEX_BITMAP, NULL);
}

Index* create_fulltext_index(const char* table_name, const char* field_name) {
    return create_index_ex(table_name, field_name, INDEX_FULLTEXT, NULL);
}

int index_insert(Index* index, int key, Record* record) {
    return index_insert_safe(index, key, record);
}

Record* index_search(Index* index, int key) {
    return index_search_safe(index, key);
}

int index_delete(Index* index, int key) {
    if (!index) return ERROR_INVALID_INPUT;
    
    pthread_rwlock_wrlock(&index->lock);
    
    int result = ERROR_GENERIC;
    switch (index->type) {
        case INDEX_HASH:
            result = hash_index_delete(index->impl.hash, key);
            break;
        case INDEX_BTREE:
            result = btree_delete(index->impl.btree, key);
            break;
        case INDEX_SKIPLIST:
            result = skiplist_delete(index->impl.skiplist, key);
            break;
        default:
            result = ERROR_INVALID_INPUT;
            break;
    }
    
    pthread_rwlock_unlock(&index->lock);
    
    if (result == SUCCESS) {
        index->statistics->delete_count++;
        index->statistics->entry_count--;
        index->statistics->last_access_time = (double)clock() / CLOCKS_PER_SEC;
    }
    
    return result;
}

int index_update(Index* index, int old_key, int new_key, Record* record) {
    int result = index_delete(index, old_key);
    if (result != SUCCESS && result != ERROR_NOT_FOUND) {
        return result;
    }
    
    return index_insert(index, new_key, record);
}

void index_free(Index* index) {
    if (!index) return;
    
    pthread_rwlock_wrlock(&index->lock);
    
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
        case INDEX_BITMAP:
            if (index->impl.bitmap) {
                bitmap_index_free((BitmapIndex*)index->impl.bitmap);
            }
            break;
        case INDEX_FULLTEXT:
            if (index->impl.fulltext) {
                fulltext_index_free((FullTextIndex*)index->impl.fulltext);
            }
            break;
        default:
            if (index->impl.generic) {
                free(index->impl.generic);
            }
            break;
    }
    
    if (index->options) free(index->options);
    if (index->statistics) free(index->statistics);
    
    pthread_rwlock_unlock(&index->lock);
    pthread_rwlock_destroy(&index->lock);
    free(index);
}

// ==================== INDEX MANAGER IMPLEMENTATION ====================

IndexManager* create_index_manager() {
    return (IndexManager*)enhanced_index_manager_create();
}

int add_index(IndexManager* manager, Index* index) {
    if (!manager || !index) return ERROR_INVALID_INPUT;
    
    EnhancedIndexManager* enhanced = (EnhancedIndexManager*)manager;
    pthread_rwlock_wrlock(&enhanced->lock);
    
    // Check if index already exists
    for (int i = 0; i < enhanced->count; i++) {
        Index* existing = enhanced->indexes[i];
        if (existing && 
            strcmp(existing->table_name, index->table_name) == 0 &&
            strcmp(existing->field_name, index->field_name) == 0) {
            pthread_rwlock_unlock(&enhanced->lock);
            return ERROR_DUPLICATE_KEY;
        }
    }
    
    // Resize if needed
    if (enhanced->count >= enhanced->capacity) {
        int new_capacity = enhanced->capacity * 2;
        Index** new_indexes = (Index**)realloc(enhanced->indexes, new_capacity * sizeof(Index*));
        IndexStatistics* new_stats = (IndexStatistics*)realloc(enhanced->stats, new_capacity * sizeof(IndexStatistics));
        
        if (!new_indexes || !new_stats) {
            free(new_indexes);
            free(new_stats);
            pthread_rwlock_unlock(&enhanced->lock);
            return ERROR_MEMORY_ALLOCATION;
        }
        
        enhanced->indexes = new_indexes;
        enhanced->stats = new_stats;
        enhanced->capacity = new_capacity;
    }
    
    // Add index
    enhanced->indexes[enhanced->count] = index;
    
    // Initialize statistics for this index
    IndexStatistics* stat = &enhanced->stats[enhanced->count];
    memset(stat, 0, sizeof(IndexStatistics));
    strncpy(stat->index_name, index->field_name, 64);
    stat->type = index->type;
    stat->creation_time = (double)clock() / CLOCKS_PER_SEC;
    
    enhanced->count++;
    
    pthread_rwlock_unlock(&enhanced->lock);
    return SUCCESS;
}

void index_manager_free(IndexManager* manager) {
    enhanced_index_manager_free((EnhancedIndexManager*)manager);
}

// ==================== UTILITY FUNCTIONS ====================

const char* index_type_to_string(IndexType type) {
    switch (type) {
        case INDEX_HASH: return "HASH";
        case INDEX_BTREE: return "BTREE";
        case INDEX_SKIPLIST: return "SKIPLIST";
        case INDEX_BITMAP: return "BITMAP";
        case INDEX_FULLTEXT: return "FULLTEXT";
        default: return "UNKNOWN";
    }
}
// ==================== BENCHMARK OPERATION TYPES ====================

typedef enum OperationType {
    OP_INSERT = 0,
    OP_SEARCH = 1,
    OP_DELETE = 2,
    OP_UPDATE = 3
} OperationType;
// ==================== INDEX BENCHMARKING ====================

typedef struct BenchmarkResult {
    char* index_name;
    IndexType type;
    double insert_time;
    double search_time;
    double delete_time;
    size_t memory_usage;
    long long insert_count;
    long long search_count;
    long long delete_count;
    double average_search_time;
    double average_insert_time;
    double max_search_time;
    double max_insert_time;
    double throughput_inserts_per_sec;
    double throughput_searches_per_sec;
    size_t peak_memory_usage;
    double cpu_usage_percent;
    bool thread_safe;
    int error_count;
} BenchmarkResult;


typedef struct BenchmarkConfig {
    int data_size;
    int num_operations;
    int num_threads;
    bool measure_memory;
    bool measure_cpu;
    bool run_warmup;
    bool verbose;
    int random_seed;
    float search_ratio;
    float insert_ratio;
    float delete_ratio;
    bool shuffle_operations;
    int batch_size;
    int repeat_count;
} BenchmarkConfig;

typedef struct BenchmarkOperation {
    int key;
    Record* record;
    OperationType type;
} BenchmarkOperation;

typedef struct ThreadBenchmarkData {
    Index* index;
    BenchmarkOperation* operations;
    int operation_count;
    BenchmarkResult* partial_result;
    pthread_t thread_id;
    int thread_index;
    pthread_barrier_t* barrier;
    bool* start_signal;
} ThreadBenchmarkData;

// ==================== BENCHMARK UTILITIES ====================

static int* generate_random_keys(int count, int min, int max, int seed) {
    if (count <= 0) return NULL;
    
    int* keys = (int*)malloc(count * sizeof(int));
    if (!keys) return NULL;
    
    srand(seed);
    for (int i = 0; i < count; i++) {
        keys[i] = min + (rand() % (max - min + 1));
    }
    
    return keys;
}

static int* generate_sequential_keys(int count, int start) {
    if (count <= 0) return NULL;
    
    int* keys = (int*)malloc(count * sizeof(int));
    if (!keys) return NULL;
    
    for (int i = 0; i < count; i++) {
        keys[i] = start + i;
    }
    
    return keys;
}

static BenchmarkOperation* generate_operations(int count, int* keys, 
                                              Record** records, 
                                              float insert_ratio,
                                              float search_ratio,
                                              float delete_ratio,
                                              bool shuffle) {
    if (count <= 0 || !keys || !records) return NULL;
    
    BenchmarkOperation* ops = (BenchmarkOperation*)malloc(count * sizeof(BenchmarkOperation));
    if (!ops) return NULL;
    
    // Normalize ratios
    float total = insert_ratio + search_ratio + delete_ratio;
    if (total <= 0) total = 1.0f;
    insert_ratio /= total;
    search_ratio /= total;
    delete_ratio /= total;
    
    int insert_count = (int)(count * insert_ratio);
    int search_count = (int)(count * search_ratio);
    int delete_count = count - insert_count - search_count;
    
    int op_index = 0;
    
    // Generate insert operations
    for (int i = 0; i < insert_count; i++) {
        if (op_index >= count) break;
        ops[op_index].key = keys[i % count];
        ops[op_index].record = records[i % count];
        ops[op_index].type = OP_INSERT;
        op_index++;
    }
    
    // Generate search operations
    for (int i = 0; i < search_count; i++) {
        if (op_index >= count) break;
        ops[op_index].key = keys[i % count];
        ops[op_index].record = NULL;
        ops[op_index].type = OP_SEARCH;
        op_index++;
    }
    
    // Generate delete operations
    for (int i = 0; i < delete_count; i++) {
        if (op_index >= count) break;
        ops[op_index].key = keys[i % count];
        ops[op_index].record = NULL;
        ops[op_index].type = OP_DELETE;
        op_index++;
    }
    
    // Shuffle if requested
    if (shuffle) {
        for (int i = count - 1; i > 0; i--) {
            int j = rand() % (i + 1);
            BenchmarkOperation temp = ops[i];
            ops[i] = ops[j];
            ops[j] = temp;
        }
    }
    
    return ops;
}

static size_t get_current_memory_usage() {
#ifdef __linux__
    FILE* file = fopen("/proc/self/statm", "r");
    if (!file) return 0;
    
    long size, resident, share, text, lib, data, dt;
    fscanf(file, "%ld %ld %ld %ld %ld %ld %ld", &size, &resident, &share, &text, &lib, &data, &dt);
    fclose(file);
    
    return (size_t)resident * sysconf(_SC_PAGESIZE);
#elif defined(__APPLE__)
    // macOS memory usage collection would go here
    return 0;
#else
    return 0;
#endif
}

static double get_cpu_usage() {
#ifdef __linux__
    static clock_t last_cpu_time = 0;
    static clock_t last_sys_time = 0;
    
    clock_t current_cpu_time = clock();
    double cpu_usage = 0.0;
    
    if (last_cpu_time != 0) {
        cpu_usage = ((double)(current_cpu_time - last_cpu_time)) / CLOCKS_PER_SEC * 100.0;
    }
    
    last_cpu_time = current_cpu_time;
    return cpu_usage;
#else
    return 0.0;
#endif
}

// ==================== SINGLE-THREADED BENCHMARK ====================

static BenchmarkResult* benchmark_index_single_thread(Index* index, 
                                                     BenchmarkOperation* operations,
                                                     int operation_count,
                                                     BenchmarkConfig* config) {
    if (!index || !operations || operation_count <= 0 || !config) {
        return NULL;
    }
    
    BenchmarkResult* result = (BenchmarkResult*)calloc(1, sizeof(BenchmarkResult));
    if (!result) return NULL;
    
    result->index_name = strdup(index->field_name);
    result->type = index->type;
    result->memory_usage = 0;
    result->peak_memory_usage = 0;
    result->thread_safe = false;
    result->error_count = 0;
    
    // Measure initial memory
    if (config->measure_memory) {
        result->memory_usage = get_current_memory_usage();
        result->peak_memory_usage = result->memory_usage;
    }
    
    // Warmup phase
    if (config->run_warmup) {
        int warmup_size = operation_count / 10;  // 10% of operations for warmup
        if (warmup_size > 0) {
            for (int i = 0; i < warmup_size; i++) {
                BenchmarkOperation op = operations[i % operation_count];
                switch (op.type) {
                    case OP_INSERT:
                        index_insert(index, op.key, op.record);
                        break;
                    case OP_SEARCH:
                        index_search(index, op.key);
                        break;
                    case OP_DELETE:
                        index_delete(index, op.key);
                        break;
                }
            }
        }
    }
    
    // Clear index statistics before main benchmark
    if (index->statistics) {
        memset(index->statistics, 0, sizeof(IndexStatistics));
    }
    
    // Main benchmark
    clock_t start_time = clock();
    clock_t insert_start, search_start, delete_start;
    double total_insert_time = 0, total_search_time = 0, total_delete_time = 0;
    
    result->max_search_time = 0;
    result->max_insert_time = 0;
    
    for (int i = 0; i < operation_count; i++) {
        BenchmarkOperation op = operations[i];
        ErrorCode error = SUCCESS;
        clock_t op_start, op_end;
        
        switch (op.type) {
            case OP_INSERT:
                insert_start = clock();
                error = index_insert(index, op.key, op.record);
                op_end = clock();
                total_insert_time += (double)(op_end - insert_start) / CLOCKS_PER_SEC;
                result->insert_count++;
                
                // Track max insert time
                double insert_time = (double)(op_end - insert_start) / CLOCKS_PER_SEC;
                if (insert_time > result->max_insert_time) {
                    result->max_insert_time = insert_time;
                }
                break;
                
            case OP_SEARCH:
                search_start = clock();
                Record* found = index_search(index, op.key);
                op_end = clock();
                total_search_time += (double)(op_end - search_start) / CLOCKS_PER_SEC;
                result->search_count++;
                
                // Track max search time
                double search_time = (double)(op_end - search_start) / CLOCKS_PER_SEC;
                if (search_time > result->max_search_time) {
                    result->max_search_time = search_time;
                }
                break;
                
            case OP_DELETE:
                delete_start = clock();
                error = index_delete(index, op.key);
                op_end = clock();
                total_delete_time += (double)(op_end - delete_start) / CLOCKS_PER_SEC;
                result->delete_count++;
                break;
        }
        
        if (error != SUCCESS && error != ERROR_NOT_FOUND) {
            result->error_count++;
        }
        
        // Update peak memory usage
        if (config->measure_memory) {
            size_t current_memory = get_current_memory_usage();
            if (current_memory > result->peak_memory_usage) {
                result->peak_memory_usage = current_memory;
            }
        }
        
        // Update CPU usage
        if (config->measure_cpu) {
            result->cpu_usage_percent = get_cpu_usage();
        }
        
        // Progress reporting
        if (config->verbose && (i % (operation_count / 10) == 0)) {
            printf("Progress: %d%%\n", (i * 100) / operation_count);
        }
    }
    
    clock_t end_time = clock();
    double total_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    
    // Calculate results
    result->insert_time = total_insert_time;
    result->search_time = total_search_time;
    result->delete_time = total_delete_time;
    
    if (result->search_count > 0) {
        result->average_search_time = total_search_time / result->search_count;
    }
    
    if (result->insert_count > 0) {
        result->average_insert_time = total_insert_time / result->insert_count;
    }
    
    if (total_time > 0) {
        result->throughput_inserts_per_sec = result->insert_count / total_time;
        result->throughput_searches_per_sec = result->search_count / total_time;
    }
    
    // Final memory measurement
    if (config->measure_memory) {
        result->memory_usage = get_current_memory_usage();
    }
    
    return result;
}

// ==================== MULTI-THREADED BENCHMARK ====================

static void* benchmark_thread_func(void* arg) {
    ThreadBenchmarkData* thread_data = (ThreadBenchmarkData*)arg;
    BenchmarkResult* result = thread_data->partial_result;
    
    // Wait for all threads to be ready
    if (thread_data->barrier) {
        pthread_barrier_wait(thread_data->barrier);
    }
    
    // Wait for start signal
    if (thread_data->start_signal) {
        while (!*(thread_data->start_signal)) {
            usleep(1000);  // Sleep 1ms
        }
    }
    
    // Run benchmark for this thread
    Index* index = thread_data->index;
    BenchmarkOperation* operations = thread_data->operations;
    int operation_count = thread_data->operation_count;
    
    clock_t thread_start = clock();
    
    for (int i = 0; i < operation_count; i++) {
        BenchmarkOperation op = operations[i];
        clock_t op_start, op_end;
        
        switch (op.type) {
            case OP_INSERT:
                op_start = clock();
                index_insert(index, op.key, op.record);
                op_end = clock();
                result->insert_time += (double)(op_end - op_start) / CLOCKS_PER_SEC;
                result->insert_count++;
                break;
                
            case OP_SEARCH:
                op_start = clock();
                index_search(index, op.key);
                op_end = clock();
                result->search_time += (double)(op_end - op_start) / CLOCKS_PER_SEC;
                result->search_count++;
                break;
                
            case OP_DELETE:
                op_start = clock();
                index_delete(index, op.key);
                op_end = clock();
                result->delete_time += (double)(op_end - op_start) / CLOCKS_PER_SEC;
                result->delete_count++;
                break;
        }
    }
    
    clock_t thread_end = clock();
    result->thread_safe = true;
    
    // Store thread execution time
    double thread_total_time = (double)(thread_end - thread_start) / CLOCKS_PER_SEC;
    
    return NULL;
}

static BenchmarkResult* benchmark_index_multi_thread(Index* index,
                                                    BenchmarkOperation* all_operations,
                                                    int total_operations,
                                                    BenchmarkConfig* config) {
    if (!index || !all_operations || total_operations <= 0 || !config || 
        config->num_threads <= 0) {
        return NULL;
    }
    
    int num_threads = config->num_threads;
    int ops_per_thread = total_operations / num_threads;
    
    // Allocate thread data and results
    ThreadBenchmarkData* thread_data = (ThreadBenchmarkData*)malloc(num_threads * sizeof(ThreadBenchmarkData));
    BenchmarkResult** thread_results = (BenchmarkResult**)malloc(num_threads * sizeof(BenchmarkResult*));
    pthread_t* threads = (pthread_t*)malloc(num_threads * sizeof(pthread_t));
    
    if (!thread_data || !thread_results || !threads) {
        free(thread_data);
        free(thread_results);
        free(threads);
        return NULL;
    }
    
    // Initialize barrier and start signal
    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, num_threads + 1);  // +1 for main thread
    
    bool start_signal = false;
    
    // Prepare thread data
    for (int i = 0; i < num_threads; i++) {
        thread_results[i] = (BenchmarkResult*)calloc(1, sizeof(BenchmarkResult));
        if (!thread_results[i]) {
            // Cleanup on error
            for (int j = 0; j < i; j++) free(thread_results[j]);
            free(thread_data);
            free(thread_results);
            free(threads);
            pthread_barrier_destroy(&barrier);
            return NULL;
        }
        
        thread_data[i].index = index;
        thread_data[i].operations = all_operations + (i * ops_per_thread);
        thread_data[i].operation_count = (i == num_threads - 1) ? 
            (total_operations - (i * ops_per_thread)) : ops_per_thread;
        thread_data[i].partial_result = thread_results[i];
        thread_data[i].thread_index = i;
        thread_data[i].barrier = &barrier;
        thread_data[i].start_signal = &start_signal;
        
        thread_results[i]->index_name = strdup(index->field_name);
        thread_results[i]->type = index->type;
    }
    
    // Create threads
    for (int i = 0; i < num_threads; i++) {
        if (pthread_create(&threads[i], NULL, benchmark_thread_func, &thread_data[i]) != 0) {
            // Cleanup on error
            for (int j = 0; j < i; j++) pthread_join(threads[j], NULL);
            for (int j = 0; j < num_threads; j++) free(thread_results[j]);
            free(thread_data);
            free(thread_results);
            free(threads);
            pthread_barrier_destroy(&barrier);
            return NULL;
        }
    }
    
    // Wait for all threads to be ready
    pthread_barrier_wait(&barrier);
    
    // Start benchmark
    clock_t total_start = clock();
    start_signal = true;
    
    // Wait for all threads to complete
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    clock_t total_end = clock();
    double total_time = (double)(total_end - total_start) / CLOCKS_PER_SEC;
    
    // Aggregate results
    BenchmarkResult* final_result = (BenchmarkResult*)calloc(1, sizeof(BenchmarkResult));
    if (!final_result) {
        // Cleanup
        for (int i = 0; i < num_threads; i++) free(thread_results[i]);
        free(thread_data);
        free(thread_results);
        free(threads);
        pthread_barrier_destroy(&barrier);
        return NULL;
    }
    
    final_result->index_name = strdup(index->field_name);
    final_result->type = index->type;
    final_result->thread_safe = true;
    
    for (int i = 0; i < num_threads; i++) {
        final_result->insert_time += thread_results[i]->insert_time;
        final_result->search_time += thread_results[i]->search_time;
        final_result->delete_time += thread_results[i]->delete_time;
        final_result->insert_count += thread_results[i]->insert_count;
        final_result->search_count += thread_results[i]->search_count;
        final_result->delete_count += thread_results[i]->delete_count;
        
        // Free thread-specific results
        free(thread_results[i]->index_name);
        free(thread_results[i]);
    }
    
    // Calculate averages and throughput
    if (final_result->search_count > 0) {
        final_result->average_search_time = final_result->search_time / final_result->search_count;
    }
    
    if (final_result->insert_count > 0) {
        final_result->average_insert_time = final_result->insert_time / final_result->insert_count;
    }
    
    if (total_time > 0) {
        final_result->throughput_inserts_per_sec = final_result->insert_count / total_time;
        final_result->throughput_searches_per_sec = final_result->search_count / total_time;
    }
    
    // Measure memory
    if (config->measure_memory) {
        final_result->memory_usage = get_current_memory_usage();
        final_result->peak_memory_usage = final_result->memory_usage;  // Simplified
    }
    
    // Cleanup
    free(thread_data);
    free(thread_results);
    free(threads);
    pthread_barrier_destroy(&barrier);
    
    return final_result;
}

// ==================== COMPARATIVE BENCHMARKING ====================

typedef struct ComparativeBenchmark {
    Index** indexes;
    int index_count;
    BenchmarkResult** results;
    BenchmarkConfig config;
    char* benchmark_name;
    time_t timestamp;
} ComparativeBenchmark;

ComparativeBenchmark* create_comparative_benchmark(const char* name, 
                                                  Index** indexes, 
                                                  int index_count,
                                                  BenchmarkConfig* config) {
    if (!name || !indexes || index_count <= 0 || !config) {
        return NULL;
    }
    
    ComparativeBenchmark* cb = (ComparativeBenchmark*)malloc(sizeof(ComparativeBenchmark));
    if (!cb) return NULL;
    
    cb->benchmark_name = strdup(name);
    cb->indexes = (Index**)malloc(index_count * sizeof(Index*));
    cb->results = (BenchmarkResult**)calloc(index_count, sizeof(BenchmarkResult*));
    
    if (!cb->benchmark_name || !cb->indexes || !cb->results) {
        free(cb->benchmark_name);
        free(cb->indexes);
        free(cb->results);
        free(cb);
        return NULL;
    }
    
    memcpy(cb->indexes, indexes, index_count * sizeof(Index*));
    cb->index_count = index_count;
    cb->config = *config;
    cb->timestamp = time(NULL);
    
    return cb;
}

ErrorCode run_comparative_benchmark(ComparativeBenchmark* cb,
                                   BenchmarkOperation* operations,
                                   int operation_count) {
    if (!cb || !operations || operation_count <= 0) {
        return ERROR_INVALID_INPUT;
    }
    
    printf("\n=== Running Comparative Benchmark: %s ===\n", cb->benchmark_name);
    printf("Indexes: %d, Operations: %d\n", cb->index_count, operation_count);
    printf("Threads: %d\n", cb->config.num_threads);
    printf("=============================================\n");
    
    for (int i = 0; i < cb->index_count; i++) {
        printf("\nBenchmarking %s (%s)...\n", 
               cb->indexes[i]->field_name,
               index_type_to_string(cb->indexes[i]->type));
        
        if (cb->config.num_threads > 1) {
            cb->results[i] = benchmark_index_multi_thread(cb->indexes[i], 
                                                         operations, 
                                                         operation_count,
                                                         &cb->config);
        } else {
            cb->results[i] = benchmark_index_single_thread(cb->indexes[i],
                                                          operations,
                                                          operation_count,
                                                          &cb->config);
        }
        
        if (!cb->results[i]) {
            printf("Failed to benchmark index %d\n", i);
            continue;
        }
        
        // Print quick results
        printf("  Insert: %.3f sec (%.0f ops/sec)\n",
               cb->results[i]->insert_time,
               cb->results[i]->throughput_inserts_per_sec);
        printf("  Search: %.3f sec (%.0f ops/sec)\n",
               cb->results[i]->search_time,
               cb->results[i]->throughput_searches_per_sec);
        printf("  Memory: %.2f MB\n",
               cb->results[i]->memory_usage / (1024.0 * 1024.0));
    }
    
    return SUCCESS;
}

void print_comparative_results(ComparativeBenchmark* cb) {
    if (!cb || !cb->results) return;
    
    printf("\n\n=== COMPARATIVE BENCHMARK RESULTS: %s ===\n", cb->benchmark_name);
    printf("Generated: %s", ctime(&cb->timestamp));
    printf("================================================================================\n");
    printf("%-15s %-10s %-12s %-12s %-12s %-10s %-10s\n",
           "Index", "Type", "Inserts/s", "Searches/s", "Avg Search", "Memory", "Thread Safe");
    printf("--------------------------------------------------------------------------------\n");
    
    for (int i = 0; i < cb->index_count; i++) {
        if (!cb->results[i]) continue;
        
        printf("%-15s %-10s %-12.0f %-12.0f %-12.6f %-10.2f %-10s\n",
               cb->indexes[i]->field_name,
               index_type_to_string(cb->results[i]->type),
               cb->results[i]->throughput_inserts_per_sec,
               cb->results[i]->throughput_searches_per_sec,
               cb->results[i]->average_search_time,
               cb->results[i]->memory_usage / (1024.0 * 1024.0),
               cb->results[i]->thread_safe ? "Yes" : "No");
    }
    printf("================================================================================\n");
    
    // Find winners
    int best_insert_idx = -1, best_search_idx = -1, best_memory_idx = -1;
    double best_insert_throughput = -1, best_search_throughput = -1, best_memory = INFINITY;
    
    for (int i = 0; i < cb->index_count; i++) {
        if (!cb->results[i]) continue;
        
        if (cb->results[i]->throughput_inserts_per_sec > best_insert_throughput) {
            best_insert_throughput = cb->results[i]->throughput_inserts_per_sec;
            best_insert_idx = i;
        }
        
        if (cb->results[i]->throughput_searches_per_sec > best_search_throughput) {
            best_search_throughput = cb->results[i]->throughput_searches_per_sec;
            best_search_idx = i;
        }
        
        if (cb->results[i]->memory_usage < best_memory) {
            best_memory = cb->results[i]->memory_usage;
            best_memory_idx = i;
        }
    }
    
    printf("\nWINNERS:\n");
    if (best_insert_idx >= 0) {
        printf("  Best Insert Performance: %s (%.0f ops/sec)\n",
               cb->indexes[best_insert_idx]->field_name,
               best_insert_throughput);
    }
    
    if (best_search_idx >= 0) {
        printf("  Best Search Performance: %s (%.0f ops/sec)\n",
               cb->indexes[best_search_idx]->field_name,
               best_search_throughput);
    }
    
    if (best_memory_idx >= 0) {
        printf("  Best Memory Efficiency: %s (%.2f MB)\n",
               cb->indexes[best_memory_idx]->field_name,
               best_memory / (1024.0 * 1024.0));
    }
}

void export_benchmark_results_csv(ComparativeBenchmark* cb, const char* filename) {
    if (!cb || !filename) return;
    
    FILE* file = fopen(filename, "w");
    if (!file) {
        printf("Error opening file for export: %s\n", filename);
        return;
    }
    
    // Write header
    fprintf(file, "Benchmark Name,Index Name,Index Type,Insert Time,Search Time,Delete Time,");
    fprintf(file, "Insert Count,Search Count,Delete Count,Avg Search Time,Avg Insert Time,");
    fprintf(file, "Max Search Time,Max Insert Time,Throughput Insert/s,Throughput Search/s,");
    fprintf(file, "Memory Usage,Peak Memory Usage,CPU Usage,Thread Safe,Error Count\n");
    
    // Write data
    for (int i = 0; i < cb->index_count; i++) {
        if (!cb->results[i]) continue;
        
        fprintf(file, "%s,%s,%s,%.6f,%.6f,%.6f,%lld,%lld,%lld,%.6f,%.6f,%.6f,%.6f,%.2f,%.2f,%zu,%zu,%.2f,%s,%d\n",
                cb->benchmark_name,
                cb->results[i]->index_name,
                index_type_to_string(cb->results[i]->type),
                cb->results[i]->insert_time,
                cb->results[i]->search_time,
                cb->results[i]->delete_time,
                cb->results[i]->insert_count,
                cb->results[i]->search_count,
                cb->results[i]->delete_count,
                cb->results[i]->average_search_time,
                cb->results[i]->average_insert_time,
                cb->results[i]->max_search_time,
                cb->results[i]->max_insert_time,
                cb->results[i]->throughput_inserts_per_sec,
                cb->results[i]->throughput_searches_per_sec,
                cb->results[i]->memory_usage,
                cb->results[i]->peak_memory_usage,
                cb->results[i]->cpu_usage_percent,
                cb->results[i]->thread_safe ? "Yes" : "No",
                cb->results[i]->error_count);
    }
    
    fclose(file);
    printf("Results exported to %s\n", filename);
}

void free_comparative_benchmark(ComparativeBenchmark* cb) {
    if (!cb) return;
    
    for (int i = 0; i < cb->index_count; i++) {
        if (cb->results[i]) {
            free(cb->results[i]->index_name);
            free(cb->results[i]);
        }
    }
    
    free(cb->benchmark_name);
    free(cb->indexes);
    free(cb->results);
    free(cb);
}

void free_benchmark_result(BenchmarkResult* result) {
    if (!result) return;
    
    free(result->index_name);
    free(result);
}

// ==================== SCENARIO-BASED BENCHMARKS ====================

BenchmarkResult* benchmark_scenario_insert_only(Index* index, int num_operations, 
                                               int max_key, bool random_keys) {
    BenchmarkConfig config = {
        .data_size = num_operations,
        .num_operations = num_operations,
        .num_threads = 1,
        .measure_memory = true,
        .measure_cpu = false,
        .run_warmup = true,
        .verbose = false,
        .random_seed = 42,
        .search_ratio = 0.0f,
        .insert_ratio = 1.0f,
        .delete_ratio = 0.0f,
        .shuffle_operations = false,
        .batch_size = 1000,
        .repeat_count = 1
    };
    
    int* keys = random_keys ? 
        generate_random_keys(num_operations, 1, max_key, config.random_seed) :
        generate_sequential_keys(num_operations, 1);
    
    if (!keys) return NULL;
    
    // Create dummy records
    Record** records = (Record**)malloc(num_operations * sizeof(Record*));
    if (!records) {
        free(keys);
        return NULL;
    }
    
    for (int i = 0; i < num_operations; i++) {
        records[i] = (Record*)malloc(sizeof(Record));
        if (records[i]) {
            records[i]->id = keys[i];
            // Initialize other fields as needed
        }
    }
    
    BenchmarkOperation* ops = generate_operations(num_operations, keys, records,
                                                 1.0f, 0.0f, 0.0f, false);
    
    BenchmarkResult* result = benchmark_index_single_thread(index, ops, num_operations, &config);
    
    // Cleanup
    free(keys);
    for (int i = 0; i < num_operations; i++) free(records[i]);
    free(records);
    free(ops);
    
    return result;
}

BenchmarkResult* benchmark_scenario_search_heavy(Index* index, int num_operations,
                                                int data_size, float search_ratio) {
    // First, populate the index with data
    int* initial_keys = generate_random_keys(data_size, 1, data_size * 10, 42);
    Record** initial_records = (Record**)malloc(data_size * sizeof(Record*));
    
    if (!initial_keys || !initial_records) {
        free(initial_keys);
        return NULL;
    }
    
    for (int i = 0; i < data_size; i++) {
        initial_records[i] = (Record*)malloc(sizeof(Record));
        if (initial_records[i]) {
            initial_records[i]->id = initial_keys[i];
            index_insert(index, initial_keys[i], initial_records[i]);
        }
    }
    
    // Now run search-heavy benchmark
    BenchmarkConfig config = {
        .data_size = data_size,
        .num_operations = num_operations,
        .num_threads = 1,
        .measure_memory = true,
        .measure_cpu = false,
        .run_warmup = true,
        .verbose = false,
        .random_seed = 123,
        .search_ratio = search_ratio,
        .insert_ratio = (1.0f - search_ratio) / 2,
        .delete_ratio = (1.0f - search_ratio) / 2,
        .shuffle_operations = true,
        .batch_size = 1000,
        .repeat_count = 1
    };
    
    int* benchmark_keys = generate_random_keys(num_operations, 1, data_size * 10, config.random_seed);
    Record** benchmark_records = (Record**)malloc(num_operations * sizeof(Record*));
    
    if (!benchmark_keys || !benchmark_records) {
        free(initial_keys);
        for (int i = 0; i < data_size; i++) free(initial_records[i]);
        free(initial_records);
        free(benchmark_keys);
        return NULL;
    }
    
    for (int i = 0; i < num_operations; i++) {
        benchmark_records[i] = (Record*)malloc(sizeof(Record));
        if (benchmark_records[i]) {
            benchmark_records[i]->id = benchmark_keys[i];
        }
    }
    
    BenchmarkOperation* ops = generate_operations(num_operations, benchmark_keys, 
                                                 benchmark_records,
                                                 config.insert_ratio,
                                                 config.search_ratio,
                                                 config.delete_ratio,
                                                 true);
    
    BenchmarkResult* result = benchmark_index_single_thread(index, ops, num_operations, &config);
    
    // Cleanup
    free(initial_keys);
    for (int i = 0; i < data_size; i++) free(initial_records[i]);
    free(initial_records);
    free(benchmark_keys);
    for (int i = 0; i < num_operations; i++) free(benchmark_records[i]);
    free(benchmark_records);
    free(ops);
    
    return result;
}

// ==================== MAIN BENCHMARKING API ====================

ErrorCode run_comprehensive_benchmark_suite(const char* output_dir) {
    if (!output_dir) return ERROR_INVALID_INPUT;
    
    printf("\n=== RUNNING COMPREHENSIVE BENCHMARK SUITE ===\n");
    
    // Create test indexes
    Index* hash_index = create_hash_index("benchmark_table", "hash_index");
    Index* btree_index = create_btree_index("benchmark_table", "btree_index", 4);
    Index* skiplist_index = create_skiplist_index("benchmark_table", "skiplist_index", 16);
    
    if (!hash_index || !btree_index || !skiplist_index) {
        printf("Failed to create test indexes\n");
        index_free(hash_index);
        index_free(btree_index);
        index_free(skiplist_index);
        return ERROR_MEMORY_ALLOCATION;
    }
    
    Index* indexes[] = {hash_index, btree_index, skiplist_index};
    int index_count = 3;
    
    // Scenario 1: Insert-only benchmark
    printf("\n--- Scenario 1: Insert-Only Benchmark (10,000 ops) ---\n");
    ComparativeBenchmark* insert_bench = create_comparative_benchmark(
        "Insert-Only Benchmark", indexes, index_count, &(BenchmarkConfig){
            .num_operations = 10000,
            .num_threads = 1,
            .measure_memory = true,
            .search_ratio = 0.0f,
            .insert_ratio = 1.0f,
            .delete_ratio = 0.0f
        });
    
    if (insert_bench) {
        int* keys = generate_random_keys(10000, 1, 1000000, 42);
        Record** records = (Record**)malloc(10000 * sizeof(Record*));
        
        if (keys && records) {
            for (int i = 0; i < 10000; i++) {
                records[i] = (Record*)malloc(sizeof(Record));
                if (records[i]) records[i]->id = keys[i];
            }
            
            BenchmarkOperation* ops = generate_operations(10000, keys, records,
                                                         1.0f, 0.0f, 0.0f, false);
            
            run_comparative_benchmark(insert_bench, ops, 10000);
            print_comparative_results(insert_bench);
            
            char filename[256];
            snprintf(filename, sizeof(filename), "%s/insert_benchmark.csv", output_dir);
            export_benchmark_results_csv(insert_bench, filename);
            
            free(ops);
        }
        
        free(keys);
        if (records) {
            for (int i = 0; i < 10000; i++) free(records[i]);
            free(records);
        }
        
        free_comparative_benchmark(insert_bench);
    }
    
    // Scenario 2: Mixed workload benchmark
    printf("\n--- Scenario 2: Mixed Workload Benchmark (20,000 ops) ---\n");
    ComparativeBenchmark* mixed_bench = create_comparative_benchmark(
        "Mixed Workload Benchmark", indexes, index_count, &(BenchmarkConfig){
            .num_operations = 20000,
            .num_threads = 1,
            .measure_memory = true,
            .search_ratio = 0.6f,
            .insert_ratio = 0.3f,
            .delete_ratio = 0.1f,
            .shuffle_operations = true
        });
    
    if (mixed_bench) {
        int* keys = generate_random_keys(20000, 1, 1000000, 123);
        Record** records = (Record**)malloc(20000 * sizeof(Record*));
        
        if (keys && records) {
            for (int i = 0; i < 20000; i++) {
                records[i] = (Record*)malloc(sizeof(Record));
                if (records[i]) records[i]->id = keys[i];
            }
            
            BenchmarkOperation* ops = generate_operations(20000, keys, records,
                                                         0.3f, 0.6f, 0.1f, true);
            
            run_comparative_benchmark(mixed_bench, ops, 20000);
            print_comparative_results(mixed_bench);
            
            char filename[256];
            snprintf(filename, sizeof(filename), "%s/mixed_benchmark.csv", output_dir);
            export_benchmark_results_csv(mixed_bench, filename);
            
            free(ops);
        }
        
        free(keys);
        if (records) {
            for (int i = 0; i < 20000; i++) free(records[i]);
            free(records);
        }
        
        free_comparative_benchmark(mixed_bench);
    }
    
    // Scenario 3: Multi-threaded benchmark
    printf("\n--- Scenario 3: Multi-Threaded Benchmark (4 threads) ---\n");
    ComparativeBenchmark* threaded_bench = create_comparative_benchmark(
        "Multi-Threaded Benchmark", indexes, index_count, &(BenchmarkConfig){
            .num_operations = 40000,
            .num_threads = 4,
            .measure_memory = true,
            .search_ratio = 0.5f,
            .insert_ratio = 0.3f,
            .delete_ratio = 0.2f,
            .shuffle_operations = true
        });
    
    if (threaded_bench) {
        int* keys = generate_random_keys(40000, 1, 1000000, 456);
        Record** records = (Record**)malloc(40000 * sizeof(Record*));
        
        if (keys && records) {
            for (int i = 0; i < 40000; i++) {
                records[i] = (Record*)malloc(sizeof(Record));
                if (records[i]) records[i]->id = keys[i];
            }
            
            BenchmarkOperation* ops = generate_operations(40000, keys, records,
                                                         0.3f, 0.5f, 0.2f, true);
            
            run_comparative_benchmark(threaded_bench, ops, 40000);
            print_comparative_results(threaded_bench);
            
            char filename[256];
            snprintf(filename, sizeof(filename), "%s/threaded_benchmark.csv", output_dir);
            export_benchmark_results_csv(threaded_bench, filename);
            
            free(ops);
        }
        
        free(keys);
        if (records) {
            for (int i = 0; i < 40000; i++) free(records[i]);
            free(records);
        }
        
        free_comparative_benchmark(threaded_bench);
    }
    
    // Cleanup indexes
    index_free(hash_index);
    index_free(btree_index);
    index_free(skiplist_index);
    
    printf("\n=== BENCHMARK SUITE COMPLETED ===\n");
    printf("Results exported to: %s\n", output_dir);
    
    return SUCCESS;
}

// ==================== PUBLIC BENCHMARKING API ====================

BenchmarkResult* benchmark_index(Index* index, BenchmarkConfig* config) {
    if (!index || !config) return NULL;
    
    // Generate test data
    int* keys = generate_random_keys(config->num_operations, 1, 
                                    config->data_size * 10, config->random_seed);
    if (!keys) return NULL;
    
    Record** records = (Record**)malloc(config->num_operations * sizeof(Record*));
    if (!records) {
        free(keys);
        return NULL;
    }
    
    for (int i = 0; i < config->num_operations; i++) {
        records[i] = (Record*)malloc(sizeof(Record));
        if (records[i]) {
            records[i]->id = keys[i];
        }
    }
    
    // Generate operations
    BenchmarkOperation* ops = generate_operations(config->num_operations, keys, records,
                                                 config->insert_ratio,
                                                 config->search_ratio,
                                                 config->delete_ratio,
                                                 config->shuffle_operations);
    
    BenchmarkResult* result = NULL;
    
    if (config->num_threads > 1) {
        result = benchmark_index_multi_thread(index, ops, config->num_operations, config);
    } else {
        result = benchmark_index_single_thread(index, ops, config->num_operations, config);
    }
    
    // Cleanup
    free(keys);
    for (int i = 0; i < config->num_operations; i++) free(records[i]);
    free(records);
    free(ops);
    
    return result;
}

void print_benchmark_result(BenchmarkResult* result) {
    if (!result) {
        printf("No benchmark results available.\n");
        return;
    }
    
    printf("\n=== BENCHMARK RESULTS: %s ===\n", result->index_name);
    printf("Index Type: %s\n", index_type_to_string(result->type));
    printf("Thread Safe: %s\n", result->thread_safe ? "Yes" : "No");
    printf("\nTIMING:\n");
    printf("  Total Insert Time: %.6f seconds\n", result->insert_time);
    printf("  Total Search Time: %.6f seconds\n", result->search_time);
    printf("  Total Delete Time: %.6f seconds\n", result->delete_time);
    printf("  Average Insert Time: %.6f seconds\n", result->average_insert_time);
    printf("  Average Search Time: %.6f seconds\n", result->average_search_time);
    printf("  Max Insert Time: %.6f seconds\n", result->max_insert_time);
    printf("  Max Search Time: %.6f seconds\n", result->max_search_time);
    
    printf("\nTHROUGHPUT:\n");
    printf("  Insert Throughput: %.2f operations/second\n", result->throughput_inserts_per_sec);
    printf("  Search Throughput: %.2f operations/second\n", result->throughput_searches_per_sec);
    
    printf("\nCOUNTS:\n");
    printf("  Insert Operations: %lld\n", result->insert_count);
    printf("  Search Operations: %lld\n", result->search_count);
    printf("  Delete Operations: %lld\n", result->delete_count);
    printf("  Errors: %d\n", result->error_count);
    
    printf("\nMEMORY:\n");
    printf("  Memory Usage: %.2f MB\n", result->memory_usage / (1024.0 * 1024.0));
    printf("  Peak Memory Usage: %.2f MB\n", result->peak_memory_usage / (1024.0 * 1024.0));
    
    if (result->cpu_usage_percent > 0) {
        printf("  CPU Usage: %.2f%%\n", result->cpu_usage_percent);
    }
    
    printf("========================================\n");
}

// ==================== TEST AND VALIDATION ====================

ErrorCode validate_index_correctness(Index* index, int num_items) {
    if (!index || num_items <= 0) return ERROR_INVALID_INPUT;
    
    printf("Validating index correctness for %s...\n", index->field_name);
    
    // Insert items
    int* keys = generate_sequential_keys(num_items, 1);
    Record** records = (Record**)malloc(num_items * sizeof(Record*));
    
    if (!keys || !records) {
        free(keys);
        return ERROR_MEMORY_ALLOCATION;
    }
    
    for (int i = 0; i < num_items; i++) {
        records[i] = (Record*)malloc(sizeof(Record));
        if (records[i]) {
            records[i]->id = keys[i];
            ErrorCode err = index_insert(index, keys[i], records[i]);
            if (err != SUCCESS) {
                printf("  Error inserting key %d: %d\n", keys[i], err);
                // Cleanup
                for (int j = 0; j <= i; j++) free(records[j]);
                free(records);
                free(keys);
                return err;
            }
        }
    }
    
    // Search for all items
    int missing_count = 0;
    for (int i = 0; i < num_items; i++) {
        Record* found = index_search(index, keys[i]);
        if (!found || found->id != keys[i]) {
            missing_count++;
            printf("  Missing key %d\n", keys[i]);
        }
    }
    
    // Delete all items
    int delete_errors = 0;
    for (int i = 0; i < num_items; i++) {
        ErrorCode err = index_delete(index, keys[i]);
        if (err != SUCCESS) {
            delete_errors++;
            printf("  Error deleting key %d: %d\n", keys[i], err);
        }
    }
    
    // Verify all items are deleted
    int remaining_count = 0;
    for (int i = 0; i < num_items; i++) {
        Record* found = index_search(index, keys[i]);
        if (found) {
            remaining_count++;
        }
    }
    
    // Cleanup
    for (int i = 0; i < num_items; i++) free(records[i]);
    free(records);
    free(keys);
    
    // Report results
    printf("  Validation results:\n");
    printf("    Inserted: %d items\n", num_items);
    printf("    Missing during search: %d items\n", missing_count);
    printf("    Delete errors: %d\n", delete_errors);
    printf("    Remaining after delete: %d items\n", remaining_count);
    
    if (missing_count == 0 && delete_errors == 0 && remaining_count == 0) {
        printf("  VALIDATION PASSED\n");
        return SUCCESS;
    } else {
        printf("  VALIDATION FAILED\n");
        return ERROR_GENERIC;
    }
}

// ==================== PERFORMANCE PROFILING ====================

typedef struct PerformanceProfile {
    char* operation_name;
    double total_time;
    int call_count;
    double average_time;
    double min_time;
    double max_time;
    struct PerformanceProfile* next;
} PerformanceProfile;

typedef struct Profiler {
    PerformanceProfile* profiles;
    bool enabled;
    pthread_mutex_t lock;
} Profiler;

static Profiler global_profiler = {0};

void profiler_init() {
    global_profiler.enabled = true;
    global_profiler.profiles = NULL;
    pthread_mutex_init(&global_profiler.lock, NULL);
}

void profiler_start_operation(const char* name) {
    if (!global_profiler.enabled) return;
    
    pthread_mutex_lock(&global_profiler.lock);
    
    // Find or create profile
    PerformanceProfile* profile = global_profiler.profiles;
    while (profile) {
        if (strcmp(profile->operation_name, name) == 0) break;
        profile = profile->next;
    }
    
    if (!profile) {
        profile = (PerformanceProfile*)malloc(sizeof(PerformanceProfile));
        if (profile) {
            profile->operation_name = strdup(name);
            profile->total_time = 0;
            profile->call_count = 0;
            profile->average_time = 0;
            profile->min_time = INFINITY;
            profile->max_time = 0;
            profile->next = global_profiler.profiles;
            global_profiler.profiles = profile;
        }
    }
    
    pthread_mutex_unlock(&global_profiler.lock);
    
    if (profile) {
        profile->call_count++;
        // In a real implementation, store start time per thread
    }
}

void profiler_end_operation(const char* name, double elapsed_time) {
    if (!global_profiler.enabled) return;
    
    pthread_mutex_lock(&global_profiler.lock);
    
    PerformanceProfile* profile = global_profiler.profiles;
    while (profile) {
        if (strcmp(profile->operation_name, name) == 0) break;
        profile = profile->next;
    }
    
    if (profile) {
        profile->total_time += elapsed_time;
        profile->average_time = profile->total_time / profile->call_count;
        
        if (elapsed_time < profile->min_time) profile->min_time = elapsed_time;
        if (elapsed_time > profile->max_time) profile->max_time = elapsed_time;
    }
    
    pthread_mutex_unlock(&global_profiler.lock);
}

void profiler_print_results() {
    if (!global_profiler.enabled) return;
    
    pthread_mutex_lock(&global_profiler.lock);
    
    printf("\n=== PERFORMANCE PROFILING RESULTS ===\n");
    printf("%-30s %-10s %-12s %-12s %-12s %-12s\n",
           "Operation", "Calls", "Total Time", "Avg Time", "Min Time", "Max Time");
    printf("----------------------------------------------------------------------------\n");
    
    PerformanceProfile* profile = global_profiler.profiles;
    while (profile) {
        printf("%-30s %-10d %-12.6f %-12.6f %-12.6f %-12.6f\n",
               profile->operation_name,
               profile->call_count,
               profile->total_time,
               profile->average_time,
               profile->min_time,
               profile->max_time);
        profile = profile->next;
    }
    
    pthread_mutex_unlock(&global_profiler.lock);
}

void profiler_cleanup() {
    pthread_mutex_lock(&global_profiler.lock);
    
    PerformanceProfile* profile = global_profiler.profiles;
    while (profile) {
        PerformanceProfile* next = profile->next;
        free(profile->operation_name);
        free(profile);
        profile = next;
    }
    
    global_profiler.profiles = NULL;
    pthread_mutex_unlock(&global_profiler.lock);
    pthread_mutex_destroy(&global_profiler.lock);
}

// ==================== END OF FILE ====================