#ifndef CONFIG_H
#define CONFIG_H
// In config.h or as constants in index.c:
#define DEFAULT_HASH_CAPACITY 1024
#define DEFAULT_LOAD_FACTOR 0.75
#define DEFAULT_SPATIAL_CAPACITY 100
#define CONCURRENT_INDEXING true  // or false based on your configuration
// config.h
#ifndef CONFIG_H
#define CONFIG_H

// Database configuration
#define DEFAULT_PAGE_SIZE 4096
#define DEFAULT_CACHE_SIZE 1000
#define DEFAULT_MAX_CONNECTIONS 10
#define DEFAULT_AUTO_VACUUM false
#define DEFAULT_ENABLE_QUERY_CACHE true
#define DEFAULT_LOG_QUERIES false
#define DEFAULT_TRANSACTION_TIMEOUT 30 // seconds

// Storage configuration
#define DATA_FILE_EXTENSION ".db"
#define INDEX_FILE_EXTENSION ".idx"
#define LOG_FILE_EXTENSION ".log"

// Performance tuning
#define ENABLE_INDEXING true
#define ENABLE_CACHING true
#define ENABLE_PARALLEL_QUERIES false
#define MAX_QUERY_THREADS 4

// Memory management
#define MEMORY_POOL_SIZE 1024 * 1024 // 1MB
#define MAX_MEMORY_USAGE 1024 * 1024 * 1024 // 1GB

// Cache policies
#define CACHE_LRU 0
#define CACHE_FIFO 1
#define CACHE_LFU 2

#endif // CONFIG_H
// Error codes (if not already defined)
#define ERROR_INVALID_ARGUMENT -1
#define ERROR_MEMORY -2
// ... other error codes
// ==================== DATABASE CONFIGURATION ====================
#define MAX_FIELD_LEN 256

#define LOAD_FACTOR 0.75
#define MAX_TABLES 100
#define MAX_RECORDS_PER_TABLE 10000
#define MAX_FIELDS_PER_TABLE 50

// ==================== INDEXING CONFIGURATION ====================
#define INITIAL_HASH_SIZE 101
#define DEFAULT_BTREE_DEGREE 3
#define DEFAULT_SKIPLIST_MAX_LEVEL 16
#define MAX_INDEXES_PER_TABLE 10
#define INDEX_RESIZE_THRESHOLD 0.75
#define INDEX_COMPACTION_THRESHOLD 0.25

// ==================== MEMORY CONFIGURATION ====================
#define MEMORY_POOL_SIZE (1024 * 1024)  // 1MB
#define MAX_ALLOCATION_SIZE (1024 * 100)  // 100KB
#define CACHE_LINE_SIZE 64
#define PAGE_SIZE 4096

// ==================== PERFORMANCE CONFIGURATION ====================
#define QUERY_CACHE_SIZE 100
#define MAX_CONCURRENT_QUERIES 1000
#define BATCH_INSERT_SIZE 100
#define BUFFER_POOL_SIZE (10 * 1024 * 1024)  // 10MB

// ==================== ERROR CODES & TYPES ====================
#include "types.h"

// ==================== STORAGE TYPES ====================
typedef enum {
    STORAGE_MEMORY,
    STORAGE_MMAP,
    STORAGE_FILE
} StorageType;

// ==================== CACHE POLICIES ====================
typedef enum {
    CACHE_LRU,      // Least Recently Used
    CACHE_LFU,      // Least Frequently Used
    CACHE_FIFO,     // First In First Out
    CACHE_RANDOM    // Random Replacement
} CachePolicy;

// ==================== LOGGING LEVELS ====================
typedef enum {
    LOG_NONE,
    LOG_ERROR,
    LOG_WARN,
    LOG_INFO,
    LOG_DEBUG,
    LOG_TRACE
} LogLevel;

// ==================== TRANSACTION ISOLATION LEVELS ====================
typedef enum {
    ISOLATION_READ_UNCOMMITTED,
    ISOLATION_READ_COMMITTED,
    ISOLATION_REPEATABLE_READ,
    ISOLATION_SERIALIZABLE
} IsolationLevel;

// ==================== COMPRESSION TYPES ====================
typedef enum {
    COMPRESSION_NONE,
    COMPRESSION_GZIP,
    COMPRESSION_LZ4,
    COMPRESSION_ZSTD
} CompressionType;

