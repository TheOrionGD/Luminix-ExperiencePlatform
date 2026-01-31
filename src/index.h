#ifndef INDEX_H
#define INDEX_H

#include "config.h"
#include "database.h"
#include <pthread.h>
#include <stdbool.h>
#include <time.h>

// ==================== FORWARD DECLARATIONS ====================
typedef struct Index Index;
typedef struct IndexManager IndexManager;
typedef struct HashIndex HashIndex;
typedef struct BTreeIndex BTreeIndex;
typedef struct SkipListIndex SkipListIndex;
typedef struct BitmapIndex BitmapIndex;
typedef struct FullTextIndex FullTextIndex;
typedef struct SpatialIndex SpatialIndex;
typedef struct CompositeIndex CompositeIndex;
typedef struct ConcurrentHashIndex ConcurrentHashIndex;
typedef struct EnhancedBTree EnhancedBTree;
typedef struct DeterministicSkipList DeterministicSkipList;
typedef struct IndexStatistics IndexStatistics;
typedef struct BenchmarkResult BenchmarkResult;

// ==================== INDEX TYPES ====================
typedef enum {
    INDEX_NONE = 0,
    INDEX_HASH,
    INDEX_BTREE,
    INDEX_SKIPLIST,
    INDEX_BITMAP,
    INDEX_FULLTEXT,
    INDEX_SPATIAL,
    INDEX_COMPOSITE,
    INDEX_CONCURRENT_HASH,
    INDEX_ENHANCED_BTREE,
    INDEX_DETERMINISTIC_SKIPLIST
} IndexType;

// ==================== QUERY PATTERNS ====================
typedef enum {
    QUERY_PATTERN_UNKNOWN = 0,
    QUERY_PATTERN_EQUALITY,
    QUERY_PATTERN_RANGE,
    QUERY_PATTERN_PREFIX,
    QUERY_PATTERN_SUFFIX,
    QUERY_PATTERN_CONTAINS,
    QUERY_PATTERN_FULLTEXT,
    QUERY_PATTERN_SPATIAL,
    QUERY_PATTERN_JOIN
} QueryPattern;

// ==================== INDEX CONFIGURATION ====================
typedef struct IndexConfig {
    IndexType type;
    int degree;                    // For B-tree
    int max_level;                 // For skip list
    bool concurrent;               // Enable concurrent access
    bool persistent;               // Persist to disk
    bool compressed;               // Enable compression
    bool unique;                   // Enforce uniqueness
    double load_factor;            // For hash tables
    int initial_capacity;          // Initial size
    bool auto_rebalance;           // Auto-rebalance trees
    bool statistics_enabled;       // Collect statistics
} IndexConfig;

// ==================== INDEX STATISTICS ====================
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
    long long update_count;
    double avg_search_time;
    double avg_insert_time;
    double avg_delete_time;
    double avg_update_time;
    long long cache_hits;
    long long cache_misses;
    int max_depth;                 // For tree indexes
    int node_count;                // For tree indexes
    double fill_factor;            // For hash tables
    double utilization;            // General utilization
} IndexStatistics;

// ==================== HASH INDEX STRUCTURES ====================
typedef struct HashNode {
    int key;                       // Record ID or hash
    Record* record;                // Pointer to record
    time_t timestamp;              // Insertion/update time
    struct HashNode* next;         // For chaining
    struct HashNode* prev;         // For LRU (optional)
} HashNode;

typedef struct HashIndex {
    int capacity;
    int size;
    double load_factor;
    HashNode** buckets;
    pthread_rwlock_t* bucket_locks; // Fine-grained locking
    int lock_count;
    IndexStatistics stats;
} HashIndex;

// ==================== B-TREE STRUCTURES ====================
typedef struct BTreeKey {
    void* data;
    size_t size;
    FieldType type;
    Record* record;
    time_t timestamp;
} BTreeKey;

typedef struct BTreeNode {
    BTreeKey* keys;
    int t;                         // Minimum degree
    int n;                         // Current number of keys
    bool leaf;
    struct BTreeNode** children;
    struct BTreeNode* parent;
    pthread_rwlock_t lock;
    IndexStatistics stats;
} BTreeNode;

typedef struct BTreeIndex {
    BTreeNode* root;
    int t;                         // Minimum degree
    int size;
    FieldType key_type;
    pthread_rwlock_t lock;
    IndexStatistics stats;
} BTreeIndex;

