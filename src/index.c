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

// ==================== INTERNAL MACROS ====================
#define MAX_LEVEL 32
#define P 0.25  // Probability for skip list
#define BTREE_MIN_DEGREE 2
#define BTREE_MAX_KEYS (2 * BTREE_MIN_DEGREE - 1)

// ==================== INTERNAL STRUCTURES ====================

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

typedef struct FullTextIndex {
    FullTextIndexNode* root;
    int term_count;
    int total_occurrences;
    pthread_rwlock_t lock;
} FullTextIndex;

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

typedef struct SpatialIndex {
    SpatialIndexNode* root;
    int node_count;
    int max_capacity;
    pthread_rwlock_t lock;
} SpatialIndex;

// ==================== HASH INDEX ENHANCEMENTS ====================

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
    
    tree->t = MAX(degree, BTREE_MIN_DEGREE);
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

static FullTextIndex* fulltext_index_create() {
    FullTextIndex* index = (FullTextIndex*)malloc(sizeof(FullTextIndex));
    if (!index) return NULL;
    
    index->root = NULL;
    index->term_count = 0;
    index->total_occurrences = 0;
    pthread_rwlock_init(&index->lock, NULL);
    
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
    double new_min_x = MIN(node->min_x, x);
    double new_min_y = MIN(node->min_y, y);
    double new_max_x = MAX(node->max_x, x);
    double new_max_y = MAX(node->max_y, y);
    
    double old_area = spatial_area(node->min_x, node->min_y, node->max_x, node->max_y);
    double new_area = spatial_area(new_min_x, new_min_y, new_max_x, new_max_y);
    
    return new_area - old_area;
}

static SpatialIndex* spatial_index_create(int max_capacity) {
    SpatialIndex* index = (SpatialIndex*)malloc(sizeof(SpatialIndex));
    if (!index) return NULL;
    
    index->max_capacity = MAX(max_capacity, 4);
    index->root = spatial_create_node(true, index->max_capacity);
    index->node_count = 1;
    
    if (!index->root) {
        free(index);
        return NULL;
    }
    
    pthread_rwlock_init(&index->lock, NULL);
    return index;
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
        
        LOG_DEBUG("Hash index compacted from %d to %d", hash->capacity, optimal_capacity);
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
        
        LOG_DEBUG("B-tree root rebalanced");
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
    
    if (result == SUCCESS) {
        LOG_DEBUG("Index %s optimized successfully", index->field_name);
    }
    
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
    if (result != SUCCESS && result != NOT_FOUND) {
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
                bitmap_index_free(index->impl.bitmap);
            }
            break;
        case INDEX_FULLTEXT:
            if (index->impl.fulltext) {
                fulltext_index_free(index->impl.fulltext);
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
        case INDEX_NONE: return "NONE";
        case INDEX_HASH: return "HASH";
        case INDEX_BTREE: return "BTREE";
        case INDEX_SKIPLIST: return "SKIPLIST";
        case INDEX_BITMAP: return "BITMAP";
        case INDEX_FULLTEXT: return "FULLTEXT";
        default: return "UNKNOWN";
    }
}

// ==================== INDEX BENCHMARKING ====================

typedef struct BenchmarkResult {
    char* index_name;
    IndexType type;
    double insert_time;
    double search_time;
    double delete_time;
    size_t memory_usage;
    int operations_completed;
} BenchmarkResult;

BenchmarkResult* benchmark_index(Index* index, int operation_count) {
    if (!index || operation_count <= 0) return NULL;
    
    BenchmarkResult* result = (BenchmarkResult*)malloc(sizeof(BenchmarkResult));
    if (!result) return NULL;
    
    result->index_name = strdup(index->field_name);
    result->type = index->type;
    
    // Benchmark insert
    clock_t start = clock();
    for (int i = 0; i < operation_count; i++) {
        // Create dummy record for testing
        Record* dummy = (Record*)malloc(sizeof(Record));
        if (dummy) {
            dummy->id = i;
            index_insert(index, i, dummy);
            // Don't actually keep the dummy record
            free(dummy);
        }
    }
    result->insert_time = (double)(clock() - start) / CLOCKS_PER_SEC;
    
    // Benchmark search
    start = clock();
    for (int i = 0; i < operation_count; i++) {
        index_search(index, i);
    }
    result->search_time = (double)(clock() - start) / CLOCKS_PER_SEC;
    
    // Benchmark delete
    start = clock();
    for (int i = 0; i < operation_count; i++) {
        index_delete(index, i);
    }
    result->delete_time = (double)(clock() - start) / CLOCKS_PER_SEC;
    
    result->memory_usage = index_get_memory_usage(index);
    result->operations_completed = operation_count;
    
    return result;
}

void print_benchmark_results(BenchmarkResult* result) {
    if (!result) return;
    
    printf("\n=== Benchmark Results: %s ===\n", result->index_name);
    printf("Type: %s\n", index_type_to_string(result->type));
    printf("Operations: %d\n", result->operations_completed);
    printf("Insert Time: %.6f seconds (%.0f ops/sec)\n", 
           result->insert_time, result->operations_completed / result->insert_time);
    printf("Search Time: %.6f seconds (%.0f ops/sec)\n", 
           result->search_time, result->operations_completed / result->search_time);
    printf("Delete Time: %.6f seconds (%.0f ops/sec)\n", 
           result->delete_time, result->operations_completed / result->delete_time);
    printf("Memory Usage: %zu bytes\n", result->memory_usage);
    printf("===================================\n");
}

// ==================== INDEX SELECTION HEURISTICS ====================

IndexType recommend_index_type(int data_size, FieldType field_type, 
                              QueryPattern pattern, int expected_operations) {
    // Simple heuristic for index type recommendation
    
    if (field_type == TYPE_STRING && pattern == QUERY_PATTERN_FULLTEXT) {
        return INDEX_FULLTEXT;
    }
    
    if (data_size < 1000) {
        // Small datasets: hash index is usually fastest
        return INDEX_HASH;
    } else if (pattern == QUERY_PATTERN_RANGE) {
        // Range queries: B-tree or skip list
        if (expected_operations > 1000000) {
            return INDEX_SKIPLIST;  // Better for concurrent operations
        } else {
            return INDEX_BTREE;
        }
    } else if (pattern == QUERY_PATTERN_EQUALITY) {
        // Equality lookups: hash table
        return INDEX_HASH;
    } else if (field_type == TYPE_BOOL || field_type == TYPE_INT && data_size < 100) {
        // Low cardinality: bitmap index
        return INDEX_BITMAP;
    } else {
        // Default: B-tree
        return INDEX_BTREE;
    }
}