// ==================== ENCRYPTION TYPES ====================
typedef enum {
    ENCRYPTION_NONE,
    ENCRYPTION_AES128,
    ENCRYPTION_AES256,
    ENCRYPTION_CHACHA20
} EncryptionType;

// ==================== QUERY OPTIMIZER SETTINGS ====================
#define OPTIMIZER_ENABLE_JOIN_REORDERING 1
#define OPTIMIZER_ENABLE_INDEX_ONLY_SCANS 1
#define OPTIMIZER_ENABLE_PREDICATE_PUSHDOWN 1
#define OPTIMIZER_ENABLE_PARTITION_PRUNING 1
#define OPTIMIZER_MAX_PLAN_DEPTH 20

// ==================== CONCURRENCY SETTINGS ====================
#define LOCK_TIMEOUT_MS 5000
#define DEADLOCK_CHECK_INTERVAL 1000
// Transaction settings
#define MAX_TRANSACTION_LEVEL 10
#define MAX_TRANSACTION_RETRIES 3
#define DEFAULT_ISOLATION_LEVEL ISOLATION_READ_COMMITTED

// ==================== STATISTICS COLLECTION ====================
#define STATS_SAMPLE_RATE 0.01  // 1% sampling
#define STATS_UPDATE_INTERVAL 1000  // Update every 1000 operations
#define HISTOGRAM_BUCKETS 100

// ==================== BACKUP & RECOVERY ====================
#define CHECKPOINT_INTERVAL 60  // Seconds
#define WAL_SEGMENT_SIZE (16 * 1024 * 1024)  // 16MB
#define MAX_WAL_SEGMENTS 10
#define BACKUP_RETENTION_DAYS 7

// ==================== NETWORK SETTINGS ====================
#define DEFAULT_PORT 8080
#define MAX_CONNECTIONS 1000
#define CONNECTION_TIMEOUT 30  // Seconds
#define MAX_REQUEST_SIZE (10 * 1024 * 1024)  // 10MB
#define KEEPALIVE_TIMEOUT 75  // Seconds

// ==================== SECURITY SETTINGS ====================
#define PASSWORD_HASH_ITERATIONS 100000
#define SALT_SIZE 32
#define TOKEN_EXPIRY_HOURS 24
#define MAX_FAILED_ATTEMPTS 5
#define ACCOUNT_LOCKOUT_MINUTES 15

// ==================== AUDIT LOGGING ====================
#define AUDIT_LOG_QUERIES 1
#define AUDIT_LOG_CONNECTIONS 1
#define AUDIT_LOG_DATA_CHANGES 1
#define AUDIT_LOG_FILE "audit.log"
#define AUDIT_LOG_ROTATE_SIZE (100 * 1024 * 1024)  // 100MB

// ==================== MONITORING ====================
#define METRICS_UPDATE_INTERVAL 5  // Seconds
#define METRICS_RETENTION_HOURS 24
#define ALERT_THRESHOLD_MEMORY 0.9  // 90%
#define ALERT_THRESHOLD_CPU 0.8     // 80%
#define ALERT_THRESHOLD_DISK 0.85   // 85%

// ==================== QUERY EXECUTION ====================
#define MAX_RESULT_SET_SIZE (100 * 1024 * 1024)  // 100MB
#define QUERY_TIMEOUT_SECONDS 30
#define MAX_SUBQUERY_DEPTH 10
#define TEMP_STORAGE_THRESHOLD (10 * 1024 * 1024)  // 10MB

// ==================== REPLICATION ====================
#define REPLICATION_HEARTBEAT_INTERVAL 1  // Second
#define REPLICATION_TIMEOUT 10  // Seconds
#define MAX_REPLICATION_LAG 1000  // Operations
#define REPLICATION_RETRY_INTERVAL 5  // Seconds

// ==================== DEBUG & DEVELOPMENT ====================
#ifdef DEBUG
    #define ASSERT_ENABLED 1
    #define LOG_LEVEL LOG_DEBUG
    #define PROFILE_QUERIES 1
    #define VALIDATE_DATASTRUCTURES 1
#else
    #define ASSERT_ENABLED 0
    #define LOG_LEVEL LOG_WARN
    #define PROFILE_QUERIES 0
    #define VALIDATE_DATASTRUCTURES 0
#endif