// ==================== SKIP LIST STRUCTURES ====================
typedef struct SkipListNode {
    int key;
    Record* record;
    struct SkipListNode** forward; // Array of forward pointers
    int level;
    time_t timestamp;
    unsigned int access_count;
} SkipListNode;

typedef struct SkipListIndex {
    SkipListNode* header;
    SkipListNode* tail;            // For reverse traversal
    int max_level;
    int level;
    int size;
    double probability;
    pthread_rwlock_t lock;
    IndexStatistics stats;
} SkipListIndex;

// ==================== BITMAP INDEX STRUCTURES ====================
typedef struct BitmapIndex {
    unsigned int* bitmap;
    int size;                      // Number of bits
    int capacity;                  // Capacity in bits
    Table* table;
    char field_name[MAX_FIELD_LEN];
    FieldType field_type;
    pthread_rwlock_t lock;
    IndexStatistics stats;
    
    // For range queries
    int min_value;
    int max_value;
    int* value_map;                // Maps values to bit positions
    int distinct_values;
} BitmapIndex;

// ==================== FULL-TEXT INDEX STRUCTURES ====================
typedef struct FullTextTerm {
    char* term;
    int* document_ids;             // Records containing this term
    int* positions;                // Positions within documents
    int document_count;
    int total_occurrences;
    int capacity;
} FullTextTerm;

typedef struct FullTextIndexNode {
    char* term;
    FullTextTerm* data;
    struct FullTextIndexNode* left;
    struct FullTextIndexNode* right;
    int height;                    // For AVL balancing
} FullTextIndexNode;

typedef struct FullTextIndex {
    FullTextIndexNode* root;
    int term_count;
    int total_documents;
    int total_occurrences;
    pthread_rwlock_t lock;
    IndexStatistics stats;
    
    // Configuration
    bool case_sensitive;
    bool stem_words;
    char** stop_words;
    int stop_word_count;
} FullTextIndex;

// ==================== SPATIAL INDEX STRUCTURES ====================
typedef struct SpatialPoint {
    double x;
    double y;
    int record_id;
    Record* record;
    void* data;                    // Additional spatial data
} SpatialPoint;

typedef struct SpatialBoundingBox {
    double min_x;
    double min_y;
    double max_x;
    double max_y;
} SpatialBoundingBox;

typedef struct SpatialIndexNode {
    SpatialBoundingBox bbox;
    SpatialPoint* points;
    int point_count;
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
    
    struct SpatialIndexNode* parent;
    pthread_rwlock_t lock;
} SpatialIndexNode;

typedef struct SpatialIndex {
    SpatialIndexNode* root;
    int node_count;
    int max_capacity;
    int min_capacity;
    pthread_rwlock_t lock;
    IndexStatistics stats;
    
    // Configuration
    int dimensions;                // 2D or 3D
    bool store_points;             // Store points or just references
    double tolerance;              // For floating point comparisons
} SpatialIndex;

// ==================== COMPOSITE INDEX STRUCTURES ====================
typedef struct CompositeKey {
    Field* fields;
    int field_count;
    unsigned int hash;
    size_t size;
} CompositeKey;

typedef struct CompositeIndex {
    char table_name[MAX_TABLE_NAME];
    char** field_names;
    int field_count;
    FieldType* field_types;
    IndexType index_type;
    
    union {
        HashIndex* hash;
        BTreeIndex* btree;
        SkipListIndex* skiplist;
        void* custom;
    } impl;
    
    pthread_rwlock_t lock;
    IndexStatistics stats;
} CompositeIndex;

// ==================== CONCURRENT HASH INDEX ====================
typedef struct ConcurrentHashIndex {
    HashIndex* hash;
    pthread_rwlock_t* segment_locks;
    int segment_count;
    pthread_rwlock_t global_lock;
    IndexStatistics stats;
} ConcurrentHashIndex;

// ==================== ENHANCED B-TREE ====================
typedef struct EnhancedBTreeNode {
    BTreeKey* keys;
    int t;
    int n;
    bool leaf;
    struct EnhancedBTreeNode** children;
    struct EnhancedBTreeNode* parent;
    pthread_rwlock_t lock;
    IndexStatistics stats;
    
    // Additional features
    bool dirty;                    // For write-back caching
    int access_count;
    time_t last_access;
} EnhancedBTreeNode;

typedef struct EnhancedBTree {
    EnhancedBTreeNode* root;
    int t;
    int size;
    FieldType key_type;
    pthread_rwlock_t lock;
    IndexStatistics stats;
    
    // Cache
    EnhancedBTreeNode** cache;
    int cache_size;
    int cache_capacity;
    
    // Performance
    bool auto_rebalance;
    int rebalance_threshold;
} EnhancedBTree;

// ==================== DETERMINISTIC SKIP LIST ====================
typedef struct DeterministicSkipListNode {
    int key;
    Record* record;
    struct DeterministicSkipListNode** forward;
    struct DeterministicSkipListNode** backward; // For bidirectional
    int level;
    time_t timestamp;
    unsigned int version;          // For optimistic locking
} DeterministicSkipListNode;

typedef struct DeterministicSkipList {
    DeterministicSkipListNode* header;
    DeterministicSkipListNode* tail;
    int max_level;
    int level;
    int size;
    pthread_rwlock_t lock;
    IndexStatistics stats;
    
    // Deterministic properties
    int seed;
    bool balanced;
    int* level_distribution;
} DeterministicSkipList;

// ==================== INDEX MANAGER STRUCTURES ====================
typedef struct IndexEntry {
    Index* index;
    char table_name[MAX_TABLE_NAME];
    char field_name[MAX_FIELD_LEN];
    bool enabled;
    bool rebuilding;
    time_t created_at;
    time_t last_maintenance;
    struct IndexEntry* next;
} IndexEntry;

typedef struct IndexManager {
    IndexEntry** hash_table;       // For quick lookup
    IndexEntry* entries;           // Linked list of all entries
    int count;
    int capacity;
    pthread_rwlock_t lock;
    
    // Statistics
    IndexStatistics global_stats;
    
    // Configuration
    int max_indexes;
    bool auto_maintenance;
    int maintenance_interval;
    
    // Cache
    Index** recent_indexes;
    int recent_count;
    int recent_capacity;
} IndexManager;

// ==================== BENCHMARK STRUCTURES ====================
typedef struct BenchmarkConfig {
    int operation_count;
    int thread_count;
    bool measure_memory;
    bool measure_cpu;
    bool warmup;
    int warmup_iterations;
    char* output_file;
    bool verbose;
} BenchmarkConfig;

typedef struct BenchmarkResult {
    char* index_name;
    IndexType type;
    
    // Timing
    double insert_time;
    double search_time;
    double delete_time;
    double update_time;
    double range_query_time;
    
    // Throughput
    double insert_throughput;
    double search_throughput;
    double delete_throughput;
    
    // Memory
    size_t peak_memory;
    size_t final_memory;
    size_t memory_overhead;
    
    // Concurrency
    double scalability;
    double contention_factor;
    
    // Quality
    double cache_hit_rate;
    double utilization;
    
    // Metadata
    int operations_completed;
    int errors_encountered;
    time_t benchmark_time;
} BenchmarkResult;

// ==================== INDEX API ====================

// Core Index Operations
Index* index_create(const char* table_name, const char* field_name, 
                   IndexType type, IndexConfig* config);
Index* index_create_ex(const char* table_name, const char* field_name,
                      IndexConfig* config);
void index_free(Index* index);

// Data Operations
ErrorCode index_insert(Index* index, int key, Record* record);
ErrorCode index_insert_ex(Index* index, void* key_data, size_t key_size, 
                         Record* record);
Record* index_search(Index* index, int key);
Record* index_search_ex(Index* index, void* key_data, size_t key_size);
ErrorCode index_delete(Index* index, int key);
ErrorCode index_delete_ex(Index* index, void* key_data, size_t key_size);
ErrorCode index_update(Index* index, int old_key, int new_key, Record* record);

// Bulk Operations
ErrorCode index_bulk_insert(Index* index, int* keys, Record** records, int count);
ErrorCode index_bulk_delete(Index* index, int* keys, int count);
ErrorCode index_bulk_update(Index* index, int* old_keys, int* new_keys, 
                           Record** records, int count);

// Range Queries
Record** index_range_query(Index* index, int start_key, int end_key, 
                          int* out_count);
Record** index_range_query_ex(Index* index, void* start_key, void* end_key,
                             int* out_count, ComparatorFunc comparator);
Record** index_prefix_query(Index* index, const char* prefix, int* out_count);
Record** index_suffix_query(Index* index, const char* suffix, int* out_count);

// Full-Text Search
Record** fulltext_search(Index* index, const char* query, int* out_count);
Record** fulltext_search_ex(Index* index, const char* query, 
                           FullTextSearchOptions* options, int* out_count);

// Spatial Queries
Record** spatial_range_query(Index* index, SpatialBoundingBox* bbox, 
                            int* out_count);