// ==================== PLATFORM DETECTION ====================
#if defined(_WIN32) || defined(_WIN64)
    #define PLATFORM_WINDOWS 1
    #define PATH_SEPARATOR '\\'
#elif defined(__APPLE__)
    #define PLATFORM_MACOS 1
    #define PATH_SEPARATOR '/'
#elif defined(__linux__)
    #define PLATFORM_LINUX 1
    #define PATH_SEPARATOR '/'
#else
    #define PLATFORM_UNKNOWN 1
    #define PATH_SEPARATOR '/'
#endif

// ==================== COMPILER HINTS ====================
#if defined(__GNUC__) || defined(__clang__)
    #define LIKELY(x)       __builtin_expect(!!(x), 1)
    #define UNLIKELY(x)     __builtin_expect(!!(x), 0)
    #define ALWAYS_INLINE   __attribute__((always_inline))
    #define NOINLINE        __attribute__((noinline))
    #define PACKED          __attribute__((packed))
    #define HOT             __attribute__((hot))
    #define COLD            __attribute__((cold))
#else
    #define LIKELY(x)       (x)
    #define UNLIKELY(x)     (x)
    #define ALWAYS_INLINE
    #define NOINLINE
    #define PACKED
    #define HOT
    #define COLD
#endif

// ==================== ALIGNMENT MACROS ====================
#define ALIGN_UP(x, alignment) (((x) + (alignment) - 1) & ~((alignment) - 1))
#define ALIGN_DOWN(x, alignment) ((x) & ~((alignment) - 1))
#define IS_ALIGNED(x, alignment) (((x) & ((alignment) - 1)) == 0)

// ==================== BIT OPERATIONS ====================
#define BIT(n) (1ULL << (n))
#define SET_BIT(var, n) ((var) |= BIT(n))
#define CLEAR_BIT(var, n) ((var) &= ~BIT(n))
#define TOGGLE_BIT(var, n) ((var) ^= BIT(n))
#define TEST_BIT(var, n) (((var) & BIT(n)) != 0)

// ==================== MATH CONSTANTS ====================
#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif
#define DEG_TO_RAD(x) ((x) * M_PI / 180.0)
#define RAD_TO_DEG(x) ((x) * 180.0 / M_PI)

// ==================== STRING UTILITIES ====================
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define CONCAT(a, b) a##b
#define CONCAT3(a, b, c) a##b##c

// ==================== VERSION INFORMATION ====================
#define LUMINIX_VERSION_MAJOR 1
#define LUMINIX_VERSION_MINOR 0
#define LUMINIX_VERSION_PATCH 0
#define LUMINIX_VERSION_STRING TOSTRING(LUMINIX_VERSION_MAJOR) "." \
                               TOSTRING(LUMINIX_VERSION_MINOR) "." \
                               TOSTRING(LUMINIX_VERSION_PATCH)

// ==================== FEATURE FLAGS ====================
#define FEATURE_INDEXING 1
#define FEATURE_TRANSACTIONS 0
#define FEATURE_REPLICATION 0
#define FEATURE_COMPRESSION 0
#define FEATURE_ENCRYPTION 0
#define FEATURE_FULLTEXT_SEARCH 0
#define FEATURE_SPATIAL_INDEX 0
#define FEATURE_JSON_PATH 0

// ==================== VALIDATION MACROS ====================
#define VALIDATE_PTR(ptr) \
    do { \
        if (UNLIKELY((ptr) == NULL)) { \
            return ERROR_INVALID_INPUT; \
        } \
    } while(0)

#define VALIDATE_RANGE(val, min, max) \
    do { \
        if (UNLIKELY((val) < (min) || (val) > (max))) { \
            return ERROR_INVALID_INPUT; \
        } \
    } while(0)

#define VALIDATE_NOT_NULL(ptr) \
    do { \
        if (UNLIKELY((ptr) == NULL)) { \
            fprintf(stderr, "Error: NULL pointer at %s:%d\n", __FILE__, __LINE__); \
            return ERROR_INVALID_INPUT; \
        } \
    } while(0)