Record** spatial_nearest_neighbors(Index* index, SpatialPoint* point, 
                                  int k, int* out_count);
Record** spatial_within_distance(Index* index, SpatialPoint* point, 
                                double distance, int* out_count);

// Statistics and Monitoring
IndexStatistics* index_get_statistics(Index* index);
ErrorCode index_reset_statistics(Index* index);
ErrorCode index_enable_statistics(Index* index, bool enable);
ErrorCode index_collect_garbage(Index* index);

// Maintenance Operations
ErrorCode index_rebuild(Index* index);
ErrorCode index_optimize(Index* index);
ErrorCode index_compact(Index* index);
ErrorCode index_validate(Index* index, bool repair);
ErrorCode index_check_integrity(Index* index);

// Persistence
ErrorCode index_save(Index* index, const char* filename);
Index* index_load(const char* filename, Table* table);
ErrorCode index_export(Index* index, const char* format, const char* filename);
Index* index_import(const char* format, const char* filename, Table* table);

// Configuration
ErrorCode index_reconfigure(Index* index, IndexConfig* config);
IndexConfig* index_get_config(Index* index);
ErrorCode index_set_option(Index* index, const char* option, const void* value);
ErrorCode index_get_option(Index* index, const char* option, void* value, 
                          size_t size);

// Thread Safety
ErrorCode index_lock_read(Index* index);
ErrorCode index_lock_write(Index* index);
ErrorCode index_unlock(Index* index);
ErrorCode index_try_lock_read(Index* index, int timeout_ms);
ErrorCode index_try_lock_write(Index* index, int timeout_ms);

// Memory Management
size_t index_get_memory_usage(Index* index);
ErrorCode index_set_memory_limit(Index* index, size_t limit);
ErrorCode index_clear_cache(Index* index);

// ==================== INDEX TYPE SPECIFIC APIS ====================

// Hash Index
HashIndex* hash_index_create(int capacity, double load_factor);
HashIndex* hash_index_create_ex(IndexConfig* config);
ErrorCode hash_index_resize(HashIndex* hash, int new_capacity);
ErrorCode hash_index_rehash(HashIndex* hash);
double hash_index_get_load_factor(HashIndex* hash);

// B-Tree Index
BTreeIndex* btree_index_create(int degree, FieldType key_type);
BTreeIndex* btree_index_create_ex(IndexConfig* config);
ErrorCode btree_index_rebalance(BTreeIndex* btree);
ErrorCode btree_index_rotate(BTreeIndex* btree, BTreeNode* node);
int btree_index_get_height(BTreeIndex* btree);

// Skip List Index
SkipListIndex* skip_list_index_create(int max_level, double probability);
SkipListIndex* skip_list_index_create_ex(IndexConfig* config);
ErrorCode skip_list_index_rebalance(SkipListIndex* list);
int skip_list_index_get_optimal_level(SkipListIndex* list);

// Bitmap Index
BitmapIndex* bitmap_index_create(Table* table, const char* field_name);
BitmapIndex* bitmap_index_create_ex(IndexConfig* config);
ErrorCode bitmap_index_compress(BitmapIndex* bitmap);
ErrorCode bitmap_index_decompress(BitmapIndex* bitmap);
Record** bitmap_index_and(BitmapIndex* a, BitmapIndex* b, int* out_count);
Record** bitmap_index_or(BitmapIndex* a, BitmapIndex* b, int* out_count);
Record** bitmap_index_not(BitmapIndex* bitmap, int* out_count);

// Full-Text Index
FullTextIndex* fulltext_index_create(bool case_sensitive);
FullTextIndex* fulltext_index_create_ex(IndexConfig* config);
ErrorCode fulltext_index_add_stop_word(FullTextIndex* index, const char* word);
ErrorCode fulltext_index_remove_stop_word(FullTextIndex* index, const char* word);
ErrorCode fulltext_index_stem_words(FullTextIndex* index, bool enable);
ErrorCode fulltext_index_build_dictionary(FullTextIndex* index);

// Spatial Index
SpatialIndex* spatial_index_create(int dimensions, int max_capacity);
SpatialIndex* spatial_index_create_ex(IndexConfig* config);
ErrorCode spatial_index_insert_point(SpatialIndex* index, double x, double y, 
                                    int record_id, Record* record);
ErrorCode spatial_index_insert_point_ex(SpatialIndex* index, double* coordinates,
                                       int dimensions, int record_id, Record* record);
ErrorCode spatial_index_remove_point(SpatialIndex* index, int record_id);
double spatial_index_calculate_distance(SpatialPoint* a, SpatialPoint* b);

// Composite Index
CompositeIndex* composite_index_create(const char* table_name, 
                                      const char** field_names,
                                      FieldType* field_types,
                                      int field_count,
                                      IndexType type);
CompositeIndex* composite_index_create_ex(const char* table_name,
                                         IndexConfig* config);
ErrorCode composite_index_insert(CompositeIndex* index, Field* fields, 
                                Record* record);
Record* composite_index_search(CompositeIndex* index, Field* fields);

// ==================== INDEX MANAGER API ====================

// Lifecycle
IndexManager* index_manager_create();
IndexManager* index_manager_create_ex(int max_indexes);
void index_manager_free(IndexManager* manager);

// Index Management
ErrorCode index_manager_add_index(IndexManager* manager, Index* index);
ErrorCode index_manager_remove_index(IndexManager* manager, const char* table_name,
                                    const char* field_name);
Index* index_manager_get_index(IndexManager* manager, const char* table_name,
                              const char* field_name);
Index** index_manager_get_table_indexes(IndexManager* manager, 
                                       const char* table_name, int* out_count);
ErrorCode index_manager_rebuild_all(IndexManager* manager);
ErrorCode index_manager_optimize_all(IndexManager* manager);
ErrorCode index_manager_vacuum(IndexManager* manager);

// Statistics
IndexStatistics* index_manager_get_statistics(IndexManager* manager);
ErrorCode index_manager_reset_statistics(IndexManager* manager);
ErrorCode index_manager_collect_statistics(IndexManager* manager, 
                                          IndexStatistics* stats);

// Query Optimization
Index* index_manager_select_best_index(IndexManager* manager, 
                                      const char* table_name,
                                      const char* field_name,
                                      QueryPattern pattern);
Index** index_manager_suggest_indexes(IndexManager* manager, 
                                     const char* table_name,
                                     QueryPattern* patterns,
                                     int pattern_count,
                                     int* out_count);

// Maintenance
ErrorCode index_manager_schedule_maintenance(IndexManager* manager, 
                                            time_t interval);
ErrorCode index_manager_cancel_maintenance(IndexManager* manager);
ErrorCode index_manager_run_maintenance(IndexManager* manager);

// ==================== BENCHMARKING API ====================

// Benchmark Execution
BenchmarkResult* benchmark_index_single(Index* index, BenchmarkConfig* config);
BenchmarkResult* benchmark_index_comparison(Index** indexes, int count, 
                                           BenchmarkConfig* config);
BenchmarkResult* benchmark_index_scalability(Index* index, 
                                            BenchmarkConfig* config);

// Result Analysis
ErrorCode benchmark_save_results(BenchmarkResult* result, const char* filename);
BenchmarkResult* benchmark_load_results(const char* filename);
void benchmark_print_results(BenchmarkResult* result);
void benchmark_print_comparison(BenchmarkResult** results, int count);
ErrorCode benchmark_generate_report(BenchmarkResult** results, int count, 
                                   const char* filename);

// Utility Functions
BenchmarkConfig* benchmark_create_default_config();
void benchmark_free_config(BenchmarkConfig* config);
void benchmark_free_result(BenchmarkResult* result);

// ==================== INDEX UTILITIES ====================

// Type Conversion
const char* index_type_to_string(IndexType type);
IndexType string_to_index_type(const char* str);
const char* query_pattern_to_string(QueryPattern pattern);

// Configuration
IndexConfig* index_create_default_config(IndexType type);
void index_free_config(IndexConfig* config);
ErrorCode index_validate_config(IndexConfig* config);

// Statistics
ErrorCode index_calculate_statistics(Index* index, IndexStatistics* stats);
ErrorCode index_compare_statistics(IndexStatistics* a, IndexStatistics* b, 
                                  double* similarity);

// Memory
ErrorCode index_estimate_memory(IndexType type, int entry_count, 
                               FieldType key_type, size_t* estimate);
ErrorCode index_calculate_overhead(Index* index, double* overhead);

// Performance
ErrorCode index_predict_performance(Index* index, QueryPattern pattern, 
                                   int operation_count, double* prediction);
ErrorCode index_measure_latency(Index* index, int iterations, 
                               double* avg_latency, double* p95, double* p99);

// ==================== COMPARATOR FUNCTIONS ====================
typedef int (*ComparatorFunc)(const void* a, const void* b, void* context);