// ==================== LOGGING MACROS ====================
#define LOG_ERROR(fmt, ...) \
    do { \
        if (LOG_LEVEL >= LOG_ERROR) { \
            fprintf(stderr, "[ERROR] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
        } \
    } while(0)

#define LOG_WARN(fmt, ...) \
    do { \
        if (LOG_LEVEL >= LOG_WARN) { \
            fprintf(stderr, "[WARN] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
        } \
    } while(0)

#define LOG_INFO(fmt, ...) \
    do { \
        if (LOG_LEVEL >= LOG_INFO) { \
            fprintf(stdout, "[INFO] " fmt "\n", ##__VA_ARGS__); \
        } \
    } while(0)

#define LOG_DEBUG(fmt, ...) \
    do { \
        if (LOG_LEVEL >= LOG_DEBUG) { \
            fprintf(stdout, "[DEBUG] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
        } \
    } while(0)

// ==================== ASSERT MACROS ====================
#if ASSERT_ENABLED
    #define ASSERT(cond, msg) \
        do { \
            if (UNLIKELY(!(cond))) { \
                fprintf(stderr, "Assertion failed: %s\nFile: %s\nLine: %d\n", \
                        msg, __FILE__, __LINE__); \
                abort(); \
            } \
        } while(0)
    
    #define ASSERT_PTR(ptr) ASSERT((ptr) != NULL, "Pointer is NULL")
    #define ASSERT_RANGE(val, min, max) ASSERT((val) >= (min) && (val) <= (max), "Value out of range")
#else
    #define ASSERT(cond, msg) ((void)0)
    #define ASSERT_PTR(ptr) ((void)0)
    #define ASSERT_RANGE(val, min, max) ((void)0)
#endif

// ==================== PERFORMANCE MACROS ====================
#define TIMER_START(name) \
    clock_t CONCAT(timer_start_, name) = clock()

#define TIMER_END(name) \
    do { \
        clock_t CONCAT(timer_end_, name) = clock(); \
        double CONCAT(timer_elapsed_, name) = \
            (double)(CONCAT(timer_end_, name) - CONCAT(timer_start_, name)) / CLOCKS_PER_SEC; \
        LOG_DEBUG("Timer %s: %.6f seconds", #name, CONCAT(timer_elapsed_, name)); \
    } while(0)

// ==================== MEMORY MACROS ====================
#define ALLOC(type, count) \
    (type*)calloc((count), sizeof(type))

#define REALLOC(ptr, type, count) \
    (type*)realloc((ptr), (count) * sizeof(type))

#define FREE(ptr) \
    do { \
        free(ptr); \
        (ptr) = NULL; \
    } while(0)

#define MEMSET_ZERO(ptr, size) \
    memset((ptr), 0, (size))

// ==================== CIRCULAR BUFFER MACROS ====================
#define CBUF_NEXT(idx, size) (((idx) + 1) % (size))
#define CBUF_PREV(idx, size) (((idx) - 1 + (size)) % (size))
#define CBUF_FULL(head, tail, size) (CBUF_NEXT(head, size) == tail)
#define CBUF_EMPTY(head, tail) ((head) == (tail))
#define CBUF_SIZE(head, tail, size) (((head) - (tail) + (size)) % (size))

// ==================== MIN/MAX MACROS ====================
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))
#define ABS(x) ((x) < 0 ? -(x) : (x))

// ==================== ENDIANNESS MACROS ====================
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    #define IS_LITTLE_ENDIAN 1
    #define IS_BIG_ENDIAN 0
#elif defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    #define IS_LITTLE_ENDIAN 0
    #define IS_BIG_ENDIAN 1
#else
    #error "Unknown endianness"
#endif

// ==================== HASH CONSTANTS ====================
#define HASH_PRIME 31
#define HASH_SEED 5381
#define FNV_OFFSET_BASIS 2166136261U
#define FNV_PRIME 16777619U

// ==================== DATETIME CONSTANTS ====================
#define SECONDS_PER_DAY 86400
#define SECONDS_PER_HOUR 3600
#define SECONDS_PER_MINUTE 60
#define MILLISECONDS_PER_SECOND 1000
#define MICROSECONDS_PER_SECOND 1000000
#define NANOSECONDS_PER_SECOND 1000000000

// ==================== DEFAULT VALUES ====================
#define DEFAULT_STRING_VALUE ""
#define DEFAULT_INT_VALUE 0
#define DEFAULT_FLOAT_VALUE 0.0f
#define DEFAULT_DOUBLE_VALUE 0.0
#define DEFAULT_BOOL_VALUE false
#define DEFAULT_TIMESTAMP 0

#endif // CONFIG_H