// Built-in comparators
int compare_int(const void* a, const void* b, void* context);
int compare_string(const void* a, const void* b, void* context);
int compare_float(const void* a, const void* b, void* context);
int compare_double(const void* a, const void* b, void* context);
int compare_bool(const void* a, const void* b, void* context);
int compare_timestamp(const void* a, const void* b, void* context);

// Composite comparators
int compare_composite(const void* a, const void* b, void* context);
int compare_fields(const Field* a, const Field* b);

// ==================== FULL-TEXT SEARCH OPTIONS ====================
typedef struct FullTextSearchOptions {
    bool case_sensitive;
    bool exact_match;
    bool use_stemming;
    bool use_synonyms;
    int max_results;
    double min_score;
    char** required_terms;
    int required_term_count;
    char** excluded_terms;
    int excluded_term_count;
    void* user_data;
} FullTextSearchOptions;

// ==================== SPATIAL SEARCH OPTIONS ====================
typedef struct SpatialSearchOptions {
    int max_results;
    double tolerance;
    bool include_distance;
    bool sort_by_distance;
    void* user_data;
} SpatialSearchOptions;

// ==================== INDEX EVENT SYSTEM ====================
typedef enum {
    INDEX_EVENT_CREATED,
    INDEX_EVENT_DELETED,
    INDEX_EVENT_INSERT,
    INDEX_EVENT_DELETE,
    INDEX_EVENT_SEARCH,
    INDEX_EVENT_UPDATE,
    INDEX_EVENT_REBUILD,
    INDEX_EVENT_OPTIMIZE,
    INDEX_EVENT_ERROR
} IndexEventType;

typedef struct IndexEvent {
    IndexEventType type;
    Index* index;
    time_t timestamp;
    void* data;
    size_t data_size;
} IndexEvent;

typedef void (*IndexEventHandler)(IndexEvent* event);

// Event Management
ErrorCode index_register_event_handler(Index* index, IndexEventType event_type,
                                      IndexEventHandler handler);
ErrorCode index_unregister_event_handler(Index* index, IndexEventType event_type,
                                        IndexEventHandler handler);
ErrorCode index_trigger_event(Index* index, IndexEventType event_type,
                             void* data, size_t data_size);

// ==================== INDEX METADATA ====================
typedef struct IndexMetadata {
    char name[64];
    IndexType type;
    char table_name[MAX_TABLE_NAME];
    char field_name[MAX_FIELD_LEN];
    time_t created_at;
    time_t modified_at;
    size_t size_on_disk;
    size_t size_in_memory;
    int version;
    char checksum[64];
    bool valid;
    bool enabled;
} IndexMetadata;

// Metadata Operations
IndexMetadata* index_get_metadata(Index* index);
ErrorCode index_update_metadata(Index* index, IndexMetadata* metadata);
ErrorCode index_validate_metadata(IndexMetadata* metadata);
ErrorCode index_serialize_metadata(IndexMetadata* metadata, char** buffer, 
                                  size_t* size);
ErrorCode index_deserialize_metadata(const char* buffer, size_t size,
                                    IndexMetadata** metadata);

// ==================== INDEX CACHING ====================
typedef struct IndexCacheEntry {
    void* key;
    size_t key_size;
    Record* record;
    time_t timestamp;
    unsigned int access_count;
    struct IndexCacheEntry* next;
    struct IndexCacheEntry* prev;
} IndexCacheEntry;

typedef struct IndexCache {
    IndexCacheEntry** table;
    IndexCacheEntry* head;
    IndexCacheEntry* tail;
    size_t capacity;
    size_t size;
    pthread_rwlock_t lock;
    
    // Statistics
    long long hits;
    long long misses;
    long long evictions;
} IndexCache;

// Cache Management
IndexCache* index_cache_create(size_t capacity);
void index_cache_free(IndexCache* cache);
Record* index_cache_get(IndexCache* cache, void* key, size_t key_size);
ErrorCode index_cache_put(IndexCache* cache, void* key, size_t key_size,
                         Record* record);
ErrorCode index_cache_remove(IndexCache* cache, void* key, size_t key_size);
ErrorCode index_cache_clear(IndexCache* cache);

// ==================== INDEX ITERATORS ====================
typedef struct IndexIterator {
    Index* index;
    void* current;
    void* state;
    bool forward;
    bool valid;
    
    // Methods
    ErrorCode (*next)(struct IndexIterator* it);
    ErrorCode (*prev)(struct IndexIterator* it);
    Record* (*get)(struct IndexIterator* it);
    ErrorCode (*seek)(struct IndexIterator* it, void* key);
    void (*free)(struct IndexIterator* it);
} IndexIterator;

// Iterator Creation
IndexIterator* index_create_iterator(Index* index, bool forward);
IndexIterator* index_create_range_iterator(Index* index, void* start_key,
                                         void* end_key, bool forward);
IndexIterator* index_create_prefix_iterator(Index* index, const char* prefix);

// Iterator Operations
ErrorCode iterator_next(IndexIterator* it);
ErrorCode iterator_prev(IndexIterator* it);
Record* iterator_get(IndexIterator* it);
ErrorCode iterator_seek(IndexIterator* it, void* key);
ErrorCode iterator_seek_first(IndexIterator* it);
ErrorCode iterator_seek_last(IndexIterator* it);
void iterator_free(IndexIterator* it);

// ==================== INDEX TRANSACTIONS ====================
typedef struct IndexTransaction {
    Index* index;
    void* snapshot;
    time_t start_time;
    bool read_only;
    bool committed;
    
    // Changes
    struct {
        void** inserted_keys;
        Record** inserted_values;
        int inserted_count;
        
        void** deleted_keys;
        Record** deleted_values;
        int deleted_count;
        
        void** updated_old_keys;
        void** updated_new_keys;
        Record** updated_values;
        int updated_count;
    } changes;
} IndexTransaction;

// Transaction Management
IndexTransaction* index_begin_transaction(Index* index, bool read_only);
ErrorCode index_commit_transaction(IndexTransaction* trans);
ErrorCode index_rollback_transaction(IndexTransaction* trans);
ErrorCode index_transaction_insert(IndexTransaction* trans, void* key,
                                  size_t key_size, Record* record);
ErrorCode index_transaction_delete(IndexTransaction* trans, void* key,
                                  size_t key_size);
ErrorCode index_transaction_update(IndexTransaction* trans, void* old_key,
                                  void* new_key, size_t key_size, Record* record);

// ==================== INDEX COMPRESSION ====================
typedef enum {
    COMPRESSION_NONE = 0,
    COMPRESSION_LZ4,
    COMPRESSION_ZSTD,
    COMPRESSION_SNAPPY,
    COMPRESSION_BITPACK
} IndexCompressionType;

typedef struct IndexCompression {
    IndexCompressionType type;
    int level;                     // Compression level
    size_t threshold;              // Minimum size to compress
    bool dictionary;               // Use dictionary
    char* dictionary_data;
    size_t dictionary_size;
} IndexCompression;

// Compression Operations
ErrorCode index_compress_data(Index* index, void* data, size_t size,
                             void** compressed, size_t* compressed_size);
ErrorCode index_decompress_data(Index* index, void* compressed, size_t size,
                               void** data, size_t* data_size);
ErrorCode index_enable_compression(Index* index, IndexCompression* config);
ErrorCode index_disable_compression(Index* index);

// ==================== INDEX ENCRYPTION ====================
typedef enum {
    ENCRYPTION_NONE = 0,
    ENCRYPTION_AES_256,
    ENCRYPTION_CHACHA20
} IndexEncryptionType;

typedef struct IndexEncryption {
    IndexEncryptionType type;
    char* key;
    size_t key_size;
    char* iv;
    size_t iv_size;
    bool authenticated;            // AEAD
} IndexEncryption;

// Encryption Operations
ErrorCode index_encrypt_data(Index* index, void* data, size_t size,
                            void** encrypted, size_t* encrypted_size);
ErrorCode index_decrypt_data(Index* index, void* encrypted, size_t size,
                            void** data, size_t* data_size);
ErrorCode index_enable_encryption(Index* index, IndexEncryption* config);
ErrorCode index_disable_encryption(Index* index);

// ==================== INDEX PARTITIONING ====================
typedef struct IndexPartition {
    Index* index;
    void* min_key;
    void* max_key;
    int partition_id;
    struct IndexPartition* next;
} IndexPartition;

typedef struct PartitionedIndex {
    IndexPartition* partitions;
    int partition_count;
    ComparatorFunc comparator;
    pthread_rwlock_t lock;
    
    // Partitioning strategy
    int (*partition_func)(void* key, int partition_count);
} PartitionedIndex;

// Partitioning Operations
PartitionedIndex* index_create_partitioned(IndexType type, int partitions,
                                          ComparatorFunc comparator,
                                          IndexConfig* config);
ErrorCode index_partition_insert(PartitionedIndex* pindex, void* key,
                                size_t key_size, Record* record);
Record* index_partition_search(PartitionedIndex* pindex, void* key,
                              size_t key_size);
ErrorCode index_rebalance_partitions(PartitionedIndex* pindex);

// ==================== INDEX REPLICATION ====================
typedef struct IndexReplica {
    Index* index;
    char* host;
    int port;
    bool synchronous;
    time_t last_sync;
    bool online;
    struct IndexReplica* next;
} IndexReplica;

typedef struct ReplicatedIndex {
    Index* primary;
    IndexReplica* replicas;
    int replica_count;
    pthread_rwlock_t lock;
    
    // Replication settings
    bool async_replication;
    int replication_factor;
    time_t replication_interval;
} ReplicatedIndex;

// Replication Operations
ReplicatedIndex* index_create_replicated(IndexType type, IndexConfig* config);
ErrorCode index_add_replica(ReplicatedIndex* rindex, const char* host, int port,
                           bool synchronous);
ErrorCode index_remove_replica(ReplicatedIndex* rindex, const char* host, int port);
ErrorCode index_sync_replicas(ReplicatedIndex* rindex);
ErrorCode index_promote_replica(ReplicatedIndex* rindex, const char* host, int port);

// ==================== MACROS FOR INDEX OPERATIONS ====================

#define INDEX_FOREACH(manager, entry_var) \
    for ((entry_var) = (manager)->entries; (entry_var) != NULL; (entry_var) = (entry_var)->next)

#define INDEX_SAFE_FREE(ptr) \
    do { \
        if (ptr) { \
            free(ptr); \
            (ptr) = NULL; \
        } \
    } while(0)

#define INDEX_RETURN_IF_ERROR(expr) \
    do { \
        ErrorCode __result = (expr); \
        if (__result != SUCCESS) return __result; \
    } while(0)

#define INDEX_LOCK_READ(index) \
    pthread_rwlock_rdlock(&(index)->lock)

#define INDEX_LOCK_WRITE(index) \
    pthread_rwlock_wrlock(&(index)->lock)

#define INDEX_UNLOCK(index) \
    pthread_rwlock_unlock(&(index)->lock)

#define INDEX_STAT_INC(index, field) \
    do { \
        if ((index)->stats_enabled) { \
            (index)->stats.field++; \
        } \
    } while(0)

#define INDEX_STAT_ADD(index, field, value) \
    do { \
        if ((index)->stats_enabled) { \
            (index)->stats.field += (value); \
        } \
    } while(0)

#define INDEX_STAT_UPDATE_TIME(index, field) \
    do { \
        if ((index)->stats_enabled) { \
            clock_t __start = (index)->stats.field##_start; \
            clock_t __end = clock(); \
            double __elapsed = (double)(__end - __start) / CLOCKS_PER_SEC; \
            (index)->stats.avg_##field##_time = \
                ((index)->stats.avg_##field##_time * ((index)->stats.field##_count - 1) + __elapsed) / \
                (index)->stats.field##_count; \
        } \
    } while(0)

// ==================== FUNCTION TYPES FOR PLUGINS ====================
typedef Index* (*IndexCreateFunc)(const char* table_name, const char* field_name,
                                 IndexConfig* config);
typedef void (*IndexFreeFunc)(Index* index);
typedef ErrorCode (*IndexInsertFunc)(Index* index, void* key, size_t key_size,
                                    Record* record);
typedef Record* (*IndexSearchFunc)(Index* index, void* key, size_t key_size);
typedef ErrorCode (*IndexDeleteFunc)(Index* index, void* key, size_t key_size);

typedef struct IndexPlugin {
    char name[64];
    IndexType type;
    IndexCreateFunc create;
    IndexFreeFunc free;
    IndexInsertFunc insert;
    IndexSearchFunc search;
    IndexDeleteFunc delete;
    void* data;
} IndexPlugin;

// Plugin Management
ErrorCode index_register_plugin(IndexPlugin* plugin);
ErrorCode index_unregister_plugin(const char* plugin_name);
IndexPlugin* index_get_plugin(IndexType type);

// ==================== GLOBAL INDEX REGISTRY ====================
typedef struct IndexRegistry {
    IndexPlugin** plugins;
    int plugin_count;
    int plugin_capacity;
    pthread_rwlock_t lock;
} IndexRegistry;

IndexRegistry* index_registry_get();
ErrorCode index_registry_init();
ErrorCode index_registry_cleanup();

#endif // INDEX_H